#pragma once

#include <array>
#include <type_traits>
#include <sstream>
#include <iomanip>
#include <memory>
#include <Windows.h>
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#define CONCAT(x, y) x##y
#define CONCAT2(x, y) CONCAT(x, y)
#define LOCK_MUTEX(x) std::lock_guard<decltype(x)> CONCAT2(lg_, __COUNTER__)(x)

template <typename T>
void zero_structure(T &dst){
	memset((void *)&dst, 0, sizeof(dst));
}

namespace detail{
template <typename T>
constexpr size_t basic_to_hex_array_length(){
	return sizeof(T) * 2 + 3;
}
}

template <typename T1, typename T2>
typename std::enable_if<std::is_integral<T2>::value, std::array<T1, detail::basic_to_hex_array_length<T2>()>>::type
basic_to_hex(T2 n){
	static const char hex_digits[] = "0123456789abcdef";
	std::array<T1, detail::basic_to_hex_array_length<T2>()> ret;
	auto s = ret.data();
	*(s++) = (T1)'0';
	*(s++) = (T1)'x';
	for (int i = sizeof(T2) * 2; i--;)
		*(s++) = (T1)hex_digits[(n >> (4 * i)) & 0x0F];
	*(s++) = 0;
	return ret;
}

template <typename T>
typename std::enable_if<std::is_integral<T>::value, std::array<char, detail::basic_to_hex_array_length<T>()>>::type
to_hex(T n){
	return basic_to_hex<char>(n);
}

template <typename T>
typename std::enable_if<std::is_integral<T>::value, std::array<wchar_t, detail::basic_to_hex_array_length<T>()>>::type
to_whex(T n){
	return basic_to_hex<wchar_t>(n);
}

inline auto to_hex(void *n){
	return basic_to_hex<char>((uintptr_t)n);
}

inline auto to_whex(void *n){
	return basic_to_hex<wchar_t>((uintptr_t)n);
}

template <size_t N>
std::ostream &operator<<(std::ostream &stream, const std::array<char, N> &s){
	return stream << s.data();
}

template <size_t N>
std::wostream &operator<<(std::wostream &stream, const std::array<wchar_t, N> &s){
	return stream << s.data();
}

class HandleReleaser{
public:
	void operator()(HANDLE h){
		if (h)
			CloseHandle(h);
	}
};

typedef std::unique_ptr<std::remove_pointer<HANDLE>::type, HandleReleaser> autohandle_t;

//Beware: allocate through std::malloc family only
template<typename T>
using unique_ptr_buffer = std::unique_ptr<T, decltype(&std::free)>;

template <typename T>
unique_ptr_buffer<T> allocate_variable_structure(size_t bytes){
	return unique_ptr_buffer<T>((T *)std::malloc(bytes), &std::free);
}

template <typename T, size_t N>
constexpr size_t array_length(const T (&)[N]){
	return N;
}

template <typename It, typename F>
It find_first_true(It begin, It end, const F &f){
	if (begin >= end)
		return end;
	if (f(*begin))
		return begin;
	auto diff = end - begin;
	while (diff > 1){
		auto pivot = begin + diff / 2;
		if (!f(*pivot))
			begin = pivot;
		else
			end = pivot;
		diff = end - begin;
	}
	return end;
}

inline void report_failed_win32_function(const char *function, DWORD error, const char *extra_context = nullptr){
	std::stringstream stream;
	if (extra_context)
		stream << "Error while " << extra_context << ": ";
	else
		stream << "Error: ";
	stream << function << "() failed with error ";
	switch (error){
		case ERROR_ACCESS_DENIED:
			stream << "access denied";
			break;
		case ERROR_FILE_NOT_FOUND:
			stream << "file not found";
			break;
		default:
			stream << error << " (" << std::hex << std::setw(8) << std::setfill('0') << error << ")";
			break;
	}
	throw std::runtime_error(stream.str());
}

inline bool invalid_handle(HANDLE h){
	return !h || h == INVALID_HANDLE_VALUE;
}

inline autohandle_t create_event(){
	autohandle_t ret(CreateEvent(nullptr, true, false, nullptr));
	if (!ret)
		report_failed_win32_function("CreateEvent", GetLastError());
	return ret;
}
