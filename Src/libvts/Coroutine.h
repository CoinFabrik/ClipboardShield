#pragma once

#include <functional>
#include <memory>
#include <Windows.h>
#include <boost/coroutine2/all.hpp>
#include <stdexcept>

class Interceptor;
class RunningProcess;

class CoroutineEntryPoint{
	friend class Coroutine;

	bool mode;
	Interceptor *i;
	const RunningProcess *rp;
	HANDLE event;
	std::uint32_t pid;
public:
	CoroutineEntryPoint(Interceptor &i, std::uint32_t pid)
		: mode(false)
		, i(&i)
		, pid(pid)
		, rp(nullptr)
		, event(nullptr){}
	CoroutineEntryPoint(Interceptor &i, const RunningProcess &rp, HANDLE event)
		: mode(true)
		, i(&i)
		, pid(0)
		, rp(&rp)
		, event(event){}
	CoroutineEntryPoint(const CoroutineEntryPoint &) = default;
	CoroutineEntryPoint &operator=(const CoroutineEntryPoint &) = default;
	void operator()() const;
};

class Coroutine{
	thread_local static Coroutine *coroutine_stack;
	Coroutine *next_coroutine;
	bool active = false;
	std::thread::id resume_thread_id;
	typedef boost::coroutines2::asymmetric_coroutine<void>::pull_type coroutine_t;
	typedef boost::coroutines2::asymmetric_coroutine<void>::push_type yielder_t;
	std::unique_ptr<coroutine_t> coroutine;
	CoroutineEntryPoint entry_point;
	yielder_t *yielder = nullptr;
	bool first_run;
	std::uint64_t wait_remainder = 0;
	std::uint64_t freq;

	void push();
	void pop();
	void init();
	void private_yield();
	void private_wait(double s);
	static Coroutine *get_current_coroutine_ptr();
public:
	Coroutine(const CoroutineEntryPoint &entry_point);
	Coroutine(const Coroutine &) = delete;
	Coroutine &operator=(const Coroutine &) = delete;
	Coroutine(Coroutine &&) = delete;
	Coroutine &operator=(Coroutine &&) = delete;
	bool resume();
	static void Coroutine::yield(){
		get_current_coroutine().private_yield();
	}
	static void Coroutine::wait(double s){
		get_current_coroutine().private_wait(s);
	}
	static Coroutine &get_current_coroutine();
};
