#include "Interceptor.h"
#include "ProcessToken.h"
#include "Coroutine.h"
#include <NktHookLib.h>
#include <TlHelp32.h>
#include <Windows.h>
#include <iomanip>
#include <iostream>

#define DEFINE_STARTUPINFO_PROCESS_INFORMATION \
	STARTUPINFO si; \
	PROCESS_INFORMATION pi; \
	zero_structure(si); \
	zero_structure(pi); \
	si.cb = sizeof(STARTUPINFO); \
	si.wShowWindow = SW_HIDE;

#ifndef ERROR_CHILD_PROCESS_BLOCKED
const DWORD ERROR_CHILD_PROCESS_BLOCKED = 367;
#endif
const DWORD ERROR_GENERIC_ERROR = 0xFFFFFFFB;

int process_bits(HANDLE process){
	switch (NktHookLibHelpers::GetProcessPlatform(process)){
		case NKTHOOKLIB_ProcessPlatformX86:
			return 32;
		case NKTHOOKLIB_ProcessPlatformX64:
			return 64;
		default:
			return 0;
	}
}

enum class ProcessIntegrity{
	Unknown = -1,
	Untrusted = 0,
	Low = 1,
	Medium = 2,
	MediumPlus = 3,
	High = 4,
	System = 5,
	Protected = 6,
};

static const wchar_t *to_string(ProcessIntegrity pi){
	switch (pi){
		case ProcessIntegrity::Unknown:
			return L"Unknown";
		case ProcessIntegrity::Untrusted:
			return L"Untrusted";
		case ProcessIntegrity::Low:
			return L"Low";
		case ProcessIntegrity::Medium:
			return L"Medium";
		case ProcessIntegrity::MediumPlus:
			return L"MediumPlus";
		case ProcessIntegrity::High:
			return L"High";
		case ProcessIntegrity::System:
			return L"System";
		case ProcessIntegrity::Protected:
			return L"Protected";
	}
	return L"";
}

ProcessIntegrity to_ProcessIntegrity(DWORD value){
	if (value == ERROR_GENERIC_ERROR)
		return ProcessIntegrity::Unknown;

	static const std::pair<DWORD, ProcessIntegrity> pairs[] = {
		{SECURITY_MANDATORY_LOW_RID,               ProcessIntegrity::Untrusted},
		{SECURITY_MANDATORY_MEDIUM_RID,            ProcessIntegrity::Low},
		{SECURITY_MANDATORY_MEDIUM_PLUS_RID,       ProcessIntegrity::Medium},
		{SECURITY_MANDATORY_HIGH_RID,              ProcessIntegrity::MediumPlus},
		{SECURITY_MANDATORY_SYSTEM_RID,            ProcessIntegrity::High},
		{SECURITY_MANDATORY_PROTECTED_PROCESS_RID, ProcessIntegrity::System},
	};

	for (auto &pair : pairs)
		if (value < pair.first)
			return pair.second;

	return ProcessIntegrity::Protected;
}

DWORD to_DWORD(ProcessIntegrity pi){
	switch (pi){
		case ProcessIntegrity::Unknown:
			return SECURITY_MANDATORY_UNTRUSTED_RID;
		case ProcessIntegrity::Untrusted:
			return SECURITY_MANDATORY_UNTRUSTED_RID;
		case ProcessIntegrity::Low:
			return SECURITY_MANDATORY_LOW_RID;
		case ProcessIntegrity::Medium:
			return SECURITY_MANDATORY_MEDIUM_RID;
		case ProcessIntegrity::MediumPlus:
			return SECURITY_MANDATORY_MEDIUM_PLUS_RID;
		case ProcessIntegrity::High:
			return SECURITY_MANDATORY_HIGH_RID;
		case ProcessIntegrity::System:
			return SECURITY_MANDATORY_SYSTEM_RID;
		case ProcessIntegrity::Protected:
			return SECURITY_MANDATORY_PROTECTED_PROCESS_RID;
		default:
			return SECURITY_MANDATORY_UNTRUSTED_RID;
	}
}

