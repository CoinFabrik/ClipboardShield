#pragma once
#include <Windows.h>
#include <cstdint>

struct SharedMemoryPipe{
	void *memory;
	HANDLE event;
};

struct InjectedIpcData{
	std::uint64_t process_unique_id;
	SharedMemoryPipe incoming;
	SharedMemoryPipe outgoing;
};

typedef HMODULE (WINAPI *LoadLibraryA_ft)(LPCSTR);
typedef FARPROC (WINAPI *GetProcAddress_ft)(HMODULE,LPCSTR);
typedef void (WINAPI *OutputDebugStringA_ft)(const char *lpOutputString);
#ifdef _M_X64
typedef BOOLEAN (*RtlAddFunctionTable_ft)(PRUNTIME_FUNCTION FunctionTable, DWORD EntryCount, DWORD64 BaseAddress);
typedef PVOID (NTAPI *RtlPcToFileHeader_ft)(PVOID PcValue, PVOID *BaseOfImage);
#endif
typedef BOOL (WINAPI *DllMain_ft)(HMODULE,DWORD,PVOID);
typedef void (*InitializeDll_ft)(const InjectedIpcData *);
 
struct MANUAL_INJECT{
	void *image_base;
	void *buffer;
#ifdef _M_X64
	void *exception_table;
	DWORD exception_table_length;
#endif
	PIMAGE_NT_HEADERS nt_headers;
	PIMAGE_BASE_RELOCATION base_relocation;
	PIMAGE_IMPORT_DESCRIPTOR import_directory;
	LoadLibraryA_ft LoadLibraryA_f;
	GetProcAddress_ft GetProcAddress_f;
	OutputDebugStringA_ft OutputDebugStringA_f;
#ifdef _M_X64
	RtlAddFunctionTable_ft RtlAddFunctionTable_f;
	RtlPcToFileHeader_ft RtlPcToFileHeader_unhooked_f;
	RtlPcToFileHeader_ft RtlPcToFileHeader_f;
#endif
	InitializeDll_ft InitializeDll_f;
	InjectedIpcData ipc;
};
