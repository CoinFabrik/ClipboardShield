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
#include "DriverInterface.h"

//-----------------------------------------------------------

static const GUID GUID_DEVCLASS_DEVIAREPROCDRV = {
  0x5D006E1A, 0x2631, 0x466C, { 0xB8, 0xA0, 0x32, 0xFD, 0x49, 0x8E, 0x44, 0x24 }
};

//------------------------------------------------------------------------------

DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD DriverUnload;

//------------------------------------------------------------------------------

__drv_dispatchType(IRP_MJ_CREATE)
DRIVER_DISPATCH OnCreate;
static NTSTATUS OnCreate(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
__drv_dispatchType(IRP_MJ_CLOSE)
DRIVER_DISPATCH OnClose;
static NTSTATUS OnClose(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
__drv_dispatchType(IRP_MJ_CLEANUP)
DRIVER_DISPATCH OnCleanup;
static NTSTATUS OnCleanup(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
__drv_dispatchType(IRP_MJ_DEVICE_CONTROL)
DRIVER_DISPATCH OnDeviceControl;
static NTSTATUS OnDeviceControl(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
__drv_dispatchType(IRP_MJ_READ)
DRIVER_DISPATCH OnRead;
static NTSTATUS OnRead(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);

//------------------------------------------------------------------------------

static VOID CsqInsertIrp(__in PIO_CSQ Csq, __in PIRP Irp);
static VOID CsqRemoveIrp(__in PIO_CSQ Csq, __in PIRP Irp);
static PIRP CsqPeekNextIrp(__in PIO_CSQ Csq, __in  PIRP Irp, __in PVOID PeekContext);
__drv_raisesIRQL(DISPATCH_LEVEL)
__drv_maxIRQL(DISPATCH_LEVEL)
static VOID CsqAcquireLock(__in PIO_CSQ Csq, __out __drv_out_deref(__drv_savesIRQL) PKIRQL Irql);
__drv_requiresIRQL(DISPATCH_LEVEL)
static VOID CsqReleaseLock(__in PIO_CSQ Csq, __in __drv_in(__drv_restoresIRQL) KIRQL Irql);
static VOID CsqCompleteCanceledIrp(__in PIO_CSQ pCsq, __in PIRP Irp);

//------------------------------------------------------------------------------

static VOID OnProcessNotify(IN HANDLE ParentId, IN HANDLE ProcessId, IN BOOLEAN Create);

#ifdef ALLOC_PRAGMA
  #pragma alloc_text(INIT, DriverEntry)
  #pragma alloc_text(PAGE, DriverUnload)
  #pragma alloc_text(PAGE, OnCreate)
  #pragma alloc_text(PAGE, OnClose)
  #pragma alloc_text(PAGE, OnCleanup)
  #pragma alloc_text(PAGE, OnDeviceControl)
  #pragma alloc_text(PAGE, OnRead)
  #pragma alloc_text(PAGE, OnProcessNotify)
#endif //ALLOC_PRAGMA

//-----------------------------------------------------------

static PDEVICE_OBJECT ControlDeviceObject = NULL;

//-----------------------------------------------------------

NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath)
{
  UNICODE_STRING deviceName, symbolicName, sddlString;
  PCONTROL_DEVICE_EXTENSION ControlDevExt;
  NTSTATUS nNtStatus;

  DEBUGPRINT(("DeviarePD: Driver loading...\n"));
  //BREAK();
  //create controller device object only accessible by administrators and system account
  RtlInitUnicodeString(&deviceName, DEVIAREPD_NTDEVICE_NAME_STRING);
  RtlInitUnicodeString(&sddlString, L"D:P(A;;GA;;;SY)(A;;GA;;;BA)");
  RtlInitUnicodeString(&symbolicName, DEVIAREPD_SYMBOLIC_NAME_STRING);
  nNtStatus = IoCreateDeviceSecure(DriverObject, sizeof(CONTROL_DEVICE_EXTENSION), &deviceName, FILE_DEVICE_UNKNOWN,
                                   FILE_DEVICE_SECURE_OPEN, (BOOLEAN)FALSE, &sddlString,
                                   &GUID_DEVCLASS_DEVIAREPROCDRV, &ControlDeviceObject);
  if (!NT_SUCCESS(nNtStatus))
  {
    DEBUGPRINT(("DeviarePD: Cannot create secure device [0x%08X]\n", nNtStatus));
    return nNtStatus;
  }
  //initialize device extension
  ControlDevExt = ControlDeviceObject->DeviceExtension;
  memset(ControlDevExt, 0, sizeof(CONTROL_DEVICE_EXTENSION));
  //init file contexts list
  ExInitializeFastMutex(&(ControlDevExt->FileContextsListMutex));
  InitializeListHead(&(ControlDevExt->FileContextsList));
  //init pending irp manager
  KeInitializeSpinLock(&(ControlDevExt->PendingIrpQueueLock));
  InitializeListHead(&(ControlDevExt->PendingIrpQueue));
  IoCsqInitialize(&(ControlDevExt->CancelSafeQueue), CsqInsertIrp, CsqRemoveIrp, CsqPeekNextIrp, CsqAcquireLock,
                  CsqReleaseLock, CsqCompleteCanceledIrp);
  //create symbolic link
  nNtStatus = IoCreateSymbolicLink(&symbolicName, &deviceName);
  if (!NT_SUCCESS(nNtStatus))
  {
    DEBUGPRINT(("DeviarePD: Cannot create symbolic link [0x%08X]\n", nNtStatus));
    IoDeleteDevice(ControlDeviceObject);
    ControlDeviceObject = NULL;
    return nNtStatus;
  }
  //create process notification
  nNtStatus = PsSetCreateProcessNotifyRoutine(OnProcessNotify, FALSE);
  if (!NT_SUCCESS(nNtStatus))
  {
    DEBUGPRINT(("DeviarePD: Cannot create process notification [0x%08X]\n", nNtStatus));
    IoDeleteSymbolicLink(&symbolicName);
    IoDeleteDevice(ControlDeviceObject);
    ControlDeviceObject = NULL;
    return nNtStatus;
  }
  //setup major functions callbacks
  DriverObject->MajorFunction[IRP_MJ_CREATE] = OnCreate;
  DriverObject->MajorFunction[IRP_MJ_CLOSE] = OnClose;
  DriverObject->MajorFunction[IRP_MJ_CLEANUP] = OnCleanup;
  DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = OnDeviceControl;
  DriverObject->MajorFunction[IRP_MJ_READ] = OnRead;
  DriverObject->DriverUnload = DriverUnload;
  ControlDeviceObject->Flags |= DO_BUFFERED_IO;
  ControlDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
  DEBUGPRINT(("DeviarePD: Driver loaded\n"));
  return nNtStatus;
}

VOID DriverUnload(IN PDRIVER_OBJECT DriverObject)
{
  PCONTROL_DEVICE_EXTENSION ControlDevExt = (PCONTROL_DEVICE_EXTENSION)(ControlDeviceObject->DeviceExtension);
  UNICODE_STRING symbolicName;

  PAGED_CODE();
  DEBUGPRINT(("DeviarePD: Driver unloading\n"));
  //----
  PsSetCreateProcessNotifyRoutine(OnProcessNotify, TRUE);
  //----
  RtlInitUnicodeString(&symbolicName, DEVIAREPD_SYMBOLIC_NAME_STRING);
  IoDeleteSymbolicLink(&symbolicName);
  //----
  IoDeleteDevice(ControlDeviceObject);
  ControlDeviceObject = NULL;
  DEBUGPRINT(("DeviarePD: Driver unloaded\n"));
  return;
}

//------------------------------------------------------------------------------

static NTSTATUS OnCreate(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
  PCONTROL_DEVICE_EXTENSION ControlDevExt = (PCONTROL_DEVICE_EXTENSION)(DeviceObject->DeviceExtension);
  PIO_STACK_LOCATION IrpStack;
  PFILE_CONTEXT FileContext;
  NTSTATUS nNtStatus;

  PAGED_CODE();
  DEBUGPRINT(("DeviarePD: Opening device...\n"));
  IrpStack = IoGetCurrentIrpStackLocation(Irp);
  ASSERT(IrpStack->FileObject != NULL);

  //allocate file context
  FileContext = ExAllocatePoolWithQuotaTag(NonPagedPool, sizeof(FILE_CONTEXT), POOL_TAG);
  if (FileContext == NULL)
  {
    DEBUGPRINT(("DeviarePD: Error allocating file context [0x%08X]\n", STATUS_INSUFFICIENT_RESOURCES));
    Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_INSUFFICIENT_RESOURCES;
  }

  //initialize file context
  InitializeListHead(&(FileContext->ListEntry));
  FileContext->FileObject = IrpStack->FileObject;
  FileContext->SignalProcessCreatedEvent = NULL;
  IoInitializeRemoveLock(&(FileContext->FileRundownLock), POOL_TAG, 0, 0);
  ASSERT(IrpStack->FileObject->FsContext == NULL);
  IrpStack->FileObject->FsContext = FileContext;

  //add to the file contexts list
  ExAcquireFastMutex(&(ControlDevExt->FileContextsListMutex));
  InsertTailList(&(ControlDevExt->FileContextsList), &(FileContext->ListEntry));
  ExReleaseFastMutex(&(ControlDevExt->FileContextsListMutex));

  //done
  DEBUGPRINT(("DeviarePD: Device opened\n"));
  Irp->IoStatus.Status = STATUS_SUCCESS;
  Irp->IoStatus.Information = FILE_OPENED;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);
  return STATUS_SUCCESS;
}

static NTSTATUS OnClose(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
  PCONTROL_DEVICE_EXTENSION ControlDevExt = (PCONTROL_DEVICE_EXTENSION)(DeviceObject->DeviceExtension);
  PIO_STACK_LOCATION IrpStack;
  PFILE_CONTEXT FileContext;

  PAGED_CODE();
  DEBUGPRINT(("DeviarePD: Device closing...\n"));
  IrpStack = IoGetCurrentIrpStackLocation(Irp);
  ASSERT(IrpStack->FileObject != NULL);
  FileContext = IrpStack->FileObject->FsContext;

  //remove from the file contexts list
  ExAcquireFastMutex(&(ControlDevExt->FileContextsListMutex));
  RemoveEntryList(&(FileContext->ListEntry));
  ExReleaseFastMutex(&(ControlDevExt->FileContextsListMutex));

  //free event
  if (FileContext->SignalProcessCreatedEvent != NULL)
    ObDereferenceObject(FileContext->SignalProcessCreatedEvent);

  //free context
  ExFreePoolWithTag(FileContext, POOL_TAG);

  //done
  DEBUGPRINT(("DeviarePD: Device closed\n"));
  Irp->IoStatus.Status = STATUS_SUCCESS;
  Irp->IoStatus.Information = 0;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);
  return STATUS_SUCCESS;
}

static NTSTATUS OnCleanup(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
  PCONTROL_DEVICE_EXTENSION ControlDevExt = (PCONTROL_DEVICE_EXTENSION)(DeviceObject->DeviceExtension);
  PIO_STACK_LOCATION IrpStack;
  PFILE_CONTEXT FileContext;
  PLIST_ENTRY ListEntry, NextListEntry;
  PIRP PendingIrp;
  NTSTATUS nNtStatus;

  PAGED_CODE();
  DEBUGPRINT(("DeviarePD: Device cleaning up...\n"));
  IrpStack = IoGetCurrentIrpStackLocation(Irp);
  ASSERT(IrpStack->FileObject != NULL);
  FileContext = IrpStack->FileObject->FsContext;

  //wait for all the threads that are currently dispatching to exit and
  //prevent any threads dispatching I/O on the same handle beyond this point.
  nNtStatus = IoAcquireRemoveLock(&(FileContext->FileRundownLock), Irp);
  ASSERT(NT_SUCCESS(nNtStatus));
  IoReleaseRemoveLockAndWait(&(FileContext->FileRundownLock), Irp);

  //cancel pending irps
  PendingIrp = IoCsqRemoveNextIrp(&(ControlDevExt->CancelSafeQueue), IrpStack->FileObject);
  while (PendingIrp != NULL)
  {
    PendingIrp->IoStatus.Information = 0;
    PendingIrp->IoStatus.Status = STATUS_CANCELLED;
    IoCompleteRequest(PendingIrp, IO_NO_INCREMENT);
    PendingIrp = IoCsqRemoveNextIrp(&(ControlDevExt->CancelSafeQueue), IrpStack->FileObject);
  }

  //done
  DEBUGPRINT(("DeviarePD: Device cleaned\n"));
  Irp->IoStatus.Status = STATUS_SUCCESS;
  Irp->IoStatus.Information = 0;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);
  return STATUS_SUCCESS;
}

