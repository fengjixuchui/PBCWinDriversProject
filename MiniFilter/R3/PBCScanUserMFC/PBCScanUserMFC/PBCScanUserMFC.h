
// PBCScanUserMFC.h : PROJECT_NAME Ӧ�ó������ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "resource.h"		// ������


// CPBCScanUserMFCApp: 
// �йش����ʵ�֣������ PBCScanUserMFC.cpp
//

class CPBCScanUserMFCApp : public CWinApp
{
public:
	CPBCScanUserMFCApp();

// ��д
public:
	virtual BOOL InitInstance();

// ʵ��

	DECLARE_MESSAGE_MAP()
};

extern CPBCScanUserMFCApp theApp;