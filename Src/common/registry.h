#pragma once

#include <string>
#include <Windows.h>

LSTATUS get_registry_value(HKEY key, const wchar_t *name, std::wstring &dst);

template<typename T>
LSTATUS get_registry_value(const HKEY key, const wchar_t *name, T &out_value) {
	DWORD value_size = sizeof(T);
	return ::RegQueryValueExW(key, name, nullptr, nullptr, (LPBYTE)&out_value, &value_size);
}
