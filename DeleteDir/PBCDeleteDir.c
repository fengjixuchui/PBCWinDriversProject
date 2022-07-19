#include <ntddk.h>

typedef struct _FILE_LIST_ENTRY
{
	LIST_ENTRY Entry;
	PWSTR NameBuffer;

} FILE_LIST_ENTRY,*PFILE_LIST_ENTRY;

typedef struct _FILE_DIRECTORY_INFORMATION {
	ULONG NextEntryOffset;
	ULONG FileIndex;
	LARGE_INTEGER CreationTime;
	LARGE_INTEGER LastAccessTime;
	LARGE_INTEGER LastWriteTime;
	LARGE_INTEGER ChangeTime;
	LARGE_INTEGER EndOfFile;
	LARGE_INTEGER AllocationSize;
	ULONG FileAttributes;
	ULONG FileNameLength;
	WCHAR FileName[1];
} FILE_DIRECTORY_INFORMATION, *PFILE_DIRECTORY_INFORMATION;

NTSTATUS PBCDeleteFile(WCHAR *);

NTSTATUS PBCDeleteDirectory(WCHAR *szDirPath);

NTSTATUS 
ZwQueryDirectoryFile(
					 __in HANDLE  FileHandle,
					 __in_opt HANDLE  Event,
					 __in_opt PIO_APC_ROUTINE  ApcRoutine,
					 __in_opt PVOID  ApcContext,
					 __out PIO_STATUS_BLOCK  IoStatusBlock,
					 __out PVOID  FileInformation,
					 __in ULONG  Length,
					 __in FILE_INFORMATION_CLASS  FileInformationClass,
					 __in BOOLEAN  ReturnSingleEntry,
					 __in_opt PUNICODE_STRING  FileName,
					 __in BOOLEAN  RestartScan
					 );

VOID DriverUnload(PDRIVER_OBJECT pDriverObject)
{
	DbgPrint("PBCDeleteDir:goodbye!\n");
	return;
}


NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject,PUNICODE_STRING pDriverPath)
{
	NTSTATUS ntStatus;

	DbgPrint("PBCDeleteDir:hello!\n");
	pDriverObject->DriverUnload = DriverUnload;

	ntStatus = PBCDeleteDirectory(L"\\??\\c:\\test");
	

	return STATUS_SUCCESS;

}

