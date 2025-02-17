#define NOMINMAX
#include "ServiceInstaller.h"
#include <iostream>
#include <memory>
#include <sstream>
#include <Windows.h>

struct ReleaseServiceHandle{
	void operator()(SC_HANDLE h){
		if (h)
			CloseServiceHandle(h);
	}
};

typedef std::unique_ptr<typename std::remove_pointer<SC_HANDLE>::type, ReleaseServiceHandle> autosch_t;

static void win32_function_failed(const char *function, DWORD error){
	std::stringstream stream;
	stream << function << "() failed with error " << error;
	throw std::runtime_error(stream.str());
}

static autosch_t open_service_manager(DWORD access){
	auto manager = OpenSCManager(nullptr, nullptr, access);
	if (!manager)
		win32_function_failed("OpenSCManager", GetLastError());
	return autosch_t(manager);
}

static autosch_t create_service(
		const autosch_t &manager,
		const wchar_t *service_name,
		const wchar_t *display_name,
		const wchar_t *executable_path,
		DWORD start_type){

	auto service = CreateService(
		manager.get(),
		service_name,
		display_name,
		SERVICE_QUERY_STATUS,
		SERVICE_WIN32_OWN_PROCESS,
		start_type,
		SERVICE_ERROR_NORMAL,
		executable_path,
		nullptr,
		nullptr,
		L"",
		nullptr,
		nullptr
	);
	if (!service)
		win32_function_failed("CreateService", GetLastError());
	return autosch_t(service);
}

static autosch_t open_service(const autosch_t &manager, const wchar_t *service_name, DWORD permissions){
	auto service = OpenService(manager.get(), service_name, permissions);
	if (!service)
		win32_function_failed("OpenService", GetLastError());
	return autosch_t(service);
}

static DWORD get_service_status(const autosch_t &service){
	SERVICE_STATUS ssSvcStatus;
	if (!QueryServiceStatus(service.get(), &ssSvcStatus))
		return std::numeric_limits<DWORD>::max();
	return ssSvcStatus.dwCurrentState;
}

static bool stop_service(const autosch_t &service){
	SERVICE_STATUS ssSvcStatus = {};
	return !!ControlService(service.get(), SERVICE_CONTROL_STOP, &ssSvcStatus);
}

static void stop_service(const autosch_t &service, const wchar_t *service_name){
	if (!stop_service(service))
		return;

	std::wcout << L"Stopping " << service_name << std::endl;

	Sleep(1000);
	DWORD status;
	while ((status = get_service_status(service)) == SERVICE_STOP_PENDING)
		Sleep(1000);

	if (status == SERVICE_STOPPED)
		std::wcout << service_name << " stopped.\n";
	else
		std::wcout << service_name << " failed to stop or something else went wrong.\n";
}

void install_service(const wchar_t *service_name, const wchar_t *display_name, DWORD start_type){
	wchar_t path[MAX_PATH];
	if (!GetModuleFileNameW(nullptr, path, ARRAYSIZE(path)))
		win32_function_failed("GetModuleFileNameW", GetLastError());

	auto manager = open_service_manager(SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE);
	auto service = create_service(manager, service_name, display_name, path, start_type);

	std::wcout << service_name << " has been installed.\n";
}

void uninstall_service(const wchar_t *service_name){
	auto manager = open_service_manager(SC_MANAGER_CONNECT);
	auto service = open_service(manager, service_name, SERVICE_STOP | SERVICE_QUERY_STATUS | DELETE);
	stop_service(service, service_name);
	if (!DeleteService(service.get()))
		win32_function_failed("DeleteService", GetLastError());
	std::wcout << service_name << " has been uninstalled.\n";
}