static NTSTATUS OnDeviceControl(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
  PCONTROL_DEVICE_EXTENSION ControlDevExt = (PCONTROL_DEVICE_EXTENSION)(DeviceObject->DeviceExtension);
  PIO_STACK_LOCATION IrpStack;
  PFILE_CONTEXT FileContext;
  NTSTATUS nNtStatus;

  PAGED_CODE();
  IrpStack = IoGetCurrentIrpStackLocation(Irp);
  ASSERT(IrpStack->FileObject != NULL);
  FileContext = IrpStack->FileObject->FsContext;

  //process control code
  nNtStatus = STATUS_INVALID_PARAMETER;
  switch (IrpStack->Parameters.DeviceIoControl.IoControlCode)
  {
    case DEVIAREPD_IOCTL_SET_NOTIFICATION_EVENT:
      {
      HANDLE h;

      if (FileContext->SignalProcessCreatedEvent != NULL)
      {
        ObDereferenceObject(FileContext->SignalProcessCreatedEvent);
        FileContext->SignalProcessCreatedEvent = NULL;
      }
#ifdef _WIN64
      if (IoIs32bitProcess(Irp) != FALSE)
      {
        if (IrpStack->Parameters.DeviceIoControl.InputBufferLength != 4)
        {
          nNtStatus = STATUS_INVALID_BUFFER_SIZE;
          break;
        }
        h = (HANDLE)(ULONGLONG)*((DWORD*)(Irp->AssociatedIrp.SystemBuffer));
      }
      else
      {
        if (IrpStack->Parameters.DeviceIoControl.InputBufferLength != 8)
        {
          nNtStatus = STATUS_INVALID_BUFFER_SIZE;
          break;
        }
        h = (HANDLE)*((ULONGLONG*)(Irp->AssociatedIrp.SystemBuffer));
      }
#else //_WIN64
      if (IrpStack->Parameters.DeviceIoControl.InputBufferLength != 4)
      {
        nNtStatus = STATUS_INVALID_BUFFER_SIZE;
        break;
      }
      h = (HANDLE)*((DWORD*)(Irp->AssociatedIrp.SystemBuffer));
#endif //_WIN64
      nNtStatus = ObReferenceObjectByHandle(h, EVENT_MODIFY_STATE, *ExEventObjectType, UserMode,
                                            &(FileContext->SignalProcessCreatedEvent), NULL);
      }
      break;
  }

  //done
  Irp->IoStatus.Status = nNtStatus;
  Irp->IoStatus.Information = 0;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);
  return nNtStatus;
}

