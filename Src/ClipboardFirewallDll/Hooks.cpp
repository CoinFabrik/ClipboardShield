#include "stdafx.h"
#include "utility.h"
#include "Hooks.h"
#include "DllState.h"

HANDLE WINAPI hooked_GetClipboardData(__in UINT uFormat){
	auto gs = global_state;
	if (gs->confirm_test_GetClipboardData())
		return nullptr;
	if (gs->lock_reentrancy() && !gs->clipboard_read_pre())
		return nullptr;
	auto original_function = gs->get_GetClipboardData();
	return original_function(uFormat);
}

HANDLE WINAPI hooked_SetClipboardData(__in UINT uFormat, __in_opt HANDLE hMem){
	auto gs = global_state;
	if (gs->confirm_test_SetClipboardData())
		return nullptr;
	auto original_function = gs->get_SetClipboardData();
	
	auto reentrancy = gs->lock_reentrancy();
	if (reentrancy)
		gs->clipboard_write_pre();
	auto ret = original_function(uFormat, hMem);
	if (reentrancy)
		gs->clipboard_write_post(!!ret);
	return ret;
}

/*
void * __stdcall hooked_NtUserSetClipboardData(void *a, void *b, void *c){
	auto gs = global_state;
	if (gs->confirm_test_NtUserSetClipboardData())
		return nullptr;
	auto original_function = gs->get_NtUserSetClipboardData();
	
	auto reentrancy = gs->lock_reentrancy();
	if (reentrancy)
		gs->clipboard_write_pre();
	auto ret = original_function(a, b, c);
	if (reentrancy)
		gs->clipboard_write_post(!!ret);
	
	return ret;
}
*/

HRESULT STDAPICALLTYPE hooked_OleSetClipboard(IN LPDATAOBJECT pDataObj){
	auto gs = global_state;
	if (gs->confirm_test_OleSetClipboard())
		return 0;
	auto original_function = gs->get_OleSetClipboard();

	auto reentrancy = gs->lock_reentrancy();
	if (reentrancy)
		gs->clipboard_write_pre();
	auto ret = original_function(pDataObj);
	if (reentrancy)
		gs->clipboard_write_post(ret == S_OK);

	return ret;
}

HRESULT STDAPICALLTYPE hooked_OleGetClipboard(_Outptr_ LPDATAOBJECT FAR *ppDataObj){
	auto gs = global_state;
	if (gs->confirm_test_OleGetClipboard())
		return 0;
	if (gs->lock_reentrancy() && !gs->clipboard_read_pre())
		return CLIPBRD_E_CANT_OPEN;
	auto original_function = gs->get_OleGetClipboard();
	return original_function(ppDataObj);
}
