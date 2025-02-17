#include "stdafx.h"
#include "DllState.h"
#include "utility.h"
#include "../common/utility.h"
#include <sstream>
#include <vector>
#include <iomanip>
#include <sstream>
#include <map>

DllState *global_state;

const bool if_not_connected_allow_pastes = true;

extern const TargetFunction functions[] = {
	{ "GetClipboardData",       L"user32.dll", hooked_GetClipboardData       },
	{ "SetClipboardData",       L"user32.dll", hooked_SetClipboardData       },
	//{ "NtUserSetClipboardData", L"win32u.dll", hooked_NtUserSetClipboardData },
	{ "OleSetClipboard",        L"ole32.dll",  hooked_OleSetClipboard        },
	{ "OleGetClipboard",        L"ole32.dll",  hooked_OleGetClipboard        },
};

DllState::DllState(){
	this->initialized = false;
}

bool DllState::initialize(const InjectedIpcData *ipc){
	bool expected = false;
	if (!this->initialized.compare_exchange_strong(expected, true))
		return true;

	if (this->hook_result != ERROR_SUCCESS)
		return false;
	this->running = true;

	OutputDebugStringA("Now initializing IPC.\r\n");
	try{
		LOCK_MUTEX(this->ipc_mutex);
		if (!ipc)
			this->ipc.reset(new IPC);
		else
			this->ipc.reset(new IPC(ipc));
	}catch (std::exception &e){
		std::string temp("Caught exception while initializing IPC: ");
		temp += e.what();
		temp += "\r\n";
		OutputDebugStringA(temp.c_str());
		return false;
	}

	OutputDebugStringA("IPC correctly initialized.\r\n");
	this->perform_selftest();
	

	return true;
}

DllState::~DllState(){
	this->running = false;
	this->uninstall_hooks();
}

void DllState::perform_selftest(){
	OutputDebugStringA("Performing selftest.\r\n");
	this->selftesting = true;
	GetClipboardData(0);
	SetClipboardData(0, nullptr);
	OleGetClipboard(nullptr);
	OleSetClipboard(nullptr);
	this->selftesting = false;
	std::string s;
	s = "Test results:";
	for (int i = 0; i < (int)FunctionId::COUNT; i++)
		s += this->selftest.find(i) != this->selftest.end() ? " P" : " F";
	s += "\r\n";
	OutputDebugStringA(s.c_str());
}

struct Unmapper{
	void operator()(char *p){
		if (p)
			UnmapViewOfFile(p);
	}
};

DWORD add_hook_entry(CNktHookLib::HOOK_INFO *info, LPCSTR name, LPVOID hookFunc, HINSTANCE hKernelBaseDll, HINSTANCE hKernel32Dll) {
	info->lpNewProcAddr = hookFunc;
	if (hKernelBaseDll)
		info->lpProcToHook = NktHookLibHelpers::GetProcedureAddress(hKernelBaseDll, name);
	if (!info->lpProcToHook)
		info->lpProcToHook = NktHookLibHelpers::GetProcedureAddress(hKernel32Dll, name);
	if (!info->lpProcToHook)
		return ERROR_PROC_NOT_FOUND;

	return ERROR_SUCCESS;
}

template<size_t N>
DWORD prepare_entries(const TargetFunction (&functions)[N], std::map<std::wstring, HINSTANCE> &modules, std::vector<CNktHookLib::HOOK_INFO> &hooks, HINSTANCE kernelbase, HINSTANCE kernel32) {
	DWORD added = 0;
	for (auto &func : functions){
		auto name = func.name;
		CNktHookLib::HOOK_INFO hook{};
		hook.lpNewProcAddr = func.handler;
		if (!func.module){
			if (kernelbase)
				hook.lpProcToHook = NktHookLibHelpers::GetProcedureAddress(kernelbase, name);
			if (!hook.lpProcToHook)
				hook.lpProcToHook = NktHookLibHelpers::GetProcedureAddress(kernel32, name);
		}else{
			auto it = modules.find(func.module);
			HINSTANCE module;
			if (it == modules.end()){
				module = NktHookLibHelpers::GetModuleBaseAddress(func.module);
				if (!module)
					continue;
				modules[func.module] = module;
			}else
				module = it->second;
			hook.lpProcToHook = NktHookLibHelpers::GetProcedureAddress(module, name);
		}
		if (hook.lpProcToHook)
			added++;
		hooks.push_back(hook);
	}
	return added;
}

