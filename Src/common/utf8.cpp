#include "utf8.hpp"

std::wstring utf8_to_string(const std::string &src){
	std::wstring ret;
	utf8_to_string(ret, (const unsigned char *)&src[0], src.size());
	return ret;
}

std::wstring utf8_to_string(const char *src){
	std::wstring ret;
	utf8_to_string(ret, (const unsigned char *)src, strlen(src));
	return ret;
}

std::string string_to_utf8(const std::wstring &src){
	std::string ret;
	string_to_utf8(ret, src);
	return ret;
}
