#include "sha256.hpp"
#include <cstring>

void Sha256::reset(){
	this->datalen = 0;
	this->bitlen = 0;
	this->state[0] = 0x6a09e667;
	this->state[1] = 0xbb67ae85;
	this->state[2] = 0x3c6ef372;
	this->state[3] = 0xa54ff53a;
	this->state[4] = 0x510e527f;
	this->state[5] = 0x9b05688c;
	this->state[6] = 0x1f83d9ab;
	this->state[7] = 0x5be0cd19;
}

void Sha256::update(const void *void_buffer, size_t length){
	auto buffer = (const std::uint8_t *)void_buffer;
	for (size_t i = 0; i < length; ++i) {
		this->data[this->datalen] = buffer[i];
		this->datalen++;
		if (this->datalen == 64) {
			this->transform();
			this->bitlen += 512;
			this->datalen = 0;
		}
	}
}

std::uint32_t rotate_left(std::uint32_t a, std::uint32_t b){
	return (a << b) | (a >> (32 - b));
}

std::uint32_t rotate_right(std::uint32_t a, std::uint32_t b){
	return (a >> b) | (a << (32 - b));
}

std::uint32_t sig0(std::uint32_t x){
	return rotate_right(x, 7) ^ rotate_right(x, 18) ^ (x >> 3);
}

std::uint32_t sig1(std::uint32_t x){
	return rotate_right(x, 17) ^ rotate_right(x, 19) ^ (x >> 10);
}

std::uint32_t ch(std::uint32_t x, std::uint32_t y, std::uint32_t z){
	return (x & y) ^ (~x & z);
}

std::uint32_t maj(std::uint32_t x, std::uint32_t y, std::uint32_t z){
	return (x & y) ^ (x & z) ^ (y & z);
}

std::uint32_t ep0(std::uint32_t x){
	return rotate_right(x, 2) ^ rotate_right(x, 13) ^ rotate_right(x, 22);
}

std::uint32_t ep1(std::uint32_t x){
	return rotate_right(x, 6) ^ rotate_right(x, 11) ^ rotate_right(x, 25);
}

static const std::uint32_t k[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

void Sha256::transform(){
	std::uint32_t t1, t2, m[64];

	{
		int i, j;
		for (i = 0, j = 0; i < 16; ++i, j += 4)
			m[i] = (data[j] << 24) | (data[j + 1] << 16) | (data[j + 2] << 8) | (data[j + 3]);
		for (; i < 64; ++i)
			m[i] = sig1(m[i - 2]) + m[i - 7] + sig0(m[i - 15]) + m[i - 16];
	}

	auto a = this->state[0];
	auto b = this->state[1];
	auto c = this->state[2];
	auto d = this->state[3];
	auto e = this->state[4];
	auto f = this->state[5];
	auto g = this->state[6];
	auto h = this->state[7];

	for (int i = 0; i < 64; ++i) {
		t1 = h + ep1(e) + ch(e, f, g) + k[i] + m[i];
		t2 = ep0(a) + maj(a, b, c);
		h = g;
		g = f;
		f = e;
		e = d + t1;
		d = c;
		c = b;
		b = a;
		a = t1 + t2;
	}

	this->state[0] += a;
	this->state[1] += b;
	this->state[2] += c;
	this->state[3] += d;
	this->state[4] += e;
	this->state[5] += f;
	this->state[6] += g;
	this->state[7] += h;
}

Sha256Digest Sha256::get_digest(){
	auto i = this->datalen;

	// Pad whatever data is left in the buffer.
	if (this->datalen < 56){
		this->data[i++] = 0x80;
		while (i < 56)
			this->data[i++] = 0x00;
	}else{
		this->data[i++] = 0x80;
		while (i < 64)
			this->data[i++] = 0x00;
		this->transform();
		memset(this->data, 0, 56);
	}

	// Append to the padding the total message's length in bits and transform.
	this->bitlen += this->datalen * 8;
	this->data[63] = (std::uint8_t)this->bitlen;
	this->data[62] = (std::uint8_t)(this->bitlen >> 8);
	this->data[61] = (std::uint8_t)(this->bitlen >> 16);
	this->data[60] = (std::uint8_t)(this->bitlen >> 24);
	this->data[59] = (std::uint8_t)(this->bitlen >> 32);
	this->data[58] = (std::uint8_t)(this->bitlen >> 40);
	this->data[57] = (std::uint8_t)(this->bitlen >> 48);
	this->data[56] = (std::uint8_t)(this->bitlen >> 56);
	this->transform();

	Sha256Digest ret;
	auto hash = ret.data;

	// Since this implementation uses little endian byte ordering and SHA uses big endian,
	// reverse all the bytes when copying the final state to the output hash.
	for (i = 0; i < 4; ++i) {
		hash[i +  0] = (this->state[0] >> (24 - i * 8)) & 0xFF;
		hash[i +  4] = (this->state[1] >> (24 - i * 8)) & 0xFF;
		hash[i +  8] = (this->state[2] >> (24 - i * 8)) & 0xFF;
		hash[i + 12] = (this->state[3] >> (24 - i * 8)) & 0xFF;
		hash[i + 16] = (this->state[4] >> (24 - i * 8)) & 0xFF;
		hash[i + 20] = (this->state[5] >> (24 - i * 8)) & 0xFF;
		hash[i + 24] = (this->state[6] >> (24 - i * 8)) & 0xFF;
		hash[i + 28] = (this->state[7] >> (24 - i * 8)) & 0xFF;
	}

	return ret;
}

std::string to_string(const Sha256Digest &digest){
	static const char * const alphabet = "0123456789abcdef";
	std::string ret;
	for (auto b : digest.data){
		ret += alphabet[b >> 4];
		ret += alphabet[b & 0x0F];
	}
	return ret;
}
