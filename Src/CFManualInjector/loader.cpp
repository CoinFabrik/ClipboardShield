#include "loader.h"
#include "declarations.h"
#include <Windows.h>

__declspec(dllexport) std::uint32_t __stdcall load_dll(void *p){
	auto manual_inject = (MANUAL_INJECT *)p;
	auto pIBR = manual_inject->base_relocation;
	auto delta = (uintptr_t)manual_inject->image_base - (uintptr_t)manual_inject->nt_headers->OptionalHeader.ImageBase;
 
	//Relocate the image.
 
	while (pIBR->VirtualAddress){
		if (pIBR->SizeOfBlock >= sizeof(IMAGE_BASE_RELOCATION)){
			auto count = (pIBR->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
			auto list = (WORD *)(pIBR + 1);
 
			for (decltype(count) i = 0; i < count; i++){
				if (list[i]){
					auto ptr = (uintptr_t *)((char *)manual_inject->image_base + ((uintptr_t)pIBR->VirtualAddress + (list[i] & 0xFFF)));
					*ptr += delta;
				}
			}
		}
 
		pIBR = (IMAGE_BASE_RELOCATION *)((char *)pIBR + pIBR->SizeOfBlock);
	}
 
	auto pIID = manual_inject->import_directory;
 
	//Resolve DLL imports.

	while (pIID->Characteristics){
		auto original_first_thunk = (IMAGE_THUNK_DATA *)((char *)manual_inject->image_base + pIID->OriginalFirstThunk);
		auto first_thunk = (IMAGE_THUNK_DATA *)((char *)manual_inject->image_base + pIID->FirstThunk);
 
		auto library_path = (const char *)manual_inject->image_base + pIID->Name;
		auto module = manual_inject->LoadLibraryA_f(library_path);
		
		if (!module)
			return 0;
 
		while (original_first_thunk->u1.AddressOfData){
			if (original_first_thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG){
				//Import by ordinal.
				auto ordinal = original_first_thunk->u1.Ordinal;
				auto function = manual_inject->GetProcAddress_f(module, (const char *)(ordinal & 0xFFFF));
				if (!function)
					return 0;
#ifdef _M_X64
				if ((void *)function == (void *)manual_inject->RtlPcToFileHeader_unhooked_f)
					first_thunk->u1.Function = (decltype(first_thunk->u1.Function))manual_inject->RtlPcToFileHeader_f;
				else
#endif
					first_thunk->u1.Function = (decltype(first_thunk->u1.Function))function;
			}else{
				//Import by name.
				auto pIBN = (IMAGE_IMPORT_BY_NAME *)((char *)manual_inject->image_base + original_first_thunk->u1.AddressOfData);
				auto function_name = (const char *)pIBN->Name;
				auto function = manual_inject->GetProcAddress_f(module, function_name);
				if (!function)
					return 0;
#ifdef _M_X64
				if ((void *)function == (void *)manual_inject->RtlPcToFileHeader_unhooked_f)
					first_thunk->u1.Function = (decltype(first_thunk->u1.Function))manual_inject->RtlPcToFileHeader_f;
				else
#endif
					first_thunk->u1.Function = (decltype(first_thunk->u1.Function))function;
			}
			original_first_thunk++;
			first_thunk++;
		}
		pIID++;
	}

#ifdef _M_X64
	manual_inject->RtlAddFunctionTable_f((RUNTIME_FUNCTION *)manual_inject->exception_table, manual_inject->exception_table_length, (uintptr_t)manual_inject->image_base);
#endif
 
	if (manual_inject->nt_headers->OptionalHeader.AddressOfEntryPoint){
		auto dll_main = (DllMain_ft)((char *)manual_inject->image_base + manual_inject->nt_headers->OptionalHeader.AddressOfEntryPoint);
		if (!dll_main((HMODULE)manual_inject->image_base, DLL_PROCESS_ATTACH, nullptr))
			return 0;
		manual_inject->InitializeDll_f(&manual_inject->ipc);
		return 1;
	}
 
	return 1;
}
