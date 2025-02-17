
//NOTE: Most traces are enabled when building in debug mode (_DEBUG)

#include "StdAfx.h"
#include "Hooks.h"
#include "utility.h"
#include "DllState.h"
#include <array>
#include <sstream>
#include <corecrt_startup.h>
#include "../CFManualInjector/declarations.h"

static void set_global_state(){
	if (global_state)
		return;
	global_state = new DllState();
	auto result = global_state->install_hooks();
	std::stringstream stream;
	stream << "install_hooks returned " << result << "\r\n";
	OutputDebugStringA(stream.str().c_str());
}

extern "C" _onexit_table_t __acrt_atexit_table;
extern "C" struct _onexit_table_t __acrt_at_quick_exit_table;

void nullify_table(_onexit_table_t &dst){
	dst._end = dst._first;
	dst._last = dst._first;
}

BOOL APIENTRY DllMain(__in HMODULE hModule, __in DWORD  ulReasonForCall, __in LPVOID lpReserved){
	switch (ulReasonForCall){
		case DLL_PROCESS_ATTACH:
			nullify_table(__acrt_atexit_table);
			nullify_table(__acrt_at_quick_exit_table);
			set_global_state();
			break;
		case DLL_THREAD_ATTACH:
			break;
		case DLL_THREAD_DETACH:
			break;
		case DLL_PROCESS_DETACH:
			break;
	}
	return TRUE;
}

typedef HANDLE (WINAPI *CreateNamedPipeW_ft)(LPCWSTR lpName, DWORD dwOpenMode, DWORD dwPipeMode, DWORD nMaxInstances, DWORD nOutBufferSize, DWORD nInBufferSize, DWORD nDefaultTimeOut, LPSECURITY_ATTRIBUTES lpSecurityAttributes);

__declspec(dllexport) void InitializeDll2(const InjectedIpcData *ipc){
#pragma comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)
	std::stringstream stream;
	stream << "Initializing DLL from PID " << GetCurrentProcessId() << "\r\n";
	OutputDebugStringA(stream.str().c_str());
	set_global_state();
	global_state->set_manually_injected();
	if (!global_state->initialize(ipc)){
		auto temp = global_state;
		global_state = nullptr;
		delete temp;
	}
}

__declspec(dllexport) DWORD __stdcall InitializeDll(){
#pragma comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)
	//Single-shot function
	static volatile LONG initialized = 0;
	if (InterlockedCompareExchange(&initialized, 1, 0) == 1)
		return 0;

	InitializeDll2(nullptr);
	return 0;
}
