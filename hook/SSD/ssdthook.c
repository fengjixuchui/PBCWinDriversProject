#include "ssdthook.h"
#include <ntddkbd.h>

#define DeviceName L"\\device\\PBCSsdtHook"
#define SymbolcLinkName L"\\dosDevices\\PBCSsdtHook"

#pragma pack(1)
/*
typedef struct ServiceDescriptorEntry {
	unsigned int *ServiceTableBase;
	unsigned int *ServiceCounterTableBase; //Used only in checked build
	unsigned int NumberOfServices;
	unsigned char *ParamTableBase;
} ServiceDescriptorTableEntry_t, *PServiceDescriptorTableEntry_t;
#pragma pack()

__declspec(dllimport)  ServiceDescriptorTableEntry_t KeServiceDescriptorTable;

#define SYSTEMSERVICE(_function) KeServiceDescriptorTable.ServiceTableBase[*(PULONG)((PUCHAR)_function + 1)]
#define SDT SYSTEMSERVICE
#define KSDT KeServiceDescriptorTable
*/

#pragma pack(1)
typedef struct ServiceDescriptorEntry
{
	unsigned int *ServiceTableBase;
	unsigned int *ServiceCounterTableBase;
	unsigned int NumberOfService;
	unsigned char *ParamTableBase;
}ServiceDescriptorTableEntry_t,*PServiceDescriptorTableEntry_t;
#pragma pack()

__declspec(dllimport) ServiceDescriptorTableEntry_t KeServiceDescriptorTable;

#define SYSTEMSERVICE(_function) KeServiceDescriptorTable.ServiceTableBase[*(PULONG)((PUCHAR)_function + 1)]
#define SDT SYSTEMSERVICE
#define KSDT KeServiceDescriptorTable


//����Ŀ�꺯���ṹ
NTKERNELAPI NTSTATUS ZwLoadDriver(IN UNICODE_STRING *DriverServiceName);
NTKERNELAPI NTSTATUS ZwTerminateProcess(IN HANDLE ProcessHandle OPTIONAL ,NTSTATUS ExitStatus);

//�����Լ���hook����
NTSTATUS PBC_ZwLoadDriver(IN UNICODE_STRING *DriverServiceName);
NTSTATUS PBC_ZwTerminateProcess(IN HANDLE ProcessHandle OPTIONAL,NTSTATUS ExitStatus);
NTSTATUS PBC_ZwEnumerateKey(HANDLE KeyHandle, ULONG index, KEY_INFORMATION_CLASS KeyInformationClass, PVOID KeyInformation, ULONG Length, PULONG ResultLength);

//���庯��ָ�룬��������ԭ������ַ
typedef NTSTATUS (*ZWLOADDRIVER)(__in UNICODE_STRING *DriverServiceName);

typedef NTSTATUS (*ZWTERMINATEPROCESS)(IN HANDLE ProcessHandle OPTIONAL,IN NTSTATUS ExitStatus);

typedef NTSYSAPI NTSTATUS(*ZWENUMERATEKEY)(HANDLE KeyHandle, ULONG index, KEY_INFORMATION_CLASS KeyInformationClass, PVOID KeyInformation, ULONG Length, PULONG ResultLength);

static ZWLOADDRIVER OldZwLoadDriver;
static ZWTERMINATEPROCESS OldZwTerminateProcess;
static ZWENUMERATEKEY OldZwEnumerateKey;

void PBCStartHook(void)
{
	//�ر��ں�д����
	__asm
	{
			pushad
			mov eax,CR0
			and eax,0xfffeffff
			mov CR0,eax
			popad
	}

	DbgPrint("write address is 0x%x\n",(ULONG)&SDT(ZwLoadDriver));
	OldZwLoadDriver = (ZWLOADDRIVER)InterlockedExchange((PLONG)&SDT(ZwLoadDriver),(ULONG)PBC_ZwLoadDriver);
	OldZwTerminateProcess = (ZWTERMINATEPROCESS)InterlockedExchange((PULONG)&SDT(ZwTerminateProcess)
		,(ULONG)PBC_ZwTerminateProcess);
	OldZwEnumerateKey = (ZWENUMERATEKEY)InterlockedExchange((PULONG)&SDT(ZwEnumerateKey), (ULONG)PBC_ZwEnumerateKey);
	

	//���¿����ں�д����
	__asm
	{
		pushad
		mov eax,CR0
		or eax, NOT 0xfffeffff
		mov CR0,eax
		popad

	}

	return;
}

