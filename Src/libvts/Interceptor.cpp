#include "Interceptor.h"
#include "utility.h"
#include "FileHash.h"
#include "../common/sha256.hpp"
#include "../libvts/constants.h"
#ifdef max
#undef max
#endif
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <limits>
#include <algorithm>
#include <vector>
#include <chrono>
#include <stdexcept>
#include <Sddl.h>
#include <TlHelp32.h>
#include <KnownFolders.h>
#include <Shlobj.h>
#include <iostream>
#include <filesystem>

extern const FileHash file_hashes[];

const std::map<std::wstring, GUID> known_folders = {
	{L"FOLDERID_System", FOLDERID_System},
	{L"FOLDERID_Windows", FOLDERID_Windows},
	{L"FOLDERID_ProgramData", FOLDERID_ProgramData},
};

const wchar_t * const forbidden_executables[] = {
	L"{FOLDERID_System}\\winlogon.exe",
	L"{FOLDERID_System}\\lsass.exe",
	L"{FOLDERID_System}\\svchost.exe",
	L"{FOLDERID_System}\\taskhost.exe",
	L"{FOLDERID_System}\\taskhostw.exe",
	L"{FOLDERID_System}\\taskhostex.exe",
	L"{FOLDERID_System}\\RuntimeBroker.exe",
	L"{FOLDERID_System}\\dwm.exe",
	L"{FOLDERID_System}\\csrss.exe",
	L"{FOLDERID_System}\\fontdrvhost.exe",
	L"{FOLDERID_System}\\winlogon.exe",
	L"{FOLDERID_System}\\services.exe",
	L"{FOLDERID_System}\\wininit.exe",
};

bool is_system_64(){
#ifdef _M_X64
	return true;
#else
	BOOL wow64;
	if (!IsWow64Process(GetCurrentProcess(), &wow64))
		return false;
	return !!wow64;
#endif
}

Interceptor::Interceptor(AbstractLogger &logger, InterceptorMode mode)
		: logger(&logger)
		, mode(mode)
		, scheduler(*this)
		, ipc(*this){
	CoInitialize(nullptr);
	this->own_pid = GetCurrentProcessId();
	this->check_executables();
	this->add_forbidden_executables();
	this->load_config();
	this->system64 = is_system_64();
}

Interceptor::~Interceptor(){
	this->stop();
}

bool read_registry_dword(HKEY key, const wchar_t *name, DWORD &value){
	DWORD type;
	DWORD size = sizeof(type);
	return RegQueryValueExW(key, name, nullptr, &type, (LPBYTE)&value, &size) == ERROR_SUCCESS && type == REG_DWORD;
}

bool read_registry_string(HKEY key, const wchar_t *name, std::wstring &dst){
	dst.resize(128);
	while (true){
		auto size = sizeof(wchar_t) * dst.size();
		if (size > std::numeric_limits<DWORD>::max())
			return false;
		auto value_size = (DWORD)size;
		DWORD type;
		auto result = RegQueryValueExW(key, name, nullptr, &type, (LPBYTE)&dst[0], &value_size);
		if (result == ERROR_SUCCESS){
			if (type != REG_SZ)
				return false;
			dst.resize(value_size / sizeof(wchar_t));
			if (dst.size() && !dst.back())
				dst.pop_back();
			return true;
		}
		if (result == ERROR_MORE_DATA){
			dst.resize(dst.size() * 2);
			continue;
		}
		return false;
	}
	return false;
}

bool read_registry_integer_as_string(HKEY key, const wchar_t *name, int &value){
	std::wstring s;
	if (!read_registry_string(key, name, s))
		return false;
	std::wstringstream stream(s);
	if (!(stream >> value))
		return false;
	return true;
}

WindowsVersion set_windows_version(DWORD major, DWORD minor){
	if (major == 5){
		switch (minor){
			case 0:
				return WindowsVersion::TwoThousand;
			case 1:
				return WindowsVersion::Xp;
			case 2:
				return WindowsVersion::Xp64;
		}
		return WindowsVersion::Unknown;
	}
	if (major == 6){
		switch (minor){
			case 0:
				return WindowsVersion::Vista;
			case 1:
				return WindowsVersion::Seven;
			case 2:
				return WindowsVersion::Eight;
			case 3:
				return WindowsVersion::EightPointOne;
		}
		return WindowsVersion::Unknown;
	}
	if (major == 10 && minor == 0)
		return WindowsVersion::Ten;
	return WindowsVersion::Unknown;
}

