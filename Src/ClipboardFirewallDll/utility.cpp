#include "stdafx.h"
#include "utility.h"
#include "Hooks.h"
#include <mutex>
#include <sstream>

template <bool F(std::unique_ptr<std::wstring> &dst)>
static const std::wstring *get_path(std::unique_ptr<std::wstring> &path){
	if (path)
		return path.get();
	std::unique_ptr<std::wstring> s;
	if (!F(s))
		return nullptr;
	if (!s)
		return nullptr;
	path = std::move(s);
	return path.get();
}
