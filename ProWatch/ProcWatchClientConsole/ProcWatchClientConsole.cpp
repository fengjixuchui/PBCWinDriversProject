// ProcWatchClientConsole.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "windows.h"
#include "winioctl.h"
#include "stdio.h"

BOOL LoadDriver(char* lpszDriverName,char* lpszDriverPath);
BOOL UnloadDriver(char * szSvrName);


#define EVENT_NAME    L"\\global\\PBCProcWatch"
#define DRIVER_NAME	  "PBCProcWatch"
#define DRIVER_PATH	  ".\\PBCProcWatch.sys"

#define		CTRLCODE_BASE 0x8000
#define		MYCTRL_CODE(i) \
CTL_CODE(FILE_DEVICE_UNKNOWN,CTRLCODE_BASE + i, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define		IOCTL_PROCWATCH		MYCTRL_CODE(0)

typedef struct _ProcMonData
{
    HANDLE  hParentId;
    HANDLE  hProcessId;
    BOOLEAN bCreate;
}ProcMonData, *PProcMonData;



int main(int argc, char* argv[])
{
	printf("this is chaged!\n");
	ProcMonData pmdInfoNow = {0};
	ProcMonData pmdInfoBefore = {0};
	
	if (!LoadDriver(DRIVER_NAME, DRIVER_PATH))
	{
		printf("LoadDriver Failed:%x\n", GetLastError());
		return -1;
	}
    // �������豸����
    HANDLE hDriver = ::CreateFile(
		"\\\\.\\PBCProcWatch",
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
    if (hDriver == INVALID_HANDLE_VALUE)
    {
        printf("Open device failed:%x\n", GetLastError());
		UnloadDriver(DRIVER_NAME);
        return -1;
    }
    // ���ں��¼�����
    HANDLE hProcessEvent = ::OpenEventW(SYNCHRONIZE, FALSE, EVENT_NAME);

    while (TRUE)
    {
        DWORD    dwRet	= 0;
        BOOL     bRet	= FALSE;
		
        ::WaitForSingleObject(hProcessEvent, INFINITE);
		
    //while (::WaitForSingleObject(hProcessEvent, INFINITE))
    //{
        //DWORD    dwRet	= 0;
        //BOOL     bRet	= FALSE;

        bRet = ::DeviceIoControl(
			hDriver,
			IOCTL_PROCWATCH,
			NULL,
			0,
			&pmdInfoNow,
			sizeof(pmdInfoNow),
			&dwRet,
			NULL);
        if (bRet)
        {
            if (pmdInfoNow.hParentId != pmdInfoBefore.hParentId || \
                pmdInfoNow.hProcessId != pmdInfoBefore.hProcessId || \
                pmdInfoNow.bCreate != pmdInfoBefore.bCreate)
            {
                if (pmdInfoNow.bCreate)
                {
                    printf("ProcCreated��PID = %d\n", (int)pmdInfoNow.hProcessId);
                } 
                else
                {
                    printf("ProcTeminated��PID = %d\n", (int)pmdInfoNow.hProcessId);
                }
                pmdInfoBefore = pmdInfoNow;
            }
        } 
        else
        {
            printf("Get Proc Info Failed��\n");
            break;
        }

		
    }

    ::CloseHandle(hDriver);
	UnloadDriver(DRIVER_NAME);

    return 0;
}

//װ��NT��������
BOOL LoadDriver(char* lpszDriverName,char* lpszDriverPath)
{
	char szDriverImagePath[256] = {0};
	//�õ�����������·��
	GetFullPathName(lpszDriverPath, 256, szDriverImagePath, NULL);

	BOOL bRet = FALSE;

	SC_HANDLE hServiceMgr=NULL;//SCM�������ľ��
	SC_HANDLE hServiceDDK=NULL;//NT��������ķ�����

	//�򿪷�����ƹ�����
	hServiceMgr = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );

	if( hServiceMgr == NULL )  
	{
		//OpenSCManagerʧ��
		printf( "OpenSCManager() Faild %d ! \n", GetLastError() );
		bRet = FALSE;
		goto BeforeLeave;
	}
	else
	{
		////OpenSCManager�ɹ�
		printf( "OpenSCManager() ok ! \n" );  
	}

	//������������Ӧ�ķ���
	hServiceDDK = CreateService( hServiceMgr,
		lpszDriverName, //�����������ע����е�����  
		lpszDriverName, // ע������������ DisplayName ֵ  
		SERVICE_ALL_ACCESS, // ������������ķ���Ȩ��  
		SERVICE_KERNEL_DRIVER,// ��ʾ���صķ�������������  
		SERVICE_DEMAND_START, // ע������������ Start ֵ  
		SERVICE_ERROR_IGNORE, // ע������������ ErrorControl ֵ  
		szDriverImagePath, // ע������������ ImagePath ֵ  
		NULL,  //GroupOrder HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\GroupOrderList
		NULL,  
		NULL,  
		NULL,  
		NULL);  

	DWORD dwRtn;
	//�жϷ����Ƿ�ʧ��
	if( hServiceDDK == NULL )  
	{  
		dwRtn = GetLastError();
		if( dwRtn != ERROR_IO_PENDING && dwRtn != ERROR_SERVICE_EXISTS )  
		{  
			//��������ԭ�򴴽�����ʧ��
			printf( "CrateService() Faild %d ! \n", dwRtn );  
			bRet = FALSE;
			goto BeforeLeave;
		}  
		else  
		{
			//���񴴽�ʧ�ܣ������ڷ����Ѿ�������
			printf( "CrateService() Faild Service is ERROR_IO_PENDING or ERROR_SERVICE_EXISTS! \n" );  
		}

		// ���������Ѿ����أ�ֻ��Ҫ��  
		hServiceDDK = OpenService( hServiceMgr, lpszDriverName, SERVICE_ALL_ACCESS );  
		if( hServiceDDK == NULL )  
		{
			//����򿪷���Ҳʧ�ܣ�����ζ����
			dwRtn = GetLastError();  
			printf( "OpenService() Faild %d ! \n", dwRtn );  
			bRet = FALSE;
			goto BeforeLeave;
		}  
		else 
		{
			printf( "OpenService() ok ! \n" );
		}
	}  
	else  
	{
		printf( "CrateService() ok ! \n" );
	}

	//�����������
	bRet= StartService( hServiceDDK, NULL, NULL );  
	if( !bRet )  
	{  
		DWORD dwRtn = GetLastError();  
		if( dwRtn != ERROR_IO_PENDING && dwRtn != ERROR_SERVICE_ALREADY_RUNNING )  
		{  
			printf( "StartService() Faild %d ! \n", dwRtn );  
			bRet = FALSE;
			goto BeforeLeave;
		}  
		else  
		{  
			if( dwRtn == ERROR_IO_PENDING )  
			{  
				//�豸����ס
				printf( "StartService() Faild ERROR_IO_PENDING ! \n");
				bRet = FALSE;
				goto BeforeLeave;
			}  
			else  
			{  
				//�����Ѿ�����
				printf( "StartService() Faild ERROR_SERVICE_ALREADY_RUNNING ! \n");
				bRet = TRUE;
				goto BeforeLeave;
			}  
		}  
	}
	bRet = TRUE;
//�뿪ǰ�رվ��
BeforeLeave:
	if(hServiceDDK)
	{
		CloseServiceHandle(hServiceDDK);
	}
	if(hServiceMgr)
	{
		CloseServiceHandle(hServiceMgr);
	}
	return bRet;
}

//ж����������  
BOOL UnloadDriver( char * szSvrName )  
{
	BOOL bRet = FALSE;
	//RTL_CRITICAL_SECTION
	SC_HANDLE hServiceMgr=NULL;//SCM�������ľ��
	SC_HANDLE hServiceDDK=NULL;//NT��������ķ�����
	SERVICE_STATUS SvrSta;
	//��SCM������
	hServiceMgr = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );  
	if( hServiceMgr == NULL )  
	{
		//����SCM������ʧ��
		printf( "OpenSCManager() Faild %d ! \n", GetLastError() );  
		bRet = FALSE;
		goto BeforeLeave;
	}  
	else  
	{
		//����SCM������ʧ�ܳɹ�
		printf( "OpenSCManager() ok ! \n" );  
	}
	//����������Ӧ�ķ���
	hServiceDDK = OpenService( hServiceMgr, szSvrName, SERVICE_ALL_ACCESS );  

	if( hServiceDDK == NULL )  
	{
		//����������Ӧ�ķ���ʧ��
		printf( "OpenService() Faild %d ! \n", GetLastError() );  
		bRet = FALSE;
		goto BeforeLeave;
	}  
	else  
	{  
		printf( "OpenService() ok ! \n" );  
	}  
	//ֹͣ�����������ֹͣʧ�ܣ�ֻ�������������ܣ��ٶ�̬���ء�  
	if( !ControlService( hServiceDDK, SERVICE_CONTROL_STOP , &SvrSta ) )  
	{  
		printf( "ControlService() Faild %d !\n", GetLastError() );  
	}  
	else  
	{
		//����������Ӧ��ʧ��
		printf( "ControlService() ok !\n" );  
	}  
	//��̬ж����������  
	if( !DeleteService( hServiceDDK ) )  
	{
		//ж��ʧ��
		printf( "DeleteSrevice() Faild %d !\n", GetLastError() );  
	}  
	else  
	{  
		//ж�سɹ�
		printf( "DelServer:deleteSrevice() ok !\n" );  
	}  
	bRet = TRUE;
BeforeLeave:
//�뿪ǰ�رմ򿪵ľ��
	if(hServiceDDK)
	{
		CloseServiceHandle(hServiceDDK);
	}
	if(hServiceMgr)
	{
		CloseServiceHandle(hServiceMgr);
	}
	return bRet;	
} 