WindowsVersionInformation get_windows_version(){
	OSVERSIONINFOEX version_info;
	version_info.dwOSVersionInfoSize = sizeof(version_info);
	GetVersionEx((LPOSVERSIONINFO)&version_info);

	WindowsVersionInformation ret;
	ret.is_server = version_info.wProductType == VER_NT_SERVER;
	ret.release_id = 0;

	ret.windows_version = set_windows_version(version_info.dwMajorVersion, version_info.dwMinorVersion);

	if ((int)ret.windows_version >= (int)WindowsVersion::Eight){
		HKEY key;
		if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &key) == ERROR_SUCCESS){
			autohkey_t ak(key);
			DWORD major, minor;
			if (read_registry_dword(key, L"CurrentMajorVersionNumber", major) && read_registry_dword(key, L"CurrentMinorVersionNumber", minor))
				ret.windows_version = set_windows_version(major, minor);

			if ((int)ret.windows_version < (int)WindowsVersion::Ten || !read_registry_integer_as_string(key, L"ReleaseId", ret.release_id))
				ret.release_id = 0;
		}
	}

	return ret;
}

void Interceptor::start(){
	if (this->running)
		return;

	this->stopping = false;

	{
		auto kernel32 = LoadLibraryW(L"kernel32.dll");
		if (kernel32){
			this->QueryFullProcessImageNameW_fp = (QueryFullProcessImageNameW_f)GetProcAddress(kernel32, "QueryFullProcessImageNameW");
			this->GetProcessMitigationPolicy_fp = (GetProcessMitigationPolicy_f)GetProcAddress(kernel32, "GetProcessMitigationPolicy");
		}
	}

	this->windows_version = get_windows_version();

	bool use_fallback = false;
	if (!this->Initialize(0)){
		auto error = GetLastError();
		if (!this->fallback_to_polling()){
			std::stringstream stream;
			stream << "Deviare monitor failed to initialize with error " << error;
			throw std::runtime_error(stream.str());
		}
		use_fallback = true;
	}

	this->hook_running_processes();

	this->running = true;
	if (use_fallback)
		this->process_polling_thread = this->create_thread([this](){ this->process_polling_thread_function(); });
}

void Interceptor::stop(){
	bool old_value = false;
	if (!this->stopping.compare_exchange_strong(old_value, true))
		return;
	if (this->running){
		safe_join(this->process_polling_thread);
		safe_join(this->configuration_monitor_thread);
		this->running = false;
	}
	this->stopping = false;
}

static void signal_critical_event(const wchar_t *event_name, const char *extra_error = nullptr){
#define EVENT_NOTIFICATION_FAILED "Could not open event in configuration reloading thread."
	auto event = ::OpenEventW(EVENT_MODIFY_STATE, false, event_name);
	if (!event){
		if (!extra_error)
			throw std::runtime_error(EVENT_NOTIFICATION_FAILED);
		throw std::runtime_error((std::string)"While handling " + extra_error + ", encountered another error: " EVENT_NOTIFICATION_FAILED);
	}
	auto raii_res_event = to_autohandle(event);
	SetEvent(event);
}

static std::vector<UniqueProcess> to_set(const std::vector<RunningProcess> &processes){
	std::vector<UniqueProcess> ret;
	for (auto &p : processes)
		ret.push_back(p);
	std::sort(ret.begin(), ret.end());
	return ret;
}

template <typename T>
std::vector<T> set_diff(const std::vector<T> &x, const std::vector<T> &y){
	std::vector<T> ret(x.size());
	auto it = std::set_difference(x.begin(), x.end(), y.begin(), y.end(), ret.begin());
	ret.resize(it - ret.begin());
	return ret;
}

void Interceptor::process_polling_thread_function(){
	auto set = to_set(this->get_running_processes(true));
	while (this->running){
		for (int i = 4; i--;){
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
			if (!this->running)
				return;
		}

		auto new_set = to_set(this->get_running_processes(true));
		auto diff_a = set_diff(set, new_set);
		auto diff_b = set_diff(new_set, set);
		
		for (auto &p : diff_a)
			this->OnProcessDestroyed(p.pid);

		for (auto &p : diff_b)
			this->OnProcessCreated(p.pid);

		set = std::move(new_set);
	}
}

