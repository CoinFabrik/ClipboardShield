#pragma once
#include <memory>
#include <vector>
#include <cstdint>

class CodeGenerator{
	class Impl;
	std::unique_ptr<Impl> pimpl;
public:
	CodeGenerator();
	~CodeGenerator();
	size_t generate(uintptr_t image_base, uintptr_t image_size, uintptr_t jump_destination);
	std::vector<std::uint8_t> finalize(uintptr_t destination);
};
