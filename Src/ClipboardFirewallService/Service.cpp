#include "Service.h"

Service::Service(AbstractLogger &logger, LPWSTR pszServiceName, BOOL fCanStop, BOOL fCanShutdown, BOOL fCanPauseContinue)
		: CServiceBase(pszServiceName, fCanStop, fCanShutdown, fCanPauseContinue)
		, Interceptor(logger, InterceptorMode::Service){
}

void Service::OnStart(DWORD dwArgc, LPWSTR *lpszArgv){
	try{
		this->start();
	}catch (...){
	}
}

void Service::OnStop(){
	this->stop();
}

void Service::request_general_stop(){
	this->Stop();
}
