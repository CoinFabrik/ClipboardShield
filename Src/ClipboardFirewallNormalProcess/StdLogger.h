#pragma once

#include "../libvts/AbstractLogger.h"
#include <mutex>

class StdLogger : public AbstractLogger{
	std::mutex mutex;
public:
	void output(const char *, size_t) override;
	void output(const wchar_t *, size_t) override;
};
