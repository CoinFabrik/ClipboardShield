#pragma once

#include "../libvts/AbstractLogger.h"
#include <fstream>
#include <mutex>
#include <memory>
#include <string>

class FileLogger : public AbstractLogger{
	std::unique_ptr<std::ofstream> file;
	std::mutex mutex;
public:
	FileLogger(const std::wstring &path);
	void output(const char *, size_t) override;
	void output(const wchar_t *, size_t) override;
};
