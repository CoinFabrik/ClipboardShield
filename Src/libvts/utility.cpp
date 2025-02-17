#include "utility.h"
#include <sstream>
#include <stdexcept>
#include <iomanip>

void release_handle(HANDLE h){
	if (h)
		CloseHandle(h);
}

void release_hkey(HKEY h){
	if (h)
		RegCloseKey(h);
}

void HkeyReleaser::operator()(HKEY h){
	release_hkey(h);
}

autohandle_t to_autohandle(HANDLE h){
	return autohandle_t(h);
}

autohkey_t to_autohandle(HKEY h){
	return autohkey_t(h);
}

void safe_join(std::thread &thread){
	if (thread.joinable() && thread.get_id() != std::this_thread::get_id()){
		thread.join();
		thread = std::thread();
	}
}

std::wstring s2ws(const std::string &s){
	if (s.size() > (size_t)std::numeric_limits<int>::max())
		throw std::runtime_error("Can't convert string to UTF-16: String is too large");
	int size_needed = ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
	std::wstring ret(size_needed, 0);
	::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &ret[0], size_needed);
	return ret;
}

std::string ws2s(const std::wstring &rs){
	if (rs.size() > (size_t)std::numeric_limits<int>::max())
		throw std::runtime_error("Can't convert string to UTF-8: String is too large");
	auto size_needed = WideCharToMultiByte(CP_UTF8, 0, rs.c_str(), (int)rs.size(), nullptr, 0, nullptr, nullptr);
	std::string ret(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, rs.c_str(), (int)rs.size(), &ret[0], size_needed, nullptr, nullptr);
	return ret;
}

std::uint64_t get_performance_frequency(){
	LARGE_INTEGER li;
	QueryPerformanceFrequency(&li);
	return li.QuadPart;
}

std::uint64_t get_performance_counter(){
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return li.QuadPart;
}

std::uint32_t get_process_session_id(std::uint32_t pid){
	DWORD ret;
	if (!ProcessIdToSessionId(pid, &ret))
		ret = std::numeric_limits<DWORD>::max();
	return (std::uint32_t)ret;
}

GUID get_file_guid(const autohandle_t &file){
	FILE_OBJECTID_BUFFER id;
	DWORD bytes_written;
	if (!DeviceIoControl(file.get(), FSCTL_CREATE_OR_GET_OBJECT_ID, nullptr, 0, &id, sizeof(id), &bytes_written, nullptr)){
		auto error = GetLastError();
		report_failed_win32_function("DeviceIoControl", error, "checking executable files");
	}
	GUID ret;
	memcpy(&ret, &id.ObjectId, sizeof(GUID));
	return ret;
}

HANDLE open_file_for_normal_read(const wchar_t *path){
	return CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
}

autohandle_t open_file_for_normal_read2(const wchar_t *path, bool no_throw = false){
	auto handle = open_file_for_normal_read(path);
	if (handle == INVALID_HANDLE_VALUE){
		if (no_throw)
			return nullptr;
		auto error = GetLastError();
		std::string context = "Opening file ";
		for (auto p = path; *p; p++)
			context += (char)*p;
		report_failed_win32_function("CreateFile", error, context.c_str());
	}
	return autohandle_t(handle);
}

autohandle_t open_file_for_normal_read2(const std::wstring &path, bool no_throw){
	return open_file_for_normal_read2(path.c_str(), no_throw);
}

int paths_are_equivalent(const wchar_t *a, const wchar_t *b, bool no_throw){
	auto af = open_file_for_normal_read2(a, no_throw);
	if (!af)
		return -1;
	auto bf = open_file_for_normal_read2(b, no_throw);
	if (!bf)
		return -1;
	BY_HANDLE_FILE_INFORMATION bhfi_a, bhfi_b;
	if (!GetFileInformationByHandle(af.get(), &bhfi_a) || !GetFileInformationByHandle(bf.get(), &bhfi_b)){
		if (no_throw)
			return -1;
		auto error = GetLastError();
		report_failed_win32_function("GetFileInformationByHandle", error, "Comparing paths");
	}
	return
		bhfi_a.dwVolumeSerialNumber == bhfi_b.dwVolumeSerialNumber &&
		bhfi_a.nFileIndexLow == bhfi_b.nFileIndexLow &&
		bhfi_a.nFileIndexHigh == bhfi_b.nFileIndexHigh;
}
