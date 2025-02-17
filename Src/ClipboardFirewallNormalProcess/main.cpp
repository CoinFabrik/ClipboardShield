#include "SimpleInterceptor.h"
#include "StdLogger.h"
#include <csignal>
#include <iostream>
#include <exception>

SimpleInterceptor *interceptor = nullptr;

static void on_stop(int){
	std::cout << "Stop request accepted.\n";
	interceptor->request_general_stop();
}

int main(){
	int ret = 0;
	try{
		{
			StdLogger std_logger;
			SimpleInterceptor si(std_logger);
			si.start();
			interceptor = &si;
			signal(SIGINT, on_stop);
			signal(SIGTERM, on_stop);
			Stopper stopper(si);
			si.loop();
		}
		std::cout << "Terminating normally.\n";
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