void RemoveHook(void)
{
	__asm
	{
		pushad
		mov eax,CR0
		and eax,0xfffeffff
		mov CR0,eax
		popad
	}

	InterlockedExchange((PULONG)&SDT(ZwLoadDriver),(ULONG)OldZwLoadDriver);
	InterlockedExchange((PULONG)&SDT(ZwTerminateProcess),(ULONG)OldZwTerminateProcess);
	InterlockedExchange((ULONG)&SDT(ZwEnumerateKey), (ULONG)OldZwEnumerateKey);
	__asm
	{
		pushad
		mov eax,CR0
		or eax,NOT 0xfffeffff
		mov CR0,eax
		popad
	}

}

VOID DriverUnload(DRIVER_OBJECT *pDriObj)
{
	NTSTATUS ntStatus = STATUS_SUCCESS;
	UNICODE_STRING uSymbolickName = {0x00};
	RtlInitUnicodeString(&uSymbolickName,SymbolcLinkName);

	RemoveHook();

	ntStatus = IoDeleteSymbolicLink(&uSymbolickName);
	if (!NT_SUCCESS(ntStatus))
	{
		DbgPrint("IoDeleteSymbolicLink faild:%d!\n",ntStatus);
	}

	IoDeleteDevice(pDriObj->DeviceObject);
	

	DbgPrint("good PBCHook!\n");

}

NTSTATUS DispatchCreate(PDEVICE_OBJECT pDevObj, PIRP pirp)
{
	KIRQL oldIrql;
	NTSTATUS ntStatus = STATUS_SUCCESS;
	if (KeGetCurrentIrql() <= DISPATCH_LEVEL)
	{
		KeRaiseIrql(DISPATCH_LEVEL,&oldIrql);
	}
	
	pirp->IoStatus.Status = ntStatus;
	pirp->IoStatus.Information = 0;

	IoCompleteRequest(pirp,IO_NO_INCREMENT);

	KeLowerIrql(oldIrql);
	return ntStatus;
}

NTSTATUS DispathIoControl(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
	NTSTATUS ioStatus = STATUS_SUCCESS;
	pIrp->IoStatus.Status = ioStatus;
	pIrp->IoStatus.Information = 0;

	DbgPrint("Receive R3 DeviceIoControl request!\n");
	IoCompleteRequest(pIrp,IO_NO_INCREMENT);

	return ioStatus;
	
}

NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriObj,IN PUNICODE_STRING pDriPath)
{
	NTSTATUS ntStatus = STATUS_SUCCESS;
	PDEVICE_OBJECT pDeviceObj = NULL;
	UNICODE_STRING uDeviceName = { 0x00 };
	UNICODE_STRING uSymName = {0x00};
	RtlInitUnicodeString(&uDeviceName,DeviceName);
	ntStatus = IoCreateDevice(pDriObj
	,NULL
	,&uDeviceName
	,FILE_DEVICE_UNKNOWN
	,0
	,FALSE
	,&pDeviceObj);
	if (!NT_SUCCESS(ntStatus))
	{
		DbgPrint("IoCreateDevice faild!\n");
		return ntStatus;
	}

	RtlInitUnicodeString(&uSymName,SymbolcLinkName);
	ntStatus = IoCreateSymbolicLink(&uSymName,&uDeviceName);
	if (!NT_SUCCESS(ntStatus))
	{
		DbgPrint("IoCreateSymbolicLink faild!\n");
		return ntStatus;
	}

	pDeviceObj->Flags |= DO_BUFFERED_IO;
	pDriObj->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispathIoControl;
	pDriObj->MajorFunction[IRP_MJ_CREATE] = DispatchCreate;
	DbgPrint("start PBCHook!\n");
	
	pDriObj->DriverUnload = DriverUnload;

	PBCStartHook();

	return ntStatus;
}

NTSTATUS PBC_ZwEnumerateKey(HANDLE KeyHandle
	, ULONG index
	, KEY_INFORMATION_CLASS KeyInformationClass
	, PVOID KeyInformation
	, ULONG Length
	, PULONG ResultLength)
{
	//DbgPrint("find EnumerateKey Process id is %d\n",PsGetCurrentProcessId());
	return OldZwEnumerateKey(KeyHandle
		, index
		, KeyInformationClass
		, KeyInformation
		, Length
		, ResultLength);
}

NTSTATUS PBC_ZwLoadDriver(IN UNICODE_STRING *DriverServiceName)
{
	DbgPrint("Hook loading Driver operation!\n");
	return OldZwLoadDriver(DriverServiceName);
}

NTSTATUS PBC_ZwTerminateProcess(IN HANDLE ProcessHandle OPTIONAL
								,IN NTSTATUS ExitStatus)
{
	return OldZwTerminateProcess(ProcessHandle,ExitStatus);
}