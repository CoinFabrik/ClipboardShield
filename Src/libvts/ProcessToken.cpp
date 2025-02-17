#include "ProcessToken.h"

ProcessToken::ProcessToken(HANDLE process, DWORD permissions){
	if (!OpenProcessToken(process, permissions, &this->handle)){
		this->error = GetLastError();
		this->valid = false;
		return;
	}
	this->error = ERROR_SUCCESS;
	this->valid = true;
}

ProcessToken::~ProcessToken(){
	if (this->valid)
		CloseHandle(this->handle);
}

const ProcessToken &ProcessToken::operator=(ProcessToken &&other){
	this->valid = other.valid;
	this->error = other.error;
	this->handle = other.handle;

	other.valid = false;
	other.error = ERROR_SUCCESS;
	other.handle = nullptr;

	return *this;
}

ProcessToken ProcessToken::duplicate() const{
	if (!this->valid)
		return {};
	ProcessToken ret;
	if (!DuplicateTokenEx(this->handle, 0, nullptr, SecurityImpersonation, TokenPrimary, &ret.handle)){
		ret.error = GetLastError();
		ret.handle = nullptr;
	}else
		ret.valid = true;
	return ret;
}
