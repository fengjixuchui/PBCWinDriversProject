#include <ntifs.h>
#include <ntddk.h>
#include <ntdef.h>
#include <wdm.h>
#include <windef.h>
#include <ntstrsafe.h>

#define IOCTRL_SEND_RESULT_TO_R0 CTL_CODE(FILE_DEVICE_UNKNOWN,0x8001,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTRL_XXX_ATTACK_CODE CTL_CODE(FILE_DEVICE_UNKNOWN,0x8002,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define DeviceName L"\\device\\PBCPopWindow"
#define DeviceSymbolicLink L"\\dosDevices\\PBCPopWindow"

LIST_ENTRY g_OperListEntry;
ERESOURCE g_OperListResourceLock;

LIST_ENTRY g_WaitListEntry;
ERESOURCE g_WaitListResourceLock;

LIST_ENTRY g_PendingIrpListEntry;
ERESOURCE g_PendingIrpListResourceLock;

//����һ��ȫ�ֱ�����������waitId
ULONG gCurrentWaitIdIndex = 0;

typedef struct _OperList
{

	WCHAR processName[MAX_PATH];
	long processId;
	long waitId;
	LIST_ENTRY listEntry;

}OperList,*POperList;

typedef struct _OperListR3
{
	WCHAR processName[MAX_PATH];
	ULONG processId;
	ULONG waitId;
	
}OperListR3,POperListR3;

typedef struct _WaitList
{
	LIST_ENTRY listEntry;
	ULONG waitId;
	KEVENT waitEvent;
	ULONG block;

}WaitList,*PWaitList;

typedef struct _R3_REPLY
{
	ULONG WaitId;
	ULONG Block;
}R3_REPLY,*PR3_REPLY;

typedef enum _R3_RESULT
{
	R3Result_Pass,
	R3Result_Block,
	R3Result_DefaultNon,
}R3_RESULT;

	//����δ�������ں˺���
extern NTSTATUS ZwQueryInformationProcess(HANDLE ProcessHandle,
										   PROCESSINFOCLASS ProcessInfoClass,
										   PVOID ProcessInformation,
										   ULONG ProcessInformationLength,
										   PULONG ReturnLength);

//д��
VOID LockWrite(ERESOURCE *pLock)
{
	//�����ٽ���
	KeEnterCriticalRegion();
	//��ʼ����
	ExAcquireResourceExclusive(pLock,TRUE);
}

//��д��
VOID UnLockWrite(ERESOURCE *pLock)
{
	//�Ƚ���
	ExReleaseResourceLite(pLock);
	//�ڳ��ٽ���
	KeLeaveCriticalRegion();
}

//����
VOID LockRead(ERESOURCE *pLock)
{
	//�����Ƚ����ٽ���
	KeEnterCriticalRegion();
	//Ȼ����
	ExAcquireResourceShared(pLock,TRUE);
}

//�����
VOID UnlockRead(ERESOURCE *pLock)
{
	ExReleaseResourceLite(pLock);
	KeLeaveCriticalRegion();
}

//����ʱ�����ö����̷߳���
VOID LockWriteStarveWrite(ERESOURCE *pLock)
{
	KeEnterCriticalRegion();
	ExAcquireSharedStarveExclusive(pLock,TRUE);
}

//��ʼ��������
VOID __stdcall initializeLock(ERESOURCE *pLock)
{
	ExInitializeResourceLite(pLock);
}

//�ͷ�������
VOID __stdcall freeLock(ERESOURCE *pLock)
{
	ExDeleteResourceLite(pLock);
}

//��ʼ������
VOID initializeList(LIST_ENTRY *pListEntry)
{
	InitializeListHead(pListEntry);
}

ULONG MakeWaitId()
{
	InterlockedIncrement(&gCurrentWaitIdIndex);
	return gCurrentWaitIdIndex;
}


BOOLEAN CompletePendingIrp(LIST_ENTRY *PendingListHeader,OperList *operList)
{
	LIST_ENTRY *CurrentEntry = NULL;
	PIRP pPendingIrp = NULL;
	BOOLEAN bFindIrp = FALSE;
	
	for(CurrentEntry = PendingListHeader->Flink; CurrentEntry != PendingListHeader; CurrentEntry = CurrentEntry->Flink)
	{
		pPendingIrp = CONTAINING_RECORD(CurrentEntry,IRP,Tail.Overlay.ListEntry);
		if(IoSetCancelRoutine(pPendingIrp,NULL))
		{
			RemoveEntryList(PendingListHeader);
			bFindIrp = TRUE;
			break;
		}
	}

	if(!bFindIrp)
		goto ret;

	RtlCopyMemory(pPendingIrp->AssociatedIrp.SystemBuffer,operList,sizeof(OperListR3));
	pPendingIrp->IoStatus.Information = sizeof(OperListR3);
	pPendingIrp->IoStatus.Status = 0;
	IoCompleteRequest(pPendingIrp,IO_NO_INCREMENT);
ret:
	return bFindIrp;
	
}

