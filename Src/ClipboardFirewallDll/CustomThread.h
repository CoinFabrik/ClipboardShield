#pragma once

#include "../common/SharedMemory.h"
#include "utility.h"

class CustomThread : public AbstractThread{
	bool started = false;
	HANDLE thread = nullptr;
	thread_entry_point f;
	void *u;

	static DWORD WINAPI thread_function(LPVOID lpThreadParameter);
public:
	CustomThread(thread_entry_point f, void *u);
	~CustomThread();
	void join() override;
	void start() override;
};
