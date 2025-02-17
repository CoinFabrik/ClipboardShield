#include <Windows.h>
#include <memory>
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>

class HandleReleaser{
public:
	void operator()(HANDLE h){
		if (h)
			CloseHandle(h);
	}
};

typedef std::unique_ptr<std::remove_pointer<HANDLE>::type, HandleReleaser> autohandle_t;

HANDLE open_file_for_normal_read(const wchar_t *path){
	return CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
}

inline void report_failed_win32_function(const char *function, DWORD error, const char *extra_context = nullptr){
	std::stringstream stream;
	if (extra_context)
		stream << "Error while " << extra_context << ": ";
	else
		stream << "Error: ";
	stream << function << "() failed with error ";
	switch (error){
		case ERROR_ACCESS_DENIED:
			stream << "access denied";
			break;
		case ERROR_FILE_NOT_FOUND:
			stream << "file not found";
			break;
		default:
			stream << error << " (" << std::hex << std::setw(8) << std::setfill('0') << error << ")";
			break;
	}
	throw std::runtime_error(stream.str());
}

autohandle_t open_file_for_normal_read2(const std::wstring &path, bool no_throw){
	auto handle = open_file_for_normal_read(path.c_str());
	if (handle == INVALID_HANDLE_VALUE){
		if (no_throw)
			return nullptr;
		auto error = GetLastError();
		std::string context = "Opening file ";
		for (auto c : path)
			context += (char)c;
		report_failed_win32_function("CreateFile", error, context.c_str());
	}
	return autohandle_t(handle);
}

template <typename T>
inline std::basic_ostream<T> &operator<<(std::basic_ostream<T> &stream, const GUID &guid){
	stream << guid.Data1 << '-' << guid.Data2 << '-' << guid.Data3;
	for (auto c : guid.Data4)
		stream << '-' << (int)c;
	return stream;
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

GUID get_file_guid(const std::wstring &path){
	return get_file_guid(open_file_for_normal_read2(path, false));
}

int wmain(int argc, wchar_t **argv, wchar_t **){
	if (argc < 2)
		return -1;
	auto guid = get_file_guid(argv[1]);
	std::cout << guid << std::endl;
	return 0;
}