int is_process_appcontainer(const ProcessToken &token){
	DWORD ret;

	const int TokenIsAppContainer = 29;
	DWORD obtained;
	if (!GetTokenInformation(token.get_handle(), (TOKEN_INFORMATION_CLASS)TokenIsAppContainer, &ret, sizeof(ret), &obtained))
		return -1;
	return !!ret;
}

DWORD Interceptor::get_process_integrity(HANDLE process, const ProcessToken &token){
	if (!token.get_valid())
		return ERROR_GENERIC_ERROR;

	std::vector<char> data(1024);
	auto label = (TOKEN_MANDATORY_LABEL *)&data[0];

	for (DWORD obtained; !GetTokenInformation(token.get_handle(), TokenIntegrityLevel, label, (DWORD)data.size(), &obtained);){
		auto error = GetLastError();
		if (error == ERROR_INSUFFICIENT_BUFFER && data.size() < (1 << 20)){
			data.resize(data.size() * 2);
			label = (TOKEN_MANDATORY_LABEL *)&data[0];
			continue;
		}
		return ERROR_GENERIC_ERROR;
	}

	auto count = *GetSidSubAuthorityCount(label->Label.Sid);

	if (!count){
		return ERROR_GENERIC_ERROR;
	}

	return *GetSidSubAuthority(label->Label.Sid, count - 1);
}

DWORD Interceptor::hook_process(HANDLE process, DWORD pid){
	auto path = this->get_process_full_path(process);
	if (!path.size() || !this->can_hook_process(pid, path.c_str()))
		return ERROR_INVALID_PARAMETER;

	this->log() << L"Hooking PID " << pid << L" path: " << path;

	DWORD error;
	std::wstring dll_path;
	error = this->get_dll_path(dll_path, process);
	if (error != ERROR_SUCCESS)
		return error;

	auto wait = this->ipc.get_connection_event(pid);

	ProcessToken token(process, TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_ADJUST_DEFAULT | TOKEN_ASSIGN_PRIMARY | TOKEN_IMPERSONATE);

	int external_hooking_mode = this->need_external_hooking(process, pid, token);
	while (true){
		switch (external_hooking_mode){
			case 0:
				SetLastError(0);
				error = NktHookLibHelpers::InjectDllByPidW(pid, dll_path.c_str(), "InitializeDll");
				if (error != 0){
					error = GetLastError();
					Coroutine::wait(0.5);
					error = NktHookLibHelpers::InjectDllByPidW(pid, dll_path.c_str(), "InitializeDll");
					if (error != 0)
						error = GetLastError();
				}
				break;
			case 1:
				error = this->do_external_hooking_1(process, pid, dll_path, token);
				break;
			case 2:
				if (this->mode != InterceptorMode::Service){
					external_hooking_mode = 3;
					continue;
				}
				error = this->do_external_hooking_2(process, pid, dll_path, token);
				if (error == ERROR_PRIVILEGE_NOT_HELD){
					external_hooking_mode = 0;
					continue;
				}
				break;
			case 3:
				error = this->do_external_hooking_3(process, pid, dll_path, token);
				break;
		}
		break;
	}
	if (error != ERROR_SUCCESS)
		wait.cancel();
	else if (!wait.complete())
		error = ERROR_TIMEOUT;

	if (error == ERROR_SUCCESS)
		this->log() << "Connection to PID " << pid << " complete.";
	else if (error == ERROR_TIMEOUT)
		this->log() << "Connection to PID " << pid << " timed out.";
	else
		this->log() << "Connection to PID " << pid << " failed with error " << error << ".";
	return error;
}

const std::wstring &Interceptor::get_dll_directory(){
	if (this->containing_directory.size())
		return this->containing_directory;
	WCHAR own_module_path[2048 + 64];

	::GetModuleFileNameW(nullptr, own_module_path, 2048);

	std::wstring full_dll_path = own_module_path;
	auto slash = full_dll_path.find_last_of(L"\\/");
	if (slash != full_dll_path.npos)
		full_dll_path = full_dll_path.substr(0, slash + 1);

	return this->containing_directory = full_dll_path;
}

