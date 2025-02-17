#pragma once

#include "utility.h"
#include "../common/md5.h"
#include "../common/IpcStructures.h"
#include "../common/SharedMemory.h"

class CustomIncomingSharedMemory;
struct InjectedIpcData;

class IPC{
	DWORD pid;
	DWORD session;
	std::uint64_t unique_id;
	CriticalSection mutex;

	std::unique_ptr<OutgoingSharedMemory<shared_memory_size>> outgoing;
	std::unique_ptr<CustomIncomingSharedMemory> incoming;
	int send_and_wait(const Payload &, int timeout = -1);
	Payload construct_payload(RequestType);
	void complete_connection();
public:
	IPC();
	IPC(const InjectedIpcData *);
	~IPC();
	IPC(const IPC &) = delete;
	IPC &operator=(const IPC &) = delete;
	IPC(IPC &&) = delete;
	IPC &operator=(IPC &&) = delete;
	int send_copy_begin();
	int send_copy_end(bool succeeded);
	int send_paste();
};
