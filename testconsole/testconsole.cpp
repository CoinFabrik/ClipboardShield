#include <Windows.h>
#include <Winternl.h>
#include <cstdio>
#include <array>
#include <iostream>

class GetSystemTimePreciseAsFileTimeState{
	typedef void (WINAPI *GetSystemTimePreciseAsFileTime_fp)(FILETIME *);
	GetSystemTimePreciseAsFileTime_fp f = nullptr;
public:
	GetSystemTimePreciseAsFileTimeState(){
		HMODULE kernelbase = GetModuleHandleW(L"KernelBase.dll");
		if (kernelbase)
			this->f = (GetSystemTimePreciseAsFileTime_fp)GetProcAddress(kernelbase, "GetSystemTimePreciseAsFileTime");
	}
	operator bool() const{
		return !!this->f;
	}
	void operator()(FILETIME &ft) const{
		if (*this)
			this->f(&ft);
	}
};

class NtQuerySystemTimeState{
	typedef __kernel_entry NTSTATUS (__stdcall *NtQuerySystemTime_fp)(LARGE_INTEGER *);
	NtQuerySystemTime_fp f = nullptr;
public:
	NtQuerySystemTimeState(){
		HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
		if (ntdll)
			this->f = (NtQuerySystemTime_fp)GetProcAddress(ntdll, "NtQuerySystemTime");
	}
	operator bool() const{
		return !!this->f;
	}
	NTSTATUS operator()(LARGE_INTEGER &li) const{
		if (*this)
			return this->f(&li);
		return STATUS_INVALID_PARAMETER;
	}
};

std::array<char, 32> time_to_string(const SYSTEMTIME &st){
	std::array<char, 32> ret;
	sprintf(ret.data(), "%04d-%02d-%02d %02d:%02d:%02d.%03d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
	return ret;
}

template <size_t N>
std::ostream &operator<<(std::ostream &stream, const std::array<char, N> &array){
	for (auto c : array){
		if (!c)
			break;
		stream << c;
	}
	return stream;
}

int main(int argc, const char* argv[])
{
#ifndef _M_X64
	printf ("Running 32 bit version of TestConsole, printing System Time\n\n");
#else
	printf ("Running 64 bit version of TestConsole, printing System Time\n\n");
#endif

	int count = 1;
	int sleep_interval = 10;

	if (argc >= 2){
		count = atoi(argv[1]);
		if (argc >= 3)
			sleep_interval = atoi(argv[2]);
	}

	GetSystemTimePreciseAsFileTimeState get_system_time_precise_as_filetime;
	NtQuerySystemTimeState nt_query_system_time;

	while (count--){
		SYSTEMTIME st;
		FILETIME tm;
		if (get_system_time_precise_as_filetime){
			get_system_time_precise_as_filetime(tm);

			FileTimeToSystemTime(&tm, &st);
			std::cout << " " << time_to_string(st) << "  (UTC time by GetSystemTimePreciseAsFileTime)\n";
		}else
			std::cout << "GetSystemTimePreciseAsFileTime() unavailable\n";

		GetSystemTime(&st);
		std::cout << " " << time_to_string(st) << "  (UTC time by GetSystemTime)\n";


		GetSystemTimeAsFileTime( &tm );
		FileTimeToSystemTime(&tm, &st);
		std::cout << " " << time_to_string(st) << "  (UTC time by GetSystemTimeAsFileTime)\n";


		if (nt_query_system_time){
			LARGE_INTEGER li;
			if (nt_query_system_time(li))
				std::cout << "NtQuerySystemTime failed\n";
			else{
				tm.dwHighDateTime = li.HighPart;
				tm.dwLowDateTime = li.LowPart;

				FileTimeToSystemTime(&tm, &st);

				std::cout << " " << time_to_string(st) << "  (UTC time by NtQuerySystemTime)\n";
			}
		}else
			std::cout << "NtQuerySystemTime() unavailable\n";

		GetLocalTime(&st);
		std::cout << " " << time_to_string(st) << "  (local time by GetLocalTime)\n";


		Sleep(sleep_interval);
		std::cout << std::endl;
	}

	return 0;
}
