#pragma once

HANDLE WINAPI hooked_GetClipboardData(__in UINT uFormat);
HANDLE WINAPI hooked_SetClipboardData(__in UINT uFormat, __in_opt HANDLE hMem);
//void * __stdcall hooked_NtUserSetClipboardData(void *a, void *b, void *c);
HRESULT STDAPICALLTYPE hooked_OleSetClipboard(IN LPDATAOBJECT pDataObj);
HRESULT STDAPICALLTYPE hooked_OleGetClipboard(_Outptr_ LPDATAOBJECT FAR *ppDataObj);
