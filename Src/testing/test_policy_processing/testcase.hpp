#pragma once

#include "utility.hpp"
#include <string>

typedef std::filesystem::path Path;

enum class FirewallPolicy;

class TestCaseRun{
public:
	std::string original_string;
	FirewallPolicy expected_result;
	std::wstring source;
	std::wstring destination;

	TestCaseRun() = default;
	TestCaseRun(const std::string &);
	TestCaseRun(const TestCaseRun &) = default;
	TestCaseRun &operator=(const TestCaseRun &) = default;
	TestCaseRun(TestCaseRun &&) = default;
	TestCaseRun &operator=(TestCaseRun &&) = default;
};

class TestCase{
	std::string config;
	std::vector<TestCaseRun> runs;

	TestCase() = default;
public:
	TestCase(const TestCase &) = default;
	TestCase &operator=(const TestCase &) = default;
	TestCase(TestCase &&) = default;
	TestCase &operator=(TestCase &&) = default;
	static TestCase parse_test_case_file(const Path &);
	void run() const;
};
