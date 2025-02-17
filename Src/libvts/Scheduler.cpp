#include "Scheduler.h"
#include "Interceptor.h"
#include "utility.h"

const int scheduler_frequency_ms = 25;

Scheduler::Scheduler(Interceptor &i): i(&i){
	this->coroutines.reserve(4096);
	this->new_coroutines.reserve(4096);
	this->thread = std::thread([this](){ this->thread_function(); });
}

Scheduler::~Scheduler(){
	this->stop = true;
	safe_join(this->thread);
}

void Scheduler::thread_function(){
	auto f = get_performance_frequency();
	while (!this->stop){
		auto t0 = get_performance_counter();
#if 0
		auto b = this->coroutines.begin();
		auto e = this->coroutines.end();
		for (auto i = b; i != e;){
			auto pointer = i->get();
			if (!(*i)->resume()){
				auto copy = i++;
				this->coroutines.erase(copy);
			}else
				++i;
		}
#else
		auto &v = this->coroutines;
		size_t write = 0;
		for (size_t read = 0; read < v.size(); read++){
			bool result = v[read]->resume();
			if (read != write)
				v[write] = std::move(v[read]);
			if (result)
				write++;
		}
		v.resize(write);
#endif

		auto t1 = get_performance_counter();
		auto delta = t1 - t0;
		auto delta_ms = delta * 1000 / f;
		if (delta_ms < scheduler_frequency_ms){
			auto delay = scheduler_frequency_ms - (int)delta_ms;
			if (delay >= 15)
				Sleep(delay);
		}
		this->get_new_coros();
	}
}

void Scheduler::get_new_coros(){
	LOCK_MUTEX(this->mutex);
#if 0
	this->coroutines.splice(this->coroutines.end(), this->new_coroutines);
#else
	for (auto &c : this->new_coroutines)
		this->coroutines.emplace_back(std::move(c));
	this->new_coroutines.clear();
#endif
}

void Scheduler::add(std::unique_ptr<Coroutine> &&c){
	LOCK_MUTEX(this->mutex);
	this->new_coroutines.emplace_back(std::move(c));
}

TaskWaiter::~TaskWaiter(){}

HANDLE TaskWaiter::get_new_event(){
	auto e = create_event();
	auto ret = e.get();
	this->events.emplace_back(std::move(e));
	return ret;
}

void TaskWaiter::wait(){
	for (auto &e : this->events)
		WaitForSingleObject(e.get(), INFINITE);
}
