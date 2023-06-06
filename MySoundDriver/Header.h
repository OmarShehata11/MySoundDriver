#pragma once

#define IOCTL_MY_SOUND CTL_CODE(0XCE54, 0x911, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _DEVICE_EXTENSION {
	
	//
	// List for our queue of the Irp
	// it's the List entry point (Head)
	//
	LIST_ENTRY IrpQueueList;

	//
	// Structure for Cancel-safe queue to
	// specify the IRP queue routines
	//
	IO_CSQ CancelSafeQueue;
	
	//
	// Number of Irps in the queue in pending state.
	//
	unsigned int nuOfQueuedIrps = 0;

}DEVICE_EXTENSION, *PDEVICE_EXTENSION;