NTSTATUS GetProcessNameByPid(__in ULONG_PTR uPid,__out UNICODE_STRING *fullPath)
{
	NTSTATUS ntStatus = STATUS_SUCCESS;
	NTSTATUS dosStatus = STATUS_SUCCESS;
	PEPROCESS pEprocessEprocess = NULL;
	HANDLE hProcessHandle = NULL;
	FILE_OBJECT *fProcessFileObject = NULL;
	//UNICODE_STRING uProcessName = {0x00};
	IO_STATUS_BLOCK ioBlock = {0x00};
	ULONG uProcessNameRealLen;
	VOID *uProcessNameBuffer = NULL;
	KAPC_STATE pRkapcState = {0x00};
	OBJECT_ATTRIBUTES oProcessObject = {0x00};
	UNICODE_STRING uProcessDosName = {0x00};
	DECLARE_UNICODE_STRING_SIZE(uProcessNameStr,MAX_PATH);
	WCHAR FileBuffer[MAX_PATH] = {0x00};
	FILE_NAME_INFORMATION *fileName = NULL;
	WCHAR fileBuffer[MAX_PATH] = {0x00};

	PAGED_CODE();

	//��ȡ���̵�Eprocess�ṹ
	ntStatus = PsLookupProcessByProcessId(uPid,&pEprocessEprocess);
	if(!NT_SUCCESS(ntStatus))
	{
		DbgPrint("PsLookupProcessByProcessId faild\n");
		return ntStatus;
	}

	__try
	{
		KeStackAttachProcess(pEprocessEprocess,&pRkapcState);

		//��һ�λ�ȡ������ʵ�ʳ���
		ntStatus = ZwQueryInformationProcess(NtCurrentProcess()
			,ProcessImageFileName
			,NULL
			,NULL
			,&uProcessNameRealLen);
		if(STATUS_INFO_LENGTH_MISMATCH != ntStatus)
		{
			DbgPrint("ZwQueryInformationProcess with name len faild\n");
			ntStatus = STATUS_MEMORY_NOT_ALLOCATED;
			__leave;
		}

		uProcessNameBuffer = ExAllocatePoolWithTag(NonPagedPool,uProcessNameRealLen,'PTEG');
		if(!uProcessNameBuffer)
		{
			DbgPrint("ExallocatePoolWithTag to uProcessNameBuffer faild\n");
			return STATUS_MEMORY_NOT_ALLOCATED;
		}

		//�ڶ��λ�ȡ������
		ntStatus = ZwQueryInformationProcess(NtCurrentProcess()
			,ProcessImageFileName
			,uProcessNameBuffer
			,uProcessNameRealLen
			,NULL);
		if(NT_ERROR(ntStatus))
		{
			DbgPrint("ZwQueryInformationProcess to name buffer faild\n");
			__leave;
		}

		RtlCopyUnicodeString(&uProcessNameStr,(UNICODE_STRING *)uProcessNameBuffer);
		InitializeObjectAttributes(&oProcessObject
			,&uProcessNameStr
			,OBJ_CASE_INSENSITIVE,
			NULL
			,NULL);
		DbgPrint("uProcessName is %wZ\n",&uProcessNameStr);

		//���ţ��򿪽��̣���ȡ���̾��
		ntStatus = ZwCreateFile(&hProcessHandle
			,FILE_READ_ATTRIBUTES
			,&oProcessObject
			,&ioBlock
			,NULL
			,FILE_ATTRIBUTE_NORMAL
			,0
			,FILE_OPEN
			,FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE
			,NULL
			,0);
		if(NT_ERROR(ntStatus))
		{
			DbgPrint("ZwCreateFile faild:0x%x\n",ntStatus);
			ExFreePool(uProcessNameBuffer);
			uProcessNameBuffer = NULL;
			__leave;
		}

		//��ȡ�ɹ������Ż�ȡ���ļ����ļ��ں˶���,�ú�����Ը��ļ��ں˶�������ü�����1
		ntStatus = ObReferenceObjectByHandle(hProcessHandle
			,NULL
			,*IoFileObjectType
			,KernelMode
			,(PVOID)&fProcessFileObject
			,NULL);
		if(!NT_SUCCESS(ntStatus))
		{
			DbgPrint("ObReferenceObjectByHandle faild\n");
			__leave;
		}

		if(fProcessFileObject->DeviceObject == NULL)
		{
			DbgPrint("fileObject->DeviceObject is NULL\n");
			__leave;
		}

		fileName = (FILE_NAME_INFORMATION *)fileBuffer;
		ntStatus = ZwQueryInformationFile(hProcessHandle
			,&ioBlock
			,fileName
			,sizeof(WCHAR) * MAX_PATH
			,FileNameInformation);
		if(!NT_SUCCESS(ntStatus))
		{
			DbgPrint("ZwQueryInformationFile faild!:%d\n",ntStatus);
			__leave;
		}


		dosStatus = RtlVolumeDeviceToDosName(fProcessFileObject->DeviceObject,&uProcessDosName);
		if(!NT_SUCCESS(ntStatus))
		{
			DbgPrint("RtlVolumeDeviceToDosName faild\n");
			__leave;
		}

	}
	
	__finally
	{
		if(hProcessHandle)
		{
			ZwClose(hProcessHandle);
		}
		if(fProcessFileObject)
		{
			ObDereferenceObject(fProcessFileObject);
		}
	}

	if(!NT_SUCCESS(ntStatus))
		return ntStatus;
	
	RtlInitUnicodeString(&uProcessNameStr,fileName->FileName);
	if(NT_SUCCESS(dosStatus))
	{
		RtlCopyUnicodeString(fullPath,&uProcessDosName);
		RtlUnicodeStringCat(fullPath,&uProcessNameStr);
	}
	else
	{
		RtlCopyUnicodeString(fullPath,&uProcessNameStr);
	}

	return ntStatus;
}

