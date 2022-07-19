#include <ntddk.h>

#define IOCTRL_BASIC 0x8000
#define MYIOCTRL_CODE(i)\
	CTL_CODE(FILE_DEVICE_UNKNOWN,IOCTRL_BASIC + i,METHOD_BUFFERED,FILE_ANY_ACCESS)

#define IOCTRL_HELLO MYIOCTRL_CODE(0)
#define IOCTRL_PRINT MYIOCTRL_CODE(1)
#define IOCTRL_BYE MYIOCTRL_CODE(2)
#define SystemHandleInformation 16

const WCHAR aDeviceName[] = L"\\DosDevices\\PBCDeleteFile";

const WCHAR aDeviceSymbolicName[] = L"\\Device\\PBCDeleteFile";

typedef struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO
{
	USHORT UniqueProcessId;
	USHORT CreatorBackTraceIndex;
	UCHAR ObjectTypeIndex;
	UCHAR HandleAttributes;
	USHORT HandleValue;
	PVOID Object;
	ULONG GrantedAccess;
} SYSTEM_HANDLE_TABLE_ENTRY_INFO, *PSYSTEM_HANDLE_TABLE_ENTRY_INFO;

typedef struct _SYSTEM_HANDLE_INFORMATION
{
	ULONG NumberOfHandles;
	SYSTEM_HANDLE_TABLE_ENTRY_INFO Handles[];
} SYSTEM_HANDLE_INFORMATION, *PSYSTEM_HANDLE_INFORMATION;

NTSTATUS PBCOpenFile(WCHAR *,
					 PHANDLE,
					 ACCESS_MASK,
					 ULONG);
BOOLEAN PBCDeleteFile(WCHAR *);

BOOLEAN PBCCloseHandle(WCHAR *);

NTSTATUS PBCQuerySymbolicLink(PUNICODE_STRING,PUNICODE_STRING);

NTSYSAPI NTSTATUS NTAPI ZwQuerySystemInformation(ULONG SystemInformationClass,
												 PULONG SystemInformation,
												 ULONG SystemInformationLength,
												 PULONG returnLength);
NTSYSAPI
NTSTATUS
NTAPI
ZwDuplicateObject(
				  IN HANDLE SourceProcessHandle,
				  IN HANDLE SourceHandle,
				  IN HANDLE TargetProcessHandle OPTIONAL,
				  OUT PHANDLE TargetHandle OPTIONAL,
				  IN ACCESS_MASK DesiredAccess,
				  IN ULONG HandleAttributes,
				  IN ULONG Options
				  );

NTSTATUS
ObQueryNameString(
				  IN PVOID,
				  OUT POBJECT_NAME_INFORMATION,
				  IN ULONG,
				  OUT PULONG
				  );

NTSTATUS
NTAPI
PBCSkillFileCompleation(PDEVICE_OBJECT,
						PIRP,
						PVOID);

VOID DriverUnload(PDRIVER_OBJECT pDriverObject)
{
	if(pDriverObject)
	{
		UNICODE_STRING uDeviceSymblicLinkName = {0x00};
		RtlInitUnicodeString(&uDeviceSymblicLinkName,aDeviceSymbolicName);
		IoDeleteSymbolicLink(&uDeviceSymblicLinkName);

		IoDeleteDevice(pDriverObject->DeviceObject);
	}

	DbgPrint("PBCDeleteFile:goodbye!\n");

	return;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject,PUNICODE_STRING pDriverUpath)
{
	NTSTATUS ntStatus = STATUS_SUCCESS;
	UNICODE_STRING uDeviceName = {0x00};
	UNICODE_STRING uDeviceSymbolicName = {0x00};
	PDEVICE_OBJECT deviceObject;
	

	pDriverObject->DriverUnload = DriverUnload;
	
	
	RtlInitUnicodeString(&uDeviceName,aDeviceName);
	RtlInitUnicodeString(&uDeviceSymbolicName,aDeviceSymbolicName);

	ntStatus = IoCreateDevice(pDriverObject,
		0,
		&uDeviceName,
		0x0000800a,
		0,
		TRUE,
		&deviceObject);

	if(!NT_SUCCESS(ntStatus))
	{
		return ntStatus;
	}

	ntStatus = PBCDeleteFile(L"\\??\\c:\\haha.doc");
	if(!NT_SUCCESS(ntStatus))
	{
		DbgPrint("PBCDeleteFile(\\c:\\haha.doc) faild:%x\n",ntStatus);
		return ntStatus;
	}
	
	ntStatus = PBCDeleteFile(L"\\??\\c:\\PCHunter32.exe");
	if(!NT_SUCCESS(ntStatus))
	{
		DbgPrint("PBCDeleteFile(\\c:\\PCHunter32.exe) faild:%x\n",ntStatus);
		return ntStatus;
	}
	

	return STATUS_SUCCESS;
}

