#pragma once

#include "Coroutine.h"
#include "utility.h"
#include <Windows.h>
#include <list>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>

class Interceptor;

class TaskWaiter{
	std::vector<autohandle_t> events;
public:
	TaskWaiter() = default;
	~TaskWaiter();
	TaskWaiter(const TaskWaiter &) = delete;
	TaskWaiter &operator=(const TaskWaiter &) = delete;
	TaskWaiter(TaskWaiter &&other) = delete;
	const TaskWaiter &operator=(TaskWaiter &&other) = delete;
	HANDLE get_new_event();
	void wait();
};

class Scheduler{
	Interceptor *i;
#if 0
	std::list<std::unique_ptr<Coroutine>> coroutines;
	std::list<std::unique_ptr<Coroutine>> new_coroutines;
#else
	std::vector<std::unique_ptr<Coroutine>> coroutines;
	std::vector<std::unique_ptr<Coroutine>> new_coroutines;
#endif
	std::mutex mutex;
	std::thread thread;
	std::atomic<bool> stop = false;

	void thread_function();
	void get_new_coros();
public:
	Scheduler(Interceptor &);
	~Scheduler();
	Scheduler(const Scheduler &) = delete;
	Scheduler &operator=(const Scheduler &) = delete;
	Scheduler(Scheduler &&) = delete;
	Scheduler &operator=(Scheduler &&) = delete;
	void add(std::unique_ptr<Coroutine> &&);
};
