#include "IPC.h"
#include "Interceptor.h"
#include "utility.h"
#include <sstream>
#include <string>
#include <chrono>

class StdThread : public AbstractThread{
	bool started = false;
	thread_entry_point f;
	void *u;
	std::thread thread;
public:
	StdThread(thread_entry_point f, void *u){
		this->f = f;
		this->u = u;
	}
	~StdThread(){
		safe_join(this->thread);
	}
	void join() override{
		safe_join(this->thread);
	}
	void start() override{
		if (!this->started){
			this->thread = std::thread([this](){ this->f(this->u); });
			this->started = true;
		}
	}
};

static std::unique_ptr<AbstractThread> thread_constructor(thread_entry_point f, void *u){
	return std::unique_ptr<AbstractThread>(std::make_unique<StdThread>(f, u));
}

IPC::IPC(Interceptor &interceptor)
	: IncomingSharedMemory(SHARED_MEMORY_PATH, GLOBAL_EVENT_PATH, ::thread_constructor)
	, interceptor(&interceptor){

		this->thread->start();
}

IPC::~IPC(){}

void IPC::handle_message(const Payload &payload){
	switch ((RequestType)payload.type){
		case RequestType::Connection:
			this->handle_connection(payload.process, payload.session, payload.process_unique_id);
			break;
		case RequestType::CopyBegin:
			this->interceptor->on_copy_begin(payload, this->find_connection(payload.process));
			break;
		case RequestType::CopyEnd:
			this->interceptor->on_copy_end(payload, this->find_connection(payload.process));
			break;
		case RequestType::Paste:
			this->interceptor->on_paste(payload, this->find_connection(payload.process));
			break;
		default:
			{
				std::stringstream stream;
				stream << "IPC::handle_message(): Missing case for RequestType: " << (LONG)payload.type;
				throw std::runtime_error(stream.str());
			}
	}
}

std::shared_ptr<OutgoingSharedMemory<return_shared_memory_size>> IPC::find_connection(std::uint32_t pid){
	auto it = this->return_paths.find(pid);
	if (it == this->return_paths.end())
		return {};
	return it->second;
}

void IPC::handle_connection(DWORD pid, DWORD session, std::uint64_t unique_id){
	auto event = this->get_process_event_weak(pid);
	if (!event){
		//TODO: Log anomalous event. A connection for a process
		//should not have arrived while it was not expected.
		return;
	}
	try{
		this->connect_return_path(pid, session, unique_id);
	}catch (std::exception &e){
		this->interceptor->log() << "Failed to connect with PID " << pid << ". Error: " << e.what();
	}
	SetEvent(event);
}

static std::wstring build_shared_path(DWORD pid, DWORD session, std::uint64_t id){
	std::wstringstream stream;
	stream << RETURN_SHARED_MEMORY_PATH_SERVER1 << session << RETURN_SHARED_MEMORY_PATH_SERVER2 << pid << '_' << id;
	return stream.str();
}

static std::wstring build_event_path(DWORD pid, DWORD session, std::uint64_t id){
	std::wstringstream stream;
	stream << RETURN_EVENT_PATH_SERVER1 << session << RETURN_EVENT_PATH_SERVER2 << pid << '_' << id;
	return stream.str();
}

void IPC::connect_return_path(DWORD pid, DWORD session, std::uint64_t unique_id){
	for (int i = 0; ; i++){
		auto path = build_shared_path(pid, session, unique_id);
		auto event_path = build_event_path(pid, session, unique_id);
		try{
			auto return_path = std::make_shared<OutgoingSharedMemory<return_shared_memory_size>>(path.c_str(), event_path.c_str());
			this->return_paths[pid] = return_path;
			return_path->send(RequestType::ConnectionConfirmation);
			break;
		}catch (std::exception &e){
			//If opening the shared memory failed, it could be because the process was
			//injected by the manual injector, which runs in session 0 and thus creates
			//the shared memory in that session, so retry with session 0. If it fails
			//again just stop trying.
			if (i)
				throw;
			session = 0;
		}
	}
}

bool IPC::complete_connection(std::uint32_t id, autohandle_t &&connection_event){
	auto t0 = std::chrono::steady_clock::now();
	while (WaitForSingleObject(connection_event.get(), 0)){
		auto t1 = std::chrono::steady_clock::now();
		auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
		if (elapsed_ms >= 5000){
			this->cancel_connection(id, std::move(connection_event));
			return false;
		}
		Coroutine::yield();
	}
	this->cancel_connection(id, std::move(connection_event));
	return true;
}
