#pragma once

#include "utility.h"
#include <cstdint>
#include <cstring>

struct Md5Digest{
	static const size_t length = 16;
	std::uint8_t data[length];
};

class Md5{
	std::uint8_t data[64];
	std::uint32_t datalen;
	std::uint64_t bitlen;
	std::uint32_t state[4];
	
	struct Parameters{
		char m_index;
		char s;
		std::uint32_t t;
	};
	
	struct MetaParameters{
		void (*f)(std::uint32_t &a, std::uint32_t b, std::uint32_t c, std::uint32_t d, std::uint32_t m, std::uint32_t s, std::uint32_t t);
		const Parameters *parameters;
		size_t length;
	};

	Parameters parameters[16 * 4];
	MetaParameters metaparameters[4];

	void init(){
		int i = 0;

		this->parameters[i++] = {  0,  7, 0xd76aa478 };
		this->parameters[i++] = {  1, 12, 0xe8c7b756 };
		this->parameters[i++] = {  2, 17, 0x242070db };
		this->parameters[i++] = {  3, 22, 0xc1bdceee };
		this->parameters[i++] = {  4,  7, 0xf57c0faf };
		this->parameters[i++] = {  5, 12, 0x4787c62a };
		this->parameters[i++] = {  6, 17, 0xa8304613 };
		this->parameters[i++] = {  7, 22, 0xfd469501 };
		this->parameters[i++] = {  8,  7, 0x698098d8 };
		this->parameters[i++] = {  9, 12, 0x8b44f7af };
		this->parameters[i++] = { 10, 17, 0xffff5bb1 };
		this->parameters[i++] = { 11, 22, 0x895cd7be };
		this->parameters[i++] = { 12,  7, 0x6b901122 };
		this->parameters[i++] = { 13, 12, 0xfd987193 };
		this->parameters[i++] = { 14, 17, 0xa679438e };
		this->parameters[i++] = { 15, 22, 0x49b40821 };

		this->parameters[i++] = {  1,  5, 0xf61e2562 };
		this->parameters[i++] = {  6,  9, 0xc040b340 };
		this->parameters[i++] = { 11, 14, 0x265e5a51 };
		this->parameters[i++] = {  0, 20, 0xe9b6c7aa };
		this->parameters[i++] = {  5,  5, 0xd62f105d };
		this->parameters[i++] = { 10,  9, 0x02441453 };
		this->parameters[i++] = { 15, 14, 0xd8a1e681 };
		this->parameters[i++] = {  4, 20, 0xe7d3fbc8 };
		this->parameters[i++] = {  9,  5, 0x21e1cde6 };
		this->parameters[i++] = { 14,  9, 0xc33707d6 };
		this->parameters[i++] = {  3, 14, 0xf4d50d87 };
		this->parameters[i++] = {  8, 20, 0x455a14ed };
		this->parameters[i++] = { 13,  5, 0xa9e3e905 };
		this->parameters[i++] = {  2,  9, 0xfcefa3f8 };
		this->parameters[i++] = {  7, 14, 0x676f02d9 };
		this->parameters[i++] = { 12, 20, 0x8d2a4c8a };

		this->parameters[i++] = {  5,  4, 0xfffa3942 };
		this->parameters[i++] = {  8, 11, 0x8771f681 };
		this->parameters[i++] = { 11, 16, 0x6d9d6122 };
		this->parameters[i++] = { 14, 23, 0xfde5380c };
		this->parameters[i++] = {  1,  4, 0xa4beea44 };
		this->parameters[i++] = {  4, 11, 0x4bdecfa9 };
		this->parameters[i++] = {  7, 16, 0xf6bb4b60 };
		this->parameters[i++] = { 10, 23, 0xbebfbc70 };
		this->parameters[i++] = { 13,  4, 0x289b7ec6 };
		this->parameters[i++] = {  0, 11, 0xeaa127fa };
		this->parameters[i++] = {  3, 16, 0xd4ef3085 };
		this->parameters[i++] = {  6, 23, 0x04881d05 };
		this->parameters[i++] = {  9,  4, 0xd9d4d039 };
		this->parameters[i++] = { 12, 11, 0xe6db99e5 };
		this->parameters[i++] = { 15, 16, 0x1fa27cf8 };
		this->parameters[i++] = {  2, 23, 0xc4ac5665 };

		this->parameters[i++] = {  0,  6, 0xf4292244 };
		this->parameters[i++] = {  7, 10, 0x432aff97 };
		this->parameters[i++] = { 14, 15, 0xab9423a7 };
		this->parameters[i++] = {  5, 21, 0xfc93a039 };
		this->parameters[i++] = { 12,  6, 0x655b59c3 };
		this->parameters[i++] = {  3, 10, 0x8f0ccc92 };
		this->parameters[i++] = { 10, 15, 0xffeff47d };
		this->parameters[i++] = {  1, 21, 0x85845dd1 };
		this->parameters[i++] = {  8,  6, 0x6fa87e4f };
		this->parameters[i++] = { 15, 10, 0xfe2ce6e0 };
		this->parameters[i++] = {  6, 15, 0xa3014314 };
		this->parameters[i++] = { 13, 21, 0x4e0811a1 };
		this->parameters[i++] = {  4,  6, 0xf7537e82 };
		this->parameters[i++] = { 11, 10, 0xbd3af235 };
		this->parameters[i++] = {  2, 15, 0x2ad7d2bb };
		this->parameters[i++] = {  9, 21, 0xeb86d391 };

		this->metaparameters[0] = {FF, this->parameters + 16 * 0, 16};
		this->metaparameters[1] = {GG, this->parameters + 16 * 1, 16};
		this->metaparameters[2] = {HH, this->parameters + 16 * 2, 16};
		this->metaparameters[3] = {II, this->parameters + 16 * 3, 16};
	}
	
