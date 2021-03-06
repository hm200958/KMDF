/*++

Module Name:

    driver.c

Abstract:

    This file contains the driver entry points and callbacks.

Environment:

    Kernel-mode Driver Framework

--*/


#include "driver.h"
#include "driver.tmh"
#include "DriverCallFun.h"

#include "KmdfManager.h"

PDRIVER_OBJECT g_DvriverObject = nullptr;
PDEVICE_OBJECT g_DvriverControlObject = nullptr;

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, KMDFEvtDeviceAdd)
#pragma alloc_text (PAGE, KMDFEvtDriverContextCleanup)
#endif

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:
    DriverEntry initializes the driver and is the first routine called by the
    system after the driver is loaded. DriverEntry specifies the other entry
    points in the function driver, such as EvtDevice and DriverUnload.

Parameters Description:

    DriverObject - represents the instance of the function driver that is loaded
    into memory. DriverEntry must initialize members of DriverObject before it
    returns to the caller. DriverObject is allocated by the system before the
    driver is loaded, and it is released by the system after the system unloads
    the function driver from memory.

    RegistryPath - represents the driver specific path in the Registry.
    The function driver can use the path to store driver related data between
    reboots. The path does not store hardware instance specific data.

Return Value:

    STATUS_SUCCESS if successful,
    STATUS_UNSUCCESSFUL otherwise.

--*/
{
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attributes;
	UNICODE_STRING DriverName;
	UNICODE_STRING DriverSymblicName;

	g_DvriverObject = DriverObject;
    //
    // Initialize WPP Tracing
    //

	KdPrint(("DriverEntry start\n"));
    WPP_INIT_TRACING(DriverObject, RegistryPath);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    //
    // Register a cleanup callback so that we can call WPP_CLEANUP when
    // the framework driver object is deleted during driver unload.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.EvtCleanupCallback = KMDFEvtDriverContextCleanup;

    WDF_DRIVER_CONFIG_INIT(&config,
                           KMDFEvtDeviceAdd
                           );

    status = WdfDriverCreate(DriverObject,
                             RegistryPath,
                             &attributes,
                             &config,
                             WDF_NO_HANDLE
                             );

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfDriverCreate failed %!STATUS!", status);
        WPP_CLEANUP(DriverObject);
        return status;
    }

	
	RtlInitUnicodeString(&DriverName, DRIVER_NAME);
	KdPrint(("DriverEntry start:%wZ\n",&DriverName));
	status = IoCreateDevice(DriverObject, 0, &DriverName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &g_DvriverControlObject);
	if ( !NT_SUCCESS(status))
	{
		KdPrint(("DriverEntry createDdriver failed :%d\n", status));
		//RtlFreeUnicodeString(&DriverName);
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "IoCreateDevice failed %!STATUS!", status);
		return status;
	}

	RtlInitUnicodeString(&DriverSymblicName, DRIVER_SYMBOLICLINK);
	status = IoCreateSymbolicLink(&DriverSymblicName, &DriverName);
	if ( !NT_SUCCESS(status))
	{
		KdPrint(("DriverEntry IoCreateSymbolicLink failed :%d \n", status));
		IoDeleteDevice(g_DvriverControlObject);
		//RtlFreeUnicodeString(&DriverName);
		//RtlFreeUnicodeString(&DriverSymblicName);
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "IoCreateSymbolicLink failed %!STATUS!", status);
		return status;
	}

	for ( int i = 0; i< IRP_MJ_MAXIMUM_FUNCTION; ++i )
	{
		DriverObject->MajorFunction[i] = DriverContrlProcess;
	}

	DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverCreate;
	DriverObject->MajorFunction[IRP_MJ_READ] = DriverRead;
	DriverObject->MajorFunction[IRP_MJ_WRITE] = DriverWrite;
	DriverObject->MajorFunction[IRP_MJ_CLEANUP] = DriverClose;
	DriverObject->DriverUnload = DriverUnload;
	//g_DvriverControlObject->Flags &= ~DO_DEVICE_INITIALIZING;
	g_DvriverControlObject->Flags |= DO_DIRECT_IO;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

	g_CKMDFmanager = new CKmdfManager;
	if ( g_CKMDFmanager != nullptr )
	{
		g_CKMDFmanager->InitManager( );
	}

	KdPrint(("DriverEntry end\n"));
    return status;
}

NTSTATUS
KMDFEvtDeviceAdd(
    _In_    WDFDRIVER       Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
/*++
Routine Description:

    EvtDeviceAdd is called by the framework in response to AddDevice
    call from the PnP manager. We create and initialize a device object to
    represent a new instance of the device.

Arguments:

    Driver - Handle to a framework driver object created in DriverEntry

    DeviceInit - Pointer to a framework-allocated WDFDEVICE_INIT structure.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS status;

    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    status = KMDFCreateDevice(DeviceInit);
	if ( NT_SUCCESS(status ))
	{
		WdfDeviceInitSetIoType( DeviceInit, WdfDeviceIoDirect );
	}

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return status;
}

VOID
KMDFEvtDriverContextCleanup(
    _In_ WDFOBJECT DriverObject
    )
/*++
Routine Description:

    Free all the resources allocated in DriverEntry.

Arguments:

    DriverObject - handle to a WDF Driver object.

Return Value:

    VOID.

--*/
{
    UNREFERENCED_PARAMETER(DriverObject);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    //
    // Stop WPP Tracing
    //
    WPP_CLEANUP(WdfDriverWdmGetDriverObject((WDFDRIVER)DriverObject));
}
