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
#include "DeviarePD.h"
#include <string.h>
#include <process.h>
#include <Psapi.h>
#include "..\..\Deviare-Process-Monitor\Src\Driver\DriverInterface.h"
#include <stdio.h>
#pragma comment(lib, "psapi.lib")

//------------------------------------------------------------------------------

typedef struct {
  OVERLAPPED sOvr;
  DEVIAREPD_READ_REQUEST sReadReq;
} READ_REQUEST;

typedef struct {
  SIZE_T nListCount, nListSize;
  DWORD dwPids[2];
} PIDLIST;

static VOID DebugPrint(__in_z LPCSTR szFormatA, ...);

//------------------------------------------------------------------------------

CDeviarePD::CDeviarePD()
{
  hDriver = NULL;
  hWorkerThread = hStopWorkerThread = NULL;
  hNotifEvent = NULL;
  hIOCP = NULL;
  dwReadAhead = 0;
  lpRequests = NULL;
  lpPidsList[0] = lpPidsList[1] = NULL;
  return;
}

CDeviarePD::~CDeviarePD()
{
  Finalize();
  return;
}


static VOID DebugPrintV(__in_z LPCSTR szFormatA, va_list ap)
{
	CHAR szBufA[2048];
	int i;

	i = vsnprintf_s(szBufA, _countof(szBufA), szFormatA, ap);
	if (i < 0)
		i = 0;
	else if (i > _countof(szBufA) - 1)
		i = _countof(szBufA) - 1;
	szBufA[i] = 0;
	OutputDebugStringA(szBufA);
	return;
}

static VOID DebugPrint(__in_z LPCSTR szFormatA, ...)
{
	va_list ap;

	va_start(ap, szFormatA);
	DebugPrintV(szFormatA, ap);
	va_end(ap);
	return;
}

DWORD CDeviarePD::InitializeInternal(__in DWORD _dwReadAhead)
{
  DWORD dw;
  DWORD dwOsErr = ERROR_SUCCESS;
  //open driver
  hDriver = ::CreateFileW(DEVIAREPD_DEVICE_NAME_STRING, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                          FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OVERLAPPED, NULL);
  if (hDriver == NULL || hDriver == INVALID_HANDLE_VALUE)
  {
    dwOsErr = ::GetLastError();
	DebugPrint("Create file error: %d", dwOsErr);
    hDriver = NULL;
    if (dwOsErr != ERROR_FILE_NOT_FOUND)
      return dwOsErr;
    //start driver if not done yet
    if (StartDriver() == FALSE)
    {
      dwOsErr = ::GetLastError();
	  DebugPrint("Startdriver error: %d", dwOsErr);

      return dwOsErr;
    }
    //try reopen
    hDriver = ::CreateFileW(DEVIAREPD_DEVICE_NAME_STRING, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OVERLAPPED, NULL);
    if (hDriver == NULL || hDriver == INVALID_HANDLE_VALUE)
    {
      dwOsErr = ::GetLastError();
	  DebugPrint("Create file 2nd error: %d", dwOsErr);
      hDriver = NULL;
      return dwOsErr;
    }
    dwOsErr = ERROR_SUCCESS;
  }
  //setup...
  dwReadAhead = _dwReadAhead;
  if (dwReadAhead > 0)
  {
    //...buffers
    lpRequests = malloc((SIZE_T)dwReadAhead * sizeof(READ_REQUEST));
    if (lpRequests == NULL)
    {
      dwOsErr = ERROR_NOT_ENOUGH_MEMORY;
      return dwOsErr;
    }
    memset(lpRequests, 0, (SIZE_T)dwReadAhead * sizeof(READ_REQUEST));
    //create I/O completion port
    hIOCP = ::CreateIoCompletionPort(hDriver, NULL, 0, 1);
    if (hIOCP == NULL)
    {
		DebugPrint("IO completion port error: %d", dwOsErr);
      dwOsErr = ::GetLastError();
      return dwOsErr;
    }
  }
  else
  {
    //...notification event
    hNotifEvent = ::CreateEventW(NULL, TRUE, FALSE, NULL);
    if (hNotifEvent == NULL)
    {
      dwOsErr = ::GetLastError();
      return dwOsErr;
    }
    if (::DeviceIoControl(hDriver, DEVIAREPD_IOCTL_SET_NOTIFICATION_EVENT, &hNotifEvent, (DWORD)sizeof(hNotifEvent),
                          NULL, 0, &dw, NULL) == FALSE)
    {
		DebugPrint("DEVICE IO Control error: %d", dwOsErr);
      dwOsErr = ::GetLastError();
      return dwOsErr;
    }
    //create worker thread's stop event
    hStopWorkerThread = ::CreateEventW(NULL, TRUE, FALSE, NULL);
    if (hStopWorkerThread == NULL)
    {
      dwOsErr = ::GetLastError();
      return dwOsErr;
    }
  }
  //create worker thread
  hWorkerThread = (HANDLE)_beginthreadex(NULL, 0, &CDeviarePD::ThreadProcStub, this, 0, NULL);
  if (hWorkerThread == NULL)
  {
    dwOsErr = ::GetLastError();
      return dwOsErr;
  }
  return dwOsErr;
}

