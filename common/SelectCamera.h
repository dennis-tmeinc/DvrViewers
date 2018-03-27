#pragma once

#include "../common/cstr.h"
#include "../common/cwin.h"

#include "dvrclientDlg.h"

// Select Camera dialog
class SelectCamera : public Dialog
{
public:
    string m_cameraname[MAXSCREEN] ;
    int    m_decoder[MAXSCREEN] ;
    int    m_channel[MAXSCREEN] ;
    int    m_used[MAXSCREEN] ;
    int    m_selected[MAXSCREEN] ;

	SelectCamera() ;

protected:
    virtual int OnInitDialog();
    virtual int OnOK();
    virtual int  OnTimer();
    virtual void PreCommand();

    // dialog box procedures
    virtual int DlgProc(UINT message, WPARAM wParam, LPARAM lParam)
    {
        int res = FALSE ;
        switch (message)
        {
        case WM_INITDIALOG:
            res = OnInitDialog();
            break;

        case WM_NOTIFY:
            res = OnNotify( (LPNMHDR)lParam ) ;
            break ;

       case WM_TIMER:
            res = OnTimer() ;
            break ;

        case WM_COMMAND:
            PreCommand();
            res = OnCommand( LOWORD(wParam), HIWORD(wParam) );
            break;

        default:
            break;

        }
        return res;
    }

};
