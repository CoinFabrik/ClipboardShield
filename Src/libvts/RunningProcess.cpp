#include "RunningProcess.h"

RunningProcess::RunningProcess(autohandle_t &&handle, DWORD pid, std::wstring &&path, std::wstring &&username)
		: process(std::move(handle))
		, pid(pid)
		, path(std::move(path))
		, username(std::move(username)){
	FILETIME start_time,
		exit_time,
		kernel_time,
		user_time;
	this->start_time_valid = !!GetProcessTimes(this->process.get(), &start_time, &exit_time, &kernel_time, &user_time);
	if (this->start_time_valid){
		this->start_time = start_time.dwHighDateTime;
		this->start_time <<= 32;
		this->start_time |= start_time.dwLowDateTime;
	}else
		this->start_time = 0;
}

const RunningProcess &RunningProcess::operator=(RunningProcess &&other){
	this->process = std::move(other.process);
	this->pid = other.pid;
	this->path = std::move(other.path);
	this->username = std::move(other.username);
	this->start_time = other.start_time;
	this->start_time_valid = other.start_time_valid;
	other.start_time_valid = 0;
	return *this;
}
