/*
 * Copyright (C) 2015 Nektra S.A., Buenos Aires, Argentina.
 * All rights reserved. Contact: http://www.nektra.com
 *
 *
 * This file is part of Deviare Process Monitor
 *
 *
 * Commercial License Usage
 * ------------------------
 * Licensees holding valid commercial Deviare licenses may use this file
 * in accordance with the commercial license agreement provided with the
 * Software or, alternatively, in accordance with the terms contained in
 * a written agreement between you and Nektra.  For licensing terms and
 * conditions see http://www.nektra.com/licensing/. Use the contact form
 * at http://www.nektra.com/contact/ for further information.
 *
 *
 * GNU General Public License Usage
 * --------------------------------
 * Alternatively, this file may be used under the terms of the GNU General
 * Public License version 3.0 as published by the Free Software Foundation
 * and appearing in the file LICENSE.GPL included in the packaging of this
 * file.  Please visit http://www.gnu.org/copyleft/gpl.html and review the
 * information to ensure the GNU General Public License version 3.0
 * requirements will be met.
 *
 **/
#include "Test.h"
#include "..\Sdk\DeviarePD.h"
#include <conio.h>

//------------------------------------------------------------------------------

class CMyDeviarePD : public CDeviarePD
{
public:
  VOID OnProcessCreated(__in DWORD dwPid);
  VOID OnProcessDestroyed(__in DWORD dwPid);
  VOID OnThreadError(__in DWORD dwOsErr);
};

//------------------------------------------------------------------------------

static int TestReadAHead();
static int TestNotificationEvent();
static int InstallDriver();
static int UninstallDriver();

//------------------------------------------------------------------------------

int wmain(int argc, _TCHAR* argv[])
{
  if (argc < 2)
  {
    wprintf_s(L"Use: Test command\n\n");
    wprintf_s(L"Where 'command' can be:\n\n");
    wprintf_s(L"  test-readahead:  Performs tests using read ahead method.\n");
    wprintf_s(L"  test-event:      Performs tests using notification event method.\n");
    wprintf_s(L"  installdriver:   Installs the driver on the system.\n");
    wprintf_s(L"  uninstalldriver: Uninstalls the driver from the system.\n");
    return 1;
  }
  if (_wcsicmp(argv[1], L"test-readahead") == 0)
    return TestReadAHead();
  if (_wcsicmp(argv[1], L"test-event") == 0)
    return TestNotificationEvent();
  if (_wcsicmp(argv[1], L"installdriver") == 0)
    return InstallDriver();
  if (_wcsicmp(argv[1], L"uninstalldriver") == 0)
    return UninstallDriver();
  wprintf_s(L"Error: Unknown parameter(s) specified.\n");
  return 1;
}

//------------------------------------------------------------------------------

static int TestReadAHead()
{
  CMyDeviarePD cDeviarePD;

  wprintf_s(L"Starting read ahead test... ");
  if (cDeviarePD.Initialize(10) == FALSE)
  {
    wprintf_s(L"Error: %lu\n", ::GetLastError());
    return 1;
  }
  wprintf_s(L"OK\nPress any key to quit.\n");
  do
  {
    ::Sleep(100);
  }
  while (_kbhit() == 0);
  _getwch();
  return 0;
}

static int TestNotificationEvent()
{
  CMyDeviarePD cDeviarePD;

  wprintf_s(L"Starting notification event test... ");
  if (cDeviarePD.Initialize(0) == FALSE)
  {
    wprintf_s(L"Error: %lu\n", ::GetLastError());
    return 1;
  }
  wprintf_s(L"OK\nPress any key to quit.\n");
  do
  {
    ::Sleep(100);
  }
  while (_kbhit() == 0);
  _getwch();
  return 0;
}

static int InstallDriver()
{
  WCHAR szBufW[1024];
  DWORD dwLen;

  wprintf_s(L"Installing driver... ");
  dwLen = ::GetModuleFileNameW(NULL, szBufW, 1024);
  while (dwLen > 0 && szBufW[dwLen-1] != L'\\')
    dwLen--;
  szBufW[dwLen] = 0;
  wcsncat_s(szBufW, _countof(szBufW), L"DeviarePD.sys", _TRUNCATE);
  if (CDeviarePD::InstallDriver(szBufW) == FALSE)
  {
    wprintf_s(L"Error: %lu\n", ::GetLastError());
    return 1;
  }
  wprintf_s(L"OK\n");
  return 0;
}

static int UninstallDriver()
{
  wprintf_s(L"Uninstalling driver... ");
  if (CDeviarePD::UninstallDriver() == FALSE)
  {
    wprintf_s(L"Error: %lu\n", ::GetLastError());
    return 1;
  }
  wprintf_s(L"OK\n");
  return 0;
}

//------------------------------------------------------------------------------

VOID CMyDeviarePD::OnProcessCreated(__in DWORD dwPid)
{
  wprintf_s(L"Process #%lu created\n", dwPid);
  return;
}

VOID CMyDeviarePD::OnProcessDestroyed(__in DWORD dwPid)
{
  wprintf_s(L"Process #%lu destroyed\n", dwPid);
  return;
}

VOID CMyDeviarePD::OnThreadError(__in DWORD dwOsErr)
{
  wprintf_s(L"System error: %lu\n", dwOsErr);
  return;
}

