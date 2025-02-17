#include "Coroutine.h"
#include "Interceptor.h"
#include "utility.h"
#include <Windows.h>

thread_local Coroutine *Coroutine::coroutine_stack = nullptr;

Coroutine::Coroutine(const CoroutineEntryPoint &entry_point):
		entry_point(entry_point){
	this->freq = get_performance_frequency();
	this->init();
}

void Coroutine::push(){
	this->next_coroutine = coroutine_stack;
	coroutine_stack = this;
}

void Coroutine::pop(){
	coroutine_stack = this->next_coroutine;
}

void Coroutine::init(){
	this->first_run = true;
	this->push();
	this->coroutine.reset(new coroutine_t([this](yielder_t &y){
		this->resume_thread_id = std::this_thread::get_id();
		this->yielder = &y;
		if (this->first_run){
			this->private_yield();
			this->first_run = false;
		}
		this->entry_point();
	}));
	this->pop();
}

bool Coroutine::resume(){
	this->resume_thread_id = std::this_thread::get_id();
	if (this->active)
		throw std::runtime_error("Attempting to resume a running coroutine!");
	this->active = true;
	if (!this->coroutine)
		this->init();
	this->push();
	auto ret = !!(*this->coroutine)();
	this->pop();
	this->active = false;
	if (!ret)
		this->coroutine.reset();
	return ret;
}

void Coroutine::private_yield(){
	if (coroutine_stack != this)
		throw std::runtime_error("Coroutine::yield() was used incorrectly!");
	if (std::this_thread::get_id() != this->resume_thread_id)
		throw std::runtime_error("Coroutine::yield() must be called from the thread that resumed the coroutine!");
	if (!this->yielder)
		throw std::runtime_error("Coroutine::yield() must be called while the coroutine is active!");
	auto yielder = this->yielder;
	this->yielder = nullptr;
	(*yielder)();
	this->yielder = yielder;
}

void Coroutine::private_wait(double s){
	auto target = get_performance_counter() + (std::uint64_t)(s * this->freq) + this->wait_remainder;
	while (true){
		this->private_yield();
		auto now = get_performance_counter();
		if (now >= target){
			this->wait_remainder = target - now;
			break;
		}
	}
}

Coroutine *Coroutine::get_current_coroutine_ptr(){
	return coroutine_stack;
}

Coroutine &Coroutine::get_current_coroutine(){
	auto p = get_current_coroutine_ptr();
	if (!p)
		throw std::runtime_error("No coroutine is running!");
	return *p;
}
