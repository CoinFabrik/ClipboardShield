#pragma once

#include <filesystem>
#include <vector>

typedef std::filesystem::path Path;
typedef std::filesystem::directory_iterator Di;

std::vector<Path> list_test_cases(const Path &dir);
