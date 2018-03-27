// DvrSetup.h

#ifndef __DVRSETUP_H__
#define __DVRSETUP_H__

#include "PLY301.h"
#include "dvrnet.h"

#include "../common/cwin.h"
#include "../common/cstr.h"

#define MAXSHEETPAGE (10)

class PropSheet ;

class PropSheetPage : public Dialog {
public:

    PROPSHEETPAGE m_psp;
    PropSheet * m_ps ;

    PropSheetPage(int dialog_id ){
        m_psp.dwSize = sizeof(PROPSHEETPAGE);
        m_psp.dwFlags = 0;
        m_psp.hInstance = ResInst();
        m_psp.pszTemplate = MAKEINTRESOURCE(dialog_id);
        m_psp.pszIcon = NULL;
        m_psp.pfnDlgProc = PropSheetPage::DialogProc;
        m_psp.lParam = (LPARAM)this;
        m_psp.pfnCallback = NULL;
    }

    static INT_PTR CALLBACK DialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam ) {
        PropSheetPage * dlg ;
        if( uMsg == WM_INITDIALOG ) {
            PROPSHEETPAGE * p = (PROPSHEETPAGE  *)lParam ;
            dlg = (PropSheetPage *)(p->lParam) ;
            dlg->attach( hwndDlg );
        }
        dlg = (PropSheetPage *) getWindow( hwndDlg ) ;
        if( dlg ) {
            return dlg->DlgProc( uMsg, wParam, lParam );
        }
        else {
            return FALSE ;
        }
    }

    virtual void PutData(){} ;
    virtual void GetData(){} ;
    virtual void Save(FILE * fh){} ;
    virtual void Load(FILE * fh){} ;
};

class PropSheet : public Window {
public:
    PropSheet(LPCTSTR title) {
        m_psh.dwSize = sizeof(PROPSHEETHEADER);
        m_psh.dwFlags = PSH_USECALLBACK | PSH_NOCONTEXTHELP ;
        m_psh.hwndParent = NULL ;
        m_psh.hInstance = ResInst();
        m_psh.pszIcon = NULL ;
        m_psh.pszCaption = title;
        m_psh.nPages = 0 ;
        m_psh.nStartPage = 0;
        m_psh.phpage = m_pages ;
        m_psh.pfnCallback = PropSheet::PropSheetProc;
        m_result = IDCANCEL ;       // default result
    }

    void addpage( PropSheetPage * page ) {
        if(m_psh.nPages>=MAXSHEETPAGE) {
            return ;
        }
        m_pages[ m_psh.nPages ] = CreatePropertySheetPage( &(page->m_psp) ) ;
        m_psh.nPages++ ;
        page->m_ps = this ;
    }

protected:
    static PropSheet * m_curpropsheet ;
    PROPSHEETHEADER m_psh;
    HPROPSHEETPAGE  m_pages[MAXSHEETPAGE] ;
    int             m_result ;

    static int CALLBACK PropSheetProc(
        HWND hwndDlg,
        UINT uMsg,
        LPARAM lParam) 
    {
        PropSheet * ps ;
        if( hwndDlg && IsWindow( hwndDlg ) ) {
            ps = (PropSheet *)Window::getWindow( hwndDlg ) ;
            if( ps==NULL ) {
                m_curpropsheet->attach( hwndDlg );
                ps = m_curpropsheet ;
            }

#ifndef PSCB_BUTTONPRESSED
#define PSCB_BUTTONPRESSED 3
#endif

            if( ps!=NULL && uMsg == PSCB_BUTTONPRESSED ) {
                switch( lParam ) {
                case PSBTN_OK:
                    ps->OnButtonOk();
                    break;
                case PSBTN_CANCEL:
                    ps->OnButtonCancel();
                    break;
                case PSBTN_APPLYNOW:
                    ps->OnButtonApply();
                    break;
                case PSBTN_FINISH:
                    ps->OnButtonFinish();
                    break;
                }
            }
        }
        return 0 ;
    }

public:
    void OnButtonOk(){
        m_result = IDOK ;
    }
    void OnButtonCancel(){
        m_result = IDCANCEL ;
    }
    void OnButtonApply(){
    }
    void OnButtonFinish(){
    }

    int DoModal() {
        m_curpropsheet = this ;
        PropertySheet(&m_psh);
        return m_result ;
    }
};

/////////////////////////////////////////////////////////////////////////////
// SystemPage dialog

class SystemPage : public PropSheetPage
{
    // Construction
public:
	SystemPage();

    virtual void PutData() ;
	virtual void GetData() ;
	virtual void Save(FILE * fh) ;
	virtual void Load(FILE * fh) ;
	void Set();

	struct system_stru sys ;

protected:
   	string	m_videopassword;

protected:
    virtual void OnKillActive();

    virtual BOOL OnInitDialog();
    virtual int OnNotify( LPNMHDR pnmhdr ) {
        if(pnmhdr->code == PSN_KILLACTIVE ) {
            OnKillActive();
            return TRUE ;
        }
		return FALSE ;
	}
};

/////////////////////////////////////////////////////////////////////////////
// CameraPage

class CameraPage : public PropSheetPage
{
   
// Construction
public:
	CameraPage();
    ~CameraPage();
	
    void Set();
    virtual void PutData() ;
	virtual void GetData() ;
	virtual void Save(FILE * fh) ;
	virtual void Load(FILE * fh) ;
    
