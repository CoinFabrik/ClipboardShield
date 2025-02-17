#include "ProcessInjector.h"
#include "utility.h"
#include <iostream>
#include <memory>
#include <stdexcept>
#include <algorithm>
#include <sstream>

std::vector<std::uint8_t> serialize_self();

int wmain(int argc, wchar_t **argv){
	if (argc < 3){
		std::cerr << "Usage: " << argv[0] << " <DLL path> <PID>\n";
		TRACE("Bad usage.");
		return -1;
	}
	try{
		ProcessInjector injector(argv[1], argv[2]);
		injector.inject();
		//injector.monitor();
		return 0;
	}catch (std::exception &e){
		std::stringstream stream;
		stream << "Exception caught: " << e.what() << std::endl;
		auto string = stream.str();
		std::cerr << string;
		OutputDebugStringA(string.c_str());
	}catch (...){
		std::cerr << "Unknown exception caught.\n";
		OutputDebugStringA("Unknown exception caught.\n");
	}
	return -1;
}
