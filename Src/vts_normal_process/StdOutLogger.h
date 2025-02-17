#pragma once

#include <Logger.h>
#include <iostream>
#include <mutex>

class StdOutLogger : public Logger{
	std::ostream *stream;
	std::mutex mutex;
public:
	StdOutLogger(std::ostream &);
	void output(const char *string) override;
	void output(const wchar_t *string) override;
};