DWORD Interceptor::get_dll_path(std::wstring &full_dll_path, HANDLE process){
	const wchar_t *remote_bitness;

	std::wstring *string;

	switch (process_bits(process)){
		case 32:
			remote_bitness = L"32";
			string = &this->inject_dll32;
			break;
		case 64:
			remote_bitness = L"64";
			string = &this->inject_dll64;
			break;
		default:
			return ERROR_NOT_SUPPORTED;
	}

	if (string->size()){
		full_dll_path = *string;
		return ERROR_SUCCESS;
	}

	full_dll_path = this->get_dll_directory();
	full_dll_path += L"ClipboardFirewallDll";
	full_dll_path += remote_bitness;
	full_dll_path += L".dll";
	*string = full_dll_path;

	return ERROR_SUCCESS;
}

int Interceptor::process_wont_load_third_party_images(HANDLE process){
	static const DWORD ProcessSignaturePolicy = 8;

	if (!this->GetProcessMitigationPolicy_fp)
		return -1;

	struct PROCESS_MITIGATION_BINARY_SIGNATURE_POLICY{
		union {
			DWORD Flags;
			struct{
				DWORD MicrosoftSignedOnly : 1;
				DWORD StoreSignedOnly : 1;
				DWORD MitigationOptIn : 1;
				DWORD AuditMicrosoftSignedOnly : 1;
				DWORD AuditStoreSignedOnly : 1;
				DWORD ReservedFlags : 27;
			};
		};
	};

	PROCESS_MITIGATION_BINARY_SIGNATURE_POLICY policy;
	memset(&policy, 0, sizeof(policy));
	if (!this->GetProcessMitigationPolicy_fp(process, ProcessSignaturePolicy, &policy, sizeof(policy)))
		return -1;
	return !!policy.MicrosoftSignedOnly
		|| !!policy.StoreSignedOnly
		|| !!policy.MitigationOptIn
		|| !!policy.AuditMicrosoftSignedOnly
		|| !!policy.AuditStoreSignedOnly
		;
}

int Interceptor::check_restriction(HANDLE process, DWORD pid, int default_value){
	//The process might not be fully initialized at this point, so wait a second to give it some time.
	Coroutine::wait(1);

	auto mitigation = this->process_wont_load_third_party_images(process);
	if (mitigation > 0)
		return 3;

	return default_value;
}

int Interceptor::need_external_hooking(HANDLE process, DWORD pid, const ProcessToken &token){
	if (this->windows_version.windows_version == WindowsVersion::Xp64 && this->windows_version.is_server && get_process_session_id(pid) > 0)
		return 1;

	auto container = is_process_appcontainer(token);
	auto integrity_value = get_process_integrity(process, token);
	auto integrity = to_ProcessIntegrity(integrity_value);
	if (container == 1){
		int fallback;
		if (integrity == ProcessIntegrity::Unknown || (int)integrity >= (int)ProcessIntegrity::Medium)
			fallback = 0;
		else
			fallback = 3;
		return this->check_restriction(process, pid, fallback);
	}


	if (integrity == ProcessIntegrity::Unknown)
		return 0;

	if ((int)integrity >= (int)ProcessIntegrity::Medium)
		return 0;

	return this->check_restriction(process, pid, 3);
}

std::wstring get_token_username(const ProcessToken &token){
	std::vector<char> buffer;
	buffer.resize(std::max<size_t>(1024, sizeof(TOKEN_OWNER)));
	auto owner = (TOKEN_OWNER *)&buffer[0];
	DWORD length;
	while (!GetTokenInformation(token.get_handle(), TokenUser, owner, (DWORD)buffer.size(), &length)){
		auto error = GetLastError();
		if (error == ERROR_INSUFFICIENT_BUFFER){
			buffer.resize(buffer.size() * 2);
			owner = (TOKEN_OWNER *)&buffer[0];
			continue;
		}
		std::wstringstream stream;
		stream << L"(GetTokenInformation() failed with error " << error << L")";
		return stream.str();
	}

	SID_NAME_USE sid_type;
	WCHAR name[256];
	WCHAR domain[256];
	DWORD name_size = (DWORD)array_length(name);
	DWORD domain_size = (DWORD)array_length(domain);
	if (!LookupAccountSidW(nullptr, owner->Owner, name, &name_size, domain, &domain_size, &sid_type)){
		auto error = GetLastError();
		std::wstringstream stream;
		stream << L"(LookupAccountSidW() failed with error " << error << L")";
		return stream.str();
	}
	return name;
}