WaitList *findWaitEntryByWaitId(LIST_ENTRY *waitListEntryHeader,ULONG waitId)
{
	WaitList *targetWaitList = NULL;
	LIST_ENTRY *waitEntry = NULL;

	for(waitEntry = waitListEntryHeader->Flink; waitEntry != waitListEntryHeader;waitEntry = waitEntry->Flink)
	{
		targetWaitList = CONTAINING_RECORD(waitEntry,WaitList,listEntry);
		if(targetWaitList->waitId == waitId)
		{
			return targetWaitList;
		}
	}

	return NULL;

}


/*
R3_RESULT _GetResultFromUser(VOID)
{
	OperList *pOper = NULL;
	WaitList *pWait = NULL;
	NTSTATUS ioStatus = STATUS_SUCCESS;
	ULONG_PTR uPid = 0;
	UNICODE_STRING unProcFullPath = {0x00};
	R3_RESULT result = R3Result_Block;
	BOOL bGetPendingIrp;
	LARGE_INTEGER lrTimeOut = {0x00};

	pOper = ExAllocatePool(PASSIVE_LEVEL,sizeof(OperList));
	if(pOper == NULL)
	{
		DbgPrint("ExallocatePoolWithTag error\n");
		return R3Result_Block;
	}

	uPid = PsGetCurrentProcessId();

	pOper->processId = uPid;
	pOper->waitId = MakeWaitId();

	pWait = (WaitList *)ExAllocatePoolWithTag(PASSIVE_LEVEL,sizeof(WaitList),'tiaw');
	if(pWait == NULL)
	{
		DbgPrint("ExAllocatePoolWithTag with wailtList faild!\n");
		goto ret;
	}

	pWait->waitId = pOper->waitId;
	KeInitializeEvent(&pWait->waitEvent,SynchronizationEvent,FALSE);

	unProcFullPath.Length = unProcFullPath.MaximumLength = MAX_PATH;
	unProcFullPath.Buffer = ExAllocatePoolWithTag(PASSIVE_LEVEL,unProcFullPath.MaximumLength,'corp');
	if(unProcFullPath.Buffer == NULL)
	{
		DbgPrint("ExAllocatePoolWithTag faild!\n");
	}

	ioStatus = GetProcessNameByPid(uPid,&unProcFullPath);
	if(!NT_SUCCESS(ioStatus))
	{
		goto ret;
	}
	
	RtlCopyMemory(pOper->processName,unProcFullPath.Buffer,MAX_PATH);

	ExFreePool(unProcFullPath.Buffer);
	
	LockWrite(&g_PendingIrpListResourceLock);
	//ȥpending��ipr������ҵ�����ֱ���ں����ڲ�����oper�ṹ���ݷ��ظ������pendingirp��systembuffer������������irp
	bGetPendingIrp = CompletePendingIrp(&g_PendingIrpListEntry,pOper);
	UnLockWrite(&g_PendingIrpListResourceLock);

	if(!bGetPendingIrp)
	{
		//���û�ҵ���Ҫ�����oper���뵽operlist��
		LockWrite(&g_OperListResourceLock);

		InsertTailList(&g_OperListEntry,&pOper->listEntry);

		UnLockWrite(&g_OperListResourceLock);

		ExFreePool(pOper);
	}

	lrTimeOut.QuadPart = -40 * 10000000;

	//��������������ȡûȡ��pending��Irp�����ȴ�40��
	ioStatus = KeWaitForSingleObject(&pWait->waitEvent
		,Executive
		,KernelMode
		,FALSE
		,&lrTimeOut);

	//�ȴ�����
	if(ioStatus != STATUS_TIMEOUT)
	{
		if(pWait->block)
		{
			result = R3Result_Pass;
		}
		else
		{
			result = R3Result_Block;
		}
	}
	else
	{
		result = R3Result_DefaultNon;
	}

	
ret:
	{
		if(pOper)
		{
			ExFreePool(pOper);
		}
		if(pWait)
		{
			ExFreePool(pWait);
		}
	}
	return result;
	

}
*/


