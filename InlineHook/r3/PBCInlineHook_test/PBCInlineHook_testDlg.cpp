
// PBCInlineHook_testDlg.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "PBCInlineHook_test.h"
#include "PBCInlineHook_testDlg.h"
#include "afxdialogex.h"
#include <winioctl.h>

#define PBCBASECTLCODE 0x800

#define PBCCTLCODE(i) CTL_CODE(FILE_DEVICE_UNKNOWN, PBCBASECTLCODE + i, METHOD_BUFFERED, FILE_ALL_ACCESS)

#define PBCINLINEHOOK_CTL PBCCTLCODE(1)
#define PBCKILLTHREADBYAPC_CTL PBCCTLCODE(2)

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// ����Ӧ�ó��򡰹��ڡ��˵���� CAboutDlg �Ի���

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// �Ի�������
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

// ʵ��
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CPBCInlineHook_testDlg �Ի���



CPBCInlineHook_testDlg::CPBCInlineHook_testDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_PBCINLINEHOOK_TEST_DIALOG, pParent)
	, m_FilePath(_T(""))
	, m_errorstr(_T(""))
	, m_ThreadId(_T(""))
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CPBCInlineHook_testDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT1, m_FilePath);
	DDX_Text(pDX, IDC_EDIT2, m_errorstr);
	DDX_Text(pDX, IDC_THREAD_EDIT, m_ThreadId);
}

BEGIN_MESSAGE_MAP(CPBCInlineHook_testDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_DROPFILES()
	ON_BN_CLICKED(IDC_LOADDRIBUTTON, &CPBCInlineHook_testDlg::OnBnClickedLoaddributton)
	ON_BN_CLICKED(IDC_UnInstallDriverButton, &CPBCInlineHook_testDlg::OnBnClickedUninstalldriverbutton)
	ON_BN_CLICKED(IDC_SendIoCtrlButton, &CPBCInlineHook_testDlg::OnBnClickedSendioctrlbutton)
	ON_BN_CLICKED(IDOK, &CPBCInlineHook_testDlg::OnBnClickedOk)
END_MESSAGE_MAP()


// CPBCInlineHook_testDlg ��Ϣ�������

BOOL CPBCInlineHook_testDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// ��������...���˵�����ӵ�ϵͳ�˵��С�

	// IDM_ABOUTBOX ������ϵͳ���Χ�ڡ�
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// ���ô˶Ի����ͼ�ꡣ  ��Ӧ�ó��������ڲ��ǶԻ���ʱ����ܽ��Զ�
	//  ִ�д˲���
	SetIcon(m_hIcon, TRUE);			// ���ô�ͼ��
	SetIcon(m_hIcon, FALSE);		// ����Сͼ��

	// TODO: �ڴ���Ӷ���ĳ�ʼ������

	return TRUE;  // ���ǽ��������õ��ؼ������򷵻� TRUE
}

void CPBCInlineHook_testDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// �����Ի��������С����ť������Ҫ����Ĵ���
//  �����Ƹ�ͼ�ꡣ  ����ʹ���ĵ�/��ͼģ�͵� MFC Ӧ�ó���
//  �⽫�ɿ���Զ���ɡ�

void CPBCInlineHook_testDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // ���ڻ��Ƶ��豸������

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// ʹͼ���ڹ����������о���
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// ����ͼ��
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//���û��϶���С������ʱϵͳ���ô˺���ȡ�ù��
//��ʾ��
HCURSOR CPBCInlineHook_testDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}




void CPBCInlineHook_testDlg::OnDropFiles(HDROP hDropInfo)
{
	// TODO: �ڴ������Ϣ�����������/�����Ĭ��ֵ
	// TODO: Add your message handler code here and/or call default
	UINT count;
	TCHAR filePath[MAX_PATH] = { 0 };

	count = DragQueryFile(hDropInfo, 0xFFFFFFFF, NULL, 0);
	if (count == 1)
	{
		DragQueryFile(hDropInfo, 0, filePath, sizeof(filePath));
		m_FilePath = filePath;
		//CheckNow(m_strPath);
		UpdateData(FALSE);
		DragFinish(hDropInfo);

		CDialog::OnDropFiles(hDropInfo);
		return;

	}
	else
	{
		//m_vectorFile.clear();
		for (UINT i = 0; i < count; i++)
		{
			int pathLen = DragQueryFile(hDropInfo, i, filePath, sizeof(filePath));
			//m_strPath = filePath;
			//m_vectorFile.push_back(filePath);
			//break;
		}
		//DoAllCheck();
		//CheckNow(m_strPath);
		UpdateData(FALSE);
		DragFinish(hDropInfo);
	}

    

	CDialogEx::OnDropFiles(hDropInfo);
}