int is_elevated(const ProcessToken &token){
	TOKEN_ELEVATION Elevation;
	DWORD cbSize = sizeof(TOKEN_ELEVATION);
	if (!GetTokenInformation(token.get_handle(), TokenElevation, &Elevation, sizeof(Elevation), &cbSize))
		return -1;
	return !!Elevation.TokenIsElevated;
}

const wchar_t *is_elevated_string(const ProcessToken &token){
	auto x = is_elevated(token);
	if (x < 0)
		return L"unknown";
	static const wchar_t * const string = L"non-admin";
	return string + x * 4;
}

DWORD set_token_integrity_level(ProcessToken &token, ProcessIntegrity integrity){
	SID_IDENTIFIER_AUTHORITY level_authority = SECURITY_MANDATORY_LABEL_AUTHORITY;
	PSID psid;
	if (!AllocateAndInitializeSid(&level_authority, 1, to_DWORD(integrity), 0, 0, 0, 0, 0, 0, 0, &psid))
		report_failed_win32_function("AllocateAndInitializeSid", GetLastError());

	TOKEN_MANDATORY_LABEL tml;
	tml.Label.Attributes = SE_GROUP_INTEGRITY;
	tml.Label.Sid = psid;

	if (!SetTokenInformation(token.get_handle(), TokenIntegrityLevel, &tml, sizeof(tml))){
		auto error = GetLastError();
		return error;
	}
	return ERROR_SUCCESS;
}

DWORD clear_child_process_flags(const ProcessToken &token){
	static const TOKEN_INFORMATION_CLASS TokenChildProcessFlags = (TOKEN_INFORMATION_CLASS)45;
	ULONG flags = 0;
	if (!SetTokenInformation(token.get_handle(), TokenChildProcessFlags, &flags, sizeof(flags)))
		return GetLastError();
	return ERROR_SUCCESS;
}

DWORD Interceptor::clear_child_process_flags_workaround(HANDLE process, const std::wstring &username, wchar_t *cmd){
	DEFINE_STARTUPINFO_PROCESS_INFORMATION;

	for (auto &proc : this->get_running_processes(true)){
		ProcessToken token2(proc.get_handle(), TOKEN_ALL_ACCESS);
		auto username2 = get_token_username(token2);
		if (username2 != username)
			continue;
		auto integrity = to_ProcessIntegrity(this->get_process_integrity(process, token2));
		if (!(integrity == ProcessIntegrity::Medium || integrity == ProcessIntegrity::MediumPlus || integrity == ProcessIntegrity::High))
			continue;
		auto duplicate_token2 = token2.duplicate();
		if (!duplicate_token2.get_valid())
			continue;
		auto error = set_token_integrity_level(duplicate_token2, ProcessIntegrity::Medium);
		if (error != ERROR_SUCCESS)
			continue;

		if (!CreateProcessAsUserW(duplicate_token2.get_handle(), nullptr, cmd, nullptr, nullptr, false, NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)){
			error = GetLastError();
			if (error == ERROR_CHILD_PROCESS_BLOCKED)
				continue;
			return error;
		}

		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);

		return ERROR_SUCCESS;
	}
	return ERROR_NOT_FOUND;
}

