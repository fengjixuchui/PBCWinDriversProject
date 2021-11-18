#include <ntifs.h>
#include <ntddk.h>
#include "PBCMultiThreadSearch.h"

#define DEVICE_NAME L"\\Device\\PBCListProc"
#define SYMBOLICLINK_NAME L"\\DosDevices\\PBCListProc"
#define CTRL_BASE 0x8000
#define PBCCTRL_CODE(code) \
	CTL_CODE(FILE_DEVICE_UNKNOWN,CTRL_BASE + code,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define PBCCTRLIoControl PBCCTRL_CODE(0)
#define FILE_DEVICE_SWAP 0x0000800a
#define IOCTRL_PROC_KILL CTL_CODE(FILE_DEVICE_SWAP,0x8009,METHOD_BUFFERED,FILE_WRITE_ACCESS)
#define IOCTRL_TRANSFER_TYPE(code) (code & 0x03)

typedef struct _SYSTEMMODULEINFORMATION
{
	ULONG Reserved[2];
	PVOID Base;
	ULONG Size;
	ULONG Flags;
	USHORT Index;
	USHORT UnKnow;
	USHORT LoadCount;
	USHORT ModuleNameOffset;
	CHAR ImageName[256];

}SYSTEMMODULEINFORMATION,*PSYSTEMMODULEINFORMATION;

//����PspTermateProcess����ָ�����ͣ��������ձ���������Ľ��
typedef NTSTATUS (*PSPTERMINATEPROCESS)(PEPROCESS process,NTSTATUS ExitStatus);
PSPTERMINATEPROCESS PBCTerminateProc = NULL;

//����NtQuerySystemInFormation���ͺ���ָ��
//��Ϊ�ú�����SSDT���У�����ֱ����ϵͳ�оͿ��Բ�ѯ��
//����ú������ͣ�������Һ�ֱ��call
typedef NTSTATUS (*PBCQUERYSYSTEMINFORMATION)(__in ULONG SystemInformationClass,
								   __out PVOID SystemInformation,
								   __in ULONG SystemInformationLength,
								   __out ULONG returnLength);
PBCQUERYSYSTEMINFORMATION PBCQuerySystemInformation = NULL;
#define PBCSystemModuleInformation 11

VOID GetFuncAddr(VOID);
VOID GetPspTerMinateProcessFuncAddr(PVOID);
ULONG GetSystemFuncAddr(WCHAR *szFuncName);
typedef unsigned long DWORD;

VOID DriverUnload(PDRIVER_OBJECT pDriverObject)
{
	UNICODE_STRING uSymbolicLinkName = {0x00};

	DbgPrint("PBCListProc:goodbye!\n");

	RtlInitUnicodeString(&uSymbolicLinkName,SYMBOLICLINK_NAME);
	IoDeleteSymbolicLink(&uSymbolicLinkName);

	if(pDriverObject->DeviceObject)
	{
		IoDeleteDevice(pDriverObject->DeviceObject);
	}
	
	return;
}

NTSTATUS DispatchCommand(PDEVICE_OBJECT pDeviceObject,PIRP pIrp)
{
	pIrp->IoStatus.Status = 0;
	pIrp->IoStatus.Information = STATUS_SUCCESS;
	
	IoCompleteRequest(pIrp,IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

NTSTATUS DispatchIoControl(PDEVICE_OBJECT pDeviceObject,PIRP pIrp)
{
	PIO_STACK_LOCATION pIrpStack = NULL;
	PVOID pInputBuffer = NULL;
	PVOID pOutputBuffer = NULL;
	ULONG uLcontrolCode;
	HANDLE hProcessHandle = NULL;
	ULONG uLprocessId;
	NTSTATUS ntStatus = STATUS_SUCCESS;
	PEPROCESS pEprocess = NULL;

	pOutputBuffer = pInputBuffer = pIrp->AssociatedIrp.SystemBuffer;

	pIrpStack = IoGetCurrentIrpStackLocation(pIrp);
	uLcontrolCode = pIrpStack->Parameters.DeviceIoControl.IoControlCode;

	switch(pIrpStack->MajorFunction)
	{
		case IRP_MJ_CREATE:
		case IRP_MJ_SHUTDOWN:
		case IRP_MJ_CLOSE:
			break;
		case IRP_MJ_DEVICE_CONTROL:
		{
			if(IOCTRL_TRANSFER_TYPE(uLcontrolCode) == METHOD_NEITHER)
			{
				pOutputBuffer = pInputBuffer = pIrp->UserBuffer;
			}

			switch(uLcontrolCode)
			{
				case IOCTRL_PROC_KILL:
				{
					if(PBCTerminateProc == NULL)
					{
						*(DWORD *)pInputBuffer = -1;
						pIrp->IoStatus.Information = sizeof(DWORD);
					}
					else
					{
						uLprocessId = *(ULONG *)pOutputBuffer;
						ntStatus = PsLookupProcessByProcessId((HANDLE)uLprocessId,&pEprocess);
						if(!NT_SUCCESS(ntStatus))
						{
							DbgPrint("PsLookupProcessByProcessId faild:pid = %8x,status = %x\n",uLprocessId,ntStatus);
							*((DWORD *)pOutputBuffer) = 1;
							pIrp->IoStatus.Information = sizeof(DWORD);
							break;
						}

						ntStatus = PBCTerminateProc(pEprocess,0);
						if(!NT_SUCCESS(ntStatus))
						{
							DbgPrint("PBCTerminateProcess faild:pid = %8x,status = %x\n",uLprocessId,ntStatus);
							*(DWORD *)pOutputBuffer = 2;
							pIrp->IoStatus.Information = sizeof(DWORD);
							break;
						}

						*((DWORD *)pOutputBuffer) = 0;
						pIrp->IoStatus.Information = sizeof(DWORD);
						pIrp->IoStatus.Status = ntStatus;
						DbgPrint("TerminateProcess successful:%8x\n",uLprocessId);
					}
					
					
				}
				break;
				default:
				{
					
				}
				break;
			}

		}
	}

	IoCompleteRequest(pIrp,IO_NO_INCREMENT);

	return ntStatus;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject,PUNICODE_STRING pDriverPath)
{
	UNICODE_STRING uDeviceName = {0x00};
	UNICODE_STRING uSymbolicLinkName = {0x00};
	NTSTATUS ntStatus = STATUS_SUCCESS;
	PDEVICE_OBJECT pDeviceObject = NULL;
	ULONG index;
	ULONG SwapContextAddr;

	RtlInitUnicodeString(&uDeviceName,DEVICE_NAME);
	RtlInitUnicodeString(&uSymbolicLinkName,SYMBOLICLINK_NAME);
	
	ntStatus = IoCreateDevice(pDriverObject,
		0,
		&uDeviceName,
		FILE_DEVICE_UNKNOWN,
		0,
		FALSE,
		&pDeviceObject);
	if(!NT_SUCCESS(ntStatus))
	{
		DbgPrint("IoCreateDevice faild:%x\n",ntStatus);
		return ntStatus;
	}

	ntStatus = IoCreateSymbolicLink(&uSymbolicLinkName,&uDeviceName);
	if(!NT_SUCCESS(ntStatus))
	{
		DbgPrint("IoCreateSymbolicLink faild:%x\n",ntStatus);
		IoDeleteDevice(pDeviceObject);
		return ntStatus;
	}

	for(index = 0; index < IRP_MJ_MAXIMUM_FUNCTION; index++)
	{
		pDriverObject->MajorFunction[index] = DispatchCommand;
	}

	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchIoControl;
	pDriverObject->DriverUnload = DriverUnload;

	GetFuncAddr();

	DbgPrint("PBCTerminateProc FuncAddr is %x\n",(ULONG)PBCTerminateProc);
	
	SwapContextAddr = PBCMultiThreadSearchFunAddr(L"SwapContext");

	//DbgPrint("SwapContextAddr is %x\n",SwapContextAddr);

	return ntStatus;

}

VOID GetFuncAddr(VOID)
{
	HANDLE thread1 = NULL;
	PVOID waitObjs = NULL;
	NTSTATUS ntStatus = STATUS_SUCCESS;

	ntStatus = PsCreateSystemThread(&thread1,
		0,
		NULL,
		(HANDLE)0,
		NULL,
		GetPspTerMinateProcessFuncAddr,
		NULL);
	if(!NT_SUCCESS(ntStatus))
	{
		DbgPrint("PsCreateSystemThread faild:%x",ntStatus);
		return;
	}

	if(KeGetCurrentIrql() > PASSIVE_LEVEL)
	{
		ntStatus = KfRaiseIrql(PASSIVE_LEVEL);
	}
	if(KeGetCurrentIrql() != PASSIVE_LEVEL)
	{
		DbgPrint("KfRaiseIrql faild:%x\n",ntStatus);
		return;
	}

	ntStatus = ObReferenceObjectByHandle(thread1,
		THREAD_ALL_ACCESS,
		NULL,
		KernelMode,
		&waitObjs,
		NULL);

	if(!NT_SUCCESS(ntStatus))
	{
		DbgPrint("ObReferenceObjectByHandle faild:%x\n",ntStatus);
		return;
	}
	//���һ����������0��ʾ���ȴ���NULL��ʾ���޵ȴ�
	//KeWaitForMutexObject(waitObjs,Executive,KernelMode,FALSE,NULL);
	KeWaitForSingleObject(waitObjs,
		Executive,
		KernelMode,
		FALSE,
		NULL);

	return;

}

VOID GetPspTerMinateProcessFuncAddr(PVOID pContext)
{
	ULONG code1 = 0x8b55ff8b;
	ULONG code2 = 0xa16456ec;
	ULONG code3 = 0x00000124;
	ULONG code4 = 0x3b08758b;
	NTSTATUS ntStatus = STATUS_SUCCESS;
	PVOID buffer = NULL;
	ULONG bufferSize;
	ULONG KernelBase;
	ULONG KernelSize;
	PSYSTEMMODULEINFORMATION pSystemModuleInfo = NULL;
	ULONG index;

	PBCQuerySystemInformation = (PBCQUERYSYSTEMINFORMATION)GetSystemFuncAddr(L"NtQuerySystemInformation");
	if(!PBCQuerySystemInformation)
	{
		DbgPrint("GetSystemFuncAddr faild\n");
		return;
	}

	ntStatus = (NTSTATUS)PBCQuerySystemInformation(PBCSystemModuleInformation,buffer,0,&bufferSize);

	buffer =  ExAllocatePoolWithTag(PagedPool,bufferSize,'FFUB');
	if(!buffer)
	{
		DbgPrint("alloc memory faild\n");
		return;
	}

	ntStatus = (NTSTATUS)PBCQuerySystemInformation(PBCSystemModuleInformation,buffer,bufferSize,&bufferSize);
	if(!NT_SUCCESS(ntStatus))
	{
		DbgPrint("PBCQuerySystemInformation2 faild:%x\n",ntStatus);
		ExFreePool(buffer);
		return;
	}
	
	pSystemModuleInfo = (PSYSTEMMODULEINFORMATION)((PULONG)buffer + 1);
	KernelBase = (ULONG)pSystemModuleInfo->Base;
	KernelSize = (ULONG)pSystemModuleInfo->Base + pSystemModuleInfo->Size;
	ExFreePool(buffer);
	buffer = NULL;
	for(index = KernelBase; index < KernelSize; index++)
	{
		if(*((ULONG *)(index + 0x00)) == code1)
		{
			if(*((ULONG *)(index +0x04)) == code2)
			{
				if(*((ULONG *)(index + 0x08)) == code3)
				{
					if(*((ULONG *)(index + 0x0c)) == code4)
					{
						PBCTerminateProc = (PSPTERMINATEPROCESS)index;
					}
				}
			}
		}
	}

	PsTerminateSystemThread(STATUS_SUCCESS);

}

ULONG GetSystemFuncAddr(WCHAR *szFuncName)
{
	UNICODE_STRING uFuncName = {0x00};
	
	RtlInitUnicodeString(&uFuncName,szFuncName);

	return (ULONG)MmGetSystemRoutineAddress(&uFuncName);

}