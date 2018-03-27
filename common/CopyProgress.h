#pragma once
#include "decoder.h"

#include "../common/cstr.h"
#include "../common/cwin.h"

// Show copy progress dialog
class CopyProgress : public Dialog
{
public:
	string m_title ;
	struct dup_state * m_dupstate ;
    string m_progressmsg;

    DWORD m_starttime ;
	int   m_savepercent ;

    CopyProgress( LPCTSTR title, struct dup_state * dupstate ){
        m_title = title ;
        m_dupstate = dupstate ;
    }

protected:
    virtual int OnInitDialog()
	{
        // Set window title
        SetWindowText( m_hWnd, m_title );

        // Copy progress timer
        SetTimer(m_hWnd, 1, 937, NULL);

        SendMessage(GetDlgItem(m_hWnd, IDC_PROGRESS), PBM_SETRANGE32, 0, 100 );
        SendMessage(GetDlgItem(m_hWnd, IDC_PROGRESS), PBM_SETPOS , 0, 0 );

        m_starttime=GetTickCount();
        m_savepercent=0 ;

		return TRUE;
	}

    virtual int OnCancel()
    {
        int i;
        string msg ;
        SetCursor(LoadCursor(NULL, IDC_WAIT));
        ShowCursor( TRUE );
        m_dupstate->cancel=1 ;
        KillTimer(m_hWnd, 1);
        SetDlgItemText(m_hWnd, IDC_STATIC_PROGRESS, _T("Cancelling, please wait...") );
        for( i=0; i<1000; i++ ) {	// wait for copying to finish in 100 seconds
            if( m_dupstate->status==0 )
                Sleep(100);
            else
                break;
        }
        ShowCursor( FALSE );
		return EndDialog(IDCANCEL);
    }

    virtual void OnTimer(int nIDEvent){
        if( m_dupstate->status==0 ) {		// copying
            if( m_dupstate->percent>0 ) {
                if( m_dupstate->percent>m_savepercent ) {
                    m_savepercent = m_dupstate->percent ;
                    SendMessage(GetDlgItem(m_hWnd, IDC_PROGRESS), PBM_SETRANGE32, 0, 100 );
                    SendMessage(GetDlgItem(m_hWnd, IDC_PROGRESS), PBM_SETPOS , m_savepercent, 0 );

                    DWORD passtime=GetTickCount()-m_starttime;
                    int remaintime ;
                    // calculate estimated remaining time
                    remaintime = (passtime * (100 - m_dupstate->percent) / m_dupstate->percent)/1000 ;
					m_progressmsg.printf("Estimated remaining time : %02d:%02d:%02d",
                        remaintime/3600,
                        remaintime%3600/60,
                        remaintime%60 );
                }
            }
            else {
                SendMessage(GetDlgItem(m_hWnd, IDC_PROGRESS), PBM_SETRANGE32, 0, m_dupstate->totoalkbytes );
                SendMessage(GetDlgItem(m_hWnd, IDC_PROGRESS), PBM_SETPOS , m_dupstate->copykbytes, 0 );
                m_progressmsg.printf("Total bytes: %d, copied bytes: %d",
                    m_dupstate->totoalkbytes,
                    m_dupstate->copykbytes );
            }
        }
        else if( m_dupstate->status<0 ) {	// error
            m_progressmsg="Copy failed!";
            KillTimer(m_hWnd, nIDEvent);
            ShowWindow( GetDlgItem(m_hWnd, IDOK), SW_SHOW);
            ShowWindow( GetDlgItem(m_hWnd, IDCANCEL), SW_HIDE);
        }
        else {								// finished
            m_progressmsg="Completed!";
            SendMessage(GetDlgItem(m_hWnd, IDC_PROGRESS), PBM_SETRANGE32, 0, 100 );
            SendMessage(GetDlgItem(m_hWnd, IDC_PROGRESS), PBM_SETPOS , 100, 0 );
            KillTimer(m_hWnd, nIDEvent);
            ShowWindow( GetDlgItem(m_hWnd, IDOK), SW_SHOW);
            ShowWindow( GetDlgItem(m_hWnd, IDCANCEL), SW_HIDE);
        }
        m_dupstate->update=1 ;
        SetDlgItemText(m_hWnd, IDC_STATIC_PROGRESS, m_progressmsg );
        string msg;
		msg=m_dupstate->msg;
        SetDlgItemText(m_hWnd, IDC_STATIC_MSG, msg );
    }

	virtual int DlgProc(UINT message, WPARAM wParam, LPARAM lParam)
	{
        if( message == WM_TIMER ) {
            OnTimer( wParam );
            return TRUE ;
        }
        else {
            return Dialog::DlgProc( message, wParam, lParam );
        }
    }

};