DWORD Interceptor::start_child_modified_token(
		wchar_t *command_line,
		HANDLE process,
		const ProcessToken &token,
		const std::wstring &username,
		bool normal_integrity,
		bool clear_child_process_flags){

	ProcessToken duplicate_token;
	auto token_to_use = &token;
	if (normal_integrity || clear_child_process_flags){
		duplicate_token = token.duplicate();
		if (!duplicate_token.get_valid())
			return duplicate_token.get_error();
		token_to_use = &duplicate_token;
	}

	
	if (normal_integrity){
		auto error = set_token_integrity_level(duplicate_token, ProcessIntegrity::Medium);
		if (error != ERROR_SUCCESS)
			return error;
	}

	if (clear_child_process_flags){
		auto error = ::clear_child_process_flags(duplicate_token);
		if (error != ERROR_SUCCESS){
			if (error != ERROR_INVALID_PARAMETER)
				return error;

			error = this->clear_child_process_flags_workaround(process, username, command_line);
			if (error == ERROR_SUCCESS)
				return ERROR_SUCCESS;
			return ERROR_GENERIC_ERROR;
		}
	}

	DEFINE_STARTUPINFO_PROCESS_INFORMATION;

	if (!CreateProcessAsUserW(token_to_use->get_handle(), nullptr, command_line, nullptr, nullptr, false, NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
		return GetLastError();
	
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return ERROR_SUCCESS;
}

DWORD Interceptor::start_child_normally(wchar_t *command_line){
	DEFINE_STARTUPINFO_PROCESS_INFORMATION;
	if (!CreateProcessW(nullptr, command_line, nullptr, nullptr, false, NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
		return GetLastError();
	
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return ERROR_SUCCESS;
}

std::wstring Interceptor::build_command_line1(DWORD pid, const std::wstring &dll_path){
	std::wstringstream stream;
	stream << L"\"" << this->get_dll_directory() << L"InjectDll" WBITNESS L".exe" << L"\" " << pid << L" \"" << dll_path << L"\"";
	return stream.str();
}

std::wstring Interceptor::build_command_line2(int bits, const std::wstring &dll, DWORD pid, const std::wstring &user){
	std::wstringstream stream;
	stream << L"\"" << this->get_dll_directory() << L"CFManualInjector" << bits << L".exe\" \"" << dll << L"\" " << pid << ' ' << this->own_pid << L" \"" << user << '"';
	return stream.str();
}

DWORD Interceptor::do_external_hooking_1(HANDLE process, DWORD pid, const std::wstring &dll_path, const ProcessToken &token){
	if (!token.get_valid())
		return token.get_error();
	auto cmd = this->build_command_line1(pid, dll_path);
	return this->start_child_modified_token(&cmd[0], process, token, {}, false, false);
}

DWORD Interceptor::do_external_hooking_2(HANDLE process, DWORD pid, const std::wstring &dll_path, const ProcessToken &token){
	if (!token.get_valid())
		return token.get_error();
	auto cmd = this->build_command_line1(pid, dll_path);
	return this->start_child_modified_token(&cmd[0], process, token, get_token_username(token), true, true);
}

DWORD Interceptor::do_external_hooking_3(HANDLE process, DWORD pid, const std::wstring &dll_path, const ProcessToken &token){
	auto bits = process_bits(process);
	if (!bits)
		return GetLastError();

	auto username = get_token_username(token);
	auto cmd = this->build_command_line2(bits, dll_path, pid, username);

	if (this->windows_version.windows_version == WindowsVersion::Ten)
		return this->start_child_normally(&cmd[0]);
		
	return this->start_child_modified_token(&cmd[0], process, token, username, true, true);
}

void Interceptor::hook_running_processes(){
	try{
		auto processes = this->get_running_processes(true);
		TaskWaiter tw;
		for (auto &process : processes)
			this->scheduler.add(std::make_unique<Coroutine>(CoroutineEntryPoint(*this, process, tw.get_new_event())));
		tw.wait();
	}catch (std::exception &){
	}
}

void Interceptor::hook_process(const RunningProcess &process){
	auto pid = process.get_pid();
	this->hook_process_pre(process.get_handle(), process.get_pid());
}

void Interceptor::OnProcessCreated_from_coroutine(DWORD pid){
	auto process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
	if (!process)
		return;
	auto raii_process = to_autohandle(process);
	this->hook_process_pre(process, pid);
}

void Interceptor::hook_process_pre(HANDLE process, DWORD pid){
	if (this->processes_being_hooked.find(pid) != this->processes_being_hooked.end())
		return;
	if (this->hooked_processes.find(pid) != this->hooked_processes.end())
		return;
	this->processes_being_hooked.insert(pid);
	struct EraseAfterwards{
		DWORD pid;
		decltype(this->processes_being_hooked) *m;
		~EraseAfterwards(){
			this->m->erase(this->pid);
		}
	};
	EraseAfterwards ea{pid, &this->processes_being_hooked};
	this->hooked_processes[pid] = this->hook_process(process, pid) == ERROR_SUCCESS;
}
