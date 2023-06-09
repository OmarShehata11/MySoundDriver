#include <ntddk.h>
#include <initguid.h>
#include <Usbiodef.h>
#include <Wdmguid.h>
#include <guiddef.h>
#include <wdm.h>
#include "Header.h"

PVOID ReturnNotificationValue;


//
// Call back routine when the notification recieved from the PnP manager
//
DRIVER_NOTIFICATION_CALLBACK_ROUTINE UsbDriverCallBackRoutine;

//
// functions for the cancel-safe Irp framework
//


IO_CSQ_INSERT_IRP_EX CsqInsertIrp;
IO_CSQ_REMOVE_IRP CsqRemoveIrp;
IO_CSQ_PEEK_NEXT_IRP CsqPeekNextIrp;
IO_CSQ_ACQUIRE_LOCK CsqAcquireLock;
IO_CSQ_RELEASE_LOCK CsqReleaseLock; 
IO_CSQ_COMPLETE_CANCELED_IRP CsqCompleteCanceledIrp;


void DriverUnload(PDRIVER_OBJECT);
NTSTATUS CreateCloseFunction(PDEVICE_OBJECT, PIRP);
NTSTATUS UsbDriverCallBackRoutine(IN PVOID Notification, IN PVOID);
NTSTATUS ControlCodeFunction(PDEVICE_OBJECT, PIRP);

class AutoEnterLeave
{
private :
	LPCSTR FuncName;

public:
	AutoEnterLeave(LPCSTR _funcName)
	{
		FuncName = _funcName;
		KdPrint(("Entering function %s", FuncName));
	}

	~AutoEnterLeave()
	{
		KdPrint(("Leaving Fucntion %s", FuncName));
	}
};

#define AUTO_ENTER_LEAVE() AutoEnterLeave omar(__FUNCTION__)

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING)
{

	AUTO_ENTER_LEAVE();

	UNICODE_STRING DeviceName, SymbolicLinkName;
	PDEVICE_OBJECT DeviceObject;
	PDEVICE_EXTENSION lpDeviceExtension;


	RtlInitUnicodeString(&DeviceName, L"\\Device\\SoundDriver");
	RtlInitUnicodeString(&SymbolicLinkName, L"\\??\\SoundSL");

	// Add the DEVICE_EXTENSION structure size ...
	NTSTATUS status = IoCreateDevice(DriverObject, sizeof(DEVICE_EXTENSION), &DeviceName, FILE_DEVICE_UNKNOWN, NULL, false, &DeviceObject);
	if (status != STATUS_SUCCESS)
	{
		KdPrint(("Error: Create a Device Object"));
		return status;
	}

	// 
	// Initialize the Dev extension structure
	//
	lpDeviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;


	// Initialzing the Queue and the Cancel-safe Framework functions.
	InitializeListHead(&lpDeviceExtension->IrpQueueList);
	IoCsqInitializeEx(&lpDeviceExtension->CancelSafeQueue, CsqInsertIrp, CsqRemoveIrp, CsqPeekNextIrp, CsqAcquireLock, CsqReleaseLock, CsqCompleteCanceledIrp);

	status = IoCreateSymbolicLink(&SymbolicLinkName, &DeviceName);
	
	if (status != STATUS_SUCCESS)
	{
		KdPrint(("Error: Create a Symbolic Link"));
		return status;
	}

	//
	// Set the Dispatch routines 
	//
	DriverObject->DriverUnload = DriverUnload;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverObject->MajorFunction[IRP_MJ_CLOSE] = CreateCloseFunction;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = ControlCodeFunction;
	
	// GUID for usb port to register PnP notification with
	GUID UsbGuid = GUID_DEVINTERFACE_USB_DEVICE;

	//
	// Adding the device object to the context paramter 
	// (to be used later in the callback routine.
	//
	status = IoRegisterPlugPlayNotification(EventCategoryDeviceInterfaceChange, 0, (PVOID) &UsbGuid, DriverObject, UsbDriverCallBackRoutine, DeviceObject, &ReturnNotificationValue);

	if (status != STATUS_SUCCESS)
	{
		KdPrint(("Error: Registering for Plug and play notification !!"));
		return status;
	}

	return STATUS_SUCCESS;
}