BOOL CDeviarePD::Initialize(__in_opt DWORD _dwReadAhead)
{
  Finalize();
  
  DWORD dwOsErr = InitializeInternal(dwReadAhead);

  if (dwOsErr != ERROR_SUCCESS)
    Finalize();
  ::SetLastError(dwOsErr);
  return (dwOsErr == ERROR_SUCCESS) ? TRUE : FALSE;
}

VOID CDeviarePD::Finalize()
{
  //stop worker thread
  if (hStopWorkerThread != NULL)
  {
    ::SetEvent(hStopWorkerThread);
  }
  else if (hIOCP != NULL)
  {
    ::PostQueuedCompletionStatus(hIOCP, 0, 0, NULL);
  }
  //wait for thread end and close handle
  if (hWorkerThread != NULL)
  {
    ::WaitForSingleObject(hWorkerThread, INFINITE);
    ::CloseHandle(hWorkerThread);
    hWorkerThread = NULL;
  }
  //close driver
  if (hDriver != NULL)
  {
    ::CloseHandle(hDriver);
    hDriver = NULL;
  }
  //close more handles
  if (hNotifEvent != NULL)
  {
    ::CloseHandle(hNotifEvent);
    hNotifEvent = NULL;
  }
  if (hIOCP != NULL)
  {
    ::CloseHandle(hIOCP);
    hIOCP = NULL;
  }
  //free internals
  if (lpRequests != NULL)
  {
    free(lpRequests);
    lpRequests = NULL;
  }
  if (lpPidsList[1] != NULL)
  {
    free(lpPidsList[1]);
    lpPidsList[1] = NULL;
  }
  if (lpPidsList[0] != NULL)
  {
    free(lpPidsList[0]);
    lpPidsList[0] = NULL;
  }
  //zero more stuff
  dwReadAhead = 0;
  return;
}

VOID CDeviarePD::OnProcessCreated(__in DWORD dwPid)
{
  return;
}

VOID CDeviarePD::OnProcessDestroyed(__in DWORD dwPid)
{
  return;
}

VOID CDeviarePD::OnThreadError(__in DWORD dwOsErr)
{
  return;
}

BOOL CDeviarePD::StartDriver()
{
  SC_HANDLE hScMgr, hServ;
  SERVICE_STATUS sSvcStatus;
  DWORD dwOsErr;

  hScMgr = ::OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
  if (hScMgr == NULL)
    return FALSE;
  hServ = ::OpenServiceW(hScMgr, DEVIAREPD_SERVICE_NAME, SERVICE_START|SERVICE_QUERY_STATUS);
  if (hServ == NULL)
  {
    dwOsErr = ::GetLastError();
	DebugPrint("error opening service %d", dwOsErr);
    ::CloseServiceHandle(hScMgr);
    ::SetLastError(dwOsErr);
    return FALSE;
  }
  //start driver
  if (::StartServiceW(hServ, 0, NULL) == FALSE)
  {
    dwOsErr = ::GetLastError();
	DebugPrint("error starting service %d", dwOsErr);

    if (dwOsErr != ERROR_SERVICE_ALREADY_RUNNING)
    {
      ::CloseServiceHandle(hServ);
      ::CloseServiceHandle(hScMgr);
      ::SetLastError(dwOsErr);
      return FALSE;
    }
  }
  //wait until service is running
  while (1)
  {
    memset(&sSvcStatus, 0, sizeof(sSvcStatus));
    if (::QueryServiceStatus(hServ, &sSvcStatus) == FALSE)
      break;
    if (sSvcStatus.dwCurrentState == SERVICE_RUNNING)
      break;
    if (sSvcStatus.dwCurrentState != SERVICE_START_PENDING)
    {
      ::CloseServiceHandle(hServ);
      ::CloseServiceHandle(hScMgr);
      ::SetLastError(ERROR_SERVICE_REQUEST_TIMEOUT);
      return FALSE;
    }
    ::Sleep(50);
  }
  //close handles
  ::CloseServiceHandle(hServ);
  ::CloseServiceHandle(hScMgr);
  //done
  ::SetLastError(ERROR_SUCCESS);
  return TRUE;
}

