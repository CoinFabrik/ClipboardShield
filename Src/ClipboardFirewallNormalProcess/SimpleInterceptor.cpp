#include "SimpleInterceptor.h"
#include <thread>

void SimpleInterceptor::request_general_stop(){
	this->atomic_stop = true;
}

void SimpleInterceptor::loop(){
	while (!this->atomic_stop)
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
}