static NTSTATUS OnRead(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
  PCONTROL_DEVICE_EXTENSION ControlDevExt = (PCONTROL_DEVICE_EXTENSION)(DeviceObject->DeviceExtension);
  PIO_STACK_LOCATION IrpStack;
  PFILE_CONTEXT FileContext;
  NTSTATUS nNtStatus;

  PAGED_CODE();
  IrpStack = IoGetCurrentIrpStackLocation(Irp);
  ASSERT(IrpStack->FileObject != NULL);
  FileContext = IrpStack->FileObject->FsContext;

  //acquire rundown lock
  nNtStatus = IoAcquireRemoveLock(&(FileContext->FileRundownLock), Irp);
  if (!NT_SUCCESS(nNtStatus))
  {
    Irp->IoStatus.Status = nNtStatus;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return nNtStatus;
  }

  //check for room size
  if (IrpStack->Parameters.Read.Length < sizeof(DEVIAREPD_READ_REQUEST))
  {
    Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
    Irp->IoStatus.Information = 0;
    IoReleaseRemoveLock(&(FileContext->FileRundownLock), Irp);
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_BUFFER_TOO_SMALL;
  }

  //queue read request, Irp will be marked as pending
  IoCsqInsertIrp(&(ControlDevExt->CancelSafeQueue), Irp, NULL);

  //release rundown lock
  IoReleaseRemoveLock(&(FileContext->FileRundownLock), Irp);
  return STATUS_PENDING;
}