	static std::uint32_t rotate_left(std::uint32_t a, std::uint32_t b){
		return (a << b) | (a >> (32 - b));
	}

	static std::uint32_t F(std::uint32_t x, std::uint32_t y, std::uint32_t z){
		return (x & y) | (~x & z);
	}

	static std::uint32_t G(std::uint32_t x, std::uint32_t y, std::uint32_t z){
		return (x & z) | (y & ~z);
	}

	static std::uint32_t H(std::uint32_t x, std::uint32_t y, std::uint32_t z){
		return x ^ y ^ z;
	}

	static std::uint32_t I(std::uint32_t x, std::uint32_t y, std::uint32_t z){
		return (y ^ (x | ~z));
	}

	static void FF(std::uint32_t &a, std::uint32_t b, std::uint32_t c, std::uint32_t d, std::uint32_t m, std::uint32_t s, std::uint32_t t){
		a += F(b, c, d) + m + t;
		a = b + rotate_left(a,s);
	}

	static void GG(std::uint32_t &a, std::uint32_t b, std::uint32_t c, std::uint32_t d, std::uint32_t m, std::uint32_t s, std::uint32_t t){
		a += G(b, c, d) + m + t;
		a = b + rotate_left(a,s);
	}

	static void HH(std::uint32_t &a, std::uint32_t b, std::uint32_t c, std::uint32_t d, std::uint32_t m, std::uint32_t s, std::uint32_t t){
		a += H(b, c, d) + m + t;
		a = b + rotate_left(a,s);
	}

	static void II(std::uint32_t &a, std::uint32_t b, std::uint32_t c, std::uint32_t d, std::uint32_t m, std::uint32_t s, std::uint32_t t){
		a += I(b, c, d) + m + t;
		a = b + rotate_left(a,s);
	}

	void transform(){
		std::uint32_t m[16];

		// MD5 specifies big endian byte order, but this implementation assumes a little
		// endian byte order CPU. Reverse all the bytes upon input, and re-reverse them
		// on output (in md5_final()).
		for (int i = 0, j = 0; i < 16; ++i, j += 4)
			m[i] = (this->data[j]) + (this->data[j + 1] << 8) + (this->data[j + 2] << 16) + (this->data[j + 3] << 24);

		std::uint32_t temp[4];
		for (int i = 4; i--;)
			temp[i] = this->state[i];

		for (auto &mp : this->metaparameters){
			int i = 0;
			for (size_t j = 0; j < mp.length; j++){
				auto &p = mp.parameters[j];
				mp.f(
					temp[i],
					temp[(i + 1) % 4],
					temp[(i + 2) % 4],
					temp[(i + 3) % 4],
					m[p.m_index],
					p.s,
					p.t
				);
				i = (i + 3) % 4;
			}
		}

		for (int i = 4; i--;)
			this->state[i] += temp[i];
	}
public:
	Md5(){
		this->init();
		this->reset();
	}
	Md5(const Md5 &) = default;
	Md5 &operator=(const Md5 &) = default;
	Md5(Md5 &&) = default;
	Md5 &operator=(Md5 &&) = default;
	void reset(){
		this->datalen = 0;
		this->bitlen = 0;
		this->state[0] = 0x67452301;
		this->state[1] = 0xEFCDAB89;
		this->state[2] = 0x98BADCFE;
		this->state[3] = 0x10325476;
	}
	void update(const void *buffer, size_t length){
		auto data = (const std::uint8_t *)buffer;
		for (size_t i = 0; i < length; i++) {
			this->data[this->datalen++] = data[i];
			if (this->datalen == 64) {
				this->transform();
				this->bitlen += 512;
				this->datalen = 0;
			}
		}
	}
	Md5Digest get_digest(){
		size_t i = this->datalen;

		// Pad whatever data is left in the buffer.
		if (this->datalen < 56) {
			this->data[i++] = 0x80;
			while (i < 56)
				this->data[i++] = 0x00;
		}else if (this->datalen >= 56){
			this->data[i++] = 0x80;
			while (i < 64)
				this->data[i++] = 0x00;
			this->transform();
			memset(this->data, 0, 56);
		}

		// Append to the padding the total message's length in bits and transform.
		this->bitlen += this->datalen * 8;
		for (int j = 0; j < 8; j++)
			this->data[56 + j] = (std::uint8_t)(this->bitlen >> (8 * j));
		this->transform();
		
		Md5Digest ret;

		// Since this implementation uses little endian byte ordering and MD uses big endian,
		// reverse all the bytes when copying the final state to the output hash.
		for (i = 0; i < 4; ++i) {
			ret.data[i]      = (this->state[0] >> (i * 8)) & 0xFF;
			ret.data[i + 4]  = (this->state[1] >> (i * 8)) & 0xFF;
			ret.data[i + 8]  = (this->state[2] >> (i * 8)) & 0xFF;
			ret.data[i + 12] = (this->state[3] >> (i * 8)) & 0xFF;
		}
		
		return ret;
	}
};
