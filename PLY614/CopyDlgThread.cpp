// CopyDlgThread.cpp : implementation file
//

#include "stdafx.h"
#include "dvrclient.h"
#include "CopyDlgThread.h"
#include "copyprogressdlg.h"

// CCopyDlgThread

IMPLEMENT_DYNCREATE(CCopyDlgThread, CWinThread)

CCopyDlgThread::CCopyDlgThread()
{
}

CCopyDlgThread::~CCopyDlgThread()
{
}

BOOL CCopyDlgThread::InitInstance()
{
	// TODO:  perform and per-thread initialization here
	CCopyProgressDlg dlg;
	dlg.Create(IDD_COPYPROGRESSDLG, NULL);
	dlg.ShowWindow(SW_SHOW);
//	dlg.SetForegroundWindow();
//	dlg.DoModal();
//	return FALSE;
	return TRUE;
}

int CCopyDlgThread::ExitInstance()
{
	// TODO:  perform any per-thread cleanup here
	return CWinThread::ExitInstance();
}

BEGIN_MESSAGE_MAP(CCopyDlgThread, CWinThread)
END_MESSAGE_MAP()


// CCopyDlgThread message handlers
