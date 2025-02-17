#include "NtMapViewOfSection.h"
#include <stdexcept>

NtMapViewOfSection_holder NktNtMapViewOfSection;

NtMapViewOfSection_holder::NtMapViewOfSection_holder(){
	auto ntdll = LoadLibraryW(L"ntdll.dll");
	if (!ntdll)
		throw std::runtime_error("Failed to load ntdll.dll");
	this->f = (T)GetProcAddress(ntdll, "NtMapViewOfSection");
	FreeLibrary(ntdll);
	if (!this->f)
		throw std::runtime_error("Failed to get address of NtMapViewOfSection");
}

NTSTATUS NtMapViewOfSection_holder::operator()(HANDLE SectionHandle, HANDLE ProcessHandle, PVOID *BaseAddress, ULONG_PTR ZeroBits, SIZE_T CommitSize, PLARGE_INTEGER SectionOffset, PSIZE_T ViewSize, ULONG InheritDisposition, ULONG AllocationType, ULONG Win32Protect){
	return this->f(SectionHandle, ProcessHandle, BaseAddress, ZeroBits, CommitSize, SectionOffset, ViewSize, InheritDisposition, AllocationType, Win32Protect);
}