std::vector<RunningProcess> Interceptor::get_running_processes(bool lightweight){
	std::vector<RunningProcess> ret;

	PROCESSENTRY32 pe32;

	auto snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE)
		return ret;
	auto raii_snap = to_autohandle(snapshot);

	pe32.dwSize = sizeof(PROCESSENTRY32);

	if (!::Process32First(snapshot, &pe32)){
		::CloseHandle(snapshot);
		return ret;
	}

	do{
		auto pid = pe32.th32ProcessID;
		auto process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);

		if (!process)
			continue;

		auto raii_process = to_autohandle(process);
		std::wstring filename, username;
		if (!lightweight){
			filename = this->get_process_full_path(process);
			if (!filename.size())
				continue;
			username = get_process_user(process);
		}

		ret.emplace_back(std::move(raii_process), pid, std::move(filename), std::move(username));
	}while (Process32Next(snapshot, &pe32));

	return ret;
}

static bool contains_session(const std::vector<Payload> &ps, const Payload &p){
	return std::find_if(ps.begin(), ps.end(), [&p](const Payload &p2){ return p2.session == p.session; }) != ps.end();
}

void Interceptor::on_copy_begin(const Payload &payload, const std::shared_ptr<OutgoingSharedMemory<return_shared_memory_size>> &connection){
	bool delay;
	{
		LOCK_MUTEX(this->copies_in_progress_mutex);
		delay = this->copies_in_progress.find(payload.session) != this->copies_in_progress.end();
		if (!delay)
			this->copies_in_progress[payload.session] = {payload, connection};
	}
	if (!delay){
		this->log() << "PID " << payload.process << " from session " << payload.session << " begins copy.";
		connection->send(RequestType::Continue);
	}else{
		this->log() << "PID " << payload.process << " from session " << payload.session << " wants to begin copy but will wait.";
		LOCK_MUTEX(this->copies_waiting_mutex);
		auto it = this->copies_waiting.find(payload.session);
		if (it == this->copies_waiting.end())
			it = this->copies_waiting.insert({payload.session, {}}).first;
		it->second.push_back({payload, connection});
	}
}

static bool copy_succeeded(const Payload &payload){
	static_assert(sizeof(payload.extra_data) >= 1, "Oops!");
	return !!payload.extra_data[0];
}

std::shared_ptr<OutgoingSharedMemory<return_shared_memory_size>> Interceptor::internal_on_copy_end(const Payload &payload, const std::shared_ptr<OutgoingSharedMemory<return_shared_memory_size>> &connection){
	std::shared_ptr<OutgoingSharedMemory<return_shared_memory_size>> ret;

	LOCK_MUTEX(this->copies_in_progress_mutex);
	auto it = this->copies_in_progress.find(payload.session);
	if (it == this->copies_in_progress.end()){
		//This should never happen.
		return ret;
	}

	{
		LOCK_MUTEX(this->copies_waiting_mutex);
		auto it2 = this->copies_waiting.find(payload.session);
		if (it2 != this->copies_waiting.end() && it2->second.size()){
			auto object = it2->second.front();
			it2->second.pop_front();
			it->second = object;
			this->log() << "PID " << object.payload.process << " from session " << object.payload.session << " begins copy (resumed).";
			ret = object.connection;
		}
	}

	this->copies_in_progress.erase(it);

	if (copy_succeeded(payload))
		this->set_last_copy(payload.process, payload.thread, payload.session, payload.process_unique_id);
	else
		this->log() << "PID " << payload.process << " from session " << payload.session << " failed to update the clipboard.";

	{
		LOCK_MUTEX(this->pastes_waiting_mutex);
		auto it2 = this->pastes_waiting.find(payload.session);
		if (it2 != this->pastes_waiting.end()){
			auto &queue = it2->second;
			for (; queue.size(); queue.pop_front()){
				auto &copy = queue.front();
				std::wstring reader;
				std::wstring explanation;
				bool ok = this->check_copy(copy.payload.process, copy.payload.session, copy.payload.process_unique_id, explanation, &reader) == FirewallPolicy::Allow;
				this->log() << "PID " << copy.payload.process << " (" << reader << ") from session " << copy.payload.session << " was waiting while pasting. Wait result: paste is "
					<< (ok ? "" : "not ") << "allowed. Explanation: " << explanation;

				copy.connection->send(ok ? RequestType::Continue : RequestType::Cancel);
			}
		}
	}

	return ret;
}