void DllState::set_manually_injected(){
	this->manually_injected = true;
}

DWORD DllState::install_hooks(){
	bool expected = false;
	if (!this->hooked.compare_exchange_strong(expected, true))
		return this->hook_result = ERROR_SUCCESS;

	this->global_hooks.reserve(array_length(functions));
	std::map<std::wstring, HINSTANCE> modules;
	for (const TargetFunction &func : functions){
		this->global_hooks.resize(this->global_hooks.size() + 1);
		LPCSTR name = func.name;
		HINSTANCE module;
		auto it = modules.find(func.module);
		if (it != modules.end())
			module = it->second;
		else{
			module = NktHookLibHelpers::GetModuleBaseAddress(func.module);
			if (!module)
				continue;
			modules[func.module] = module;
		}
		CNktHookLib::HOOK_INFO hook;
		hook.lpNewProcAddr = func.handler;
		hook.lpProcToHook = NktHookLibHelpers::GetProcedureAddress(module, name);
		this->global_hooks.back() = hook;
	}
	return this->hook_result = this->hook_manager.Hook(global_hooks.data(), this->global_hooks.size(), NKTHOOKLIB_SkipNullProcsToHook);
}

void DllState::uninstall_hooks(){
	OutputDebugStringA("Uninstalling hooks.\r\n");
	this->hook_manager.UnhookAll();
}

void DllState::disconnect(){
	OutputDebugStringW(L"Disconnecting from IPC.\r\n");
	this->connected = false;
}

void DllState::clipboard_write_pre(){
	if (!this->connected)
		return;
	{
		LOCK_MUTEX(this->ipc_mutex);
		if (!this->ipc || this->ipc->send_copy_begin() >= 0)
			return;
	}
	this->disconnect();
}

void DllState::clipboard_write_post(bool succeeded){
	if (!this->connected)
		return;
	{
		LOCK_MUTEX(this->ipc_mutex);
		if (!this->ipc || this->ipc->send_copy_end(succeeded) >= 0)
			return;
	}
	this->disconnect();
}

bool DllState::clipboard_read_pre(){
	if (!this->connected)
		return if_not_connected_allow_pastes;
	{
		LOCK_MUTEX(this->ipc_mutex);
		if (!this->ipc)
			return if_not_connected_allow_pastes;
		auto result = this->ipc->send_paste();
		if (result >= 0)
			return !!result;
	}
	this->disconnect();
	return if_not_connected_allow_pastes;
}

Reentrancy DllState::lock_reentrancy(){
	LOCK_MUTEX(this->reentrancy_locks_mutex);
	auto tid = GetCurrentThreadId();
	if (this->reentrancy_locks.find(tid) != this->reentrancy_locks.end())
		return Reentrancy(nullptr, 0);
	this->reentrancy_locks.insert(tid);
	return Reentrancy(this, tid);
}

void DllState::unlock_reentrancy(DWORD tid){
	LOCK_MUTEX(this->reentrancy_locks_mutex);
	auto it = this->reentrancy_locks.find(tid);
	if (it == this->reentrancy_locks.end())
		return;
	this->reentrancy_locks.erase(it);
}

#define DEFINE_GETUNHOOKED(x) \
decltype(&hooked_##x) DllState::get_##x(){ \
	return decltype(&hooked_##x)(this->global_hooks[(int)FunctionId::x].lpCallOriginal); \
} \
bool DllState::confirm_test_##x(){ \
	if (!this->selftesting) \
		return false; \
	this->selftest.insert((int)FunctionId::x); \
	return true; \
}

DEFINE_GETUNHOOKED(GetClipboardData)
DEFINE_GETUNHOOKED(SetClipboardData)
//DEFINE_GETUNHOOKED(NtUserSetClipboardData)
DEFINE_GETUNHOOKED(OleSetClipboard)
DEFINE_GETUNHOOKED(OleGetClipboard)
