#include "StdOutLogger.h"
#include "SimpleInterceptor.h"
#include <csignal>

SimpleInterceptor *interceptor = nullptr;

static void on_stop(int){
	interceptor->request_general_stop();
}

bool am_i_elevated(){
	bool ret = false;
	HANDLE hToken;
	if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)){
		TOKEN_ELEVATION Elevation;
		DWORD cbSize = sizeof(TOKEN_ELEVATION);
		if (GetTokenInformation(hToken, TokenElevation, &Elevation, sizeof(Elevation), &cbSize))
			ret = !!Elevation.TokenIsElevated;
		CloseHandle(hToken);
	}
	return ret;
}

int main(){
	if (!am_i_elevated()){
		std::cerr << "ERROR: Please run as administrator.\n";
		return -1;
	}

	int ret = 0;
	try{
		StdOutLogger logger(std::cout);
		SimpleInterceptor si(logger);
		si.start();
		interceptor = &si;
		signal(SIGINT, on_stop);
		signal(SIGTERM, on_stop);
		Stopper stopper(si);
		si.loop();
	}catch (std::bad_alloc &){
		std::cerr << "Exception caught: bad allocation\n";
		ret = -1;
	}catch (std::exception &e){
		std::cerr << "Exception caught: " << e.what() << std::endl;
		ret = -1;
	}catch (...){
		std::cerr << "Exception caught: unknown\n";
		ret = -1;
	}
	return ret;
}
