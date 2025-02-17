#include <Windows.h>
#include <iostream>
#include <vector>
#include <cstdint>

bool match_service_name(const wchar_t *s){
	size_t length = wcslen(s);
	static const wchar_t pattern[] = L"cbdhsvc_";
	static const size_t pattern_length = sizeof(pattern) / 2 - 1;
	if (length < pattern_length)
		return false;
	for (size_t i = pattern_length; i--;)
		if (pattern[i] != s[i])
			return false;
	for (size_t i = 8; i < length; i++){
		auto c = s[i];
		if (!(c >= '0' && c <= '9' || c >= 'a' && c <= 'f'))
			return false;
	}
	return true;
}

std::vector<DWORD> find_all_user_clipboard_services(){
	std::vector<DWORD> ret;
	auto scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE);
	if (scm == nullptr)
		return ret;

	std::vector<std::uint8_t> buffer;
	buffer.resize(256 << 10);
	DWORD needed;
	DWORD count;
	DWORD resume_handle = 0;
	while (true){
		bool no_more_data = false;
		if (!EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32 | SERVICE_WIN32_OWN_PROCESS | SERVICE_WIN32_SHARE_PROCESS, SERVICE_ACTIVE, &buffer[0], (DWORD)buffer.size(), &needed, &count, &resume_handle, nullptr)){
			auto error = GetLastError();
			if (error == ERROR_MORE_DATA){
				if (count == 0){
					buffer.resize(needed * 3 / 2);
					resume_handle = 0;
					continue;
				}
				no_more_data = false;
			}else
				break;
		}else
			no_more_data = true;

		auto services = (ENUM_SERVICE_STATUS_PROCESSW *)&buffer[0];
		for (size_t i = 0; i < count; i++)
			if (match_service_name(services[i].lpServiceName))
				ret.push_back(services[i].ServiceStatusProcess.dwProcessId);
		if (no_more_data)
			break;
	}

	CloseServiceHandle(scm);
	return ret;
}

int main(){
	for (auto pid : find_all_user_clipboard_services())
		std::cout << pid << std::endl;
	return 0;
}