NTSTATUS PBCOpenFile(WCHAR *szFilePath,
					 PHANDLE pFileHandle,
					 ACCESS_MASK accessmaskm,
					 ULONG share)
{
	NTSTATUS ntStatus;
	UNICODE_STRING uFilePath = {0x00};
	OBJECT_ATTRIBUTES objFileAttributes = {0x00};
	IO_STATUS_BLOCK ioStatusBlock = {0x00};

	if(KeGetCurrentIrql() > PASSIVE_LEVEL)
		return 0;
	RtlInitUnicodeString(&uFilePath,szFilePath);

	InitializeObjectAttributes(&objFileAttributes,
		&uFilePath,
		OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
		NULL,
		NULL);
	
	ntStatus = IoCreateFile(pFileHandle,
		accessmaskm,
		&objFileAttributes,
		&ioStatusBlock,
		NULL,
		FILE_ATTRIBUTE_NORMAL,
		share,
		FILE_OPEN,
		0,
		NULL,
		0,
		0,
		NULL,
		IO_NO_PARAMETER_CHECKING);

	return ntStatus;
}

/*
	��ȡ�������Ӷ�Ӧ���豸����
	pSymbolicLinkName:\DosDevice\c:
	pOutPut:����Դ
*/
NTSTATUS PBCQuerySymbolicLink(PUNICODE_STRING pSymbolicLinkName,PUNICODE_STRING pOutPut)
{
	NTSTATUS ntStatus;
	OBJECT_ATTRIBUTES objSymbolicLinkAttributes = {0x00};
	HANDLE hDeviceHandle = NULL;
	
	//���ȣ�ҲҪ��ʼ��һ���ļ�����
	InitializeObjectAttributes(&objSymbolicLinkAttributes,
		pSymbolicLinkName,
		OBJ_CASE_INSENSITIVE,
		NULL,
		NULL);

	//��ȡ�������Ӷ�Ӧ���豸���
	ntStatus = ZwOpenSymbolicLinkObject(&hDeviceHandle,
		GENERIC_READ,
		&objSymbolicLinkAttributes);
	if(!NT_SUCCESS(ntStatus))
	{
		return ntStatus;
	}
	
	//����紫����output����ռ䣬����������
	pOutPut->Length = 0;
	pOutPut->MaximumLength = 0x400 * sizeof(WCHAR);
	pOutPut->Buffer = ExAllocatePoolWithTag(PagedPool,pOutPut->MaximumLength,'A0');

	if(!pOutPut->Buffer)
	{
		DbgPrint("alloc memory faild \n");
		ZwClose(hDeviceHandle);
		ntStatus = STATUS_INSUFFICIENT_RESOURCES;
		return ntStatus;
	}

	RtlZeroMemory(pOutPut->Buffer,pOutPut->MaximumLength);

	ntStatus = ZwQuerySymbolicLinkObject(hDeviceHandle,pOutPut,NULL);

	ZwClose(hDeviceHandle);

	//�����ѯʧ�ܣ��������ͷ�pOutPut->Buffer
	if(!NT_SUCCESS(ntStatus))
	{
		ExFreePool(pOutPut->Buffer);
	}

	return ntStatus;
	
}

