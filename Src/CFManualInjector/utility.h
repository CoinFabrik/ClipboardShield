#pragma once
#include "../common/utility.h"
#include <algorithm>
#include <cstdint>
#include <vector>
#include <type_traits>
#include <Windows.h>

#if (!defined NDEBUG) || defined FORCE_TRACE
#define TRACE(x) OutputDebugStringA(x)
#else
#define TRACE(x)
#endif

class RemoteMemory{
	HANDLE process;
	void *memory;
	size_t size;
public:
	RemoteMemory(): process(nullptr), memory(nullptr), size(0){}
	RemoteMemory(HANDLE process, void *memory, size_t size): process(process), memory(memory), size(size){}
	RemoteMemory(const RemoteMemory &) = delete;
	RemoteMemory &operator=(const RemoteMemory &) = delete;
	RemoteMemory(RemoteMemory &&other){
		*this = std::move(other);
	}
	const RemoteMemory &operator=(RemoteMemory &&other){
		this->process = other.process;
		this->memory = other.memory;
		this->size = other.size;
		other.release();
		return *this;
	}
	~RemoteMemory(){
		if (this->memory)
			VirtualFreeEx(this->process, this->memory, 0, MEM_RELEASE);
	}
	void *get() const{
		return this->memory;
	}
	DWORD copy_to_remote(const void *src, size_t size, size_t destination_offset){
		if (!WriteProcessMemory(this->process, (std::uint8_t *)this->memory + destination_offset, src, size, nullptr))
			return GetLastError();
		return ERROR_SUCCESS;
	}
	DWORD copy_from_remote(void *dst, size_t size, size_t source_offset){
		if (!ReadProcessMemory(this->process, (const std::uint8_t *)this->memory + source_offset, dst, size, nullptr))
			return GetLastError();
		return ERROR_SUCCESS;
	}
	template <typename T>
	DWORD copy_struct_to_remote(const T &src, size_t destination_offset){
		return this->copy_to_remote(&src, sizeof(T), destination_offset);
	}
	template <typename T>
	DWORD copy_struct_from_remote(T &dst, size_t source_offset){
		return this->copy_from_remote(&dst, sizeof(T), source_offset);
	}
	size_t get_size() const{
		return this->size;
	}
	void release(){
		this->memory = nullptr;
		this->process = nullptr;
		this->size = 0;
	}
	DWORD remove_write_permission(){
		DWORD ignored;
		if (!VirtualProtectEx(this->process, this->memory, this->size, PAGE_EXECUTE_READ, &ignored))
			return GetLastError();
		return ERROR_SUCCESS;
	}
};
 
void adjust_token_privileges();
autohandle_t open_dll(const wchar_t *path);
std::uint64_t get_file_size(const autohandle_t &file);
std::string wide_to_narrow(const wchar_t *s);
int watoi(const wchar_t *s);
std::vector<std::uint8_t> read_dll(const wchar_t *path);
RemoteMemory allocate_remote_memory(const autohandle_t &process, size_t size);
void print_buffer(std::ostream &dst, const std::vector<std::uint8_t> &buffer);
bool find_export(uintptr_t &dst, const std::uint8_t *dll_buffer, const char *name_to_find);

template <typename T>
typename std::enable_if<std::is_pointer<T>::value, bool>::type
find_export(T &dst, void *base_address, const std::uint8_t *dll_buffer, const char *name_to_find){
	uintptr_t temp;
	if (!find_export(temp, dll_buffer, name_to_find))
		return false;
	dst = (T)((uintptr_t)base_address + temp);
	return true;
}

template <typename T>
T integer_ceiling(T n, T modulo){
	if (n < modulo)
		return modulo;
	auto m = n % modulo;
	if (!m)
		return n;
	return n + (modulo - m);
}