//------------------------------------------------------------------------------

static VOID CsqInsertIrp(__in PIO_CSQ Csq, __in PIRP Irp)
{
  PCONTROL_DEVICE_EXTENSION ControlDevExt;

  ControlDevExt = (PCONTROL_DEVICE_EXTENSION)CONTAINING_RECORD(Csq, CONTROL_DEVICE_EXTENSION, CancelSafeQueue);
  InsertTailList(&(ControlDevExt->PendingIrpQueue), &(Irp->Tail.Overlay.ListEntry));
  return;
}

static VOID CsqRemoveIrp(__in PIO_CSQ Csq, __in PIRP Irp)
{
  UNREFERENCED_PARAMETER(Csq);
  RemoveEntryList(&(Irp->Tail.Overlay.ListEntry));
  return;
}

static PIRP CsqPeekNextIrp(__in PIO_CSQ Csq, __in PIRP Irp, __in PVOID PeekContext)
{
  PCONTROL_DEVICE_EXTENSION ControlDevExt;
  PLIST_ENTRY ListHead, NextListEntry;
  PIO_STACK_LOCATION IrpStack;
  PIRP NextIrp;

  ControlDevExt = (PCONTROL_DEVICE_EXTENSION)CONTAINING_RECORD(Csq, CONTROL_DEVICE_EXTENSION, CancelSafeQueue);
  ListHead = &(ControlDevExt->PendingIrpQueue);
  if (Irp == NULL)
    NextListEntry = ListHead->Flink;
  else
    NextListEntry = Irp->Tail.Overlay.ListEntry.Flink;
  while (NextListEntry != ListHead)
  {
    NextIrp = (PIRP)CONTAINING_RECORD(NextListEntry, IRP, Tail.Overlay.ListEntry);
    IrpStack = IoGetCurrentIrpStackLocation(NextIrp);
    if (PeekContext == NULL || IrpStack->FileObject == (PFILE_OBJECT)PeekContext)
      return NextIrp;
    NextListEntry = NextListEntry->Flink;
  }
  return NULL;
}

