
// PBCInlineHook_testDlg.h : ͷ�ļ�
//

#pragma once


// CPBCInlineHook_testDlg �Ի���
class CPBCInlineHook_testDlg : public CDialogEx
{
// ����
public:
	CPBCInlineHook_testDlg(CWnd* pParent = NULL);	// ��׼���캯��

// �Ի�������
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_PBCINLINEHOOK_TEST_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV ֧��


// ʵ��
protected:
	HICON m_hIcon;

	// ���ɵ���Ϣӳ�亯��
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnTestBtnClick();
	CString m_FilePath;
	
	afx_msg void OnDropFiles(HDROP hDropInfo);
	afx_msg void OnBnClickedLoaddributton();
	CString m_errorstr;
	afx_msg void OnBnClickedUninstalldriverbutton();
	afx_msg void OnBnClickedSendioctrlbutton();
	CString m_ThreadId;
	afx_msg void OnBnClickedOk();
};
