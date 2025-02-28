#pragma once

#include "declspec.h"
#include "Scheduler.h"
#include "DeviarePD.h"
#include "utility.h"
#include "RunningProcess.h"
#include "AbstractLogger.h"
#include "IPC.h"
#include "Rules.h"
#include <atomic>
#include <sstream>
#include <thread>
#include <mutex>
#include <map>
#include <unordered_map>
#include <set>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <Windows.h>

class ProcessToken;

typedef BOOL (WINAPI *QueryFullProcessImageNameW_f)(_In_ HANDLE hProcess, _In_ DWORD dwFlags, _Out_ LPWSTR lpExeName, _Inout_ PDWORD lpdwSize); 
typedef BOOL (WINAPI *GetProcessMitigationPolicy_f)(HANDLE hProcess, DWORD MitigationPolicy, PVOID lpBuffer, SIZE_T dwLength);

enum class LogEvent{
	Low = 1,
	High = 2,
};

enum class LoggingLevel{
	Off = 0,
	Low = 1,
	High = 2,
};

enum class InterceptorMode{
	Service,
	NormalProcess,
};

enum class WindowsVersion{
	Unknown = 0,
	TwoThousand,
	Xp,
	Xp64,          // Also: Server 2003, Server 2003 R2
	Vista,         // Also: Server 2008
	Seven,         // Also: Server 2008 R2
	Eight,         // Also: Server 2012
	EightPointOne, // Also: Server 2012 R2
	Ten,           // Also: Server 2016, Server 2019
};

struct WindowsVersionInformation{
	WindowsVersion windows_version;
	int release_id;
	bool is_server;
};

#pragma warning(push)
#pragma warning(disable: 4275; disable: 4251)
class LIBVTS_EXPORT Interceptor : private CDeviarePD, public AbstractFirewall{
	AbstractLogger *logger;
	const InterceptorMode mode;
	std::atomic<bool> running = false;
	std::atomic<bool> stopping = false;
	std::thread configuration_monitor_thread,
		process_polling_thread;
	QueryFullProcessImageNameW_f QueryFullProcessImageNameW_fp = nullptr;
	GetProcessMitigationPolicy_f GetProcessMitigationPolicy_fp = nullptr;
	WindowsVersionInformation windows_version;
	LoggingLevel logging_level = LoggingLevel::Low;
	std::unordered_map<DWORD, bool> hooked_processes;
	std::unordered_set<DWORD> processes_being_hooked;
	bool system64;
	DWORD own_pid;
	std::wstring containing_directory,
		inject_dll32,
		inject_dll64;
	Scheduler scheduler;
	IPC ipc;
	std::vector<autohandle_t> locked_executables;
	std::set<GUID, GUID_compare> special_executables;

	class CopyObject{
	public:
		Payload payload;
		std::shared_ptr<OutgoingSharedMemory<return_shared_memory_size>> connection;
	};

	std::mutex copies_in_progress_mutex;
	std::map<std::uint32_t, CopyObject> copies_in_progress;
	std::mutex copies_waiting_mutex;
	std::map<std::uint32_t, std::deque<CopyObject>> copies_waiting;
	std::mutex pastes_waiting_mutex;
	std::map<std::uint32_t, std::deque<CopyObject>> pastes_waiting;
	std::mutex last_clipboard_writers_mutex;
	struct ClipboardWriter{
		std::wstring path;
		std::uint32_t pid;
		std::uint32_t tid;
		std::uint32_t session;
		std::uint64_t unique_id;
		ClipboardWriter() = default;
		ClipboardWriter(std::wstring &&path, std::uint32_t pid, std::uint32_t tid, std::uint32_t session, std::uint64_t unique){
			this->path = std::move(path);
			this->pid = pid;
			this->tid = tid;
			this->session = session;
			this->unique_id = unique;
		}
		ClipboardWriter(const ClipboardWriter &) = default;
		ClipboardWriter &operator=(const ClipboardWriter &) = default;
		ClipboardWriter(ClipboardWriter &&other){
			*this = std::move(other);
		}
		const ClipboardWriter &operator=(ClipboardWriter &&other){
			this->path = std::move(other.path);
			this->pid = other.pid;
			this->tid = other.tid;
			this->session = other.session;
			this->unique_id = other.unique_id;
			return *this;
		}
	};
	std::map<std::uint32_t, ClipboardWriter> last_clipboard_writers;

	void process_polling_thread_function();
	void hook_running_processes();
	std::vector<RunningProcess> get_running_processes(bool lightweight = false);

