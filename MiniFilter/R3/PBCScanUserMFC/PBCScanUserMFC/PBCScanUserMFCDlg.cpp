
// PBCScanUserMFCDlg.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "PBCScanUserMFC.h"
#include "PBCScanUserMFCDlg.h"
#include "afxdialogex.h"
#include <fltUser.h>
#include "scanuk.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


#define SCANNER_DEFAULT_REQUEST_COUNT 5
#define SCANNER_DEFAULT_THREAD_COUNT 2
#define SCANNER_MAX_THREAD_COUNT  64

typedef struct _SCANNER_MESSAGE
{
	FILTER_MESSAGE_HEADER MessageHeader;

	SCANNER_NOTIFICATION Notification;

	OVERLAPPED Ovlp;

}SCANNER_MESSAGE,*PSCANNER_MESSAGE;

typedef struct _SCANNER_THREAD_CONTEXT
{
	HANDLE Port;
	HANDLE Completion;

}SCANNER_THREAD_CONTEXT,*PSCANNER_THREAD_CONTEXT;
 
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


// CPBCScanUserMFCDlg �Ի���



CPBCScanUserMFCDlg::CPBCScanUserMFCDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_PBCSCANUSERMFC_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CPBCScanUserMFCDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CPBCScanUserMFCDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDOK, &CPBCScanUserMFCDlg::OnBnClickedOk)
END_MESSAGE_MAP()


// CPBCScanUserMFCDlg ��Ϣ�������

BOOL CPBCScanUserMFCDlg::OnInitDialog()
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

void CPBCScanUserMFCDlg::OnSysCommand(UINT nID, LPARAM lParam)
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

void CPBCScanUserMFCDlg::OnPaint()
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
HCURSOR CPBCScanUserMFCDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

DWORD ScannerWorker(PSCANNER_THREAD_CONTEXT lpContext)
{
	HANDLE hPort;
	HANDLE hIoCompletion;


	return 1;
}

VOID ConnectR0Port(VOID)
{
	HRESULT hr;
	HANDLE hPort = NULL;
	HANDLE hIoCompletion = NULL;
	SCANNER_THREAD_CONTEXT context;
	ULONG i,j;
	HANDLE threads[SCANNER_DEFAULT_THREAD_COUNT] = {0x00};
	DWORD threadId = 0;
	PSCANNER_MESSAGE lpMessage;

	hr = FilterConnectCommunicationPort(ScannerPortName
		, 0
		, NULL
		, 0
		, NULL
		, &hPort);
	if (IS_ERROR(hr))
	{
		MessageBox(0,_T("�����ں˶˿�ʧ��"), _T("message"), 0);
		goto ret;
	}

	hIoCompletion = CreateIoCompletionPort(hPort
		, NULL
		, 0
		, SCANNER_DEFAULT_THREAD_COUNT);
	if (hIoCompletion == NULL)
	{
		MessageBox(0,_T("������ɶ˿�ʧ��!"), _T("message"), 0);
		goto ret;
	}

	FilterSendMessage(hPort, _T("QQ,360,txt,inf,doc,bat,cmd"), wcslen(_T("QQ,360,txt,inf,doc,bat,cmd")), NULL, 0, NULL);
	context.Port = hPort;
	context.Completion = hIoCompletion;

	for (i = 0; i < SCANNER_DEFAULT_THREAD_COUNT; ++i)
	{
		threads[i] = CreateThread(NULL
		,0
		,(LPTHREAD_START_ROUTINE)ScannerWorker
		,&context
		,0
		,&threadId);

		if (threads[i] == NULL)
		{
			printf("Create Thread Error:%d!\n",GetLastError());
			goto ret;
		}

		for (j = 0; j < SCANNER_DEFAULT_REQUEST_COUNT; ++j)
		{
			lpMessage = (PSCANNER_MESSAGE)malloc(sizeof(SCANNER_MESSAGE));
			if (!lpMessage)
			{
				printf("Malloc Error:%d!\n",GetLastError());
				goto ret;
			}

			memset(&lpMessage->Ovlp,0x00,sizeof(OVERLAPPED));

			hr = FilterGetMessage(hPort
				, &lpMessage->MessageHeader
				, sizeof(FILTER_MESSAGE_HEADER)
				, &lpMessage->Ovlp);
			if (hr != HRESULT_FROM_WIN32(ERROR_IO_PENDING))
			{
				free(lpMessage);
				lpMessage = NULL;
				goto ret;
			}
		}

	}

	WaitForMultipleObjectsEx(SCANNER_DEFAULT_REQUEST_COUNT
	,threads
	,TRUE
	,INFINITE
	,FALSE);


ret:
	if (hr)
	{
		CloseHandle(hPort);
	}
	if (hIoCompletion)
	{
		CloseHandle(hIoCompletion);
	}
}

void CPBCScanUserMFCDlg::OnBnClickedOk()
{
	// TODO: �ڴ���ӿؼ�֪ͨ����������
	//CDialogEx::OnOK();

	ConnectR0Port();

}
