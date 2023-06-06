#include <iostream>
#include <Windows.h>
#include "../MySoundDriver/Header.h"
#include <mmsystem.h>

// To disable the CRT warnings from the compiler.
#define _CRT_SECURE_NO_WARNINGS  

unsigned short numOfPendingIrps = 0;

DWORD WINAPI ThreadStartRoutine(HANDLE hCompletionPort);

int main()
{
	OVERLAPPED overLappedStructure = { 0 };
	OVERLAPPED *lpOverLappedStructure = nullptr;
	DWORD threadId;
	int choice = 0, errorCode = 0;
	HANDLE hDevice, hCompletionPort, hThread;
	BOOL status;
	DWORD     NumberOfBytesTransferred = 0;
	ULONG_PTR   CompletionKey = 0;
	
	hDevice = CreateFileA("\\\\.\\SoundSL", GENERIC_ALL, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	
	if (hDevice == INVALID_HANDLE_VALUE)
	{
		std::cout << "Error: while getting a handle to the driver, error code : " << GetLastError() << std::endl;
		exit(1);
	}

	hCompletionPort = CreateIoCompletionPort(hDevice, nullptr, 0, 0);

	if (hCompletionPort == INVALID_HANDLE_VALUE)
	{
		std::cout << "Error: while getting a handle to the completion port, error code : " << GetLastError() << std::endl;
		exit(1);
	}

	hThread = CreateThread(nullptr, 0, ThreadStartRoutine, hCompletionPort, 0, &threadId);

	
	while (TRUE) {
		 
		std::cout << "choose:\n\t1) send an Irp.\n\t2) Exit." << std::endl;

		std::cin >> choice;

		switch (choice)
		{
			case 1:
				DeviceIoControl(hDevice, IOCTL_MY_SOUND, nullptr, 0, nullptr, 0, nullptr, &overLappedStructure);
				
				errorCode = GetLastError();


				if (errorCode != ERROR_IO_PENDING)
				{
					std::cout << "Error: the irp is not in the pending state. Error code " << errorCode << std::endl;
					exit(1);
				}
				
				std::cout << "Success: the Irp in the pending state\n";

				break;

			case 2: 
				std::cout << "Leaving. Have a nice day." << std::endl;
				return 0;
			
			default:
				std::cout << "ERROR: choice a valid choic" << std::endl;
				break;
		
		}

	}
	/*
	while(1){ // Max number of queued irps = 5
		while (numOfPendingIrps < 5) {

			// Send an irp
			DeviceIoControl(hDevice, IOCTL_MY_SOUND, nullptr, 0, nullptr, 0, nullptr, &overLappedStructure);

			errorCode = GetLastError();

			// Check if it's in the pending state or not
			if (errorCode != ERROR_IO_PENDING)
			{
				std::cout << "Error: the irp is not in the pending state. Error code " << errorCode << std::endl;
				return 0;
			}

			std::cout << "Success: the Irp in the pending state\n";

			numOfPendingIrps++;
			std::cout << "number of irps that was sent = " << numOfPendingIrps << std::endl;
		}
		// test if one of the queued IOCP packet is completed or not. If not then wait.

		status = GetQueuedCompletionStatus(hCompletionPort, &NumberOfBytesTransferred, &CompletionKey, &lpOverLappedStructure, INFINITE);
		
		// Catched an IOCP packet
		if (status)
			numOfPendingIrps--;
	
		std::cout << "number of irps that was sent = " << numOfPendingIrps << std::endl;
	}
		*/
	return 0;
}

DWORD WINAPI ThreadStartRoutine(HANDLE hCompletionPort) {
	bool success;
	OVERLAPPED* overLappedStructure = nullptr;
	DWORD     NumberOfBytesTransferred = 0;
	ULONG_PTR   CompletionKey = 0;


	while(1){
		
		success = GetQueuedCompletionStatus(hCompletionPort, &NumberOfBytesTransferred, &CompletionKey, &overLappedStructure, INFINITE);
	
		if (!success)
		{
			std::cout << "error: while dequeue the pending IOCP packet. with error code: " << GetLastError() << std::endl;
			continue;
		}
	
		std::cout << "now dequeued the pending IOCP packet" << std::endl;
		bool succ = PlaySound( L"D:\\MySoundDriver\\MySound_UserMode\\slack.wav", NULL, SND_FILENAME | SND_ASYNC);
	
	}
	return 0;

}