R3_RESULT GetResultFromUser(VOID)
{
	OperList *pOperEntry = NULL;
	WaitList *pWaitEntry = NULL;
	UNICODE_STRING processFullPath = {0x00};
	R3_RESULT result = R3Result_Pass;
	ULONG_PTR ulPid = 0;
	NTSTATUS ntStatus = STATUS_SUCCESS;
	BOOLEAN getPending = FALSE;
	LARGE_INTEGER lWaitInteger = {0x00};

	//����һ��operList���
	pOperEntry = (OperList *)ExAllocatePool(PASSIVE_LEVEL,sizeof(OperList));
	if(!pOperEntry)
	{
		DbgPrint("ExAllocatePool faild\n");
		return result;
	}

	//���ڸú�������deviceIoControl���õģ�ioControl����r3����Ľ��������ģ���������ֱ�ӻ�ȡr3��pid
	ulPid = (ULONG_PTR)PsGetCurrentProcessId();
	pOperEntry->processId = ulPid;
	pOperEntry->waitId = MakeWaitId();

	processFullPath.Length = processFullPath.MaximumLength = MAX_PATH * sizeof(WCHAR) + sizeof(WCHAR);
	processFullPath.Buffer = (UNICODE_STRING *)ExAllocatePoolWithTag(PASSIVE_LEVEL,
		processFullPath.MaximumLength,
		"HPUF");
	if(!processFullPath.Buffer)
	{
		DbgPrint("ExAllocatePoolWithTag to processFullPath.Buffer faild\n");
		goto ret;
	}
	
	//���ݽ���pid��ȡ����ȫ·��
	ntStatus = GetProcessNameByPid(ulPid,&processFullPath);
	
	//��ȡ�ɹ�֮��ֱ��copy��operList�ṹ�оͿ�����
	DbgPrint("get process full path is %wZ\n",&processFullPath);
	RtlCopyMemory(pOperEntry->processName,processFullPath.Buffer,MAX_PATH);
	
	//copy��ֱ���ͷ�����Ķѿռ�
	ExFreePool(processFullPath.Buffer);
	
	//ͬʱ����wait�ṹ
	pWaitEntry = (WaitList *)ExAllocatePoolWithTag(PagedPool,sizeof(WaitList),'TIAW');
	pWaitEntry->waitId = pOperEntry->waitId;

	//�����ǳ�ʼ��event...
	KeInitializeEvent(&pWaitEntry->waitEvent
		,SynchronizationEvent
		,FALSE);

	LockWrite(&g_WaitListResourceLock);
	InsertTailList(&g_WaitListEntry,&pWaitEntry->listEntry);
	UnLockWrite(&g_WaitListResourceLock);

	LockWrite(&g_PendingIrpListResourceLock);
	getPending = CompletePendingIrp(&g_PendingIrpListEntry,pOperEntry);
	UnLockWrite(&g_PendingIrpListResourceLock);
	
	if(!getPending)
	{
		//���û���ҵ�pending��IRP���򽫵�ǰoperEntry���뵽operList�����У��ȴ�����R3����read����ʱ���ظ�R3
		LockWrite(&g_OperListResourceLock);
		InsertTailList(&g_OperListEntry,&pOperEntry->listEntry);
		UnLockWrite(&g_OperListResourceLock);
		pOperEntry = NULL;
	}

	lWaitInteger.QuadPart = -40 * 10000000;
	ntStatus = KeWaitForSingleObject(&pWaitEntry->waitEvent
		,Executive
		,KernelMode
		,FALSE
		,&lWaitInteger);

	//�������˵���ȴ��������Ƴ��ȴ����
	LockWrite(&g_WaitListResourceLock);
	RemoveEntryList(pWaitEntry);
	UnLockWrite(&g_WaitListResourceLock);

	if(ntStatus != STATUS_TIMEOUT)
	{
		if(pWaitEntry->block)
		{
			result = R3Result_Block;
		}
		else
		{
			result = R3Result_Pass;
		}
	}
	else
	{
		result = R3Result_DefaultNon;
	}
	
ret:
	{
		if(pOperEntry != NULL)
		{
			ExFreePool(pOperEntry);
		}
		if(pWaitEntry)
		{
			ExFreePool(pWaitEntry);
		}
		return result;
	}

}



