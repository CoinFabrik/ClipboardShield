#pragma once

#include <cstdint>
#include <string>

struct Sha256Digest{
	static const size_t length = 32;
	std::uint8_t data[length];
};

class Sha256{
	std::uint8_t data[64];
	std::uint32_t datalen;
	std::uint64_t bitlen;
	std::uint32_t state[8];

	void transform();
public:
	Sha256(){
		this->reset();
	}
	Sha256(const Sha256 &) = default;
	Sha256 &operator=(const Sha256 &) = default;
	Sha256(Sha256 &&) = default;
	Sha256 &operator=(Sha256 &&) = default;
	void reset();
	void update(const void *buffer, size_t length);
	Sha256Digest get_digest();
	static Sha256Digest compute(const void *buffer, size_t length) noexcept{
		Sha256 hash;
		hash.update(buffer, length);
		return hash.get_digest();
	}
};

std::string to_string(const Sha256Digest &);
