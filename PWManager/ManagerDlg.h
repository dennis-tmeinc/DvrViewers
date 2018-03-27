// ManagerDlg.h : PWManager main dialog header file
//

#pragma once

#include "../common/cwin.h"
#include "../common/cstr.h"
#include "PWManager.h"

class ManagerDlg : public Dialog {
public:
    int  DiskChange();
    void UpdateTargetFreeSpace();
    void updsrcdiskspace();

protected:

    int m_checktargetspace ;

	virtual int OnInitDialog();
   	
    void OnTimer(UINT_PTR nIDEvent);
    void OnDeviceChange( UINT nEventType, DWORD dwData );
    void OnBnClickedButtonClean();
    void OnBnClickedButtonBrowseDataDir();
    void OnEnKillfocusEditFreespace();
    void OnBnClickedButtonCleanTarget();
    void OnBnClickedButtonSrcclean();
    void OnBnClickedButtonSetupId();
    void OnBnClickedButtonSrcEject();

    virtual int OnCommand( int Id, int Event ) {
		switch( Id ) {
			case IDC_BUTTON_BROWSEDATADIR: OnBnClickedButtonBrowseDataDir(); break ;
            case IDC_BUTTON_CLEANTARGET: OnBnClickedButtonCleanTarget(); break ;
			case IDC_BUTTON_SRCCLEAN: OnBnClickedButtonSrcclean(); break ;
            case IDC_BUTTON_SETUPID: OnBnClickedButtonSetupId(); break ;
            case IDC_BUTTON_EJECT: OnBnClickedButtonSrcEject(); break ;
			case IDC_EDIT_FREESPACE:
				if( Event == EN_KILLFOCUS )
					OnEnKillfocusEditFreespace();
				break ;

			default:
				return Dialog::OnCommand( Id, Event ) ;
		}
		return TRUE ;
	}

    virtual void OnSysCommand(UINT nID, LPARAM lParam) ;

    // dialog box procedures
	virtual int DlgProc(UINT message, WPARAM wParam, LPARAM lParam)
	{
		int res = TRUE ;
		switch (message)
		{
        case WM_TIMER:
			OnTimer(wParam);
			break;

        case WM_DEVICECHANGE:
			OnDeviceChange( wParam, lParam );
			break;

        case WM_SYSCOMMAND:
			OnSysCommand(wParam, lParam);
			res = FALSE ;	// let system do the default
			break;

		default:
			return Dialog::DlgProc(message,wParam,lParam) ;
		}

		return res ;
	}

};
