#include "FileLogger.h"
#include "../common/utility.h"

FileLogger::FileLogger(const std::wstring &path){
	this->file.reset(new std::ofstream(path, std::ios::binary | std::ios::app));
	if (!*this->file)
		this->file.reset();
	else{
		static const char separator[] = "--------------------------------------------------------------------------------\r\n";
		this->file->write(separator, strlen(separator));
	}
}

template <typename T>
void integer_to_string(char *&dst, T n, int width){
	for (int i = width; i--;){
		dst[i] = n % 10 + '0';
		n /= 10;
	}
	dst += width;
}

std::array<char, 32> get_time_as_string(size_t &length){
	SYSTEMTIME st;
	GetLocalTime(&st);

	std::array<char, 32> ret;
	char *p = ret.data();
	*(p++) = '[';
	integer_to_string(p, st.wYear, 4);
	*(p++) = '-';
	integer_to_string(p, st.wMonth, 2);
	*(p++) = '-';
	integer_to_string(p, st.wDay, 2);
	*(p++) = ' ';
	integer_to_string(p, st.wHour, 2);
	*(p++) = ':';
	integer_to_string(p, st.wMinute, 2);
	*(p++) = ':';
	integer_to_string(p, st.wSecond, 2);
	*(p++) = '.';
	integer_to_string(p, st.wMilliseconds, 3);
	*(p++) = ']';
	*(p++) = ' ';
	length = p - ret.data();
	*(p++) = 0;
	return ret;
}

void FileLogger::output(const char *s, size_t n){
	if (!this->file)
		return;
	LOCK_MUTEX(this->mutex);
	if (this->file){
		size_t length;
		auto time = get_time_as_string(length);
		this->file->write(time.data(), length);
		this->file->write(s, n);
		static const char newline[] = "\r\n";
		this->file->write(newline, 2);
		this->file->flush();
	}
}

static std::string ws2s(const wchar_t *s, size_t n){
	if (n > (size_t)std::numeric_limits<int>::max())
		throw std::runtime_error("Can't convert string to UTF-8: String is too large");
	auto size_needed = WideCharToMultiByte(CP_UTF8, 0, s, (int)n, nullptr, 0, nullptr, nullptr);
	std::string ret(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, s, (int)n, &ret[0], size_needed, nullptr, nullptr);
	return ret;
}

void FileLogger::output(const wchar_t *s, size_t n){
	if (!this->file)
		return;
	auto s2 = ws2s(s, n);
	this->output(s2.c_str(), s2.size());
}