void DriverUnload(PDRIVER_OBJECT DriverObject) {
	AUTO_ENTER_LEAVE();
	
	UNICODE_STRING SymbolicLink;
	
	RtlInitUnicodeString(&SymbolicLink, L"\\??\\SoundSL");

	// Remove all pending Irps in the queue
	
	
	IoUnregisterPlugPlayNotification(ReturnNotificationValue);
	IoDeleteSymbolicLink(&SymbolicLink);
	IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS CreateCloseFunction(PDEVICE_OBJECT , PIRP Irp) {
	AUTO_ENTER_LEAVE();

	Irp->IoStatus.Information = 0;
	Irp->IoStatus.Status = STATUS_SUCCESS;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;

}

NTSTATUS UsbDriverCallBackRoutine(IN PVOID Notification, IN PVOID Context)
{
	AUTO_ENTER_LEAVE();

	//
	// Here I will remove the first Irp I meet in the queue and complete it.
	//

	// Chech about the Notification..
	PDEVICE_OBJECT devObj = (PDEVICE_OBJECT) Context;
	PDEVICE_EXTENSION lpDeviceExtension = (PDEVICE_EXTENSION) devObj->DeviceExtension;

	PDEVICE_INTERFACE_CHANGE_NOTIFICATION CurrentNotification = (PDEVICE_INTERFACE_CHANGE_NOTIFICATION)Notification;
	
	
	if ( IsEqualGUID(CurrentNotification->Event , GUID_DEVICE_INTERFACE_ARRIVAL) )
	{
		KdPrint(("MY_SOUND DRIVER: there was a device with symbolic Link (%ws) added", CurrentNotification->SymbolicLinkName->Buffer));
	}
	else if (IsEqualGUID(CurrentNotification->Event, GUID_DEVICE_INTERFACE_REMOVAL))
	{
		KdPrint(("MY_SOUND DRIVER: there was a device with symbolic Link (%ws) removed", CurrentNotification->SymbolicLinkName->Buffer));
	}
	
	//
	// Now the Queue work..
	// but first check if the queue is empty or not before pulling
	//
	
	if (IsListEmpty(&lpDeviceExtension->IrpQueueList))
	{
		KdPrint(("The Queue is empty, Can't dequeue any."));
		return STATUS_UNSUCCESSFUL;
	}

	PIRP Irp = IoCsqRemoveNextIrp(&lpDeviceExtension->CancelSafeQueue, nullptr);

	if (Irp == NULL)
	{ 
		KdPrint(("Error: Can't get the Irp from the Queue. Queue seems to be empty")); 
		return STATUS_UNSUCCESSFUL;
	}

	KdPrint(("SUCCESS: the Irp now pulled from the queue."));

	// Decrement the number of Irps.
	lpDeviceExtension->nuOfQueuedIrps--;
	KdPrint((" Number of Irps in the queue : %i.", lpDeviceExtension->nuOfQueuedIrps));


	//
	// Now complete the Irp
	//
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

NTSTATUS ControlCodeFunction(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	AUTO_ENTER_LEAVE();

	PDEVICE_EXTENSION lpDeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
	// here; i should push the irp into the queue and then just return (after ofcourse checking if the the comming 
	// IOCTL is what we need not any IOCTL. 

	// and in the UsbDriverCallBackRoutine(); I should pull up the last Irp and complete it.

	NTSTATUS status = STATUS_UNSUCCESSFUL;

	auto StackLoc = IoGetCurrentIrpStackLocation(Irp);

	switch (StackLoc->Parameters.DeviceIoControl.IoControlCode)
	{
	case IOCTL_MY_SOUND:
		KdPrint(("Catched a new incoming IRP. Queuing it."));
		KdPrint(("is the list is empty: %d", IsListEmpty(&lpDeviceExtension->IrpQueueList)));

		// push the Irp to the queue
		status = IoCsqInsertIrpEx(&lpDeviceExtension->CancelSafeQueue, Irp, nullptr, nullptr);

		if (!NT_SUCCESS(status))
		{
			KdPrint(("Error: can't push the Irp to the queue."));
			break;
		}
	
		// Everything is well..
		KdPrint(("SUCCESS: the Irp now pushed to the queue."));


		// Increment the number of Irps.
		lpDeviceExtension->nuOfQueuedIrps++;


		KdPrint((" Number of Irps in the queue : %i.", lpDeviceExtension->nuOfQueuedIrps));
		KdPrint(("is the list is empty: %d", IsListEmpty(&lpDeviceExtension->IrpQueueList)));

		status = STATUS_SUCCESS;

		break;
	}

	Irp->IoStatus.Status = STATUS_PENDING;

	status = STATUS_PENDING;

	return status;
}


NTSTATUS CsqInsertIrp(IN _IO_CSQ *Csq, IN PIRP Irp, IN PVOID InsertContext) {

	AUTO_ENTER_LEAVE();

	// Get the start of the DevExtension structure
	PDEVICE_EXTENSION lpDeviceExtension = CONTAINING_RECORD(Csq, DEVICE_EXTENSION, CancelSafeQueue);
	// 
	// this fucntion just insert the Irp to the doubly-linked list.
	// and we have the List Head as a global variable, so we don't need the 
	// CONTAINING_RECORD macro. we will add to queue using : InsertTailList  
	//

	// just to avoid the W4 error: unreferenced parameter
	UNREFERENCED_PARAMETER(InsertContext);


	// Insert that Irp to the tail, to act as a queue
	InsertTailList(&lpDeviceExtension->IrpQueueList, &Irp->Tail.Overlay.ListEntry);

	if (IsListEmpty(&lpDeviceExtension->IrpQueueList))
	{
		KdPrint(("FAIL: ADDING THE IRP TO THE QUEUE, list is empty in function %s", __FUNCTION__));
		return STATUS_UNSUCCESSFUL;
	}

	KdPrint(("SUCCESS: ADDING THE IRP TO THE QUEUE, in function %s", __FUNCTION__));

	return STATUS_SUCCESS;
}

void CsqRemoveIrp(IN PIO_CSQ Csq, IN PIRP Irp) {

	AUTO_ENTER_LEAVE();

	//
	// This function just removes the Irp from the queue
	// I won't use this function Directly (using the IoCsqRemoveIrp), because it need a specific Irp to be removed
	// So it won't be usefull for me. Instead i will use IoCsqRemoveNextIrp. 
	// And this function also calls the CsqRemoveIrp too (after PeekNextIrp).
	//

	UNREFERENCED_PARAMETER(Csq);

	// we will remove the Irp using the ListEntry member inside the structure
	RemoveEntryList(&Irp->Tail.Overlay.ListEntry);

}


PIRP CsqPeekNextIrp(IN PIO_CSQ Csq, IN PIRP Irp, IN PVOID PeekContext) {
	
	AUTO_ENTER_LEAVE();

	PDEVICE_EXTENSION lpDeviceExtension = CONTAINING_RECORD(Csq, DEVICE_EXTENSION, CancelSafeQueue);
	//
	// The idea here is to jsut get the first Irp I meet (only for my case).
	// So I won't use the PeekContext
	// 

	UNREFERENCED_PARAMETER(PeekContext);
	UNREFERENCED_PARAMETER(Irp);


	// chech first if the list is empty
	if (IsListEmpty(&lpDeviceExtension->IrpQueueList))
	{
		KdPrint(("ERROR: there's no pending Irps to be removed from the list, from function %s", __FUNCTION__));
		return NULL;
	}
	
	//
	// Now we know that there are IRPs in the list. Now we got two choices.
	// First if the framework passed the Irp as NULL. then we should get the FLINK 
	// to the LIST HEAD, and if it specified a valid Irp, we will get the FLINK to that
	// passed Irp
	//
	PLIST_ENTRY ListEntryIrp;
	
	if (Irp != NULL)
		ListEntryIrp = Irp->Tail.Overlay.ListEntry.Flink;
	else
		ListEntryIrp = lpDeviceExtension->IrpQueueList.Flink;
 
	//
	// Now we get the List entry for the wanted Irp. Let's first check if it's not 
	// equals to the List Head itself 
	//
	if (ListEntryIrp != &lpDeviceExtension->IrpQueueList)
	{
		// Get the start of the irp
		PIRP pIrp = CONTAINING_RECORD(ListEntryIrp, IRP, Tail.Overlay.ListEntry); 
		
		// There's no PeekContext Passed
		if (!PeekContext)
		{
			KdPrint(("PRAVO: We Found the Irp to Be dequeued!"));
			return pIrp;
		}
		
		else
			KdPrint(("WARNING: You specified a peekContext Value !!"));
	
	}

	//
	// The returned value from the above function is the ListEntry member from the Irp structure
	// Irp->Tail.Overlay.ListEntry. So now we need to get the start of the Irp Structure Itself using 
	// CONTAINING_RECORD() macro.
	// You can actually get the Irp with another way; by before removing the Irp, get the 
	// Head List then get the value of FLINK of it (pointer to the removed Irp)
	// then use the macro to get the actuall point of the star of the Irp
	//
	return NULL;
}

void CsqAcquireLock(IN PIO_CSQ Csq, OUT PKIRQL Irql) {
	
	AUTO_ENTER_LEAVE();

	//
	// I don't care now about the synchronization control, so I will just ignore this function
	//
	
	UNREFERENCED_PARAMETER(Csq);
	*Irql = KeGetCurrentIrql();

}

void CsqReleaseLock(IN PIO_CSQ Csq, IN KIRQL Irql) {

	AUTO_ENTER_LEAVE();

	// Same as above ...
	UNREFERENCED_PARAMETER(Csq);
	UNREFERENCED_PARAMETER(Irql);

}

void CsqCompleteCanceledIrp(IN PIO_CSQ Csq, IN PIRP Irp) {

	AUTO_ENTER_LEAVE();

	// Also do Nothing ...
	UNREFERENCED_PARAMETER(Csq);
	UNREFERENCED_PARAMETER(Irp);
} 