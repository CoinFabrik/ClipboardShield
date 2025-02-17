#pragma once

#include <string>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <memory>

template <typename T>
void utf8_to_string(std::basic_string<T> &dst, const std::uint8_t *buffer, size_t n){
	static const std::uint8_t utf8_lengths[] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
		3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 0, 0,
		0xFF, 0x1F, 0x0F, 0x07, 0x03, 0x01
	};
	static const std::uint8_t bom[] = { 0xEF, 0xBB, 0xBF };
	static const std::uint8_t *masks = utf8_lengths + 0x100;
	if (n >= 3 && !memcmp(buffer, bom, 3)){
		buffer += 3;
		n -= 3;
	}
	dst.resize(n);
	T *temp_pointer = &dst[0];
	size_t writer = 0;
	for (size_t i = 0; i != n;){
		std::uint8_t byte = buffer[i++];
		std::uint8_t length = utf8_lengths[byte];
		if (length > n - i)
			break;
		unsigned wc = byte & masks[length];
		for (;length; length--){
			wc <<= 6;
			wc |= (T)(buffer[i++] & 0x3F);
		}
		temp_pointer[writer++] = wc;
	}
	dst.resize(writer);
}

inline unsigned utf8_bytes(unsigned c){
	static const unsigned masks[] = {
		0x0000007F,
		0x000007FF,
		0x0000FFFF,
		0x001FFFFF,
		0x03FFFFFF,
	};
	for (unsigned i = 0; i != sizeof(masks) / sizeof(*masks); i++)
		if ((c & masks[i]) == c)
			return i + 1;
	return 6;
}

template <typename T>
size_t utf8_size(const std::basic_string<T> &s){
	size_t ret = 0;
	for (auto c : s)
		ret += utf8_bytes(c);
	return ret;
}

template <typename T>
void string_to_utf8(std::string &dst, const std::basic_string<T> &src){
	static const unsigned char masks[] = { 0, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE };
	dst.resize(utf8_size(src));
	auto pointer = (unsigned char *)&dst[0];
	const T *src_pointer = &src[0];
	for (size_t i = 0, n = src.size(); i != n; i++){
		unsigned c = src_pointer[i];
		unsigned size = utf8_bytes(c) - 1;

		if (!size){
			*(pointer++) = (unsigned char)c;
			continue;
		}

		unsigned char temp[10];
		unsigned temp_size = 0;

		do{
			temp[temp_size++] = c & 0x3F | 0x80;
			c >>= 6;
		}while (temp_size != size);
		*(pointer++) = c | masks[temp_size];
		while (temp_size)
			*(pointer++) = temp[--temp_size];
	}
}

inline std::wstring utf8_to_string(const std::string &src){
	std::wstring ret;
	utf8_to_string(ret, (const unsigned char *)&src[0], src.size());
	return ret;
}

inline std::wstring utf8_to_string(const char *src){
	std::wstring ret;
	utf8_to_string(ret, (const unsigned char *)src, strlen(src));
	return ret;
}

inline std::string string_to_utf8(const std::wstring &src){
	std::string ret;
	string_to_utf8(ret, src);
	return ret;
}

template <typename T>
std::wstring load_utf8_file(const T *path){
	std::ifstream file(path, std::ios::binary);
	if (!file)
		return {};
	file.seekg(0, std::ios::end);
	auto size = file.tellg();
	std::unique_ptr<unsigned char[]> data(new unsigned char[size]);
	file.seekg(0);
	file.read((char *)data.get(), size);

	std::wstring ret;
	utf8_to_string(ret, data.get(), size);
	return ret;
}
