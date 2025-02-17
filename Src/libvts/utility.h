#pragma once

#include <memory>
#include <type_traits>
#include <thread>
#include <array>
#include <set>
#include <cstring>
#include <map>
#include <Windows.h>
#include "../common/utility.h"

#ifdef _M_X64
#define WBITNESS L"64"
#else
#define WBITNESS L"32"
#endif

class HkeyReleaser{
public:
	void operator()(HKEY h);
};

typedef std::unique_ptr<std::remove_pointer<HANDLE>::type, HandleReleaser> autohandle_t;
typedef std::unique_ptr<std::remove_pointer<HKEY>::type, HkeyReleaser> autohkey_t;

void release_handle(HANDLE h);
autohandle_t to_autohandle(HANDLE h);
autohkey_t to_autohandle(HKEY h);
autohandle_t create_event();
void safe_join(std::thread &thread);
std::wstring s2ws(const std::string &);
std::string ws2s(const std::wstring &);
std::uint64_t get_performance_frequency();
std::uint64_t get_performance_counter();
std::uint32_t get_process_session_id(std::uint32_t pid);
GUID get_file_guid(const autohandle_t &file);
HANDLE open_file_for_normal_read(const wchar_t *path);
autohandle_t open_file_for_normal_read2(const std::wstring &path, bool no_throw = false);
int paths_are_equivalent(const wchar_t *, const wchar_t *, bool no_throw = false);

inline int paths_are_equivalent(const std::wstring &a, const std::wstring &b, bool no_throw = false){
	return paths_are_equivalent(a.c_str(), b.c_str(), no_throw);
}

struct GUID_compare {
	bool operator()(const GUID &a, const GUID &b) const {
		return memcmp(&a, &b, sizeof(GUID)) < 0;
	}
};

inline void change_backslashes(std::wstring &string){
	for (wchar_t &c : string)
		if (c == L'\\')
			c = L'/';
}

template <typename T, typename F>
bool contained_in_set(const std::set<T, F> &set, const T &x){
	return set.find(x) != set.end();
}

template <typename T, size_t MAX = std::numeric_limits<size_t>::max()>
bool case_insensitive_less_than_f(const T *a, const T *b, size_t na = MAX, size_t nb = MAX) {
	if (na == MAX){
		na = 0;
		while (a[na])
			na++;
	}
	if (nb == MAX) {
		nb = 0;
		while (b[nb])
			nb++;
	}
	auto n = std::min(na, nb);
	for (decltype(n) i = 0; i < n; i++) {
		auto x = tolower(a[i]);
		auto y = tolower(b[i]);
		if (x < y)
			return true;
		if (x > y)
			return false;
	}
	return na < nb;
}

template <typename T>
int case_insensitive_compare(const T *a, const T *b) {
	size_t na = 0;
	while (a[na])
		na++;
	size_t nb = 0;
	while (b[nb])
		nb++;
	auto n = std::min(na, nb);
	for (decltype(n) i = 0; i < n; i++) {
		auto x = tolower(a[i]);
		auto y = tolower(b[i]);
		if (x < y)
			return -1;
		if (x > y)
			return 1;
	}
	return na < nb ? -1 : (na > nb ? 1 : 0);
}

template <typename T>
bool case_insensitive_less_than_f(const std::basic_string<T> &a, const std::basic_string<T> &b){
	return case_insensitive_less_than_f(a.c_str(), b.c_str(), a.size(), b.size());
}

struct case_insensitive_less_than{
	bool operator()(const std::wstring &a, const std::wstring &b) const{
		return case_insensitive_less_than_f<wchar_t>(a, b);
	}
};

template<class V>
using ciws_map = std::map<std::wstring, V, case_insensitive_less_than>;
typedef std::set<std::wstring, case_insensitive_less_than> ciws_set;