__drv_raisesIRQL(DISPATCH_LEVEL)
__drv_maxIRQL(DISPATCH_LEVEL)
static VOID CsqAcquireLock(__in PIO_CSQ Csq, __out __drv_out_deref(__drv_savesIRQL) PKIRQL Irql)
{
  PCONTROL_DEVICE_EXTENSION ControlDevExt;

  ControlDevExt = (PCONTROL_DEVICE_EXTENSION)CONTAINING_RECORD(Csq, CONTROL_DEVICE_EXTENSION, CancelSafeQueue);
#pragma prefast(suppress: __WARNING_BUFFER_UNDERFLOW, "Underflow using expression 'devExtension->QueueLock'")
  KeAcquireSpinLock(&(ControlDevExt->PendingIrpQueueLock), Irql);
  return;
}

__drv_requiresIRQL(DISPATCH_LEVEL)
static VOID CsqReleaseLock(__in PIO_CSQ Csq, __in __drv_in(__drv_restoresIRQL) KIRQL Irql)
{
  PCONTROL_DEVICE_EXTENSION ControlDevExt;

  ControlDevExt = (PCONTROL_DEVICE_EXTENSION)CONTAINING_RECORD(Csq, CONTROL_DEVICE_EXTENSION, CancelSafeQueue);
#pragma prefast(suppress: __WARNING_BUFFER_UNDERFLOW, "Underflow using expression 'devExtension->QueueLock'")
  KeReleaseSpinLock(&(ControlDevExt->PendingIrpQueueLock), Irql);
  return;
}

