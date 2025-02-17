#pragma once

#include "utility.h"

class RunningProcess;

struct UniqueProcess{
	DWORD pid;
	ULONGLONG start_time;

	bool operator<(const UniqueProcess &other) const{
		if (this->pid < other.pid)
			return true;
		if (this->pid > other.pid)
			return false;
		return this->start_time < other.start_time;
	}
};

class RunningProcess{
	autohandle_t process;
	DWORD pid;
	std::wstring path;
	std::wstring username;
	bool start_time_valid = false;
	ULONGLONG start_time;
public:
	RunningProcess() = default;
	RunningProcess(autohandle_t &&handle, DWORD pid, std::wstring &&path, std::wstring &&username);
	RunningProcess(const RunningProcess &) = delete;
	RunningProcess &operator=(const RunningProcess &) = delete;
	RunningProcess(RunningProcess &&other){
		*this = std::move(other);
	}
	const RunningProcess &operator=(RunningProcess &&);
	const std::wstring &get_path() const{
		return this->path;
	}
	const std::wstring &get_username() const{
		return this->username;
	}
	bool get_start_time_valid() const{
		return this->start_time_valid;
	}
	ULONGLONG get_start_time() const{
		return this->start_time_valid ? this->start_time : 0;
	}
	DWORD get_pid() const{
		return this->pid;
	}
	HANDLE get_handle() const{
		return this->process.get();
	}
	operator UniqueProcess() const{
		return {this->pid, this->get_start_time()};
	}
};
