#pragma once

#include "utility.h"
#include "md5.h"
#include "IpcStructures.h"
#include <memory>
#include <atomic>
#include <map>
#include <mutex>
#include <sstream>
#include <Windows.h>

template <typename T>
struct ViewUnmapper{
	void operator()(T *p){
		if (p)
			UnmapViewOfFile(p);
	}
};

template <typename T>
using autoview_t = std::unique_ptr<T, ViewUnmapper<T>>;

struct AccessibleObject{
	SECURITY_DESCRIPTOR sd;
	SECURITY_ATTRIBUTES sa;
	AccessibleObject(){
		InitializeSecurityDescriptor(&this->sd, SECURITY_DESCRIPTOR_REVISION);
		SetSecurityDescriptorDacl(&this->sd, true, nullptr, false);
		zero_structure(this->sa);
		this->sa.nLength = sizeof(this->sa);
		this->sa.bInheritHandle = false;
		this->sa.lpSecurityDescriptor = &this->sd;
	}
};

inline autohandle_t create_global_event(const wchar_t *path){
	AccessibleObject ao;
	auto event = CreateEventW(&ao.sa, false, false, path);
	if (!event){
		auto error = GetLastError();
		if (error == ERROR_ALREADY_EXISTS)
			throw std::runtime_error("Failed to start IPC. Event already exists. Is the service already running?");
		report_failed_win32_function("CreateEvent", error);
	}
	return autohandle_t(event);
}

template <size_t BlockSize>
class OutgoingSharedMemory{
	Md5 hash;
	autohandle_t file_mapping;
	autoview_t<basic_shared_memory_block<BlockSize>> shared_memory;
	autohandle_t event;
public:
	OutgoingSharedMemory(const wchar_t *file_mapping_path, const wchar_t *event_path){
		auto file_map = OpenFileMappingW(FILE_MAP_ALL_ACCESS, false, file_mapping_path);
		auto error = GetLastError();
		if (invalid_handle(file_map)){
			std::string context = "initializing return IPC ";
			for (auto p = file_mapping_path; *p; p++)
				context += (char)*p;
			report_failed_win32_function("OpenFileMapping", error, context.c_str());
		}
		autohandle_t file_mapping(file_map);
		auto shared_memory = (basic_shared_memory_block<BlockSize> *)MapViewOfFile(file_map, FILE_MAP_ALL_ACCESS, 0, 0, BlockSize);
		error = GetLastError();
		if (!shared_memory)
			report_failed_win32_function("MapViewOfFile", error, "initializing return IPC");
		autoview_t<basic_shared_memory_block<BlockSize>> autoshared_memory(shared_memory);

		auto event = OpenEventW(EVENT_ALL_ACCESS, false, event_path);
		error = GetLastError();
		if (invalid_handle(event))
			report_failed_win32_function("OpenEvent", error, "initializing return IPC");
		autohandle_t autoevent(event);

		this->file_mapping = std::move(file_mapping);
		this->shared_memory = std::move(autoshared_memory);
		this->event = std::move(autoevent);
	}
	OutgoingSharedMemory(basic_shared_memory_block<BlockSize> *mem, HANDLE event)
		: shared_memory(mem)
		, event(event){}
	OutgoingSharedMemory(const OutgoingSharedMemory &) = delete;
	OutgoingSharedMemory &operator=(const OutgoingSharedMemory &) = delete;
	OutgoingSharedMemory(OutgoingSharedMemory &&) = delete;
	OutgoingSharedMemory &operator=(OutgoingSharedMemory &&) = delete;
	bool send(const Payload &payload){
		return push_new_message(*this->shared_memory, payload, this->event, this->hash);
	}
	bool send(RequestType type){
		Payload payload;
		zero_structure(payload);
		payload.data.type = type;
		return this->send(payload);
	}
	void *get_pointer() const{
		return this->shared_memory.get();
	}
	HANDLE get_event() const{
		return this->event.get();
	}
};

class AbstractThread{
public:
	virtual ~AbstractThread(){}
	virtual void join() = 0;
	virtual void start() = 0;
};

typedef void (*thread_entry_point)(void *);
typedef std::unique_ptr<AbstractThread> (*thread_constructor_t)(thread_entry_point, void *);

template <typename MutexType, size_t BlockSize>
class IncomingSharedMemory{
protected:
	std::unique_ptr<AbstractThread> thread;
	autohandle_t global_event;
	std::atomic<bool> stop = false;
	autohandle_t file_mapping;
	autoview_t<basic_shared_memory_block<BlockSize>> shared_memory;
	std::vector<autohandle_t> free_events;
	std::map<DWORD, HANDLE> pid_events;
	MutexType free_events_mutex,
		pid_events_mutex;

