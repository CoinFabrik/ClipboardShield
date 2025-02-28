#include "testcase.hpp"
#include "../../libvts/Rules.h"
#include <fstream>
#include <stdexcept>
#include <sstream>

const char * const hline = "--------------------------------------------------------------------------------";

class TestFirewall : public AbstractFirewall{
public:
	TestFirewall(FirewallConfiguration &&config){
		this->config = std::move(config);
	}
	FirewallPolicy on_previously_unknown_state() const override{
		return FirewallPolicy::Allow;
	}
};

TestCase TestCase::parse_test_case_file(const Path &path){
	std::ifstream file(path);
	if (!file)
		throw std::runtime_error("File not found: " + path.u8string());

	TestCase ret;
	{
		std::string line;
		while (file){
			std::getline(file, line);
			if (line == hline)
				break;
			ret.config += line;
			ret.config += '\n';
		}
	}

	while (file){
		std::string line;
		std::getline(file, line);
		if (line.empty())
			continue;
		try{
			ret.runs.emplace_back(line);
		}catch (std::exception &e){
			throw std::runtime_error("Error while parsing test case " + path.filename().u8string() + ": " + e.what());
		}
	}

	return ret;
}

TestCaseRun::TestCaseRun(const std::string &line){
	this->original_string = line;
	std::wstringstream stream(std::wstring(line.begin(), line.end()));
	std::wstring allow_string;
	stream >> allow_string >> this->source >> this->destination;
	if (allow_string == L"allow")
		this->expected_result = FirewallPolicy::Allow;
	else if (allow_string == L"deny")
		this->expected_result = FirewallPolicy::Deny;
	else
		throw std::runtime_error("invalid run: " + line);
}

void TestCase::run() const{
	TestFirewall fw(FirewallConfiguration::parse_config_string(this->config));
	size_t i = 0;
	for (auto &run : this->runs){
		std::wstring explanation;
		auto actual_result = fw.run_permissions_logic(run.source, run.destination, explanation);
		if (actual_result != run.expected_result){
			std::wstringstream stream;
			stream <<
				L"test case '" << std::wstring(run.original_string.begin(), run.original_string.end()) << L"' failed.\n"
				L"Expected: " << to_string(run.expected_result) << L"\n"
				L"Actual:   " << to_string(actual_result) << L"\n"
				L"Explanation: " << explanation << std::endl;
			auto error = stream.str();
			throw std::runtime_error(std::string(error.begin(), error.end()));
		}
	}
}
