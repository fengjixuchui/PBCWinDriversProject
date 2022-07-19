#include <ntddk.h>
#include <windef.h>

NTSTATUS PBCUncToLocal(__in PUNICODE_STRING,__out PUNICODE_STRING);

WCHAR *PBCUnicodeStringChar(PUNICODE_STRING,WCHAR);

WCHAR *PBCUnicodeStrStr(PUNICODE_STRING,PUNICODE_STRING);

NTSTATUS DriverUnload(PDRIVER_OBJECT pDriverObject)
{
	DbgPrint("PBCUncToLocal:goodbye!\n");

	return STATUS_SUCCESS;
}


NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject,PUNICODE_STRING pDriverPath)
{
	NTSTATUS ntStatus = STATUS_SUCCESS;
	DECLARE_UNICODE_STRING_SIZE(uSharePath,MAX_PATH + 64);
	DECLARE_UNICODE_STRING_SIZE(uLocalPath,MAX_PATH + 64);
	//UNICODE_STRING uSharePath = {0x00};
	//UNICODE_STRING uLocalPath = {0x00};
	
	DbgPrint("PBCUncToLocal:hello\n");
	pDriverObject->DriverUnload = DriverUnload;
	
	RtlInitUnicodeString(&uSharePath,L"12\\p0sixB1ackcatDeleteFile.sys");

	ntStatus = PBCUncToLocal(&uSharePath,&uLocalPath);
	
	if(!NT_SUCCESS(ntStatus))
	{
		DbgPrint("ת��ʧ��,errorCode:%x\n",ntStatus);
	}
	else
	{
		DbgPrint("%wZ ---> %wZ",&uSharePath,&uLocalPath);
	}

	return STATUS_SUCCESS;
}