BOOL CDeviarePD::StopDriver()
{
  SC_HANDLE hScMgr, hServ;
  SERVICE_STATUS sSvcStatus;
  DWORD dwOsErr;

  //connect to service manager
  hScMgr = ::OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
  if (hScMgr == NULL)
    return FALSE;
  //open service
  hServ = ::OpenServiceW(hScMgr, DEVIAREPD_SERVICE_NAME, SERVICE_STOP|SERVICE_QUERY_STATUS);
  if (hServ != NULL)
  {
    //stop service
    if (::ControlService(hServ, SERVICE_CONTROL_STOP, &sSvcStatus) != FALSE)
    {
      memset(&sSvcStatus, 0, sizeof(sSvcStatus));
      ::Sleep(50);
      while (::QueryServiceStatus(hServ, &sSvcStatus) != FALSE)
      {
        if (sSvcStatus.dwCurrentState != SERVICE_STOP_PENDING)
          break;
        ::Sleep(50);
      }
      if (sSvcStatus.dwCurrentState != SERVICE_STOPPED)
      {
        ::CloseServiceHandle(hServ);
        ::CloseServiceHandle(hScMgr);
        ::SetLastError(ERROR_SERVICE_REQUEST_TIMEOUT);
        return FALSE;
      }
    }
    else
    {
      dwOsErr = ::GetLastError();
      ::CloseServiceHandle(hServ);
      ::CloseServiceHandle(hScMgr);
      ::SetLastError(dwOsErr);
      return FALSE;
    }
    //close service handles
    ::CloseServiceHandle(hServ);
  }
  else
  {
    //if service was not found, continue
    dwOsErr = ::GetLastError();
    if (dwOsErr != ERROR_FILE_NOT_FOUND && dwOsErr != ERROR_PATH_NOT_FOUND && dwOsErr != ERROR_SERVICE_DOES_NOT_EXIST)
    {
      ::CloseServiceHandle(hScMgr);
      ::SetLastError(dwOsErr);
      return FALSE;
    }
  }
  //close service handles
  ::CloseServiceHandle(hScMgr);
  //done
  ::SetLastError(ERROR_SUCCESS);
  return TRUE;
}

