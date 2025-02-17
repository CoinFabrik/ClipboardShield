#pragma once

#include "ServiceBase.h"
#include <Interceptor.h>

class Service : public CServiceBase, public Interceptor{
	void OnStart(DWORD dwArgc, LPWSTR *lpszArgv) override;
	void OnStop() override;
public:
	Service(AbstractLogger &logger,
		LPWSTR pszServiceName,
		BOOL fCanStop = TRUE,
		BOOL fCanShutdown = TRUE,
		BOOL fCanPauseContinue = FALSE);
	void request_general_stop() override;
};
