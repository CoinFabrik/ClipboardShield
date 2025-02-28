#include "utility.hpp"

std::vector<Path> list_test_cases(const Path &dir){
	std::vector<Path> ret;
	for (Di i(dir), e; i != e; ++i){
		if (!i->is_regular_file())
			continue;
		ret.push_back(i->path());
	}
	return ret;
}