BOOL CDeviarePD::InstallDriver(__in_z LPCWSTR szDriverFileNameW)
{
  WCHAR szDestW[1024];
  DWORD dwLen, dwOsErr;
  SC_HANDLE hScMgr, hServ;

  if (szDriverFileNameW == NULL || szDriverFileNameW[0] == 0)
  {
    ::SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  //setup target driver folder and file
  dwLen = ::GetSystemDirectoryW(szDestW, 512);
  if (dwLen > 0 && szDestW[dwLen - 1] != L'\\')
    szDestW[dwLen++] = L'\\';
  szDestW[dwLen] = 0;
  wcsncat_s(szDestW, _countof(szDestW), L"drivers\\DeviarePD.sys", _TRUNCATE);
  //copy file
  if (::CopyFileW(szDriverFileNameW, szDestW, FALSE) == FALSE)
    return FALSE;
  //connect to service manager
  hScMgr = ::OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
  if (hScMgr == NULL)
    return FALSE;
  //create service
  hServ = ::CreateServiceW(hScMgr, DEVIAREPD_SERVICE_NAME, DEVIAREPD_SERVICE_NAME, SERVICE_ALL_ACCESS,
                           SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE, szDestW, NULL, NULL,
                           NULL, NULL, NULL);
  if (hServ == NULL)
  {
    dwOsErr = ::GetLastError();
    ::CloseServiceHandle(hScMgr);
    ::SetLastError(dwOsErr);
    return FALSE;
  }
  //close handles
  ::CloseServiceHandle(hServ);
  ::CloseServiceHandle(hScMgr);
  //done
  ::SetLastError(ERROR_SUCCESS);
  return TRUE;
}

BOOL CDeviarePD::UninstallDriver()
{
  WCHAR szSrcW[1024];
  DWORD dwLen, dwOsErr, dwRetryCount;
  SC_HANDLE hScMgr, hServ;
  SERVICE_STATUS sServStatus;

  //setup source driver folder and file
  dwLen = ::GetSystemDirectoryW(szSrcW, 512);
  if (dwLen > 0 && szSrcW[dwLen - 1] != L'\\')
    szSrcW[dwLen++] = L'\\';
  szSrcW[dwLen] = 0;
  wcsncat_s(szSrcW, _countof(szSrcW), L"drivers\\DeviarePD.sys", _TRUNCATE);
  //connect to service manager
  hScMgr = ::OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
  if (hScMgr == NULL)
    return FALSE;
  //open service
  hServ = ::OpenServiceW(hScMgr, DEVIAREPD_SERVICE_NAME, SERVICE_ALL_ACCESS);
  if (hServ != NULL)
  {
    //stop service
    ::ControlService(hServ, SERVICE_CONTROL_STOP, &sServStatus);
    //wait some time until stopped
    dwRetryCount = 20;
    while (dwRetryCount > 0 && sServStatus.dwCurrentState == SERVICE_STOP_PENDING)
    {
      ::Sleep(100);
      if (::QueryServiceStatus(hServ, &sServStatus) == FALSE)
        break;
      dwRetryCount--;
    }
    //delete service
    if (::DeleteService(hServ) == FALSE)
    {
      dwOsErr = ::GetLastError();
      ::CloseServiceHandle(hServ);
      ::CloseServiceHandle(hScMgr);
      ::SetLastError(dwOsErr);
      return FALSE;
    }
    ::CloseServiceHandle(hServ);
  }
  else
  {
    //if service was not found, continue
    dwOsErr = ::GetLastError();
    if (dwOsErr != ERROR_FILE_NOT_FOUND && dwOsErr != ERROR_PATH_NOT_FOUND && dwOsErr != ERROR_SERVICE_DOES_NOT_EXIST)
    {
      ::CloseServiceHandle(hScMgr);
      ::SetLastError(dwOsErr);
      return FALSE;
    }
  }
  ::CloseServiceHandle(hScMgr);
  //delete the driver file
  if (::DeleteFileW(szSrcW) == FALSE)
  {
    dwOsErr = ::GetLastError();
    //ignore if file does not exists
    if (dwOsErr != ERROR_FILE_NOT_FOUND && dwOsErr != ERROR_PATH_NOT_FOUND)
    {
      if (::MoveFileExW(szSrcW, NULL, MOVEFILE_DELAY_UNTIL_REBOOT) == FALSE)
        return FALSE;
    }
  }
  //done
  ::SetLastError(ERROR_SUCCESS);
  return TRUE;
}

VOID CDeviarePD::ThreadProc()
{
  if (dwReadAhead > 0)
    TP_ReadAheadMethod();
  else
    TP_EventNotificationMethod();
  return;
}

VOID CDeviarePD::TP_ReadAheadMethod()
{
  READ_REQUEST *lpReq;
  DWORD dw, dwOsErr, dwReaded;
  OVERLAPPED *lpOvr;
  ULONG_PTR dummy1;

  //send initial read aheads
  for (dw=0; dw<dwReadAhead; dw++)
  {
    lpReq = &(((READ_REQUEST*)lpRequests)[dw]);
    memset(&(lpReq->sOvr), 0, sizeof(lpReq->sOvr));
    if (::ReadFile(hDriver, &(lpReq->sReadReq), sizeof(lpReq->sReadReq), &dwReaded, &(lpReq->sOvr)) == FALSE)
    {
      dwOsErr = ::GetLastError();
      if (dwOsErr != ERROR_IO_PENDING)
      {
        ::CancelIo(hDriver);
        OnThreadError(dwOsErr);
        return;
      }
    }
  }
  while (1)
  {
    if (::GetQueuedCompletionStatus(hIOCP, &dw, &dummy1, &lpOvr, INFINITE) == FALSE)
      continue;
    //quit thread?
    if (lpOvr == NULL)
      break;
    //find request
    lpReq = (READ_REQUEST*)CONTAINING_RECORD(lpOvr, READ_REQUEST, sOvr);
    //call callback
    if (lpReq->sReadReq.Created != 0)
      OnProcessCreated(lpReq->sReadReq.dwPid);
    else
      OnProcessDestroyed(lpReq->sReadReq.dwPid);
    //do a new read ahead
    memset(&(lpReq->sOvr), 0, sizeof(lpReq->sOvr));
    if (::ReadFile(hDriver, &(lpReq->sReadReq), sizeof(lpReq->sReadReq), &dwReaded, &(lpReq->sOvr)) == FALSE)
    {
      dwOsErr = ::GetLastError();
      if (dwOsErr != ERROR_IO_PENDING)
      {
        ::CancelIo(hDriver);
        OnThreadError(dwOsErr);
        return;
      }
    }
  }
  return;
}

VOID CDeviarePD::TP_EventNotificationMethod()
{
  PIDLIST *lpList[2];
  LPDWORD lpdwPids[2];
  HANDLE hEvents[2];
  SIZE_T i, k, nCountInActiveList, nCount;
  int nActiveList, nChecksCounter;
  DWORD dwOsErr;

  hEvents[0] = hStopWorkerThread;
  hEvents[1] = hNotifEvent;
  //retrieve initial list
  dwOsErr = GetProcessesList(nActiveList = 0);
  if (dwOsErr != ERROR_SUCCESS)
  {
    OnThreadError(dwOsErr);
    return;
  }

  //loop
  nChecksCounter = 0;
  while (1)
  {
    if (nChecksCounter == 0)
    {
      if (::WaitForMultipleObjects(2, hEvents, FALSE, INFINITE) == WAIT_OBJECT_0)
        break;
      ::ResetEvent(hNotifEvent);
    }
    else
    {
      if (::WaitForSingleObject(hStopWorkerThread, 0) == WAIT_OBJECT_0)
        break;
      nChecksCounter = 20;
    }
    //retrieve process list
    dwOsErr = GetProcessesList(1 - nActiveList);
    if (dwOsErr != ERROR_SUCCESS)
    {
      OnThreadError(dwOsErr);
      return;
    }
    //look for new processes and raise events
    lpList[0] = (PIDLIST*)lpPidsList[0];
    lpList[1] = (PIDLIST*)lpPidsList[1];
    nCountInActiveList = lpList[nActiveList]->nListCount;
    nCount = lpList[1 - nActiveList]->nListCount;
    lpdwPids[0] = lpList[nActiveList]->dwPids;
    lpdwPids[1] = lpList[1 - nActiveList]->dwPids;
    //if list counts are the same, perform a quick check
    if (nCountInActiveList == nCount)
    {
      for (i=0; i<nCount && lpdwPids[0][i]==lpdwPids[1][i]; i++);
      if (i >= nCount)
        continue;
    }
    //slow path
    for (i=0; i<nCount; i++)
    {
      for (k=0; k<nCountInActiveList; k++)
      {
        if (lpdwPids[0][k] == lpdwPids[1][i])
        {
          lpdwPids[0][k] = 0;
          break;
        }
      }
      //if the process was not found => a new process was detected
      if (k >= nCountInActiveList)
      {
        OnProcessCreated(lpdwPids[1][i]);
        //check for abort
        if (::WaitForSingleObject(hStopWorkerThread, 0) == WAIT_OBJECT_0)
          break;
      }
    }
    //non-zero processes that remains in the original active list are those which no longer exists
    for (k=0; k<nCountInActiveList; k++)
    {
      if (lpdwPids[0][k] != 0)
      {
        OnProcessDestroyed(lpdwPids[0][k]);
        //check for abort
        if (::WaitForSingleObject(hStopWorkerThread, 0) == WAIT_OBJECT_0)
          break;
      }
    }
    //switch lists
    nActiveList = 1 - nActiveList;
  }
  return;
}

unsigned __stdcall CDeviarePD::ThreadProcStub(void* param)
{
  reinterpret_cast<CDeviarePD*>(param)->ThreadProc();
  return 0;
}

DWORD CDeviarePD::GetProcessesList(__in int nListIndex)
{
  PIDLIST **lplpList = (PIDLIST **)&(lpPidsList[nListIndex]);
  SIZE_T nCount;
  DWORD dwNeeded;

  //get initial size
  nCount = (*lplpList != NULL) ? ((*lplpList)->nListSize) : 1024;
  while (1)
  {
    //create list if not done yet
    if (*lplpList == NULL)
    {
      *lplpList = (PIDLIST*)malloc(sizeof(PIDLIST) + nCount * sizeof(DWORD));
      if (*lplpList == NULL)
        return ERROR_NOT_ENOUGH_MEMORY;
      (*lplpList)->nListCount = 0;
      (*lplpList)->nListSize = nCount;
    }
    //get processes
    if (::EnumProcesses((*lplpList)->dwPids, (DWORD)(nCount * sizeof(DWORD)), &dwNeeded) == FALSE)
      return ::GetLastError();
    dwNeeded /= (DWORD)sizeof(DWORD);
    if ((SIZE_T)dwNeeded < nCount)
    {
      (*lplpList)->nListCount = (SIZE_T)dwNeeded;
      break;
    }
    //we need to grow the list
    nCount = (SIZE_T)dwNeeded + 128;
    free(*lplpList);
    *lplpList = NULL;
  }
  return ERROR_SUCCESS;
}
