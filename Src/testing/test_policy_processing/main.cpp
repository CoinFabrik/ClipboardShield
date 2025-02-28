#include "testcase.hpp"
#include "utility.hpp"
#include <iostream>

int paths_are_equivalent(const wchar_t *a, const wchar_t *b, bool no_throw){
	return wcscmp(a, b) == 0;
}

int main(int argc, char **argv){
	if (argc < 2){
		std::cerr << "Pass as an argument the path to the test cases directory.\n";
		return 0;
	}

	try{
		auto test_cases = list_test_cases(argv[1]);
		std::cout << "Found " << test_cases.size() << " test cases.\n";

		for (auto &path : test_cases){
			try{
				TestCase::parse_test_case_file(path).run();
			}catch (std::exception &e){
				throw std::runtime_error("Error while processing " + path.filename().u8string() + ": " + e.what());
			}
		}

		std::cout << "No errors.\n";
	}catch (std::exception &e){
		std::cerr << e.what() << std::endl;
		return -1;
	}catch (...){
		std::cerr << "Unknown exception caught.\n";
		return -1;
	}

	return 0;
}
