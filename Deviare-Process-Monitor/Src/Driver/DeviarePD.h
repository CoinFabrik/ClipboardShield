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
#ifndef _MAIN_H_
#define _MAIN_H_

#include <ntifs.h>
#include <wdmsec.h> // for SDDLs
#define NTSTRSAFE_LIB
#include <ntstrsafe.h>
#pragma warning(default:4201)

//------------------------------------------------------------------------------

#define POOL_TAG          (ULONG)'MPvD'

#if DBG
  #define BREAK()         DbgBreakPoint()
  #define DEBUGPRINT(_x_) DbgPrint _x_
#else //DBG
  #define BREAK()
  #define DEBUGPRINT(_x_)
#endif //DBG

//------------------------------------------------------------------------------

typedef struct _FILE_CONTEXT {
  LIST_ENTRY ListEntry;
  //lock to rundown threads that are dispatching I/Os on a file handle
  //while the cleanup for that handle is in progress.
  PFILE_OBJECT FileObject;
  IO_REMOVE_LOCK FileRundownLock;
  //----
  ERESOURCE Mutex;
  PKEVENT SignalProcessCreatedEvent;
} FILE_CONTEXT, *PFILE_CONTEXT;

typedef struct _CONTROL_DEVICE_EXTENSION {
  FAST_MUTEX FileContextsListMutex;
  LIST_ENTRY FileContextsList;
  //----
  KSPIN_LOCK PendingIrpQueueLock;
  LIST_ENTRY PendingIrpQueue;
  IO_CSQ CancelSafeQueue;
} CONTROL_DEVICE_EXTENSION, *PCONTROL_DEVICE_EXTENSION;

//------------------------------------------------------------------------------

#endif //_MAIN_H_
