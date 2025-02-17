#pragma once

#include "Time.h"
#include "Hooks.h"
#include "IPC.h"
#include "utility.h"
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <atomic>
#include <unordered_set>

struct InjectedIpcData;

class DllState{
	friend class Reentrancy;
	CNktHookLib hook_manager;
	std::vector<CNktHookLib::HOOK_INFO> global_hooks;
	std::atomic<bool> running,
		initialized = false,
		hooked = false,
		connected = true;
	DWORD hook_result = ERROR_SUCCESS;
	bool manually_injected = false;
	std::unique_ptr<IPC> ipc;
	CriticalSection reentrancy_locks_mutex,
		ipc_mutex;
	std::unordered_set<DWORD> reentrancy_locks;
	std::atomic<bool> selftesting = false;
	std::unordered_set<int> selftest;

	void uninstall_hooks();
	void unlock_reentrancy(DWORD tid);
	void disconnect();
	void perform_selftest();
public:
	DllState();
	DllState(const DllState &) = delete;
	DllState &operator=(const DllState &) = delete;
	DllState(DllState &&) = delete;
	DllState &operator=(DllState &&) = delete;
	~DllState();
	bool initialize(const InjectedIpcData *ipc);
	DWORD install_hooks();
	void set_manually_injected();

#define DECLARE_GETUNHOOKED(x) decltype(&hooked_##x) get_##x(); bool confirm_test_##x()

	DECLARE_GETUNHOOKED(GetClipboardData);
	DECLARE_GETUNHOOKED(SetClipboardData);
	//DECLARE_GETUNHOOKED(NtUserSetClipboardData);
	DECLARE_GETUNHOOKED(OleSetClipboard);
	DECLARE_GETUNHOOKED(OleGetClipboard);

	void clipboard_write_pre();
	void clipboard_write_post(bool succeeded);
	bool clipboard_read_pre();
	Reentrancy lock_reentrancy();
};

class Reentrancy{
	DllState *state;
	DWORD tid;
public:
	Reentrancy(DllState *state, DWORD tid)
		: state(state)
		, tid(tid){}
	Reentrancy(const Reentrancy &) = delete;
	Reentrancy &operator=(const Reentrancy &) = delete;
	Reentrancy(Reentrancy &&other){
		*this = std::move(other);
	}
	const Reentrancy &operator=(Reentrancy &&other){
		this->state = other.state;
		this->tid = tid;
		other.state = nullptr;
		return *this;
	}
	~Reentrancy(){
		if (this->state)
			this->state->unlock_reentrancy(this->tid);
	}
	operator bool() const{
		return !!this->state;
	}
};

extern DllState *global_state;

void send_notification(int i);
