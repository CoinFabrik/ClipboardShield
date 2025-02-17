#include "StdLogger.h"
#include "../common/utility.h"
#include <iostream>

void StdLogger::output(const char *s, size_t n){
	LOCK_MUTEX(this->mutex);
	std::cout.write(s, n);
	std::cout << std::endl;
}

void StdLogger::output(const wchar_t *s, size_t n){
	LOCK_MUTEX(this->mutex);
	std::wcout.write(s, n);
	std::wcout << std::endl;
}