NTSTATUS PBCDeleteDirectory(WCHAR *szDirPath)
{
	NTSTATUS ntStatus;
	UNICODE_STRING uDirPath = {0x00};
	LIST_ENTRY listHeader = {0x00};
	PWSTR tmpNameBuffer = NULL;
	UNICODE_STRING uTmpNameStr = {0x00};
	PFILE_LIST_ENTRY pCurrentListEntry = NULL;
	PFILE_LIST_ENTRY pPreListEntry = NULL;
	HANDLE hFileHandle = NULL;
	OBJECT_ATTRIBUTES objFileAttributes = {0x00};
	IO_STATUS_BLOCK ioStatusBlock = {0x00};
	PVOID queryOutputBuffer = NULL;
	ULONG queryOutputBufferSize;

	PFILE_DIRECTORY_INFORMATION pCurrentDirInfo = NULL;
	BOOLEAN reStartScan;
	FILE_DISPOSITION_INFORMATION fileDisInfo = {0x00};
	

	//��ʼ��...
	RtlInitUnicodeString(&uDirPath,szDirPath);
	
	tmpNameBuffer = (WCHAR *)ExAllocatePoolWithTag(PagedPool,uDirPath.Length + sizeof(WCHAR),'EMAN');
	if(!tmpNameBuffer)
	{
		DbgPrint("alloc memory with tmpNameBuffer faild\n");
		ntStatus = STATUS_BUFFER_ALL_ZEROS;
		return ntStatus;
	}
	
	pCurrentListEntry = (PFILE_LIST_ENTRY)ExAllocatePoolWithTag(PagedPool,sizeof(FILE_LIST_ENTRY),'TSIL');
	if(!pCurrentListEntry)
	{
		DbgPrint("alloc memory with pCurrentListEntry faild\n");
		ntStatus = STATUS_BUFFER_ALL_ZEROS;
		return ntStatus;
	}
	
	RtlCopyMemory(tmpNameBuffer,uDirPath.Buffer,uDirPath.Length);
	tmpNameBuffer[uDirPath.Length / sizeof(WCHAR)] = L'\0';

	InitializeListHead(&listHeader);
	pCurrentListEntry->NameBuffer = tmpNameBuffer;
	InsertHeadList(&listHeader,&pCurrentListEntry->Entry);

	while(!IsListEmpty(&listHeader))//�����������������ʾÿ���ļ���
	{
		pCurrentListEntry = (PFILE_LIST_ENTRY)RemoveEntryList(&listHeader);
		//�����ǰҪɾ�����ļ��л�����һ��Ҫɾ���ģ�˵���ϴ�û�н���ɾ������Ҳ���Ǹ��ļ��в�Ϊ��
		if(pPreListEntry == pCurrentListEntry)
		{
			ntStatus = STATUS_DIRECTORY_NOT_EMPTY;
			break;
		}
		//���汾��Ҫɾ���ģ�����һ�α���ʱ���ο�
		pPreListEntry = pCurrentListEntry;
		
		RtlInitUnicodeString(&uTmpNameStr,pCurrentListEntry->NameBuffer);

		InitializeObjectAttributes(&objFileAttributes,
			&uTmpNameStr,
			OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
			NULL,
			NULL);

		ntStatus = ZwCreateFile(&hFileHandle,
			FILE_ANY_ACCESS,
			&objFileAttributes,
			&ioStatusBlock,
			NULL,
			0,
			0,
			FILE_OPEN,
			FILE_SYNCHRONOUS_IO_NONALERT,
			NULL,
			0);

		if(!NT_SUCCESS(ntStatus))
		{
			DbgPrint("ZwCreateFile failed(%zW):%x\n",&uTmpNameStr,ntStatus);
			break;
		}
		
		reStartScan = TRUE;//��ʾ��ѯ�ļ�ʱ���Ƿ�����ϴβ�ѯ��λ��
		//��ʼɨ��
		while(TRUE)
		{
			queryOutputBuffer = NULL;
			queryOutputBufferSize = 64;
			ntStatus = STATUS_BUFFER_OVERFLOW;

			while((ntStatus == STATUS_BUFFER_OVERFLOW) || 
				ntStatus == STATUS_INFO_LENGTH_MISMATCH)
			{
				if(queryOutputBuffer)
				{
					ExFreePool(queryOutputBuffer);
				}
				//���ڲ�����ǰ��ô洢�ļ�����Ϣ�ڴ�ĳ���
				//���ԣ���Ҫʹ�����ַ�ʽ�������ڴ棬��֤output�����ܹ�����
				queryOutputBufferSize *= 2;

				queryOutputBuffer = ExAllocatePoolWithTag(PagedPool,queryOutputBufferSize,'REUQ');
				if(!queryOutputBuffer)
				{
					DbgPrint("alloc memory queryOutputBuffer faild\n");
					//������break��̫�ðɣ�����Ĵ��뻹��ִ��...
					break;
				}

				ntStatus = ZwQueryDirectoryFile(hFileHandle,
					NULL,
					NULL,
					NULL,
					&ioStatusBlock,
					&queryOutputBuffer,
					queryOutputBufferSize,
					FileDirectoryInformation,
					FALSE,
					NULL,
					reStartScan);
			}

			if(ntStatus == STATUS_NO_MORE_FILES)
			{
				//������ҵ����ļ��е�ĩβ
				//�������ͷ��ڴ�
				ExFreePool(queryOutputBuffer);
				//���óɹ����
				ntStatus = STATUS_SUCCESS;
				break;
			}

			if(!NT_SUCCESS(ntStatus))
			{
				//�������ʾ����Ĳ�����ʧ��...
				//��ʾerror
				DbgPrint("ZwQueryDirectoryFile faild(%wZ):%x\n",pCurrentListEntry->NameBuffer,ntStatus);
				//���Ǳ������ͷ��ڴ棬������Ϊc����Ա�ں˿���Ҫ�μǵģ�"ʱ�̲���'��ʬ����...'"
				ExFreePool(queryOutputBuffer);
				break;
			}

			//���⣬��˵��һ�ж���׼������...
			pCurrentDirInfo = (PFILE_DIRECTORY_INFORMATION)queryOutputBuffer;

			//��ȡ��ǰĿ¼�µĵ�ǰ�ļ�
			//���·����ں˶�
			//�ϼ�Ŀ¼��+��ǰ�ļ���+L"\\"
			tmpNameBuffer = ExAllocatePoolWithTag(PagedPool,
				wcslen(pCurrentListEntry->NameBuffer) + sizeof(pCurrentDirInfo->FileName) + 2 * sizeof(WCHAR),
				'PMT');

			
			RtlZeroMemory(tmpNameBuffer,
				wcslen(pCurrentListEntry->NameBuffer) + sizeof(pCurrentDirInfo->FileName) + 2 * sizeof(WCHAR),
				);
			wcscpy(tmpNameBuffer,pCurrentListEntry->NameBuffer);
			wcscat(tmpNameBuffer,L"\\");
			//�������������?
			//wcscpy(tmpNameBuffer,pCurrentDirInfo->FileName);
			RtlCopyMemory(tmpNameBuffer,pCurrentDirInfo->FileName,pCurrentDirInfo->FileNameLength);

			RtlInitUnicodeString(&uTmpNameStr,tmpNameBuffer);

			//�õ��ļ���֮�����ж��ļ�����
			//������ļ�������...
			if(pCurrentDirInfo->FileAttributes | FILE_ATTRIBUTE_DIRECTORY)
			{
				if((pCurrentDirInfo->FileNameLength == sizeof (WCHAR)) && (pCurrentDirInfo->FileName[0] == L'.'))
				{

				}
				else if((pCurrentDirInfo->FileNameLength == 2 * sizeof(WCHAR)) 
					&& (pCurrentDirInfo->FileName[0] == L'.')
					&& (pCurrentDirInfo->FileName[1] == L'.'))
				{

				}
				else
				{
					//�������.��..����ô�����ļ������Ʋ��������Ⱥ͸��ļ���ͬ��Ŀ¼�µķ��ļ����ļ�ɾ����
					//����ɾ�����ļ������������
					//ע�⣬�����Ѿ���scaning...
					PFILE_LIST_ENTRY tmpListEntry;

					tmpListEntry = (PFILE_LIST_ENTRY)ExAllocatePoolWithTag(PagedPool,
						sizeof(FILE_LIST_ENTRY),
						"LPMT");
					if(!tmpListEntry)
					{
						DbgPrint("alloc memory tmpListEntry faild(%wZ)!\n",&uTmpNameStr);
						break;
					}

					tmpListEntry->NameBuffer = tmpNameBuffer;
					ExFreePool(tmpNameBuffer);
					InsertHeadList(&listHeader,&tmpListEntry->Entry);
				}
			}
			else
			{
				//���򣬷��ļ��У�ֱ�Ӹɵ�
				ntStatus = PBCDeleteFile(tmpNameBuffer);
				if(!NT_SUCCESS(ntStatus))
				{
					DbgPrint("PBCDeleteFile faild:%x\n",ntStatus);
					ExFreePool(tmpNameBuffer);
					ExFreePool(queryOutputBuffer);
					break;
				}
			}

			ExFreePool(queryOutputBuffer);
			if(tmpNameBuffer)
			{
				ExFreePool(tmpNameBuffer);
			}
		}//while(TRUE)�������ĵ�ɨ����������������
	
		//ִ���������˵��ǰ�ļ����µ����з��ļ������͵����ļ��ѱ�ɾ��
		//��Ҫɾ��ǰ�ļ�����
		if(NT_SUCCESS(ntStatus))//��Ҫ�ж�һ������Ĳ�����û�г���ĵط�
		{
			fileDisInfo.DeleteFile = TRUE;
			ntStatus = ZwSetInformationFile(hFileHandle,
				&ioStatusBlock,
				&fileDisInfo,
				sizeof(FILE_DISPOSITION_INFORMATION),
				FileDispositionInformation);
			if(!NT_SUCCESS(ntStatus))
			{
				//˵�����ļ����»������ļ���
				//����Ǵ������ĸ�Ŀ¼����ô�Ͳ���ӡ������Ϣ��
				UNICODE_STRING uCompStr = {0x00};
				RtlInitUnicodeString(&uCompStr,pCurrentListEntry->NameBuffer);
				if(RtlCompareUnicodeString(&uCompStr,&uDirPath,TRUE) != 0)
				{
					DbgPrint("ZwSetInformationFile fail(%ws):%x\n",pCurrentListEntry->NameBuffer,ntStatus);
					break;
				}
			}
		}

		ZwClose(hFileHandle);

		if(NT_SUCCESS(ntStatus))
		{
			//ִ�е��ˣ�˵����ǰ�ļ����ѱ�ɾ��
			//����ǰ�ļ��������������Ƴ�
			RemoveHeadList(&listHeader);
			ExFreePool(pCurrentListEntry->NameBuffer);
			ExFreePool(pCurrentListEntry);
		}
		//�����ʧ�ܣ�˵�����ļ����»������ļ��У���ô����ͷ��λ�ô�����¼�Ŀ¼��
		//Ҳ���ǲ��ܴ��������Ƴ���
		//��ȥɾ�����ļ���

	}//while(!IsListEmpty(&listHeader)) �����������ڽ������

	//����ڶ��������һ���ͷ�
	while(!IsListEmpty(&listHeader))
	{
		PFILE_LIST_ENTRY freeListEntry = (PFILE_LIST_ENTRY)RemoveHeadList(&listHeader);
		ExFreePool(freeListEntry->NameBuffer);
		ExFreePool(freeListEntry);
	}

	return ntStatus;


}

