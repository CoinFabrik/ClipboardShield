#include "stdafx.h"
#include "IPC.h"
#include "CustomThread.h"
#include "../CFManualInjector/declarations.h"
#include <queue>
#include <random>
#include <cassert>

const int default_timeout = 5000;

class CustomIncomingSharedMemory : public IncomingSharedMemory<CriticalSection, return_shared_memory_size>{
protected:
	CriticalSection mutex;
	autohandle_t event;
	std::deque<Payload> queue;
	std::atomic<bool> destroying = false;
	static const size_t max_queue_size = 1024;
	HANDLE outgoing_event;

	void handle_message(const Payload &payload) override{
		if (this->destroying)
			return;
		if (payload.data.type == RequestType::ConnectionConfirmation){
			auto event = this->get_process_event_weak(0);
			if (!event)
				//This shouldn't happen.
				return;
			SetEvent(event);
			return;
		}
		{
			LOCK_MUTEX(this->mutex);
			while (this->queue.size() >= max_queue_size && !this->queue.empty())
				this->queue.pop_front();
			this->queue.push_back(payload);
			SetEvent(this->event.get());
		}
	}

public:
	CustomIncomingSharedMemory(const wchar_t *shared_memory_path, const wchar_t *event_path, HANDLE outgoing_event, thread_constructor_t tc): IncomingSharedMemory(shared_memory_path, event_path, tc){
		this->outgoing_event = outgoing_event;
		this->event = create_event(false);
		this->thread->start();
	}
	CustomIncomingSharedMemory(basic_shared_memory_block<return_shared_memory_size> *mem, HANDLE event, HANDLE outgoing_event, thread_constructor_t tc): IncomingSharedMemory(mem, event, tc){
		this->outgoing_event = outgoing_event;
		this->event = create_event(false);
		this->thread->start();
	}
	~CustomIncomingSharedMemory(){
		this->destroying = true;
	}
	void clear_queue(){
		LOCK_MUTEX(this->mutex);
		this->queue.clear();
	}
	Payload wait_for_data(int timeout){
		DWORD to = timeout < 0 ? INFINITE : timeout;
		while (true){
			Payload ret;
			if (WaitForSingleObject(this->event.get(), to) == WAIT_TIMEOUT)
				ret.data.type = RequestType::Timeout;
			else{
				LOCK_MUTEX(this->mutex);
				if (this->queue.empty())
					continue;
				ret = this->queue.front();
				this->queue.pop_front();
			}
			return ret;
		}
	}
	ConnectionWait get_connection_confirmation_event(){
		return this->get_connection_event(0);
	}
};

static std::wstring build_shared_path(DWORD pid, std::uint64_t id){
	std::wstringstream stream;
	stream << RETURN_SHARED_MEMORY_PATH_CLIENT << pid << '_' << id;
	return stream.str();
}

static std::wstring build_event_path(DWORD pid, std::uint64_t id){
	std::wstringstream stream;
	stream << RETURN_EVENT_PATH_CLIENT << pid << '_' << id;
	return stream.str();
}

static std::unique_ptr<AbstractThread> thread_constructor(thread_entry_point f, void *u){
	return std::unique_ptr<AbstractThread>(std::make_unique<CustomThread>(f, u));
}

static DWORD get_current_session_id(){
	DWORD ret;
	if (!ProcessIdToSessionId(GetCurrentProcessId(), &ret))
		ret = 0;
	return ret;
}

IPC::IPC(){
	this->outgoing.reset(new OutgoingSharedMemory<shared_memory_size>(SHARED_MEMORY_PATH, GLOBAL_EVENT_PATH));

	this->pid = GetCurrentProcessId();
	this->session = get_current_session_id();
	
	std::random_device dev;
	std::mt19937_64 rng((std::uint64_t)dev() | ((std::uint64_t)dev() << 32));

	this->unique_id = rng();
	
	auto path = build_shared_path(this->pid, this->unique_id);
	auto event_path = build_event_path(this->pid, this->unique_id);
	this->incoming.reset(new CustomIncomingSharedMemory(path.c_str(), event_path.c_str(), this->outgoing->get_event(), thread_constructor));

	this->complete_connection();
}

IPC::IPC(const InjectedIpcData *iid){
	this->outgoing.reset(new OutgoingSharedMemory<shared_memory_size>((basic_shared_memory_block<shared_memory_size> *)iid->outgoing.memory, iid->outgoing.event));

	this->pid = GetCurrentProcessId();
	this->session = get_current_session_id();
	
	this->unique_id = iid->process_unique_id;
	
	this->incoming.reset(new CustomIncomingSharedMemory((basic_shared_memory_block<return_shared_memory_size> *)iid->incoming.memory, iid->incoming.event, this->outgoing->get_event(), thread_constructor));

	this->complete_connection();
}

IPC::~IPC(){}

void IPC::complete_connection(){
	{
		auto event = this->incoming->get_connection_confirmation_event();
		auto payload = this->construct_payload(RequestType::Connection);
		if (this->outgoing->send(payload))
			OutputDebugStringA("Completing connection.\r\n");
		else{
			auto error = GetLastError();
			std::stringstream stream;
			stream << "Completing connection. WARINING: send failed with error " << error << "\r\n";
			OutputDebugStringA(stream.str().c_str());
		}
		event.complete();
	}

	std::stringstream stream;
	stream << "Connected pipes. server = 0x"
		<< std::hex << std::setw(16) << std::setfill('0') << (uintptr_t)this->outgoing->get_pointer() << " client = 0x"
		<< std::hex << std::setw(16) << std::setfill('0') << (uintptr_t)this->incoming->get_pointer() << "\r\n";
	OutputDebugStringA(stream.str().c_str());
}

Payload IPC::construct_payload(RequestType type){
	Payload ret;
	zero_structure(ret);
	auto &data = ret.data;
	data.type = type;
	data.process = this->pid;
	data.session = this->session;
	data.process_unique_id = this->unique_id;
	data.thread = GetCurrentThreadId();
	return ret;
}

int IPC::send_and_wait(const Payload &payload, int timeout){
	LOCK_MUTEX(this->mutex);
	this->incoming->clear_queue();
	this->outgoing->send(payload);
	auto result = this->incoming->wait_for_data(timeout);
	if (result.data.type == RequestType::Timeout)
		return -1;
	return result.data.type == RequestType::Continue;
}

int IPC::send_copy_begin(){
	return this->send_and_wait(this->construct_payload(RequestType::CopyBegin), default_timeout);
}

int IPC::send_copy_end(bool succeeded){
	auto payload = this->construct_payload(RequestType::CopyEnd);
	static_assert(sizeof(payload.extra_data) >= 1, "Oops!");
	payload.extra_data[0] = succeeded ? 1 : 0;
	return this->send_and_wait(payload, default_timeout);
}

int IPC::send_paste(){
	return this->send_and_wait(this->construct_payload(RequestType::Paste), default_timeout);
}
