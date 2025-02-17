#include "SimpleInterceptor.h"
#include <thread>
#include <iostream>

void SimpleInterceptor::request_general_stop(){
	this->atomic_stop = true;
}

void SimpleInterceptor::loop(){
	this->log(LogEvent::Low, L"SimpleInterceptor::loop(): entered.");
	while (!this->atomic_stop)
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	this->log(LogEvent::Low, L"SimpleInterceptor::loop(): leaving.");
}
