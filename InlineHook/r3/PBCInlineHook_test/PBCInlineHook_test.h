
// PBCInlineHook_test.h : PROJECT_NAME Ӧ�ó������ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "resource.h"		// ������


// CPBCInlineHook_testApp: 
// �йش����ʵ�֣������ PBCInlineHook_test.cpp
//

class CPBCInlineHook_testApp : public CWinApp
{
public:
	CPBCInlineHook_testApp();

// ��д
public:
	virtual BOOL InitInstance();

// ʵ��

	DECLARE_MESSAGE_MAP()
};

extern CPBCInlineHook_testApp theApp;