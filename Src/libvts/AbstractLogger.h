#pragma once

#include <sstream>
#include <string>
#include <cstring>

class AbstractLogger{
public:
	virtual ~AbstractLogger(){}
	virtual void output(const char *, size_t) = 0;
	virtual void output(const wchar_t *, size_t) = 0;
	void output(const char *s){
		this->output(s, strlen(s));
	}
	void output(const wchar_t *s){
		this->output(s, wcslen(s));
	}
};

class LogLine{
	AbstractLogger *logger;
	std::wstringstream stream;
public:
	LogLine(AbstractLogger &logger): logger(&logger){}
	~LogLine(){
		if (this->logger){
			auto s = std::move(this->stream.str());
			this->logger->output(s.c_str(), s.size());
		}
	}
	LogLine(const LogLine &) = delete;
	LogLine &operator=(const LogLine &) = delete;
	LogLine(LogLine &&other){
		*this = std::move(other);
	}
	const LogLine &operator=(LogLine &&other){
		this->logger = other.logger;
		other.logger = nullptr;
		this->stream = std::move(other.stream);
		return *this;
	}
	template <typename T>
	LogLine operator<<(const T &x){
		this->stream << x;
		return std::move(*this);
	}
	LogLine operator<<(std::ostream &(*f)(std::ostream &)){
		this->stream << f;
		return std::move(*this);
	}
	LogLine operator<<(std::ios &(*f)(std::ios &)){
		this->stream << f;
		return std::move(*this);
	}
	LogLine operator<<(std::ios_base &(*f)(std::ios_base &)){
		this->stream << f;
		return std::move(*this);
	}
	template <size_t N>
	LogLine operator<<(const std::array<char, N> &array){
		for (auto c : array){
			if (!c)
				break;
			this->stream << c;
		}
		return std::move(*this);
	}
	template <size_t N>
	LogLine operator<<(const std::array<wchar_t, N> &array){
		for (auto c : array){
			if (!c)
				break;
			this->stream << c;
		}
		return std::move(*this);
	}
};