static VOID CsqCompleteCanceledIrp(__in PIO_CSQ pCsq, __in PIRP Irp)
{
  UNREFERENCED_PARAMETER(pCsq);

  Irp->IoStatus.Status = STATUS_CANCELLED;
  Irp->IoStatus.Information = 0;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);
  return;
}

//------------------------------------------------------------------------------

static VOID OnProcessNotify(IN HANDLE ParentId, IN HANDLE ProcessId, IN BOOLEAN Create)
{
  PCONTROL_DEVICE_EXTENSION ControlDevExt = (PCONTROL_DEVICE_EXTENSION)(ControlDeviceObject->DeviceExtension);
  PLIST_ENTRY ListHead, ListEntry;
  PFILE_CONTEXT FileContext;
  PIRP NextIrp;
  NTSTATUS nNtStatus;

  ExAcquireFastMutex(&(ControlDevExt->FileContextsListMutex));
  ListHead = &(ControlDevExt->FileContextsList);
  for (ListEntry=ListHead->Flink; ListEntry!=ListHead; ListEntry=ListEntry->Flink)
  {
    FileContext = (PFILE_CONTEXT)CONTAINING_RECORD(ListEntry, FILE_CONTEXT, ListEntry);
    nNtStatus = IoAcquireRemoveLock(&(FileContext->FileRundownLock), FileContext->FileObject);
    if (NT_SUCCESS(nNtStatus))
    {
      //if a notification event was set, signal it
      if (FileContext->SignalProcessCreatedEvent != NULL)
      {
        KeSetEvent(FileContext->SignalProcessCreatedEvent, 0, FALSE);
      }
      //if there is a pending read...
      NextIrp = IoCsqRemoveNextIrp(&(ControlDevExt->CancelSafeQueue), FileContext->FileObject);
      if (NextIrp != NULL)
      {
        LPDEVIAREPD_READ_REQUEST lpReadReq = (LPDEVIAREPD_READ_REQUEST)(NextIrp->AssociatedIrp.SystemBuffer);

        lpReadReq->dwPid = (DWORD)ProcessId;
        lpReadReq->Created = Create ? 1 : 0;
        NextIrp->IoStatus.Status = STATUS_SUCCESS;
        NextIrp->IoStatus.Information = sizeof(DEVIAREPD_READ_REQUEST);
        IoCompleteRequest(NextIrp, IO_NO_INCREMENT);
      }
      //release rundown lock
      IoReleaseRemoveLock(&(FileContext->FileRundownLock), FileContext->FileObject);
    }
  }
  ExReleaseFastMutex(&(ControlDevExt->FileContextsListMutex));
  return;
}
