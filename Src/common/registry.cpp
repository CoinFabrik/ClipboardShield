#include "registry.h"
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
#include <limits>

LSTATUS get_registry_value(HKEY key, const wchar_t *name, std::wstring &dst){
	LSTATUS error;
	dst.resize(128);
	while (true){
		auto size = sizeof(wchar_t) * dst.size();
		if (size > std::numeric_limits<DWORD>::max())
			return ERROR_OUTOFMEMORY;
		auto value_size = (DWORD)size;
		DWORD type;
		auto result = ::RegQueryValueExW(key, name, nullptr, &type, (LPBYTE)&dst[0], &value_size);
		if (result == ERROR_SUCCESS){
			if (type != REG_SZ){
				error = ERROR_NOT_FOUND;
				break;
			}
			dst.resize(value_size / sizeof(wchar_t));
			if (dst.size() && !dst.back())
				dst.pop_back();
			return ERROR_SUCCESS;
		}
		if (result == ERROR_MORE_DATA){
			dst.resize(dst.size() * 2);
			continue;
		}
		error = result;
		break;
	}
	dst.clear();
	return error;
}
