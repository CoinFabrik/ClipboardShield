#include "StdOutLogger.h"
#include "..\spdlog\spdlog.h"
#include <Windows.h>
#ifdef max
#undef max
#endif
#include <algorithm>
#include <string>

#define REG_CONFIG_PATH L"Software\\Classes\\Vornex\\Config\\"

static std::string ws2s(const wchar_t *rs, size_t n = 0){
	if (!n)
		n = wcslen(rs);
	if (n > (size_t)std::numeric_limits<int>::max())
		throw std::runtime_error("Can't convert string to UTF-8: String is too large");
	auto size_needed = WideCharToMultiByte(CP_UTF8, 0, rs, (int)n, nullptr, 0, nullptr, nullptr);
	std::string ret(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, rs, (int)n, &ret[0], size_needed, nullptr, nullptr);
	return ret;
}

static std::string ws2s(const std::wstring &rs){
	return ws2s(rs.c_str(), rs.size());
}

StdOutLogger::StdOutLogger(std::ostream &stream): stream(&stream){
	WCHAR fsConfigPath[256];
	DWORD buffsize = 256;
	HKEY hkeyDXVer;

	long success = RegOpenKeyExW(HKEY_LOCAL_MACHINE, REG_CONFIG_PATH, 0, KEY_READ, &hkeyDXVer);

	if (success != 0)
		return;

	success = RegQueryValueExW(hkeyDXVer, L"logPath", nullptr, nullptr, (LPBYTE)fsConfigPath, &buffsize);
	RegCloseKey(hkeyDXVer);

	if (success != 0)
		return;

	auto fsConfigFilePath = ws2s(fsConfigPath) + "VTSlog";
	auto sfsConfigFilePath = fsConfigFilePath;
	auto file_logger = spdlog::rotating_logger_mt("VTSlog", sfsConfigFilePath, 5 << 20, 20, true);
	spdlog::set_async_mode(1048576);
	spdlog::set_level(spdlog::level::debug);
}

void StdOutLogger::output(const char *string){
	std::lock_guard<std::mutex> lg(this->mutex);
	*this->stream << string << std::endl;
	spdlog::get("VTSlog")->info(string);
}

void StdOutLogger::output(const wchar_t *string){
	auto narrow = ws2s(string);
	{
		std::lock_guard<std::mutex> lg(this->mutex);
		*this->stream << narrow << std::endl;
		spdlog::get("VTSlog")->info(narrow.c_str());
	}
}
