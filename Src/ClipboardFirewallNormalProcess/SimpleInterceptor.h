#pragma once

#include "../libvts/Interceptor.h"
#include <atomic>

class SimpleInterceptor : public Interceptor{
	std::atomic<bool> atomic_stop = false;
public:
	SimpleInterceptor(AbstractLogger &logger): Interceptor(logger, InterceptorMode::NormalProcess){}
	void request_general_stop() override;
	void loop();
};
