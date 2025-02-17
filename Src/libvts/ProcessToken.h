#pragma once

#include <Windows.h>
#include <algorithm>

class ProcessToken{
	bool valid;
	HANDLE handle;
	DWORD error;
public:
	ProcessToken(): valid(false), handle(nullptr), error(ERROR_SUCCESS){}
	ProcessToken(HANDLE process, DWORD permissions);
	~ProcessToken();
	ProcessToken(const ProcessToken &) = delete;
	ProcessToken &operator=(const ProcessToken &) = delete;
	ProcessToken(ProcessToken &&other){
		*this = std::move(other);
	}
	ProcessToken duplicate() const;
	const ProcessToken &operator=(ProcessToken &&);
	bool get_valid() const{
		return this->valid;
	}
	DWORD get_error() const{
		return this->error;
	}
	HANDLE get_handle() const{
		return this->handle;
	}
};