BOOLEAN PBCCloseHandle(WCHAR *szFilePath)
{
	ULONG size;
	ULONG index;
	NTSTATUS ntStatus;
	PVOID buffer = NULL;
	BOOLEAN bRet = FALSE;
	PSYSTEM_HANDLE_INFORMATION handleTable = NULL;
	SYSTEM_HANDLE_TABLE_ENTRY_INFO currentHandleEntryInfo;
	HANDLE currentHandle = NULL;
	ULONG handleNumber = 0;
	UNICODE_STRING uVolume = {0x00};
	UNICODE_STRING uLink = {0x00};
	UNICODE_STRING uLinkName = {0x00};
	WCHAR letterTmp[3] = {0x00};
	WCHAR *aFileName = NULL;
	UNICODE_STRING delFileName = {0x00};
	HANDLE hProcess;
	ULONG uProcessId;
	CLIENT_ID cid;
	OBJECT_ATTRIBUTES objProcessAttributes = {0x00};
	HANDLE hCopyHandle = NULL;
	PVOID fileObject = NULL;
	POBJECT_NAME_INFORMATION pObjectHandleInformation = NULL;
	ULONG oBresultSize;

	//ʹ�����ַ�ʽ������Ļ�ȡȫ�־����ĳ��ȣ����һ��ȫ�־���е�����
	for(size = 1; ; size *= 2)
	{
		buffer = ExAllocatePoolWithTag(PagedPool,size,'CCBP');
		
		if(!buffer)
		{
			DbgPrint("alloc memory faild\n");
			goto ret;
		}
		RtlZeroMemory(buffer,size);
		ntStatus = ZwQuerySystemInformation(SystemHandleInformation,buffer,size,NULL);
		if(!NT_SUCCESS(ntStatus))
		{
			//������صĴ�������legth mismatch����˵������ȥ�Ŀռ䲻��
			//��������β�������������size�����·��䲢��ȡȫ�־����
			if(ntStatus == STATUS_INFO_LENGTH_MISMATCH)
			{
				ExFreePool(buffer);
				buffer = NULL;
			}
			else
			{
				//����˵��������������ô��ֱ�ӷ��ذ�
				DbgPrint("ZwQuerySystemInformation faild:%x\n",ntStatus);
				goto ret;
			}
		}
		else
		{
			//��ʱ��˵����ȡ�ɹ��������ѭ��
			break;
		}
	}
	
	handleTable = (PSYSTEM_HANDLE_INFORMATION)buffer;
	handleNumber = handleTable->NumberOfHandles;
	
	//"\??\c:\Dri\xxx.txt"
	letterTmp[0] = szFilePath[4];
	letterTmp[1] = szFilePath[5];
	letterTmp[2] = 0;

	RtlInitUnicodeString(&uVolume,letterTmp);
	RtlInitUnicodeString(&uLink,L"\\DosDevices\\");
	
	uLinkName.Buffer = ExAllocatePoolWithTag(PagedPool,256 + sizeof(WCHAR),'KNIL');
	uLinkName.MaximumLength = 256;

	RtlCopyUnicodeString(&uLinkName,&uLink);

	ntStatus = RtlAppendUnicodeStringToString(&uLinkName,&uVolume);
	if(!NT_SUCCESS(ntStatus))
	{
		DbgPrint("RtlAppendUnicodeStringToString faild:%x\n",ntStatus);
		goto ret;
	}
	
	ntStatus = PBCQuerySymbolicLink(&uLinkName,&delFileName);
	RtlFreeUnicodeString(&uLinkName);

	if(!NT_SUCCESS(ntStatus))
	{
		DbgPrint("PBCQuerySymbolicLink faild:%x\n",ntStatus);
		goto ret;
	}
	
	aFileName = &szFilePath[6];
	RtlAppendUnicodeToString(&delFileName,aFileName);
	DbgPrint("delFileName is %wZ\n",&delFileName);
	
	//���������
	for(index = 0; index < handleNumber; index++)
	{
		//��ȡ��ǰ������еĽ��
		currentHandleEntryInfo = handleTable->Handles[index];
		//��ȡ��ǰ�����ŵľ��
		currentHandle = (HANDLE)currentHandleEntryInfo.HandleValue;

		if(currentHandleEntryInfo.ObjectTypeIndex != 25 && currentHandleEntryInfo.ObjectTypeIndex != 28)
		{
			continue;
		}
		//���ݵ�ǰ�����ȡ�þ����Ӧ�ļ��Ķ�ռ����ID
		uProcessId = (ULONG)currentHandleEntryInfo.UniqueProcessId;

		cid.UniqueProcess = (HANDLE)uProcessId;
		cid.UniqueThread = (HANDLE)0;
		InitializeObjectAttributes(&objProcessAttributes,NULL,0,NULL,NULL);

		ntStatus = ZwOpenProcess(&hProcess,
			PROCESS_DUP_HANDLE,
			&objProcessAttributes,
			&cid);
		if(!NT_SUCCESS(ntStatus))
		{
			DbgPrint("ZwOpenProcess id:%d,faild:%x\n",uProcessId,ntStatus);
			continue;
		}
		
		//��һ�ο����ļ������Ŀ���ǻ�ȡ�þ�����ļ��ں˶��󣬴Ӷ���ȡ�ļ��ں˶����е��ļ���
		ntStatus = ZwDuplicateObject(hProcess,currentHandle,NtCurrentProcess(),&hCopyHandle,
			PROCESS_ALL_ACCESS,0,DUPLICATE_SAME_ACCESS);

		if(!NT_SUCCESS(ntStatus))
		{
			DbgPrint("ZwDuplicateObject faild:%x\n",ntStatus);
			continue;
		}

		//���ݿ��������ı���ռ���ļ��������ȡ���ļ����ļ��ں˶���
		ntStatus = ObReferenceObjectByHandle(hCopyHandle,
			FILE_ANY_ACCESS,
			0,
			KernelMode,
			&fileObject,
			NULL);
		if(!NT_SUCCESS(ntStatus))
		{
			ObDereferenceObject(fileObject);
			continue;
		}

		pObjectHandleInformation = (POBJECT_NAME_INFORMATION)ExAllocatePoolWithTag(NonPagedPool,
			1024 * sizeof(WCHAR) + sizeof(OBJECT_NAME_INFORMATION),
			'0C');
		if(!pObjectHandleInformation)
		{
			DbgPrint("alloc memory with pObjectHandleInformation faild\n");
			continue;
		}

		//��ȡ���ļ��ں˶����OBJECT_HANDLE_INFORMATION
		ntStatus = ObQueryNameString(fileObject,
			pObjectHandleInformation,
			sizeof(OBJECT_NAME_INFORMATION) + 1024 * sizeof(WCHAR),
			&oBresultSize);
		if(!NT_SUCCESS(ntStatus))
		{
			ObDereferenceObject(fileObject);
			continue;
		}
		
		//0��ʾƥ�䵽��
		if(RtlCompareUnicodeString(&delFileName,&pObjectHandleInformation->Name,TRUE) == 0)
		{
			ObDereferenceObject(fileObject);
			//����һ�ο����������Լ����̵��ļ�����ر�
			ZwClose(hCopyHandle);
			
			//�ڶ��ο���������DUPLICATE_CLOSE_SOURCE����ԭ��ռ�и��ļ��Ľ��̹ر�ռ������
			ntStatus = ZwDuplicateObject(hProcess,
				currentHandle,
				NtCurrentProcess(),
				&hCopyHandle,
				PROCESS_ALL_ACCESS,
				0,
				DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE);
			if(!NT_SUCCESS(ntStatus))
			{
				DbgPrint("ZwDuplicateObject2 failed:%x\n",ntStatus);
			}
			else
			{
				bRet = TRUE;
				ZwClose(hCopyHandle);
			}
			break;
		}
		ExFreePool(pObjectHandleInformation);
		pObjectHandleInformation = NULL;

		ObDereferenceObject(fileObject);
		ZwClose(currentHandle);
		ZwClose(hProcess);

	}

ret:
	if(buffer)
	{
		ExFreePool(buffer);
		buffer = NULL;
	}
	if(uLinkName.Buffer)
	{
		ExFreePool(uLinkName.Buffer);
	}
	if(delFileName.Buffer)
	{
		ExFreePool(delFileName.Buffer);
	}
	if(pObjectHandleInformation)
	{
		ExFreePool(pObjectHandleInformation);
	}
	return bRet;
}

