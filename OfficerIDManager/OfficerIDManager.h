#pragma once

#include "resource.h"


class OfficerIdManagerDlg : public Dialog {
public:
    int  DiskChange();
    void UpdateTargetFreeSpace();
    void updsrcdiskspace();

protected:

    int m_checktargetspace ;

	int OnInitDialog();
   	
    void OnTimer(UINT_PTR nIDEvent);
    void OnDeviceChange( UINT nEventType, DWORD dwData );
	void OnBnCreate(); 
	void OnBnEdit();
	void OnBnFormat();
	void OnBnDelete();
	void OnBnLog(); 

    virtual int OnCommand( int Id, int Event ) {
		
        if( Id == IDCANCEL || Id == IDOK ) {
//            EndDialog(m_hWnd, IDCANCEL );
            DestroyWindow(m_hWnd);
            return TRUE ;
        }
		else if( Id == IDC_BUTTON_CREATE ) {
			OnBnCreate(); 
		}
		else if( Id == IDC_BUTTON_EDIT ) {
			OnBnEdit(); 
		}
		else if( Id == IDC_BUTTON_FORMAT ) {
			//OnBnFormat(); 
			OnBnDelete();
		}
		else if( Id == IDC_BUTTON_LOG ) {
			OnBnLog(); 
		}
		return FALSE ;
	}

	virtual int OnNotify( LPNMHDR pnmhdr ) {
		return FALSE ;
	}

//    virtual void OnSysCommand(UINT nID, LPARAM lParam) ;

    // dialog box procedures
	virtual int DlgProc(UINT message, WPARAM wParam, LPARAM lParam)
	{
		int res = TRUE ;
		switch (message)
		{
		case WM_INITDIALOG:
			res = OnInitDialog();
			break;

		case WM_NOTIFY:
			res = OnNotify( (LPNMHDR)lParam ) ;
			break ;

		case WM_COMMAND:
			res = OnCommand( LOWORD(wParam), HIWORD(wParam) );
			break;

//		case WM_DESTROY:
//			OnDestroy();
//			break;
            		
        case WM_TIMER:
			OnTimer(wParam);
			break;

        case WM_DEVICECHANGE:
			OnDeviceChange( wParam, lParam );
			break;

		default:
			res = FALSE ;
			break ;
		}

		return res ;
	}

};

// data header on disk
struct officerid_header {
	char tag[4] ;				// tag of id block 'OFID"
	long si ;					// size of data ( officerid_t )
	long enctype ;				// 0: 
	long pad ;					// align structure
	char checksum[16] ;
	char salt[64] ;
	long data[1] ;
} ;

// Office ID data format
struct officerid_t {
	char officerid[32] ;
	char firstname[32] ;
	char lastname[32] ;
	char department[64] ;
	char badgenumber[32] ;
	char info[384] ;
	char res[8] ;				// can be more
} ;


class OfficerIdEditDlg : public Dialog {
public:
	struct officerid_t officerid ; 

protected:
	virtual int OnInitDialog();
    virtual int OnOK();
};

class OfficerIdLogDlg : public Dialog {
public:
	int dw, dh ;

protected:
	virtual int OnInitDialog();
	void OnSize( int code, int w, int h);
	void OnBnClear();

	virtual int OnCommand( int Id, int Event ) {
        if( Id == IDC_BUTTON_CLEAR ) {
			OnBnClear();
        }
		return Dialog::OnCommand(Id, Event);
	}
	
    // dialog box procedures
	virtual int DlgProc(UINT message, WPARAM wParam, LPARAM lParam)
	{
		int res = TRUE ;
		switch (message)
		{
		case WM_SIZE:
			OnSize( wParam, LOWORD(lParam), HIWORD(lParam) );
			break;

		default:
			break;
		}

		return Dialog::DlgProc(message, wParam, lParam);
	}
};
