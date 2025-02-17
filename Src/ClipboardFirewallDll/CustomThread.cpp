#include "stdafx.h"
#include "CustomThread.h"

const std::uint8_t thread_proxy[] = {
#ifndef _M_X64
	// x86 code
	0x8B, 0x44, 0x24, 0x04, // mov eax,[esp+4]
	0xFF, 0x20,             // jmp [eax]
#else
	// x86-64 code
	0xFF, 0x21,             // jmp [rcx]
#endif
};

static LPTHREAD_START_ROUTINE generate_proxy_function(){
	const size_t size = sizeof(thread_proxy);
	auto proc = GetCurrentProcess();
	auto memory = VirtualAllocEx(proc, nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!memory)
		return (LPTHREAD_START_ROUTINE)memory;
	memcpy(memory, thread_proxy, size);
	DWORD ignored;
	VirtualProtectEx(proc, memory, size, PAGE_EXECUTE_READ, &ignored);
	return (LPTHREAD_START_ROUTINE)memory;
}

static LPTHREAD_START_ROUTINE get_proxy_function(){
	static volatile LONG initialized = 0;
	static volatile LPTHREAD_START_ROUTINE proxy_function;
	switch (InterlockedCompareExchange(&initialized, 1, 0)){
		case 0:
			proxy_function = generate_proxy_function();
			initialized = 2;
			break;
		case 1:
			while (initialized == 1);
			break;
		case 2:
			break;
	}
	return proxy_function;
}

CustomThread::CustomThread(thread_entry_point f, void *u){
	this->f = f;
	this->u = u;
}

CustomThread::~CustomThread(){
	this->CustomThread::join();
}

void CustomThread::join(){
	if (!this->started)
		return;
	WaitForSingleObject(this->thread, INFINITE);
}

struct Param{
	LPTHREAD_START_ROUTINE f;
	CustomThread *thread;
};

void CustomThread::start(){
	this->thread = CreateThread(nullptr, 0, get_proxy_function(), new Param{ CustomThread::thread_function, this }, 0, nullptr);
}

DWORD WINAPI CustomThread::thread_function(LPVOID lpThreadParameter){
	auto p = (Param *)lpThreadParameter;
	auto This = p->thread;
	delete p;
	This->f(This->u);
	return 0;
}