void Interceptor::on_copy_end(const Payload &payload, const std::shared_ptr<OutgoingSharedMemory<return_shared_memory_size>> &connection){
	auto also = this->internal_on_copy_end(payload, connection);
	if (also)
		also->send(RequestType::Continue);
	connection->send(RequestType::Continue);
}

void Interceptor::on_paste(const Payload &payload, const std::shared_ptr<OutgoingSharedMemory<return_shared_memory_size>> &connection){
	bool delay;
	bool ok = false;
	std::wstring reader, explanation;
	{
		LOCK_MUTEX(this->copies_in_progress_mutex);
		delay = this->copies_in_progress.find(payload.session) != this->copies_in_progress.end();
		if (!delay)
			ok = this->check_copy(payload.process, payload.session, payload.process_unique_id, explanation, &reader) == FirewallPolicy::Allow;
	}
	if (!delay){
		this->log() << "PID " << payload.process << " (" << reader << ") from session " << payload.session << " wants to paste. Result: paste is "
			<< (ok ? "" : "not ") << "allowed. Explanation: " << explanation;
		connection->send(ok ? RequestType::Continue : RequestType::Cancel);
	}else{
		this->log() << "PID " << payload.process << " from session " << payload.session << " wants to paste but the session is locked, so it will have to wait.";
		LOCK_MUTEX(this->pastes_waiting_mutex);
		auto it = this->pastes_waiting.find(payload.session);
		if (it == this->pastes_waiting.end())
			it = this->pastes_waiting.insert({payload.session, {}}).first;
		it->second.push_back({payload, connection});
	}
}

void Interceptor::set_last_copy(std::uint32_t pid, std::uint32_t tid, std::uint32_t session, std::uint64_t unique_id){
	auto filename = this->get_process_full_path(pid);
	if (!filename.size()){
		this->log() << "PID " << pid << " from session " << session << " successfully updated the clipboard, but its executable name could not be determined.";
		return;
	}
	this->log() << L"PID " << pid << L" from session " << session << L", executable " << filename << L" successfully updated the clipboard.";
	LOCK_MUTEX(this->last_clipboard_writers_mutex);
	this->last_clipboard_writers[session] = ClipboardWriter(std::move(filename), pid, tid, session, unique_id);
}

FirewallPolicy Interceptor::check_copy(std::uint32_t pid, std::uint32_t session, std::uint64_t unique_id, std::wstring &explanation, std::wstring *preader){
	std::wstring last_writer;
	{
		LOCK_MUTEX(this->last_clipboard_writers_mutex);
		auto it = this->last_clipboard_writers.find(session);
		if (it != this->last_clipboard_writers.end()){
			if (it->second.pid == pid && it->second.unique_id == unique_id){
				explanation = L"source and destination are the same process";
				//Always allow a process to copy to itself.
				return FirewallPolicy::Allow;
			}
			last_writer = it->second.path;
		}
	}
	auto reader = this->get_process_full_path(pid);
	auto ret = this->run_permissions_logic(last_writer, reader, explanation);
	if (preader)
		*preader = std::move(reader);
	return ret;
}

static std::string hash_file(const wchar_t *path, autohandle_t &file){
	Sha256 hash;
	auto handle = open_file_for_normal_read(path);
	if (handle == INVALID_HANDLE_VALUE){
		auto error = GetLastError();
		report_failed_win32_function("CreateFile", error, "checking executable files");
	}
	file.reset(handle);
	const DWORD buffer_size = 1 << 20;
	std::unique_ptr<std::uint8_t[]> buffer(new std::uint8_t[buffer_size]);
	while (true){
		DWORD bytes_read;
		if (!ReadFile(handle, buffer.get(), buffer_size, &bytes_read, nullptr)){
			auto error = GetLastError();
			if (error == ERROR_HANDLE_EOF)
				break;
			report_failed_win32_function("ReadFile", error, "checking executable files");
		}
		if (!bytes_read)
			break;
		hash.update(buffer.get(), bytes_read);
	}
	return to_string(hash.get_digest());
}