BOOLEAN PBCDeleteFile(WCHAR *szFilePath)
{
	NTSTATUS ntStatus;
	BOOLEAN bRet = FALSE;
	HANDLE hFileHandle = NULL;
	PFILE_OBJECT fileObject = NULL;
	PDEVICE_OBJECT fileDeviceObject = NULL;
	PIRP customIrp = NULL;
	FILE_DISPOSITION_INFORMATION fileDisInformation;
	KEVENT event;
	IO_STATUS_BLOCK ioStatusBlock;
	PIO_STACK_LOCATION customIrpStack = NULL;
	PSECTION_OBJECT_POINTERS pSectionObjectPointers = NULL;

	//�������openfile��ACCESS_MASK���Ͳ���ʱ����FILE_READ_ACCESS����ȥ��
	//�����ڹرվ��ʱ������Ŀ���ļ��Ķ�ռ���̾��ʱ������ʾ0c000022��STATUS_ACCESS_DENIED
	ntStatus = PBCOpenFile(szFilePath,
		&hFileHandle,
		FILE_READ_ATTRIBUTES | DELETE,
		FILE_SHARE_DELETE);
	
	if(!NT_SUCCESS(ntStatus))
	{
		if(ntStatus == STATUS_OBJECT_NAME_NOT_FOUND ||
			ntStatus == STATUS_OBJECT_PATH_NOT_FOUND)
		{
			DbgPrint("no such file\n");
			goto ret;
		}
		else
		{
			bRet = PBCCloseHandle(szFilePath);
			if(!bRet)
			{
				goto ret;
			}
			//���´�
			bRet = PBCOpenFile(szFilePath,&hFileHandle,FILE_READ_ATTRIBUTES | DELETE | FILE_WRITE_ACCESS,
				FILE_SHARE_READ | FILE_SHARE_WRITE |FILE_SHARE_DELETE);
			if(!bRet)
			{
				goto ret;
			}

		}
	}

	//˵����ռ���ļ��Ѿ������ǻ�ȡ�������ر��˶�ռ������
	//���ţ���ȡ���ļ����ļ��ں˶���
	ntStatus = ObReferenceObjectByHandle(hFileHandle,
		DELETE,
		*IoFileObjectType,
		KernelMode,
		&fileObject,
		NULL);
	if(!NT_SUCCESS(ntStatus))
	{
		DbgPrint("ObReferenceObjectByHandle faild:%x\n",ntStatus);
		ZwClose(hFileHandle);
		return FALSE;
	}

	//�����ļ��ں˶���fileobject��ȡ��Ӧ���豸����deviceobject
	fileDeviceObject = IoGetRelatedDeviceObject(fileObject);
	//��ʼ��irp
	customIrp = IoAllocateIrp(fileDeviceObject->StackSize,TRUE);
	
	if(!customIrp)
	{
		ObDereferenceObject(fileObject);
		ZwClose(hFileHandle);
		goto ret;
	}

	fileDisInformation.DeleteFile = TRUE;
	
	//��ʼ��һ���¼�
	//&&**&&%����&%��%��%����*&����&*����
	KeInitializeEvent(&event,SynchronizationEvent,FALSE);

	//���Զ���Irp������������
	//����irp��dobuffer ioģʽ��buffer
	customIrp->AssociatedIrp.SystemBuffer = &fileDisInformation;
	customIrp->Tail.Overlay.OriginalFileObject = fileObject;
	customIrp->Tail.Overlay.Thread = NtCurrentThread();
	customIrp->UserEvent = &event;
	customIrp->UserIosb = &ioStatusBlock;
	customIrp->RequestorMode = KernelMode;

	//�õ�irp��ջ
	customIrpStack = IoGetNextIrpStackLocation(customIrp);
	customIrpStack->DeviceObject = fileDeviceObject;
	customIrpStack->FileObject = fileObject;
	customIrpStack->MajorFunction = IRP_MJ_SET_INFORMATION;
	customIrpStack->Parameters.SetFile.Length = sizeof(FILE_DISPOSITION_INFORMATION);
	customIrpStack->Parameters.SetFile.FileInformationClass = FileDispositionInformation;
	customIrpStack->Parameters.SetFile.FileObject = fileObject;
	

	IoSetCompletionRoutine(customIrp,
		PBCSkillFileCompleation,
		&event,
		TRUE,
		TRUE,
		TRUE);
	
	//��һ���������е�exe�����ؼ�����Ĩ����ɾ��ʱϵͳ�Ͳ���ܾ���
	pSectionObjectPointers = fileObject->SectionObjectPointer;
	if(pSectionObjectPointers)
	{
		pSectionObjectPointers->ImageSectionObject = 0;
		pSectionObjectPointers->DataSectionObject = 0;
	}

	ntStatus = IoCallDriver(fileDeviceObject,customIrp);
	if(!NT_SUCCESS(ntStatus))
	{
		DbgPrint("IoCallDriver faild:%x\n",ntStatus);
		goto ret;
	}

	//���ţ����ǵȴ�eventִ�н�����
	KeWaitForSingleObject(&event,
		Executive,
		KernelMode,
		TRUE,
		NULL);

	ObDereferenceObject(fileObject);
	ZwClose(hFileHandle);


ret:
	return bRet;
}

NTSTATUS
NTAPI
PBCSkillFileCompleation(PDEVICE_OBJECT pDeviceObject,
						PIRP irp,
						PVOID context)
{
	irp->UserIosb->Status = irp->IoStatus.Status;
	irp->UserIosb->Information = irp->IoStatus.Information;

	KeSetEvent(irp->UserEvent,IO_NO_INCREMENT,FALSE);

	IoFreeIrp(irp);

	return STATUS_MORE_PROCESSING_REQUIRED;
}