NTSTATUS PBCUncToLocal(__in PUNICODE_STRING uPsharePath,__out PUNICODE_STRING uPlocalPath)
{
	NTSTATUS ntStatus;
	WCHAR *pTmp;
	//UNICODE_STRING  uShareNameStr;
	//UNICODE_STRING uFileNameStr;
	UNICODE_STRING uRegTableName = {0x00};
	DECLARE_UNICODE_STRING_SIZE(uFileNameStr,MAX_PATH + 64);
	DECLARE_UNICODE_STRING_SIZE(uShareNameStr,MAX_PATH + 64);
	OBJECT_ATTRIBUTES objRegTableAttributes = {0x00};
	HANDLE hRegTableHandle = NULL;
	ULONG resultSize;
	PKEY_VALUE_PARTIAL_INFORMATION pKeyValuesBuffer = NULL;
	UNICODE_STRING uKeyValueStr = {0x00};

	//12\\p0sixB1ackcatDeleteFile.sys
	//12��Ϊ����·������p0six...sys��Ϊ�ļ���
	//ͨ��pTmpָ�룬������·�������ļ�����ֳ���

	pTmp = PBCUnicodeStringChar(uPsharePath,'\\');

	//�ڹ���·���л�ȡ����·������Ҳ���ǹ����ļ�������Ŀ¼·��
	uShareNameStr.Length = (USHORT)((ULONG)pTmp - (ULONG)uPsharePath->Buffer);
	RtlCopyMemory(uShareNameStr.Buffer,uPsharePath->Buffer,uShareNameStr.Length);

	//�ļ����ĳ��ȵ�Ȼ���ܵĳ��ȼ�ȥǰ���·��������
	uFileNameStr.Length = (USHORT)((ULONG)uPsharePath->Length - uShareNameStr.Length);
	RtlCopyMemory(uFileNameStr.Buffer,pTmp,uFileNameStr.Length);

	//Ȼ�󣬴�ע������λ��...
	RtlInitUnicodeString(&uRegTableName,L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Services\\LanmanServer\\Shares");

	InitializeObjectAttributes(&objRegTableAttributes,
		&uRegTableName,
		OBJ_CASE_INSENSITIVE,
		NULL,
		NULL);

	ntStatus = ZwOpenKey(&hRegTableHandle,
		KEY_ALL_ACCESS,
		&objRegTableAttributes);
	if(!NT_SUCCESS(ntStatus))
	{
		return ntStatus;
	}

	//��֮�󣬸����ļ�����ȥע����Ŀ¼��\\..\\shares�У������ļ�����Ӧ��valueKey
	//2�β�ѯ����һ�λ�ȡʵ��output���ȣ��ڶ��β�����Ļ�ȡ����
	ntStatus = ZwQueryValueKey(hRegTableHandle,
		&uShareNameStr,
		KeyValuePartialInformation,
		NULL,
		0,
		&resultSize);

	if(ntStatus != STATUS_BUFFER_OVERFLOW && ntStatus != STATUS_BUFFER_TOO_SMALL && !NT_SUCCESS(ntStatus))
	{
		return ntStatus;
	}

	pKeyValuesBuffer = (PKEY_VALUE_PARTIAL_INFORMATION)ExAllocatePoolWithTag(PagedPool,resultSize,'VK');
	if(!pKeyValuesBuffer)
	{
		DbgPrint("alloc memory faild\n");
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	ntStatus = ZwQueryValueKey(hRegTableHandle,
		&uShareNameStr,
		KeyValuePartialInformation,
		pKeyValuesBuffer,
		resultSize,
		&resultSize);
	if(!NT_SUCCESS(ntStatus))
	{
		DbgPrint("ZwQueryValueKey2 faild\n");
		ExFreePool(pKeyValuesBuffer);
		return ntStatus;
	}

	if(pKeyValuesBuffer->Type != REG_MULTI_SZ)
	{
		DbgPrint("pKeyValueBuffer->type is not REG_MULTI_SZ\n");
		ExFreePool(pKeyValuesBuffer);
		ZwClose(hRegTableHandle);
		return !STATUS_SUCCESS;
	}


	//����ͻ��õ�һ���ȽϾ����͵��ַ����㷨�ˣ�������·���������valueKey���ַ����г�ȡ����
	/*
		���磺
		CSCFlags=2048
		MaxUses=4294967295
		Path=C:\12
		Permissions=0
		Remark=
		ShareName=12
		Type=0
		
	*/
	//�������ַ�����C:\12��ȡ������ע�⣬����Ӳ����

	uKeyValueStr.Buffer = (WCHAR *)pKeyValuesBuffer->Data;
	uKeyValueStr.MaximumLength = uKeyValueStr.Length = (USHORT)pKeyValuesBuffer->DataLength;

	RtlInitUnicodeString(&uShareNameStr,L"path=");

	//����path=\\.....���׵�ַ
	pTmp = PBCUnicodeStrStr(&uKeyValueStr,&uShareNameStr);

	if(!pTmp)
	{
		ExFreePool(pKeyValuesBuffer);
		ZwClose(hRegTableHandle);
		return !STATUS_SUCCESS;
	}

	//Ҫ��ס���ַ����Ĺ���"str1\nstr2\nstr3\0\0"
	//��ȡ����·�����������ļ���
	//���"path="ǰҪ��L
	RtlInitUnicodeString(&uShareNameStr,pTmp + wcslen(L"path="));

	//������·�������ļ�������ƴ��
	RtlCopyUnicodeString(uPlocalPath,&uShareNameStr);
	RtlAppendUnicodeStringToString(uPlocalPath,&uFileNameStr);

	ExFreePool(pKeyValuesBuffer);
	ZwClose(hRegTableHandle);
	ntStatus = STATUS_SUCCESS;

	return ntStatus;

}

WCHAR *PBCUnicodeStringChar(PUNICODE_STRING uSourceStr,WCHAR referenceCh)
{
	//���ȱ����ˣ��ַ����е�ÿ���ַ�ռ2���ֽ�
	//���ԣ�����������ĳ��ȵ�һ��ͺ���
	ULONG souSize = uSourceStr->Length >> 1;
	ULONG index;

	for(index = 0; index < souSize; index++)
	{
		if(*(uSourceStr->Buffer+index) == referenceCh)
		{
			return uSourceStr->Buffer + index;
		}
	}
	return NULL;

}

WCHAR *PBCUnicodeStrStr(PUNICODE_STRING uSource,PUNICODE_STRING uReferenceStr)
{
	ULONG loopNums;
	ULONG index;
	UNICODE_STRING uStr1;
	UNICODE_STRING uStr2;

	if(uSource->Length < uReferenceStr->Length)
	{
		return NULL;
	}

	loopNums = ((uSource->Length - uReferenceStr->Length) >> 1) + 1;

	for(index = 0; index < loopNums; index++)
	{
		uStr1.MaximumLength = uStr1.Length = uReferenceStr->Length;
		uStr2.MaximumLength = uStr2.Length = uReferenceStr->Length;
		uStr1.Buffer = uSource->Buffer + index;
		uStr2.Buffer = uReferenceStr->Buffer;
		
		if(RtlCompareUnicodeString(&uStr1,&uStr2,TRUE) == 0)
		{
			return uSource->Buffer + index;
		}

	}

	return NULL;


}