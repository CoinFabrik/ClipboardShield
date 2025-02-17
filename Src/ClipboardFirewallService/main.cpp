#include <iostream>
#define NOMINMAX
#include <windows.h>
#include "ServiceInstaller.h"
#include "ServiceBase.h"
#include "Service.h"
#include "FileLogger.h"

#define SERVICE_NAME             L"ClipboardFirewall"
#define SERVICE_DISPLAY_NAME     L"Nektra Clipboard Shield"
#define SERVICE_START_TYPE       SERVICE_AUTO_START

void DebugPrint(__in_z LPWSTR format, ...){
	va_list ap;
	va_start(ap, format);
	wchar_t buf[2048];
	swprintf_s(buf, _countof(buf), format, ap);
	OutputDebugStringW(buf);
	va_end(ap);
}

static std::wstring get_directory(){
	WCHAR own_module_path[2048 + 64];

	::GetModuleFileNameW(nullptr, own_module_path, 2048);

	std::wstring ret = own_module_path;
	auto slash = ret.find_last_of(L"\\/");
	if (slash != ret.npos)
		ret = ret.substr(0, slash + 1);

	return ret;
}

int wmain(int argc, wchar_t* argv[]){
	try{
		if (argc >= 2){
			if (wcscmp(L"-install", argv[1]) == 0)
				install_service(SERVICE_NAME, SERVICE_DISPLAY_NAME, SERVICE_START_TYPE);
			else if (wcscmp(L"-uninstall", argv[1]) == 0)
				uninstall_service(SERVICE_NAME);
		}else{
			FileLogger logger(get_directory() + L"\\log.txt");
			Service service(logger, SERVICE_NAME);
			if (!CServiceBase::Run(service)){
				uint32_t err = GetLastError();
				DebugPrint(L"Service failed to run w/err 0x%08lx\n", err);
			}
		}
	}catch (std::exception &e){
		std::cerr << L"Exception caught: " << e.what() << std::endl;
		return -1;
	}catch (...){
		std::cerr << L"Unknown exception caught.\n";
		return -1;
	}

	return 0;
}
