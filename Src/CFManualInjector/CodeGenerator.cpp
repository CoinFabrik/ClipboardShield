#include "CodeGenerator.h"
#include "asmjit/asmjit.h"
#include "utility.h"
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <string>

using namespace asmjit;

class PrintErrorHandler : public asmjit::ErrorHandler{
public:
	bool handleError(asmjit::Error err, const char *message, asmjit::CodeEmitter *origin) override{
		throw std::runtime_error(message);
	}
};

class CodeGenerator::Impl{
	CodeHolder holder;
	PrintErrorHandler peh;
	StringLogger logger;
	size_t size;
public:
	Impl(){
		CodeInfo info(ArchInfo::kTypeX64);
		this->holder.init(info);
		this->holder.setErrorHandler(&this->peh);
		this->holder.setLogger(&this->logger);
	}
	size_t generate(uintptr_t image_base, uintptr_t image_size, uintptr_t jump_destination){
		X86Assembler a(&holder);

		auto default_call_label = a.newNamedLabel("default_call");auto ax = a.zax();
		auto cx = a.zcx();
		auto dx = a.zdx();

		//a.int3();
		a.mov(ax, image_base + image_size);
		a.cmp(cx, ax);
		a.jae(default_call_label);
		a.mov(ax, image_base);
		a.cmp(cx, ax);
		a.jb(default_call_label);
		a.mov(X86Mem(dx, 0), ax);
		a.ret();
		a.bind(default_call_label);
		a.jmp(Imm(jump_destination));

		return this->size = this->holder.getCodeSize();
	}
	std::vector<std::uint8_t> finalize(uintptr_t destination){
		std::vector<std::uint8_t> ret(this->size);
		this->holder.relocate(&ret[0], destination);

		std::string s = this->logger.getString();
		TRACE(s.c_str());
		std::cout << s << std::endl;
		std::stringstream temp_stream;
		print_buffer(temp_stream, ret);
		TRACE(temp_stream.str().c_str());
		std::cout << temp_stream.str();

		return ret;
	}
};

CodeGenerator::CodeGenerator(){
	this->pimpl.reset(new Impl);
}

CodeGenerator::~CodeGenerator(){}

size_t CodeGenerator::generate(uintptr_t image_base, uintptr_t image_size, uintptr_t jump_destination){
	return this->pimpl->generate(image_base, image_size, jump_destination);
}

std::vector<std::uint8_t> CodeGenerator::finalize(uintptr_t destination){
	return this->pimpl->finalize(destination);
}
