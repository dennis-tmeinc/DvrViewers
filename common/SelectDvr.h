#pragma once

#include "../common/cstr.h"
#include "../common/cwin.h"

// Select Dvr dialog
class SelectDvr : public Dialog
{

public:
    // Dialog Data
	enum { IDD = IDD_DIALOG_SELECTDVR };
	struct dvrlistitem {
		int dvrtype ;   // 0: dvr listed from player dll, 1: net ip, 2: directory
		int idx ;       // index in dvrlist
		int feature ;
		string displayname ;
		string dvrname ;    // what listed on screen. could be: dvrname+info/ip address/directory
		dvrlistitem * next ;
	} ;

    dvrlistitem * m_dvrlist ;
	int m_numdvr ;

	int m_localdrives ;
	char m_drive ;

	dvrlistitem * m_hidelist ;	// dvr items hidden by filter
	int m_filterchanged ;

    int m_opentype ;
	int m_dev ;

    int m_autoopen ;
    string m_autoopen_server ;

	SelectDvr(int opentype) ;
	~SelectDvr() ;

protected:
	virtual BOOL OnInitDialog();
	virtual int OnOK();

	void OnTimer(int nIDEvent);
    void OnLbnSelchangeListDvrserver();
	void OnBnClickedButtonOpenremote();
	void OnBnClickedButtonOpenharddrive();
	void OnLbnDblclkListDvrserver();
	void OnBnClickedButtonCleandisk();

    void OnFilter( int Event );
    
    virtual int OnCommand( int Id, int Event ) 
    {
        if( Event == BN_CLICKED ) {
            switch( Id ) {
            case IDC_BUTTON_OPENREMOTE:
                OnBnClickedButtonOpenremote();
                break;
            case IDC_BUTTON_OPENHARDDRIVE:
                OnBnClickedButtonOpenharddrive();
                break;
            case IDC_BUTTON_CLEANDISK:
                OnBnClickedButtonCleandisk();
                break;
            }
        }
        else if( Id == IDC_LIST_DVRSERVER ) {
            if( Event == LBN_SELCHANGE ) {
                OnLbnSelchangeListDvrserver();
            }
            else if( Event == LBN_DBLCLK ) {
                OnLbnDblclkListDvrserver();
            }
        }
#ifdef IDC_EDIT_DVRFILTER
        else if( Id == IDC_EDIT_DVRFILTER ) {
			OnFilter(Event);
		}
#endif
        return Dialog::OnCommand( Id, Event ) ;
    }

    void OnDestroy();
	
    virtual int DlgProc(UINT message, WPARAM wParam, LPARAM lParam)
	{
        if( message == WM_TIMER ) {
            OnTimer( wParam );
            return TRUE ;
        }
        else if( message == WM_DESTROY ) {
            OnDestroy();
            return TRUE ;
        }
        else {
            return Dialog::DlgProc( message, wParam, lParam );
        }
    }

	void localscan_1( char * folder ) ;
	void localscan();
	void scanOptionfolder();		// scan folders in options file
};

