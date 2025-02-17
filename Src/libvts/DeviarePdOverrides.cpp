#include "Interceptor.h"
#include "utility.h"
#include <Psapi.h>
#include <iomanip>
#include <Sddl.h>
#include "../common/utility.h"

std::wstring Interceptor::get_process_full_path(HANDLE hProcess, bool throwing){
	const DWORD n = 4096;
	wchar_t szProcessNameW[n] = L"<unknown>";
	DWORD size = n;

	if (!this->QueryFullProcessImageNameW_fp) {
		int i = 0;
		while (::GetModuleFileNameExW(hProcess, nullptr, szProcessNameW, size) == 0) {
			Sleep(10);
			if (i++ > 100){
				auto error = GetLastError();
				if (throwing)
					report_failed_win32_function("GetModuleFileNameExW", error, "getting process path");
				return {};
			}
		}
	}else if (!this->QueryFullProcessImageNameW_fp(hProcess, 0, szProcessNameW, &size)){
		auto error = GetLastError();
		if (throwing)
			report_failed_win32_function("QueryFullProcessImageNameW", error, "getting process path");
		return {};
	}

	std::wstring ret = szProcessNameW;
	change_backslashes(ret);
	return ret;
}

std::wstring Interceptor::get_process_full_path(std::uint32_t pid, bool throwing){
	auto process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
	if (!process)
		return {};
	auto raii_process = to_autohandle(process);
	return this->get_process_full_path(process, throwing);
}

std::wstring get_process_user(HANDLE hProcess, bool throwing){
	DWORD length = 0;
	HANDLE token;
	if (!::OpenProcessToken(hProcess, TOKEN_QUERY, &token))
		return {};

	auto raii_token = to_autohandle(token);

	if (!GetTokenInformation(token, TokenUser, nullptr, 0, &length))
		if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER)
			return {};

	auto tu = allocate_variable_structure<TOKEN_USER>(length);

	if (!tu)
		throw std::bad_alloc();

	if (!::GetTokenInformation(token, TokenUser, tu.get(), length, &length))
		return {};

	SID_NAME_USE SidType;
	WCHAR name[256],
		domain[256];
	DWORD dwNameSize = (DWORD)array_length(name), dwDomainSize = (DWORD)array_length(domain);
	if (!LookupAccountSidW(nullptr, tu->User.Sid, name, &dwNameSize, domain, &dwDomainSize, &SidType))
		return {};

	return name;
}

std::wstring hookingOffsetToString(const std::pair<ULONGLONG, DWORD> &hookingOffset){
	std::wstringstream stream;
	const wchar_t *mode;
	switch (hookingOffset.second) {
		case 0:
			mode = L"frozen";
			break;
		case 1:
			mode = L"subtracted";
			break;
		case 2:
			mode = L"added";
			break;
		case 3:
			mode = L"not in json";
			break;
		default:
			mode = L"(invalid)";
	}

	stream
		<< (hookingOffset.first == 0 ? L"(DONT HOOK BAN=" : L"(BAN=")
		<< hookingOffset.first << L"/mode=" << mode << L")";
	return stream.str();
}

void CoroutineEntryPoint::operator()() const{
	try{
		if (!this->mode)
			this->i->OnProcessCreated_from_coroutine(this->pid);
		else{
			this->i->hook_process(*this->rp);
			SetEvent(this->event);
		}
	}catch (std::exception &e){
		this->i->log() << "CoroutineEntryPoint: Exception caught: " << e.what();
	}
}

void Interceptor::OnProcessCreated_throwing(DWORD pid){
	this->scheduler.add(std::make_unique<Coroutine>(CoroutineEntryPoint(*this, pid)));
}

void Interceptor::OnProcessDestroyed_throwing(DWORD pid){
	this->hooked_processes.erase(pid);
}

void Interceptor::OnProcessCreated(DWORD pid){
	try{
		this->OnProcessCreated_throwing(pid);
	}catch (...){}
}

void Interceptor::OnProcessDestroyed(DWORD pid){
	try{
		this->OnProcessDestroyed_throwing(pid);
	}catch (...){}
}

void Interceptor::OnThreadError(DWORD error){}
