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
#ifndef _DEVIAREPD_H_
#define _DEVIAREPD_H_

#ifndef WINVER
  #define WINVER          _WIN32_WINNT_WINXP
#endif //!WINVER
#ifndef _WIN32_WINNT
  #define  _WIN32_WINNT   _WIN32_WINNT_WINXP
#endif //!_WIN32_WINNT
#include <SDKDDKVer.h>
#include <windows.h>

//------------------------------------------------------------------------------

#define DEVIAREPD_USE_EVENT_NOTIFICATION_METHOD 0

class CDeviarePD
{
public:
  CDeviarePD();
  virtual ~CDeviarePD();

  BOOL Initialize(__in_opt DWORD dwReadAhead=DEVIAREPD_USE_EVENT_NOTIFICATION_METHOD);
  VOID Finalize();

  virtual VOID OnProcessCreated(__in DWORD dwPid);
  virtual VOID OnProcessDestroyed(__in DWORD dwPid);
  virtual VOID OnThreadError(__in DWORD dwOsErr);

  static BOOL StartDriver();
  static BOOL StopDriver();
  static BOOL InstallDriver(__in_z LPCWSTR szDriverFileNameW);
  static BOOL UninstallDriver();

private:
  VOID ThreadProc();
  VOID TP_ReadAheadMethod();
  VOID TP_EventNotificationMethod();

  DWORD GetProcessesList(__in int nListIndex);
  DWORD InitializeInternal(__in DWORD dwReadAhead);

  static unsigned __stdcall ThreadProcStub(void*);

private:
  HANDLE hDriver;
  HANDLE hWorkerThread, hStopWorkerThread;
  HANDLE hNotifEvent;
  HANDLE hIOCP;
  DWORD dwReadAhead;
  LPVOID lpRequests;
  LPVOID lpPidsList[2];
};

//------------------------------------------------------------------------------

#endif //_DEVIAREPD_H_