	template <typename F>
	std::thread create_thread(const F &f){
		return std::thread([this, f](){
			try{
				f();
				return;
			}catch (std::exception &){
			}
			this->request_general_stop();
		});
	}
	void check_executables();
	void add_forbidden_executables();
	void load_config();
	std::wstring get_process_full_path(HANDLE hProcess, bool throwing = false);
	std::wstring get_process_full_path(std::uint32_t pid, bool throwing = false);
	void hook_process_pre(HANDLE process, DWORD pid);
	DWORD hook_process(HANDLE process, DWORD pid);
	DWORD get_dll_path(std::wstring &full_dll_path, HANDLE hProcess);
	int need_external_hooking(HANDLE process, DWORD pid, const ProcessToken &token);
	DWORD get_process_integrity(HANDLE process, const ProcessToken &token);
	DWORD do_external_hooking_1(HANDLE process, DWORD pid, const std::wstring &dll_path, const ProcessToken &token);
	DWORD do_external_hooking_2(HANDLE process, DWORD pid, const std::wstring &dll_path, const ProcessToken &token);
	DWORD do_external_hooking_3(HANDLE process, DWORD pid, const std::wstring &dll_path, const ProcessToken &token);
	int process_wont_load_third_party_images(HANDLE process);
	int check_restriction(HANDLE process, DWORD pid, int default_value);
	DWORD start_child_modified_token(
		wchar_t *command_line,
		HANDLE process,
		const ProcessToken &token,
		const std::wstring &username,
		bool normal_integrity,
		bool clear_child_process_flags
	);
	DWORD start_child_normally(wchar_t *command_line);
	bool fallback_to_polling() const{
		return true;
	}
	DWORD clear_child_process_flags_workaround(HANDLE process, const std::wstring &username, wchar_t *cmd);
	std::wstring build_command_line1(DWORD pid, const std::wstring &dll_path);
	std::wstring build_command_line2(int, const std::wstring &, DWORD, const std::wstring &);
	const std::wstring &get_dll_directory();

	std::shared_ptr<OutgoingSharedMemory<return_shared_memory_size>> internal_on_copy_end(const Payload &, const std::shared_ptr<OutgoingSharedMemory<return_shared_memory_size>> &);
	void set_last_copy(std::uint32_t pid, std::uint32_t tid, std::uint32_t session, std::uint64_t unique_id);
	static FirewallPolicy to_ClipboardPermission(FirewallPolicy policy);
	FirewallPolicy check_copy(std::uint32_t pid, std::uint32_t session, std::uint64_t unique_id, std::wstring &explanation, std::wstring *reader = nullptr);
	FirewallPolicy on_previously_unknown_state() const override{
		return FirewallPolicy::Allow;
	}
	bool can_hook_process(DWORD pid, const wchar_t *path);

	void OnProcessCreated(DWORD pid) override;
	void OnProcessCreated_throwing(DWORD pid);
	void OnProcessDestroyed(DWORD pid) override;
	void OnProcessDestroyed_throwing(DWORD pid);
	void OnThreadError(DWORD dwOsErr) override;
public:
	Interceptor(AbstractLogger &, InterceptorMode);
	Interceptor(const Interceptor &) = delete;
	Interceptor &operator=(const Interceptor &) = delete;
	Interceptor(Interceptor &&) = delete;
	Interceptor &operator=(Interceptor &&) = delete;
	virtual ~Interceptor();
	void start();
	void stop();
	virtual void request_general_stop() = 0;
	void on_copy_begin(const Payload &, const std::shared_ptr<OutgoingSharedMemory<return_shared_memory_size>> &);
	void on_copy_end(const Payload &, const std::shared_ptr<OutgoingSharedMemory<return_shared_memory_size>> &);
	void on_paste(const Payload &, const std::shared_ptr<OutgoingSharedMemory<return_shared_memory_size>> &);
	void OnProcessCreated_from_coroutine(DWORD pid);
	void hook_process(const RunningProcess &);
	LogLine log();
};
#pragma warning(pop)

class Stopper{
	Interceptor *i;
public:
	Stopper(): i(nullptr){}
	Stopper(Interceptor &i): i(&i){}
	Stopper(const Stopper &) = delete;
	Stopper &operator=(const Stopper &) = delete;
	Stopper(Stopper &&other){
		*this = std::move(other);
	}
	const Stopper &operator=(Stopper &&other){
		this->i = other.i;
		other.i = nullptr;
		return *this;
	}
	~Stopper(){
		if (!this->i)
			return;
		this->i->request_general_stop();
		this->i->stop();
	}
};

std::wstring get_process_user(HANDLE hProcess, bool throwing = false);