NTSTATUS DispatchClose(DEVICE_OBJECT *pDev,PIRP pIrp)
{
	pIrp->IoStatus.Information = 0;
	pIrp->IoStatus.Status = STATUS_SUCCESS;
	IoCompleteRequest(pIrp,IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

NTSTATUS DeviceIoCommon(DEVICE_OBJECT *pDeviceObject,PIRP pIrp)
{
	DbgPrint("execute Common!\n");
	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;

	IoCompleteRequest(pIrp,IO_NO_INCREMENT);
	
	
	return STATUS_SUCCESS;
}
VOID DispatchCancel(PDEVICE_OBJECT pDeviceObject,PIRP pIrp)
{
	//����IRQL
	KIRQL CancelIrql = pIrp->CancelIrql;
	IoReleaseCancelSpinLock(DISPATCH_LEVEL);
	KeLowerIrql(CancelIrql);

	LockWrite(&g_PendingIrpListResourceLock);
	RemoveEntryList(&pIrp->Tail.Overlay.ListEntry);
	UnLockWrite(&g_PendingIrpListResourceLock);

	pIrp->IoStatus.Status = STATUS_CANCELLED;
	pIrp->IoStatus.Information = 0;

	IoCompleteRequest(pIrp,IO_NO_INCREMENT);
}

VOID PendingIrpToList(PIRP pIrp,LIST_ENTRY *listHeader,PDRIVER_CANCEL cancelRoutine)
{
	InsertTailList(listHeader,&pIrp->Tail.Overlay.ListEntry);
	IoMarkIrpPending(pIrp);
	IoSetCancelRoutine(pIrp,cancelRoutine);

}


/*
NTSTATUS DispatchIoRead(__in DEVICE_OBJECT *pDeviceObject,__in PIRP pIrp)
{
	NTSTATUS ioStatus = STATUS_SUCCESS;
	IO_STACK_LOCATION *CurrentIrpStack = IoGetCurrentIrpStackLocation(&pIrp);
	OperList *pOper = NULL;
	LIST_ENTRY *pOperListEntry = NULL;

	if(CurrentIrpStack->Parameters.Read.Length < sizeof(OperListR3))
	{
		pIrp->IoStatus.Status = STATUS_SUCCESS;
		pIrp->IoStatus.Information = 0;
		IoCompleteRequest(pIrp,IO_NO_INCREMENT);

		return ioStatus;
	}
	
	LockWrite(&g_OperListResourceLock);

	if(IsListEmpty(&g_OperListEntry))
	{
		//pending
		UnLockWrite(&g_OperListResourceLock);
		LockWrite(&g_PendingIrpListResourceLock);
		PendingIrpToList(pIrp,&g_PendingIrpListEntry,DispatchCancel);
		UnLockWrite(&g_PendingIrpListResourceLock);

		//the big microsoft papa say:return pending!
		return STATUS_PENDING;
	}
	pOperListEntry = g_OperListEntry.Flink;
	UnLockWrite(&g_OperListResourceLock);
	pOper = CONTAINING_RECORD(pOperListEntry,OperList,listEntry);

	RemoveEntryList(pOperListEntry);
	RtlCopyMemory(pIrp->AssociatedIrp.SystemBuffer,pOper,sizeof(OperListR3));

	ExFreePool(pOper);
	pOper = NULL;

	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp,IO_NO_INCREMENT);
	
	return ioStatus;
}



*/












NTSTATUS DeviceIoRead(DEVICE_OBJECT *pDeviceObject,PIRP pIrp)
{
	NTSTATUS ioStatus = STATUS_SUCCESS;
	LIST_ENTRY *pOperEntry = NULL;
	OperList *pOperList = NULL;
	ULONG uRealLength;
	IO_STACK_LOCATION *pIrpStackLocaton = NULL;

	pIrpStackLocaton = IoGetCurrentIrpStackLocation(pIrp);
	if(pIrpStackLocaton->Parameters.Read.Length < sizeof(OperListR3))
	{
		pIrp->IoStatus.Information = 0;
		pIrp->IoStatus.Status = ioStatus;
		IoCompleteRequest(pIrp,IO_NO_INCREMENT);
		return ioStatus;
	}
	LockWrite(&g_OperListResourceLock);

	if(IsListEmpty(&g_OperListEntry))
	{
		//������湥����Ϣ����Ϊ�գ��򽫴˴�irp pending����
		UnLockWrite(&g_OperListResourceLock);
		LockWrite(&g_PendingIrpListResourceLock);
		PendingIrpToList(pIrp,&g_PendingIrpListEntry,DispatchCancel);

		UnLockWrite(&g_PendingIrpListResourceLock);
		return STATUS_PENDING;
	}

	//���������Ϣ����Ϊ�գ����ȡһ����㣬���������в����copy��R3
	pOperEntry = g_OperListEntry.Flink;
	UnLockWrite(&g_OperListResourceLock);
	pOperList = CONTAINING_RECORD(pOperEntry,OperList,listEntry);
	RemoveEntryList(pOperEntry);
	uRealLength = sizeof(OperListR3);
	RtlCopyMemory(pIrp->AssociatedIrp.SystemBuffer,pOperList,uRealLength);

	//copy֮���ͷ��ڴ�
	ExFreePool(pOperList);
	pOperList = NULL;

	pIrp->IoStatus.Information = uRealLength;
	pIrp->IoStatus.Status = ioStatus;
	IoCompleteRequest(pIrp,IO_NO_INCREMENT);

	return ioStatus; 
}





/*
NTSTATUS DispatchIoControl(__in DEVICE_OBJECT *pDeviceObject,PIRP pIrp)
{
	NTSTATUS ioStatus = STATUS_SUCCESS;
	IO_STACK_LOCATION *pCurrentIrpStack = IoGetCurrentIrpStackLocation(pIrp);
	ULONG uCtrolCode = 0;
	R3_REPLY *lpReply = NULL;
	WaitList *pWaitListEntry = {0x00};
	R3_RESULT result = R3Result_DefaultNon;
	VOID *outputBuffer = NULL;

	uCtrolCode = pCurrentIrpStack->Parameters.DeviceIoControl.IoControlCode;
	switch(uCtrolCode)
	{
		case IOCTRL_SEND_RESULT_TO_R0://self r3,user select result!
			{
				if(pCurrentIrpStack->Parameters.Read.Length < sizeof(R3_REPLY))
				{
					pIrp->IoStatus.Status = STATUS_PARITY_ERROR;
					pIrp->IoStatus.Information = 0;
					break;
				}

				lpReply = (R3_REPLY *)pIrp->AssociatedIrp.SystemBuffer;
				LockWrite(&g_WaitListResourceLock);
				findWaitEntryByWaitId(&g_WaitListEntry,lpReply->WaitId);
				if(pWaitListEntry == NULL)
				{
					break;
				}
				
				pWaitListEntry->block = lpReply->Block;

				//wake up getResultFromUser func��
				KeSetEvent(&pWaitListEntry->waitEvent,0,FALSE);

				UnLockWrite(&g_WaitListResourceLock);

			}
			break;
		case IOCTRL_XXX_ATTACK_CODE:
			{
				result = _GetResultFromUser();
				outputBuffer = pIrp->AssociatedIrp.SystemBuffer;
				switch(result)
				{
				case R3Result_Block:
					{
						*(ULONG *)outputBuffer = 1;
						ioStatus = STATUS_SUCCESS;
					}
					break;
				case R3Result_Pass:
					{
						*(ULONG *)outputBuffer = 1;
						ioStatus = STATUS_SUCCESS;
					}
					break;
				case R3Result_DefaultNon:
					{
						*(ULONG *)outputBuffer = 1;
						ioStatus = STATUS_SUCCESS;
					}
					break;
				}

				pIrp->IoStatus.Status = STATUS_SUCCESS;
				pIrp->IoStatus.Information = 0;

			}
			break;
	}
	
	IoCompleteRequest(pIrp,IO_NO_INCREMENT);
	return ioStatus;
}
*/



















NTSTATUS DeviceIoControl(DEVICE_OBJECT *pDeviceObject,PIRP pIrp)
{
	PIO_STACK_LOCATION  pCurrentIprStack = NULL;
	VOID *inputBuffer = NULL;
	VOID* outputBuffer = NULL;
	ULONG inputBufferLength,outputBufferLength;
	NTSTATUS ntStatus = STATUS_SUCCESS;
	ULONG uControlCode = 0;
	WaitList *pWaitTargetEntry = NULL;
	R3_REPLY *lpReply = NULL;

	pIrp->IoStatus.Information = 0;
	pIrp->IoStatus.Status = ntStatus;

	//��ȡIRP��ջ����ջ�л�ȡbuffer���ȣ��Լ�������
	pCurrentIprStack = IoGetCurrentIrpStackLocation(pIrp);
	if(!pCurrentIprStack)
	{
		DbgPrint("IoGetCurrentIrpStackLocation faild\n");
		return ntStatus;
	}

	inputBuffer = pIrp->AssociatedIrp.SystemBuffer;
	inputBufferLength = pCurrentIprStack->Parameters.DeviceIoControl.InputBufferLength;

	outputBuffer = pIrp->AssociatedIrp.SystemBuffer;
	outputBufferLength = pCurrentIprStack->Parameters.DeviceIoControl.OutputBufferLength;

	uControlCode = pCurrentIprStack->Parameters.DeviceIoControl.IoControlCode;
	

	switch(uControlCode)
	{
		case IOCTRL_SEND_RESULT_TO_R0:
		{
			if(pCurrentIprStack->Parameters.DeviceIoControl.InputBufferLength < sizeof(R3_REPLY))
			{
				pIrp->IoStatus.Information = 0;
				pIrp->IoStatus.Status = STATUS_PARITY_ERROR;
				break;
			}

			lpReply = (R3_REPLY *)pIrp->AssociatedIrp.SystemBuffer;

			LockWrite(&g_WaitListResourceLock);
			pWaitTargetEntry = findWaitEntryByWaitId(&g_WaitListEntry,lpReply->WaitId);
			
			if(!pWaitTargetEntry)
				break;
			pWaitTargetEntry->block = lpReply->Block;
			KeSetEvent(&pWaitTargetEntry->waitEvent
				,0
				,FALSE);

			UnLockWrite(&g_WaitListResourceLock);
			pIrp->IoStatus.Status = 0;
			pIrp->IoStatus.Information = STATUS_SUCCESS;
			
			
		}
		break;
		//�÷�֧ģ���⵽������ʱ�Ĳ���
		case IOCTRL_XXX_ATTACK_CODE:
		{
			R3_RESULT R3_RES = R3Result_DefaultNon;

			//���ù���������Ϣͨ��R3���򴫵ݸ��û������û���ѡ����������ֹ
			R3_RES = GetResultFromUser();

			//�ȴ������󣬻ᵽ������������֧
			switch(R3_RES)
			{
				case R3Result_Block:
				{
					//�������仺����д������
					DbgPrint("�û���ֹ\n");
					*(ULONG *)outputBuffer = 0;
					ntStatus = STATUS_SUCCESS;
				}
				break;
				case R3Result_Pass:
				{
					//ͬ��
					DbgPrint("�û�����\n");
					*(ULONG *)outputBuffer = 1;
					ntStatus = STATUS_SUCCESS;
				}
				break;
				case R3Result_DefaultNon:
				{
					DbgPrint("��ʱ\n");
					*(ULONG *)outputBuffer = 1;
					ntStatus = STATUS_SUCCESS;
				}
				break;
			}

			pIrp->IoStatus.Information = sizeof(ULONG);
			pIrp->IoStatus.Status = ntStatus;

		}
		break;
	}

	IoCompleteRequest(pIrp,IO_NO_INCREMENT);
	return ntStatus;
}


VOID DriverUnLoad(DRIVER_OBJECT *pDriverObject)
{
	UNICODE_STRING symbolicLink = {0x00};
	RtlInitUnicodeString(&symbolicLink,DeviceSymbolicLink);

	IoDeleteSymbolicLink(&symbolicLink);
	IoDeleteDevice(pDriverObject->DeviceObject);

	DbgPrint("goodbye!!!\n");
	
	return;

}

NTSTATUS DriverEntry(__in DRIVER_OBJECT *pDriverObject,__in UNICODE_STRING *pDriverPath)
{
	NTSTATUS ntStatus;
	UNICODE_STRING uDeviceName = {0x00};
	UNICODE_STRING uSybolicName = {0x00};
	DEVICE_OBJECT *pDeviceObject = NULL;
	ULONG uDisFuncCount;


	RtlInitUnicodeString(&uDeviceName,DeviceName);
	RtlInitUnicodeString(&uSybolicName,DeviceSymbolicLink);

	ntStatus = IoCreateDevice(pDriverObject
		,NULL
		,&uDeviceName
		,FILE_DEVICE_UNKNOWN
		,0
		,FALSE
		,&pDeviceObject);
	if(!NT_SUCCESS(ntStatus))
	{
		DbgPrint("IoCreateDevice faild:%d\n",ntStatus);
		return ntStatus;
	}

	ntStatus = IoCreateSymbolicLink(&uSybolicName,&uDeviceName);
	if(!NT_SUCCESS(ntStatus))
	{
		DbgPrint("IoCreateSymbolicLink faild:%d\n",ntStatus);
		return ntStatus;
	}

	pDeviceObject->Flags |= DO_BUFFERED_IO;
	for(uDisFuncCount = 0; uDisFuncCount < IRP_MJ_MAXIMUM_FUNCTION; uDisFuncCount++)
	{
		pDriverObject->MajorFunction[uDisFuncCount] = DeviceIoCommon;
	}

	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceIoControl;
	pDriverObject->MajorFunction[IRP_MJ_CLOSE] = DispatchClose;
	//pDriverObject->MajorFunction[IRP_MJ_READ] = DeviceIoRead;
	pDriverObject->MajorFunction[IRP_MJ_READ] = DeviceIoRead;
	pDriverObject->DriverUnload = DriverUnLoad;
	initializeLock(&g_OperListResourceLock);
	initializeLock(&g_WaitListResourceLock);
	initializeLock(&g_PendingIrpListResourceLock);

	initializeList(&g_OperListEntry);
	initializeList(&g_WaitListEntry);
	initializeList(&g_PendingIrpListEntry);

	return ntStatus;

}



/*
NTSTATUS DriverEntry(__in DRIVER_OBJECT *pDriverObject,__in PUNICODE_STRING pDriverPath)
{
	NTSTATUS ioStatus = STATUS_SUCCESS;
	UNICODE_STRING uDeviceName = {0x00};
	UNICODE_STRING uDeviceSymbolicLink = {0x00};
	DEVICE_OBJECT *pDeviceObject = NULL;
	long i;

	RtlInitUnicodeString(&uDeviceName,DeviceName);
	RtlInitUnicodeString(&uDeviceSymbolicLink,DeviceSymbolicLink);

	ioStatus = IoCreateDevice(pDriverObject,0,&uDeviceName,FILE_DEVICE_UNKNOWN,0,FALSE,&pDeviceObject);
	if(!NT_SUCCESS(ioStatus))
	{
		DbgPrint("IoCreateDevice faild\n");
		return ioStatus;
	}

	ioStatus = IoCreateSymbolicLink(&uDeviceSymbolicLink,&uDeviceName);
	if(!NT_SUCCESS(ioStatus))
	{
		DbgPrint("IoCreateSymbolicLink faild\n");
		return ioStatus;
	}

	pDeviceObject->Flags |= DO_BUFFERED_IO;

	for(i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
	{
		pDriverObject->MajorFunction[i] = DeviceIoCommon;
	}

	pDriverObject->DriverUnload = DriverUnLoad;
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceIoControl;
	pDriverObject->MajorFunction[IRP_MJ_READ] = DeviceIoRead;
	pDriverObject->MajorFunction[IRP_MJ_CLOSE] = DispatchClose;


	//initialize
	initializeLock(&g_OperListResourceLock);
	initializeLock(&g_WaitListResourceLock);
	initializeLock(&g_PendingIrpListResourceLock);

	initializeList(&g_OperListEntry);
	initializeList(&g_WaitListEntry);
	initializeList(&g_PendingIrpListEntry);

	

	return ioStatus;
	

}
*/