BOOL LoadDriver(TCHAR* lpszDriverName, TCHAR* lpszDriverPath,CString &errorstr)
{
	TCHAR szDriverImagePath[256] = { 0 };
	//�õ�����������·��
	GetFullPathName(lpszDriverPath, 256, szDriverImagePath, NULL);

	BOOL bRet = FALSE;

	SC_HANDLE hServiceMgr = NULL;//SCM�������ľ��
	SC_HANDLE hServiceDDK = NULL;//NT��������ķ�����

	//�򿪷�����ƹ�����
	hServiceMgr = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	
	if (hServiceMgr == NULL)
	{
		//OpenSCManagerʧ��
		//printf( "OpenSCManager() Failed %d ! \n", GetLastError() );
		bRet = FALSE;
		errorstr = "OpenSCManager faild!";
		goto BeforeLeave;
	}
	else
	{
		////OpenSCManager�ɹ�
		printf("OpenSCManager() ok ! \n");
	}

	//������������Ӧ�ķ���
	hServiceDDK = CreateService(hServiceMgr,
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
	if (hServiceDDK == NULL)
	{
		dwRtn = GetLastError();
		if (dwRtn != ERROR_IO_PENDING && dwRtn != ERROR_SERVICE_EXISTS)
		{
			//��������ԭ�򴴽�����ʧ��
			//printf( "CrateService() Failed %d ! \n", dwRtn ); 
			errorstr = "other error!";
			bRet = FALSE;
			goto BeforeLeave;
			
		}
		else
		{
			//���񴴽�ʧ�ܣ������ڷ����Ѿ�������
			printf("CrateService() Faild Service is ERROR_IO_PENDING or ERROR_SERVICE_EXISTS! \n");
			errorstr = "service is created!\n";
		}

		// ���������Ѿ����أ�ֻ��Ҫ��  
		hServiceDDK = OpenService(hServiceMgr, lpszDriverName, SERVICE_ALL_ACCESS);
		if (hServiceDDK == NULL)
		{
			//����򿪷���Ҳʧ�ܣ�����ζ����
			dwRtn = GetLastError();
			//printf( "OpenService() Failed %d ! \n", dwRtn );  
			bRet = FALSE;
			errorstr = "OpenService faild!";
			goto BeforeLeave;
		}
		else
		{
			//printf( "OpenService() ok ! \n" );
		}
	}
	else
	{
		//printf( "CrateService() ok ! \n" );
	}

	//�����������
	bRet = StartService(hServiceDDK, NULL, NULL);
	if (!bRet)
	{
		DWORD dwRtn = GetLastError();
		if (dwRtn != ERROR_IO_PENDING && dwRtn != ERROR_SERVICE_ALREADY_RUNNING)
		{
			//printf( "StartService() Failed %d ! \n", dwRtn );  
			bRet = FALSE;
			errorstr = "start service faild!";
			goto BeforeLeave;
		}
		else
		{
			if (dwRtn == ERROR_IO_PENDING)
			{
				//�豸����ס
				//printf( "StartService() Failed ERROR_IO_PENDING ! \n");
				errorstr = "device is pending!";
				bRet = FALSE;
				goto BeforeLeave;
			}
			else
			{
				//�����Ѿ�����
				//printf( "StartService() Failed ERROR_SERVICE_ALREADY_RUNNING ! \n");
				bRet = TRUE;
				errorstr = "service is already running!";
				goto BeforeLeave;
			}
		}
	}
	bRet = TRUE;
	//�뿪ǰ�رվ��
BeforeLeave:
	if (hServiceDDK)
	{
		CloseServiceHandle(hServiceDDK);
	}
	if (hServiceMgr)
	{
		CloseServiceHandle(hServiceMgr);
	}
	return bRet;
}

void CPBCInlineHook_testDlg::OnBnClickedLoaddributton()
{
	if (m_FilePath.GetLength() > 1)
	{
		TCHAR *T_Sysname, *T_SysPath;
		T_Sysname = (TCHAR *)malloc(sizeof(TCHAR)*MAX_PATH);
		T_SysPath = (TCHAR *)malloc(sizeof(TCHAR)*MAX_PATH);
		memset(T_Sysname, 0x00, sizeof(TCHAR)*MAX_PATH);
		memset(T_SysPath, 0x00, sizeof(TCHAR)*MAX_PATH);
		
		//wcstombs(T_SysPath, m_FilePath, m_FilePath.GetLength());
		_memccpy(T_SysPath, m_FilePath,sizeof(TCHAR)*MAX_PATH,sizeof(TCHAR)*MAX_PATH);
		//wcscpy_s(T_SysPath,MAX_PATH, m_FilePath);
		TCHAR *p1, *p2;
		p1 = T_SysPath;
		p2 = NULL;
		while (*p1 != '\0')
		{
			if (*p1++ == '\\')
			{
				p2 = p1;
			}
			
		}
		if (!p2)
		{
			printf("file path error!\n");
			goto ret;
			return;
		}

		size_t copysize = (size_t)((wcslen(p2) * 2) - (4 * sizeof(TCHAR)));
		
		if (copysize >= MAX_PATH || copysize < 0)
		{
			copysize = 18;
		}

		
		_memccpy(T_Sysname, p2, copysize, copysize);
		BOOL bLoad = FALSE;
		
		bLoad = LoadDriver(T_Sysname, T_SysPath, m_errorstr);
		if (bLoad == FALSE)
		{
			UpdateData(FALSE);
			char buffer[100];
			sprintf_s(buffer, 100, "loading driver faild:len is %d,p2 is %ws!", copysize,p2);
			MessageBoxA(0, buffer,"message", 0);
			goto ret;
		}
		m_errorstr = "load driver success!";
		UpdateData(FALSE);
	ret:
		free(T_Sysname);
		free(T_SysPath);
		T_Sysname = T_SysPath = NULL;
	}
}

BOOL UnloadDriver(TCHAR * szSvrName,CString &errorstr)
{
	BOOL bRet = FALSE;
	SC_HANDLE hServiceMgr = NULL;//SCM�������ľ��
	SC_HANDLE hServiceDDK = NULL;//NT��������ķ�����
	SERVICE_STATUS SvrSta;
	//��SCM������
	hServiceMgr = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (hServiceMgr == NULL)
	{
		//����SCM������ʧ��
		printf("OpenSCManager() Failed %d ! \n", GetLastError());
		errorstr = "OpenSCManager faild!";
		bRet = FALSE;
		goto BeforeLeave;
	}
	else
	{
		//����SCM������ʧ�ܳɹ�
		printf("OpenSCManager() ok ! \n");
	}
	//����������Ӧ�ķ���
	hServiceDDK = OpenService(hServiceMgr, szSvrName, SERVICE_ALL_ACCESS);

	if (hServiceDDK == NULL)
	{
		//����������Ӧ�ķ���ʧ��
		printf("OpenService() Failed %d ! \n", GetLastError());
		bRet = FALSE;
		errorstr = "OpenService faild!";
		goto BeforeLeave;
	}
	else
	{
		printf("OpenService() ok ! \n");
	}
	//ֹͣ�����������ֹͣʧ�ܣ�ֻ�������������ܣ��ٶ�̬���ء�  
	if (!ControlService(hServiceDDK, SERVICE_CONTROL_STOP, &SvrSta))
	{
		printf("ControlService() Failed %d !\n", GetLastError());
		errorstr = "ControlService faild!";
	}
	else
	{
		//����������Ӧ��ʧ��
		printf("ControlService() ok !\n");
	}
	//��̬ж����������  
	if (!DeleteService(hServiceDDK))
	{
		//ж��ʧ��
		printf("DeleteSrevice() Failed %d !\n", GetLastError());
		errorstr = "DeleteService faild!";
	}
	else
	{
		//ж�سɹ�
		printf("DelServer:eleteSrevice() ok !\n");
	}
	bRet = TRUE;
BeforeLeave:
	//�뿪ǰ�رմ򿪵ľ��
	if (hServiceDDK)
	{
		CloseServiceHandle(hServiceDDK);
	}
	if (hServiceMgr)
	{
		CloseServiceHandle(hServiceMgr);
	}
	return bRet;
}

void CPBCInlineHook_testDlg::OnBnClickedUninstalldriverbutton()
{
	TCHAR *lptSysPath, *p1, *p2;
	TCHAR *lptSysName = NULL;
	BOOL bIsUnload = FALSE;
	if (m_FilePath.GetLength() <= 0)
	{
		MessageBoxA(0,"path is NULL!","message",0);
		return;
	}

	lptSysPath = (TCHAR *)malloc(sizeof(TCHAR)*MAX_PATH);
	if (!lptSysPath)
	{
		MessageBoxA(0,"Unistall vaild!","message",0);
		goto ret;
	}

	_memccpy(lptSysPath, m_FilePath, sizeof(TCHAR)*MAX_PATH, sizeof(TCHAR)*MAX_PATH);
	p1 = lptSysPath;
	p2 = NULL;
	while (*p1 != '\0')
	{
		if (*p1++ == '\\')
		{
			p2 = p1;
		}
	}

	if (p2)
	{
		lptSysName = (TCHAR *)malloc(sizeof(TCHAR) * MAX_PATH);
		memset(lptSysName, 0x00, sizeof(TCHAR) * MAX_PATH);
		_memccpy(lptSysName, p2, sizeof(TCHAR)*MAX_PATH, wcslen(p2) * sizeof(TCHAR) - 4 * sizeof(TCHAR));

		
		bIsUnload = UnloadDriver(lptSysName, m_errorstr);
		if (!bIsUnload)
		{
			UpdateData(FALSE);
			MessageBoxA(0, "UnInstall Driver faild!", "message", 0);
			goto ret;
		}
		m_errorstr = "UnInstall Driver Success!";
		UpdateData(FALSE);

	}
	ret:
	if (lptSysPath)
	{
		free(lptSysPath);
		lptSysPath = NULL;
	}
	if (lptSysName)
	{
		free(lptSysName);
		lptSysName = NULL;
	}

}


void CPBCInlineHook_testDlg::OnBnClickedSendioctrlbutton()
{
	BOOL DeviceIoCtrlResult = FALSE;
	DWORD errorCode;
	ULONG BufferSize = sizeof(TCHAR) * 0x100;
	TCHAR *inputBuffer = (TCHAR *)malloc(BufferSize);
	TCHAR *outputBuffer = (TCHAR *)malloc(BufferSize);
	DWORD dwResult = 0;
	if (!inputBuffer || !outputBuffer)
	{
		MessageBoxA(0, "malloc faild!", "message", 0);
		return;
	}
	
	// TODO: �ڴ���ӿؼ�֪ͨ����������
	HANDLE hFile = CreateFile(_T("\\\\.\\PBCSsdtHook")
	,GENERIC_WRITE | GENERIC_READ
	,0
	,NULL
	,OPEN_EXISTING
	,0
	,NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		errorCode = GetLastError();
		TCHAR lpszErrorBuffer[30] = {0x00};
		TCHAR *errorMsg = (TCHAR *)malloc(sizeof(TCHAR) * MAX_PATH);
		memset(errorMsg, 0x00, sizeof(TCHAR) * MAX_PATH);
		_memccpy(errorMsg, m_FilePath, wcslen(m_FilePath) * sizeof(TCHAR), sizeof(TCHAR) * MAX_PATH);
		wcscat(errorMsg, _T(":CreateFile error = "));
		wsprintfW(lpszErrorBuffer, _T("0x%x!"), errorCode);
		wcscat(errorMsg, lpszErrorBuffer);
		MessageBoxW( errorMsg, (TCHAR *)"message", 0);
		return;
	}

	
	DeviceIoCtrlResult = DeviceIoControl(hFile
		, PBCINLINEHOOK_CTL
		, inputBuffer
		, BufferSize
		, outputBuffer
		, BufferSize
		, &dwResult
		, NULL);
	if (!DeviceIoCtrlResult)
	{
		errorCode = GetLastError();
		TCHAR errorBuffer[100] = {0x00};
		wsprintfW(errorBuffer, _T("%ws:0x%x"), _T("DeviceIoCtrl faild"), errorCode);
		MessageBoxW(errorBuffer, _T("message"), 0);
		goto ret;
	}

	if (inputBuffer)
	{
		free(inputBuffer);
		inputBuffer = NULL;
	}
	if (outputBuffer)
	{
		free(outputBuffer);
		outputBuffer = NULL;
	}
	ret:
	CloseHandle(hFile);
}


void CPBCInlineHook_testDlg::OnBnClickedOk()
{
	// TODO: �ڴ���ӿؼ�֪ͨ����������
	ULONG Status = ERROR_SUCCESS;
	HANDLE hDevice = 0;
	BYTE inputBuffer[20] = {0};
	ULONG dwRetLen = 0;
	LPWSTR p = NULL;
	BYTE *b = NULL;
	ULONG i = 0;

	UpdateData(TRUE);

	if (!m_ThreadId || !m_ThreadId.GetLength())
	{
		MessageBox(L"�������߳�ID",L"Message");
		return;
	}

	hDevice = CreateFile(L"\\\\.\\PBCDriver",GENERIC_READ | GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
	if (hDevice == INVALID_HANDLE_VALUE)
	{
		Status = GetLastError();
		MessageBox(L"CreateFile Fail!\n");
		return;
	}

	p = m_ThreadId.GetBuffer();
	b = inputBuffer;

	while(i < m_ThreadId.GetLength())
	{
		b[i] = p[i];
		b[i] -= '0';
		++i;
	}

	Status = DeviceIoControl(hDevice, PBCKILLTHREADBYAPC_CTL, inputBuffer, sizeof(inputBuffer), NULL, 0, &dwRetLen, NULL);

	if (!Status)
	{
		MessageBox(L"DeviceIoControl fail!\n");
		return;
	}


}
