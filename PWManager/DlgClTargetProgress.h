#pragma once

#include "PWManager.h"

#include "../common/cstr.h"
#include "../common/cwin.h"

// RemoteIpDlg dialog

class DlgClTargetProgress : public Dialog
{
public:
    string m_title ;
    int m_update ;
    string m_msg ;
    int m_percentage ;
    int m_complete ;
    int m_lifttime ;
    int m_nfileonly ;
    string m_rootdir;

    DlgClTargetProgress() {
        m_update = 1 ;
        m_msg = "Start clean" ;
        m_percentage = 0 ;
        m_complete = 0 ;
        m_lifttime = 0 ;
    }

protected:
    int OnInitDialog() {
        if( strlen( m_title )>0 ) {
            SetWindowText( m_hWnd, m_title );
        }
        SetTimer( m_hWnd, 1, 50, NULL );
        return TRUE ;
    }

    void OnTimer( int idtimer )
    {
        string msg ;
        lock();
        msg = m_msg ;
        unlock();
        if( m_update ) {
            m_update = 0 ;
            SetDlgItemText( m_hWnd, IDC_STATIC_CLRTARGETMSG, msg );
        }
        if( m_complete ) {
            msg = "Completed!" ;
            SetDlgItemText( m_hWnd, IDC_STATIC_CLRTARGETMSG, msg );
            msg = "Quit" ;
            SetDlgItemText( m_hWnd, IDCANCEL, msg );
        }
    }

	// dialog box procedures
	virtual int DlgProc(UINT message, WPARAM wParam, LPARAM lParam)
	{
		int res = FALSE ;
		switch (message)
		{
		case WM_TIMER:
			OnTimer( (int)wParam ) ;
			res = TRUE ;
			break;

        default :
            res = Dialog::DlgProc( message, wParam, lParam );

        }
		return res;
	}


};
