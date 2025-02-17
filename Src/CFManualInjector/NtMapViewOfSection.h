#pragma once

#include <Windows.h>

class NtMapViewOfSection_holder{
	typedef NTSTATUS (__stdcall *T)(HANDLE SectionHandle, HANDLE ProcessHandle, PVOID *BaseAddress, ULONG_PTR ZeroBits, SIZE_T CommitSize, PLARGE_INTEGER SectionOffset, PSIZE_T ViewSize, ULONG InheritDisposition, ULONG AllocationType, ULONG Win32Protect);
	T f;
public:
	NtMapViewOfSection_holder();
	NtMapViewOfSection_holder(const NtMapViewOfSection_holder &) = delete;
	NtMapViewOfSection_holder &operator=(const NtMapViewOfSection_holder &) = delete;
	NtMapViewOfSection_holder(NtMapViewOfSection_holder &&) = delete;
	NtMapViewOfSection_holder &operator=(NtMapViewOfSection_holder &&) = delete;
	NTSTATUS operator()(HANDLE SectionHandle, HANDLE ProcessHandle, PVOID *BaseAddress, ULONG_PTR ZeroBits, SIZE_T CommitSize, PLARGE_INTEGER SectionOffset, PSIZE_T ViewSize, ULONG InheritDisposition, ULONG AllocationType, ULONG Win32Protect);
};

extern NtMapViewOfSection_holder NktNtMapViewOfSection;