    Window      m_Tab;
	int tabindex ;

	struct  DvrChannel_attr  * cam_attr ;

protected:
	virtual void OnKillActive();
	virtual BOOL OnInitDialog();
	virtual void OnSelchangeTabCamera();
	virtual void OnReleasedcaptureMotsensitive();
	virtual void OnEnableBitrate();

    virtual int OnNotify( LPNMHDR pnmhdr ) {
        if( pnmhdr->code == TCN_SELCHANGE && pnmhdr->idFrom == IDC_TAB_CAMERA ) {
            OnSelchangeTabCamera();
            return TRUE ;
        }
        else if( pnmhdr->code == NM_RELEASEDCAPTURE && pnmhdr->idFrom == IDC_MOTSENSITIVE ) {
            OnReleasedcaptureMotsensitive();
            return TRUE ;
        }
        else if(pnmhdr->code == PSN_KILLACTIVE ) {
            OnKillActive();
            return TRUE ;
        }
		return FALSE ;
	}

    virtual int OnCommand( int Id, int Event ) {
        if( Id == IDC_ENABLE_BITRATE ) {
            OnEnableBitrate();
            return TRUE ;
        }
        return FALSE ;
    }
};

/////////////////////////////////////////////////////////////////////////////
// SensorPage 

class SensorPage : public PropSheetPage
{
// Construction
public:
	SensorPage():
      PropSheetPage(IDD_SENSOR_PAGE)
    {
    }

    virtual void PutData() ;
	virtual void GetData() ;
	virtual void Save(FILE * fh) ;
	virtual void Load(FILE * fh) ;

    Window	m_Tab;
	int tabindex;

protected:
	virtual BOOL OnInitDialog();
	virtual void OnSelchangeTabsensor();
	virtual void OnKillActive();
    virtual int OnNotify( LPNMHDR pnmhdr ) {
        if( pnmhdr->code == TCN_SELCHANGE && pnmhdr->idFrom == IDC_TABSENSOR ) {
            OnSelchangeTabsensor();
            return TRUE ;
        }
        else if(pnmhdr->code == PSN_KILLACTIVE ) {
            OnKillActive();
            return TRUE ;
        }
		return FALSE ;
	}
};

/////////////////////////////////////////////////////////////////////////////
// StatusPage

class StatusPage : public PropSheetPage
{
// Construction
public:
   	StatusPage():
      PropSheetPage(IDD_DVRSTATUS_PAGE)
    {	
        m_timer = 0 ;
        tzchanged = 0;
    }

protected:
   	virtual BOOL OnInitDialog();
   	virtual void OnSetActive();
   	virtual void OnKillActive();

	void UpdateStatus();

	int	m_timer ;
	TIME_ZONE_INFORMATION tzi ;
	TIME_ZONE_INFORMATION localtzi ;
	int tzchanged ;
	int version[8] ;

	virtual void OnTimer(UINT nIDEvent);
	virtual void OnSynTime();
//	virtual void OnSettimezone();
	virtual void OnSelchangeTimezone();
	virtual void OnDaylight();

    virtual int OnNotify( LPNMHDR pnmhdr ) {
        if(pnmhdr->code == PSN_KILLACTIVE ) {
            OnKillActive();
            return TRUE ;
        }
        else if(pnmhdr->code == PSN_SETACTIVE ) {
            OnSetActive();
            return TRUE ;
        }
		return FALSE ;
	}

    virtual int OnCommand( int Id, int Event ) {
        if( Id == IDC_SYN_TIME ) {
            OnSynTime();
            return TRUE ;
        }
        else if( Id == IDC_TIMEZONE ) {
            OnSelchangeTimezone();
        }
		return FALSE ;
	}

    virtual int DlgProc(UINT message, WPARAM wParam, LPARAM lParam)
	{
        if( message==WM_TIMER ) {
            OnTimer( wParam );
        }
        return PropSheetPage::DlgProc(message, wParam, lParam );
    }
};

/////////////////////////////////////////////////////////////////////////////
// DvrSetup

class DvrSetup : public PropSheet
{

    // Construction
public:
	DvrSetup(LPCTSTR pszCaption);

	Window		BtSave;
	Window		BtLoad;
	Window		BtCommit;
	
	PropSheetPage * pSystemPage ;
	PropSheetPage * pCameraPage ;
	PropSheetPage * pSensorPage ;
    int m_commit ;

protected:
   	virtual void OnAttach();
	virtual void OnSaveSetup();
	virtual void OnLoadSetup();
	virtual void OnCommit();

  	virtual int OnSize(int width, int height);

    virtual int OnCommand(int Id, int Event)
	{
        if( Id==IDC_COMMITSETUP ) {
            OnCommit();
            return TRUE ;
        }
        else if( Id==IDC_LOADSETUP ) {
            OnLoadSetup();
            return TRUE ;
        }
        else if( Id==IDC_SAVESETUP ) {
            OnSaveSetup();
            return TRUE ;
        }
		return FALSE;
	}

    virtual LRESULT WndProc(UINT message, WPARAM wParam, LPARAM lParam) {
        switch (message) {
        case WM_SIZE :
            OnSize(LOWORD(lParam), HIWORD(lParam));
			break;
		case WM_COMMAND:
			OnCommand( LOWORD(wParam), HIWORD(wParam) );
            break;
        }
		return DefWndProc(message, wParam, lParam);
	}

};

#endif //__DVRSETUP_H__