	static void static_thread_function(void *p){
		((IncomingSharedMemory *)p)->thread_function();
	}
	void thread_function(){
		Md5 hash;
		std::vector<Payload> payloads;
		auto f = [this](const Payload &p){ this->handle_message(p); };
		while (!this->stop)
			pull_messages(*this->shared_memory, payloads, hash, this->global_event, 100, f);
	}
	void allocate_shared_memory(const wchar_t *path){
		AccessibleObject ao;
		auto file_map = CreateFileMappingW(INVALID_HANDLE_VALUE, &ao.sa, PAGE_READWRITE, 0, BlockSize, path);
		auto error = GetLastError();
		if (invalid_handle(file_map))
			report_failed_win32_function("CreateFileMapping", error, "initializing IncomingSharedMemory");
		this->file_mapping.reset(file_map);
		auto shared_memory = (basic_shared_memory_block<BlockSize> *)MapViewOfFile(file_map, FILE_MAP_ALL_ACCESS, 0, 0, BlockSize);
		error = GetLastError();
		if (!shared_memory)
			report_failed_win32_function("MapViewOfFile", error, "initializing IncomingSharedMemory");
		memset(shared_memory, 0, BlockSize);
		shared_memory->first_message = basic_shared_memory_block<BlockSize>::max_messages;
		this->shared_memory.reset(shared_memory);
	}
	autohandle_t allocate_event(){
		{
			LOCK_MUTEX(this->free_events_mutex);
			if (this->free_events.size()){
				auto ret = std::move(this->free_events.back());
				this->free_events.pop_back();
				ResetEvent(ret.get());
				return ret;
			}
		}
		return create_event();
	}
	autohandle_t get_process_event(DWORD pid){
		auto event = this->allocate_event();
		{
			LOCK_MUTEX(this->pid_events_mutex);
			if (this->pid_events.find(pid) == this->pid_events.end()){
				this->pid_events[pid] = event.get();
				return event;
			}
		}
		this->return_event(event);
		return {};
	}
	HANDLE get_process_event_weak(DWORD pid){
		LOCK_MUTEX(this->pid_events_mutex);
		auto it = this->pid_events.find(pid);
		if (it == this->pid_events.end())
			return nullptr;
		return it->second;
	}
	void remove_process_event(DWORD pid){
		LOCK_MUTEX(this->pid_events_mutex);
		auto it = this->pid_events.find(pid);
		if (it != this->pid_events.end())
			this->pid_events.erase(it);
	}
	void return_event(autohandle_t &event){
		if (!event)
			return;
		LOCK_MUTEX(this->free_events_mutex);
		this->free_events.emplace_back(std::move(event));
	}
	virtual void handle_message(const Payload &) = 0;
public:
	class ConnectionWait{
		IncomingSharedMemory *ipc;
		std::uint32_t id;
		autohandle_t event;
	public:
		ConnectionWait(IncomingSharedMemory &ipc, std::uint32_t id, autohandle_t &&event)
			: ipc(&ipc)
			, id(id)
			, event(std::move(event)){}
		ConnectionWait(const ConnectionWait &) = delete;
		ConnectionWait &operator=(const ConnectionWait &) = delete;
		ConnectionWait(ConnectionWait &&other){
			*this = std::move(other);
		}
		const ConnectionWait &operator=(ConnectionWait &&other){
			this->ipc = other.ipc;
			this->id = other.id;
			this->event = std::move(other.event);
			other.ipc = nullptr;
			return *this;
		}
		~ConnectionWait(){
			if (std::uncaught_exception())
				this->cancel();
		}
		void cancel(){
			if (this->ipc){
				this->ipc->cancel_connection(this->id, std::move(this->event));
				this->ipc = nullptr;
			}
		}
		bool complete(){
			bool ret = true;
			if (this->ipc && this->event) {
				ret = this->ipc->complete_connection(this->id, std::move(this->event));
				this->ipc = nullptr;
			}
			return ret;
		}
	};

	IncomingSharedMemory(const wchar_t *shared_memory_path, const wchar_t *event_path, thread_constructor_t constructor){
		this->global_event = create_global_event(event_path);
		this->allocate_shared_memory(shared_memory_path);
		this->thread = constructor(static_thread_function, this);
	}
	IncomingSharedMemory(basic_shared_memory_block<BlockSize> *mem, HANDLE event, thread_constructor_t constructor){
		this->shared_memory.reset(mem);
		this->global_event.reset(event);
		this->thread = constructor(static_thread_function, this);
	}
	virtual ~IncomingSharedMemory(){
		this->stop = true;
		if (this->thread)
			this->thread->join();
	}
	IncomingSharedMemory(const IncomingSharedMemory &) = delete;
	IncomingSharedMemory &operator=(const IncomingSharedMemory &) = delete;
	IncomingSharedMemory(IncomingSharedMemory &&) = delete;
	IncomingSharedMemory &operator=(IncomingSharedMemory &&) = delete;
	ConnectionWait get_connection_event(std::uint32_t pid){
		auto event = this->allocate_event();
		ResetEvent(event.get());
		{
			LOCK_MUTEX(this->pid_events_mutex);
			this->pid_events[pid] = event.get();
		}
		return ConnectionWait(*this, pid, std::move(event));
	}
	virtual bool complete_connection(std::uint32_t id, autohandle_t &&connection_event){
		WaitForSingleObject(connection_event.get(), INFINITE);
		this->cancel_connection(id, std::move(connection_event));
		return true;
	}
	void cancel_connection(std::uint32_t id, autohandle_t &&connection_event){
		this->remove_process_event(id);
		this->return_event(connection_event);
	}
	void *get_pointer() const{
		return this->shared_memory.get();
	}
};
