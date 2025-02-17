#pragma once

#include "utility.h"
#include "../common/md5.h"
#include "../common/IpcStructures.h"
#include "../common/SharedMemory.h"
#include <thread>
#include <atomic>
#include <vector>
#include <map>
#include <mutex>
#include <Windows.h>

class Interceptor;

class IPC : public IncomingSharedMemory<std::mutex, shared_memory_size>{
	Interceptor *interceptor;
	std::map<std::uint32_t, std::shared_ptr<OutgoingSharedMemory<return_shared_memory_size>>> return_paths;

	void handle_message(const Payload &) override;
	void handle_connection(DWORD pid, DWORD session, std::uint64_t unique_id);
	void connect_return_path(DWORD pid, DWORD session, std::uint64_t unique_id);
	std::shared_ptr<OutgoingSharedMemory<return_shared_memory_size>> find_connection(std::uint32_t);
public:
	IPC(Interceptor &);
	~IPC();
	IPC(const IPC &) = delete;
	IPC &operator=(const IPC &) = delete;
	IPC(IPC &&) = delete;
	IPC &operator=(IPC &&) = delete;
	bool complete_connection(std::uint32_t id, autohandle_t &&connection_event) override;
};
