#include <Windows.h>

void install_service(const wchar_t *pszServiceName, 
                    const wchar_t *pszDisplayName, 
                    DWORD dwStartType);
void uninstall_service(const wchar_t *pszServiceName);