void Interceptor::check_executables(){
	auto &directory = this->get_dll_directory();
	for (auto p = file_hashes; p->path; p++){
		auto path = directory + p->path;
		autohandle_t file;
		auto hash = hash_file(path.c_str(), file);
		if (hash != p->sha256sum){
			this->log() << path << L" failed check. Expected: " << p->sha256sum << L", actual: " << hash.c_str();
			throw std::runtime_error("Integrity verification failed for a file");
		}
		this->special_executables.insert(get_file_guid(file));
		this->locked_executables.emplace_back(std::move(file));
	}
}

bool Interceptor::can_hook_process(DWORD pid, const wchar_t *path){
	if (pid == this->own_pid)
		return false;
#if 0
	size_t offset = 0;
	for (int i = 0; path[i]; i++)
		if (path[i] == '\\' || path[i] == '/') 
			offset = i + 1;
	return !case_insensitive_compare<wchar_t>(path + offset, L"Time.exe");
#endif

	auto handle = open_file_for_normal_read(path);
	if (handle == INVALID_HANDLE_VALUE)
		return true;
	autohandle_t file(handle);
	return this->special_executables.find(get_file_guid(file)) == this->special_executables.end();
}

static std::wstring get_known_folder(const GUID &id){
	PWSTR pointer;
	auto result = SHGetKnownFolderPath(id, 0, nullptr, &pointer);
	if (!SUCCEEDED(result)){
		std::stringstream stream;
		stream << "SHGetKnownFolderPath() failed with error 0x" << std::hex << std::setw(8) << std::setfill('0') << result;
		throw std::runtime_error(stream.str());
	}
	std::wstring ret(pointer);
	CoTaskMemFree(pointer);
	return ret;
}

static std::wstring process_path(const std::wstring &s){
	std::wstring ret;
	std::wstring identifier;

	enum class State{
		DefaultState,
		SeenOpeningBrace,
		ReadingIdentifier,
	};

	State state = State::DefaultState;
	for (auto c : s){
		switch (state){
			case State::DefaultState:
				if (c == '{'){
					state = State::SeenOpeningBrace;
					continue;
				}
				ret += c;
				break;
			case State::SeenOpeningBrace:
				if (c == '{'){
					ret += '{';
					state = State::DefaultState;
					continue;
				}
				identifier.clear();
				state = State::ReadingIdentifier;
			case State::ReadingIdentifier:
				if (c == '}'){
					auto it = known_folders.find(identifier);
					if (it == known_folders.end())
						throw std::runtime_error("Unknown folder");
					ret += get_known_folder(it->second);
					state = State::DefaultState;
					continue;
				}
				identifier += c;
				break;
		}
	}

	return ret;
}

void Interceptor::add_forbidden_executables(){
	for (auto &path : forbidden_executables){
		auto path2 = process_path(path);
		auto file = open_file_for_normal_read2(path2, true);
		if (!file)
			continue;
		auto guid = get_file_guid(file);
		this->special_executables.insert(guid);
	}
}

LogLine Interceptor::log(){
	return LogLine(*this->logger);
}

std::wstring get_config_directory(){
	try{
		std::filesystem::path root = get_known_folder(FOLDERID_ProgramData);
		if (!std::filesystem::is_directory(root))
			return {};

		auto nektra = root / SERVICE_DISPLAY_NAME;

		if (!std::filesystem::is_directory(nektra)){
			std::filesystem::create_directory(nektra);
			return {};
		}

		return (nektra / "config.txt").wstring();
	}catch (std::exception &){
		return {};
	}
}

void Interceptor::load_config(){
	auto config_path = get_config_directory();
	this->log() << L"Loading configuration from " << config_path;
	try{
		this->config = FirewallConfiguration::parse_config_file(config_path.c_str());
	}catch (std::exception &e){
		this->log() << "Configuration error: " << e.what();
		throw;
	}
	this->log() << "Configuration description:\r\n" << this->config.describe_rules();
}
