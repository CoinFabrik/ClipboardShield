#include "utility.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <sstream>

void adjust_token_privileges(){
	TOKEN_PRIVILEGES tp;
	HANDLE token;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)){
		auto error = GetLastError();
		std::cout << "Can't open own process token: " << error << std::endl;
		return;
	}

	autohandle_t atoken(token);
	tp.PrivilegeCount = 1;
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
 
	tp.Privileges[0].Luid.LowPart = 20;
	tp.Privileges[0].Luid.HighPart = 0;
 
	if (!AdjustTokenPrivileges(token, false, &tp, 0, nullptr, nullptr)){
		auto error = GetLastError();
		std::cout << "Failed to adjust token privileges: " << error << std::endl;
	}else
		std::cout << "Token privileges adjusted successfully.\n";
}

autohandle_t open_dll(const wchar_t *path){
	std::cout << "Opening the DLL.\n";
	auto file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
	if (file == INVALID_HANDLE_VALUE)
		file = nullptr;
	return autohandle_t(file);
}

std::uint64_t get_file_size(const autohandle_t &file){
	LARGE_INTEGER li;
	if (!GetFileSizeEx(file.get(), &li))
		return 0;
	return li.QuadPart;
}

std::string wide_to_narrow(const wchar_t *s){
	std::string ret;
	for (; *s; s++)
		ret += (char)*s;
	return ret;
}

int watoi(const wchar_t *s){
	auto original_pointer = s;
	int ret = 0;
	for (; *s; s++){
		if (!isdigit(*s))
			throw std::runtime_error("Invalid number: " + wide_to_narrow(original_pointer));
		ret *= 10;
		ret += *s - '0';
	}
	return ret;
}

std::vector<std::uint8_t> read_dll(const wchar_t *path){
	auto file = open_dll(path);
	if (!file)
		throw std::runtime_error("DLL not found");

	auto file_size = get_file_size(file);
	if (!file_size)
		throw std::runtime_error("DLL is an empty file or failed to get its size");

	if (file_size > (std::uint64_t)std::numeric_limits<size_t>::max())
		throw std::runtime_error("Can't load DLL into memory.");

	std::vector<std::uint8_t> ret((size_t)file_size);
	size_t total_read = 0;
	while (total_read < ret.size()){
		auto read_size = (DWORD)std::min<size_t>(ret.size() - total_read, std::numeric_limits<DWORD>::max());
		DWORD bytes_read = 0;
		if (!ReadFile(file.get(), &ret[total_read], read_size, &bytes_read, nullptr)){
			auto error = GetLastError();
			throw std::runtime_error("Failed to read file: " + std::to_string(error));
		}
		total_read += bytes_read;
	}
	return ret;
}

RemoteMemory allocate_remote_memory(const autohandle_t &process, size_t size){
	std::cout << "Attempting to allocate " << size << " bytes remotely.\n";
	auto memory = VirtualAllocEx(process.get(), nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (!memory){
		auto error = GetLastError();
		throw std::runtime_error("Failed to allocate memory remotely: " + std::to_string(error));
	}
	return RemoteMemory(process.get(), memory, size);
}

void print_buffer(std::ostream &dst, const std::vector<std::uint8_t> &buffer){
	if (!buffer.size()){
		dst << std::endl;
		return;
	}

	std::stringstream stream;
	bool newline = false;
	for (size_t i = 0; i < buffer.size(); i++){
		if (i % 16)
			stream << ' ';
		stream << std::hex << std::setw(2) << std::setfill('0') << (int)buffer[i];
		if (i % 16 == 15){
			stream << std::endl;
			newline = true;
		}else
			newline = false;
	}
	dst << stream.str();
	if (!newline)
		dst << std::endl;
}

struct ExportsTable{
	std::uint32_t export_flags;
	std::uint32_t timestamp;
	std::uint16_t major_version;
	std::uint16_t minor_version;
	std::uint32_t name_rva;
	std::uint32_t ordinal_base;
	std::uint32_t address_table_entry_count;
	std::uint32_t name_pointer_count;
	std::uint32_t export_address_table_rva;
	std::uint32_t name_pointer_rva;
	std::uint32_t ordinal_table_rva;
};

bool find_export(uintptr_t &dst, const std::uint8_t *dll_buffer, const char *name_to_find){
	auto dos_header = (PIMAGE_DOS_HEADER)dll_buffer;
	auto nt_header = (PIMAGE_NT_HEADERS)(dll_buffer + dos_header->e_lfanew);
	auto &exports_info = nt_header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
	std::vector<char> temp_buffer(nt_header->OptionalHeader.SizeOfImage);

	memcpy(&temp_buffer[0], dll_buffer, nt_header->OptionalHeader.SizeOfHeaders);
	auto section_header = (PIMAGE_SECTION_HEADER)(nt_header + 1);
	for (decltype(nt_header->FileHeader.NumberOfSections) i = 0; i < nt_header->FileHeader.NumberOfSections; i++){
		auto &section = section_header[i];
		memcpy(&temp_buffer[section.VirtualAddress], dll_buffer + section.PointerToRawData, section.SizeOfRawData);
	}
	auto &exports_table = *(ExportsTable *)&temp_buffer[exports_info.VirtualAddress];

	for (std::uint32_t i = 0; i < exports_table.address_table_entry_count; i++){
		auto offset = ((std::uint32_t *)&temp_buffer[exports_table.name_pointer_rva])[i];
		auto name = &temp_buffer[offset];
		if (!strcmp(name, name_to_find)){
			dst = ((std::uint32_t *)&temp_buffer[exports_table.export_address_table_rva])[i];
			return true;
		}
	}

	return false;
}
