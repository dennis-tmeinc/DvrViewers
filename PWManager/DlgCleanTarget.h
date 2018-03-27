#pragma once

#include "PWManager.h"

#include "../common/cstr.h"
#include "../common/cwin.h"

// RemoteIpDlg dialog

class DlgCleanTarget : public Dialog
{
public:
    int m_filelifeday ;
protected:
    virtual int OnInitDialog();
    virtual int OnOK() ;
};

class DlgCleanSource : public Dialog
{
public:
    int m_nfileonly ;
    int m_drive ;
protected:
    virtual int OnInitDialog();
    virtual int OnOK() ;
    void OnTimer();

    // dialog box procedures
	virtual int DlgProc(UINT message, WPARAM wParam, LPARAM lParam)
	{
        if( message == WM_TIMER ) {
            OnTimer();
            return TRUE ;
        }
        else {
            return Dialog::DlgProc( message, wParam, lParam );
        }
    }
};