NTSTATUS PBCDeleteFile(WCHAR * szFileName)
{
	UNICODE_STRING uFileName = {0x00};
	OBJECT_ATTRIBUTES objFileAttributes = {0x00};
	HANDLE hFileHandle = NULL;
	NTSTATUS ntStatus;
	IO_STATUS_BLOCK ioStatusBlock = {0x00};
	FILE_DISPOSITION_INFORMATION fileDisInfo = {0x00};

	RtlInitUnicodeString(&uFileName,szFileName);

	InitializeObjectAttributes(&objFileAttributes,
		&uFileName,
		OBJ_CASE_INSENSITIVE,
		NULL,
		NULL);
	
	ntStatus = ZwCreateFile(&hFileHandle,
		SYNCHRONIZE | FILE_WRITE_ATTRIBUTES | DELETE,
		&objFileAttributes,
		&ioStatusBlock,
		NULL,
		FILE_ATTRIBUTE_NORMAL,
		FILE_SHARE_READ | FILE_SHARE_WRITE |FILE_SHARE_DELETE,
		FILE_OPEN,
		FILE_SYNCHRONOUS_IO_NONALERT |FILE_DELETE_ON_CLOSE,
		NULL,
		0);
	if(!NT_SUCCESS(ntStatus))
	{
		if(ntStatus == STATUS_ACCESS_DENIED)
		{
			ntStatus = ZwCreateFile(&hFileHandle,
				SYNCHRONIZE | FILE_WRITE_ACCESS | FILE_READ_ATTRIBUTES,
				&objFileAttributes,
				&ioStatusBlock,
				0,
				FILE_ATTRIBUTE_NORMAL,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				FILE_OPEN,
				FILE_SYNCHRONOUS_IO_NONALERT | FILE_DELETE_ON_CLOSE,
				NULL,
				0);
			
			if(NT_SUCCESS(ntStatus))
			{
				FILE_BASIC_INFORMATION fileBasicInfo = {0x00};

				ntStatus = ZwQueryInformationFile(hFileHandle,
					&ioStatusBlock,
					&fileBasicInfo,
					sizeof(FILE_BASIC_INFORMATION),
					FileBasicInformation);
				if(!NT_SUCCESS(ntStatus))
				{
					DbgPrint("ZwQueryInformationFile faild:%x\n",ntStatus);
				}

				//ȥֻ��
				fileBasicInfo.FileAttributes = FILE_ATTRIBUTE_NORMAL;
				ntStatus = ZwSetInformationFile(hFileHandle,
					&ioStatusBlock,
					&fileBasicInfo,
					sizeof(FILE_BASIC_INFORMATION),
					FileBasicInformation);
				if(!NT_SUCCESS(ntStatus))
				{
					DbgPrint("ZwSetInformationFile faild:%x\n",ntStatus);
				}

				ZwClose(hFileHandle);
				ntStatus = ZwCreateFile(&hFileHandle,
					SYNCHRONIZE | FILE_WRITE_ATTRIBUTES | DELETE,
					&objFileAttributes,
					&ioStatusBlock,
					NULL,
					FILE_ATTRIBUTE_NORMAL,
					FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
					FILE_OPEN,
					FILE_SYNCHRONOUS_IO_NONALERT |FILE_DELETE_ON_CLOSE,
					NULL,
					0);
			}

		}

	}

	if(!NT_SUCCESS(ntStatus))
	{
		return ntStatus;
	}

	fileDisInfo.DeleteFile = TRUE;
	ntStatus = ZwSetInformationFile(hFileHandle,
		&ioStatusBlock,
		&fileDisInfo,
		sizeof(FILE_DISPOSITION_INFORMATION),
		FileDispositionInformation);
	
	if(!NT_SUCCESS(ntStatus))
	{
		DbgPrint("ZwSetInformationFile faild:%x\n",ntStatus);
	}

	ZwClose(hFileHandle);
	return ntStatus;

}