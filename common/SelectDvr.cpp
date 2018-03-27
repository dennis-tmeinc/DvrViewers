// SelectDvr.cpp : implementation file
//
#include "stdafx.h"

#include <Shlobj.h>

#include "dvrclient.h"
#include "SelectDvr.h"
#include "RemoteIpDlg.h"
#include "decoder.h"
#include "../common/util.h"
#include "../common/cdir.h"

SelectDvr::SelectDvr(int opentype) 
{
    m_opentype=opentype;
    m_dev = -1 ;
    m_autoopen = 0 ;
    m_dvrlist = NULL ;
    m_numdvr = 0 ;
	m_hidelist = NULL ;
	m_filterchanged = 0 ;
}

SelectDvr::~SelectDvr()
{
    decoder::stopfinddevice();
    if( m_numdvr>0 && m_dvrlist!=NULL ) {
        delete [] m_dvrlist ;
		m_dvrlist = NULL ;
    }
}

BOOL SelectDvr::OnInitDialog()
{
	if( m_opentype== PLY_PLAYBACK ) {
#ifdef APP_TVS_PR1ME
		SetWindowText(m_hWnd, _T("Select Officer ID"));
#else
		SetWindowText(m_hWnd, _T("Select DVR or Media Device"));
#endif
		ShowWindow( GetDlgItem(m_hWnd, IDC_BUTTON_OPENHARDDRIVE), SW_SHOW);
	}
    if( m_autoopen ) {
		SetWindowText(m_hWnd, _T("Searching for DVR unit..."));
		ShowWindow( GetDlgItem(m_hWnd, IDC_BUTTON_OPENREMOTE), SW_HIDE);
		ShowWindow( GetDlgItem(m_hWnd, IDOK), SW_HIDE);
    }
	SetTimer(m_hWnd, 1, USER_TIMER_MINIMUM, NULL );

	m_localdrives = GetLogicalDrives();
	m_localdrives >>= 2 ;		// skip A,B drive
	m_drive = 'C' ;

    return TRUE ;
}

void SelectDvr::OnDestroy()
{
    int i ;
	dvrlistitem * dvritem ;
    int count = SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DVRSERVER),  LB_GETCOUNT, 0, 0 );
    for( i=0; i<count; i++) {
        dvritem = (dvrlistitem *) SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DVRSERVER),  LB_GETITEMDATA, i, 0 ) ;
        if( dvritem ) {
            delete dvritem ;
        }
    }
	
	while( m_hidelist != NULL ) {
		dvritem = m_hidelist -> next ;
		delete m_hidelist ;
		m_hidelist = dvritem ;
	}
}

int SelectDvr::OnOK()
{
    int i ;
    int count = SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DVRSERVER),  LB_GETCOUNT, 0, 0 );
    m_numdvr = 0 ;
    for( i=0; i<count; i++) {
        if( SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DVRSERVER),  LB_GETSEL, i, 0 )>0 ) {
            m_numdvr++ ;
        }
    }
    if( m_numdvr>0 ) {
        m_dvrlist = new dvrlistitem [m_numdvr] ;
        int idvr = 0 ;
        for( i=0; i<count; i++) {
            dvrlistitem * dvritem ;
            dvritem = (dvrlistitem *) SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DVRSERVER),  LB_GETITEMDATA, i, 0 ) ;
            if( SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DVRSERVER),  LB_GETSEL, i, 0 )>0 && idvr<m_numdvr ) {
                m_dvrlist[idvr].dvrtype=dvritem->dvrtype ;
                m_dvrlist[idvr].idx=dvritem->idx ;
                m_dvrlist[idvr].dvrname = dvritem->dvrname ;
				m_dvrlist[idvr].displayname = dvritem->displayname ;
                idvr++;
            }
        }
        EndDialog( IDOK );
    }
    else {
        EndDialog( IDCANCEL ) ;
    }
	return TRUE ;
}

// Change button states base on new item selected
void SelectDvr::OnLbnSelchangeListDvrserver()
{
	int i ;
    int count ;
	// enable/disable connect button
    EnableWindow( GetDlgItem(m_hWnd, IDOK), 0 );
    count = SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DVRSERVER),  LB_GETCOUNT, 0, 0 ) ;
	for( i=0; i<count; i++) {
		if( SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DVRSERVER),  LB_GETSEL, i, 0 )>0 ) {
			EnableWindow( GetDlgItem(m_hWnd, IDOK), 1);
			break;
		}
	}

	// show / hide Clean Disk Button
	ShowWindow( GetDlgItem(m_hWnd, IDC_BUTTON_CLEANDISK), SW_HIDE);
	int sel = SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DVRSERVER),  LB_GETCURSEL, 0, 0 ) ; 
	if( sel>=0 &&
		SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DVRSERVER),  LB_GETSEL, sel, 0 ) > 0 ) {
        dvrlistitem * dvritem = (dvrlistitem *)SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DVRSERVER),  LB_GETITEMDATA, sel, 0 ) ;
        if( (dvritem->feature & PLYFEATURE_CLEAN)!=0 && m_opentype == PLY_PLAYBACK ) {
			ShowWindow( GetDlgItem(m_hWnd, IDC_BUTTON_CLEANDISK), SW_SHOW); 
		}
	}
}

// scan 1 folder (harddrive)
void SelectDvr::localscan_1( char * folder ) 
{
	int l;
	string filename ;
	char * servername;
	dvrlistitem * dlist = NULL ;
	HWND list = GetDlgItem(m_hWnd, IDC_LIST_DVRSERVER) ;

	dirfind dir(folder, "_*_");
	while( dir.finddir() && m_hWnd != NULL ) {
		filename = dir.filename();
		servername= ((char *)filename)+1 ;
		l=strlen(servername) ;
		servername[l-1]=0 ;

		// add server name to list
		dvrlistitem * newdvritem = new dvrlistitem ;
		newdvritem->idx = -1 ;
		newdvritem->dvrtype = 2 ;					// local disk (folder)
		newdvritem->dvrname = dir.filepath() ;
		newdvritem->displayname = servername ;
		newdvritem->feature = 0 ;

		newdvritem->next = dlist ;
		dlist = newdvritem ;
	}

	while ( dlist!= NULL ) {
		dvrlistitem * n = dlist->next ;
		int idx=SendMessage( list,  LB_ADDSTRING, 0, (LPARAM)(LPCTSTR)(dlist->displayname) );
		SendMessage( list,  LB_SETITEMDATA, idx, (LPARAM)dlist );
		dlist = n ;
	}
}

void SelectDvr::localscan()
{
    char dirpath[] = "C:\\" ;

	dvrlistitem * dlist = NULL ;

	while( m_localdrives !=0 && (m_localdrives&1) == 0 && m_drive<='Z' ) {
		m_localdrives >>= 1 ;
		m_drive++ ;
	}

	if(  m_drive>'Z' || m_localdrives == 0 ) {
		m_localdrives = 0 ;
		return ;
	}

	dirpath[0] = m_drive ;
	localscan_1(dirpath);
	
	m_localdrives >>= 1 ;
	m_drive++ ;

}

void SelectDvr::scanOptionfolder()
{
	void * opt ;
	opt = option_open();
	if( opt ) {
		string * v ;
		while( (v = option_get( opt, "dvr.rootfolder" ))!=NULL ) {
			localscan_1( (char *)(*v) );
			delete v ;
		}
		option_close( opt );
	}

}

void SelectDvr::OnTimer(int nIDEvent)
{
	string info ;
	int index ;

	// find DVR flags
	//		bit0: DVR server, 
	//		bit1: local hard drive, 
	//		bit2: Smart Server
	static int serverflag[4] = { 1, 3, 4, 0 };

	if( m_dev < 0 ) {
		WaitCursor waitcursor ;

		int sflag = serverflag[m_opentype] ;

#ifdef  APP_TVS_PR1ME

		if( ( sflag & 2 )!=0 ) {
			sflag &= ~2 ;		// disable local folder server search for Pr1me viewer
			if( m_localdrives != 0 ) {
				localscan();
				return ;
			}
		}

#endif	

		if( m_opentype == 1 ) {
			scanOptionfolder();
		}

		decoder::finddevice(sflag);

		// auto open server
		if( m_autoopen ) {
			decoder::detectdevice( m_autoopen_server );
		}

        // send current remote host
        if( strlen(decoder::remotehost)>0 ) {
            decoder::detectdevice(decoder::remotehost);
        }

   		// detect DVRs on specified in "dvrlist" file
        info = "dvrlist" ;
        if( getfilepath( info ) ) {
            FILE * flist = fopen( info, "r" );
            if( flist ) {
                while( fscanf(flist, "%s", info.strsize(500))==1 ) {
                    decoder::detectdevice(info);
                }
                fclose( flist );
            }
        }
        m_dev = 0 ;
	}

	if( m_dev<decoder::polldevice() ) {
        WaitCursor waitcursor ;
       	decoder dec ;
		if( dec.open(m_dev,m_opentype)>=0 ) {
			if( strlen( dec.getserverinfo()) ==0 ) {
				info=dec.getservername();
			}
			else {
				info.printf( "%s - %s", dec.getservername(), dec.getserverinfo());
			}
            dvrlistitem * newdvritem = new dvrlistitem ;
            newdvritem->dvrtype = 0 ;
            newdvritem->idx = m_dev ;
            newdvritem->dvrtype = 0 ;
            newdvritem->dvrname = info ;
			newdvritem->displayname = info ;
            newdvritem->feature = dec.getfeatures() ;
			dec.close();

			index=SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DVRSERVER),  LB_ADDSTRING, 0, (LPARAM)(LPCTSTR)(newdvritem->displayname) );
			SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DVRSERVER),  LB_SETITEMDATA, index, (LPARAM)newdvritem );
            if( m_autoopen && strstr( info, m_autoopen_server )!=NULL ) {   // found autoopen server   
                SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DVRSERVER),  LB_SETSEL, TRUE, (LPARAM)index );
                OnOK();
            }
		}
        m_dev++;
	}
	else {
		// auto open server
		if( m_autoopen ) {
			decoder::detectdevice( m_autoopen_server );
		}
	}

#ifdef IDC_EDIT_DVRFILTER
	MSG msg ;
	if( m_filterchanged && PeekMessage( &msg, m_hWnd, 0, 0, PM_NOREMOVE ) == 0 ) {
		// idle now
		m_filterchanged = 0 ;
		HWND list = GetDlgItem(m_hWnd, IDC_LIST_DVRSERVER) ;
		string filter ;
		GetWindowText(GetDlgItem(m_hWnd, IDC_EDIT_DVRFILTER), filter.tcssize(512), 512 );

		// add hidder dvr to list
		dvrlistitem * hidelist = NULL ;

		dvrlistitem * n ;
		int idx ;
		while( m_hidelist != NULL ) {
			n = m_hidelist->next ;

			if( strstr( m_hidelist->displayname, filter ) == NULL ) {
				// put to new hide list
				m_hidelist->next = hidelist ;
				hidelist = m_hidelist ;
			}
			else {
				// put to listbox
				idx=SendMessage( list,  LB_ADDSTRING, 0, (LPARAM)(LPCTSTR)m_hidelist->displayname );
				SendMessage( list,  LB_SETITEMDATA, idx, (LPARAM)m_hidelist );
			}

			m_hidelist = n ;
		}
		m_hidelist = hidelist ;

		// remove item by filter
		if( strlen(filter) > 0 ) {
			int i;
			for( i=0; i<SendMessage( list, LB_GETCOUNT, 0, 0 ); ) {
				string servername ;
				SendMessage( list, LB_GETTEXT, i, (LPARAM)servername.tcssize(512) );
				if( strstr( servername, filter ) == NULL ) {
					n = (dvrlistitem *) SendMessage( list, LB_GETITEMDATA, i, 0 ) ;
					n->next = m_hidelist ;
					m_hidelist = n ;
					SendMessage( list, LB_DELETESTRING, i, (LPARAM)NULL );
				}
				else {
					i++ ;
				}
			}
		}
	}

#endif

}

void SelectDvr::OnFilter( int Event )
{
#ifdef IDC_EDIT_DVRFILTER
	if( Event == EN_CHANGE ) {
		m_filterchanged = 1 ;
	}
#endif
}

void SelectDvr::OnBnClickedButtonOpenremote()
{
	RemoteIpDlg remoteip ;
    remoteip.m_ipaddr = (char *)decoder::remotehost ;

	ShowWindow(m_hWnd, SW_HIDE);
    if( remoteip.DoModal(IDD_DIALOG_REMOTEIP, this->m_hWnd)==IDOK ) {
        m_numdvr = 1 ;
        m_dvrlist = new dvrlistitem[m_numdvr] ;
        m_dvrlist[0].dvrname = remoteip.m_ipaddr ;
        m_dvrlist[0].dvrtype = 1 ;      // an ip address in "dvrname"
        m_dvrlist[0].idx = 0 ;
        m_dvrlist[0].feature = 0 ;
	    EndDialog( IDOK );
	}
	else {
		ShowWindow(m_hWnd, SW_SHOW);
	}
}

void SelectDvr::OnBnClickedButtonOpenharddrive()
{
	ShowWindow(m_hWnd, SW_HIDE);
	string folder;
	reg_read("recenthd", folder);
	if( openfolder(m_hWnd, folder ) ) {
		reg_save("recenthd", folder);
        m_numdvr = 1 ;
        m_dvrlist = new dvrlistitem[m_numdvr] ;
        m_dvrlist[0].dvrname = folder;
        m_dvrlist[0].dvrtype = 2 ;      // a directory
        m_dvrlist[0].idx = 0 ;
        m_dvrlist[0].feature = 0 ;
        EndDialog( IDOK );
		return ;
	}

	/*
	//  Used GetOpenFileName() to open dir

	TCHAR displayname[MAX_PATH] ;
	LPCITEMIDLIST pidl ;
	BROWSEINFO browsinfo ;
	memset(&browsinfo, 0, sizeof(browsinfo));
	browsinfo.hwndOwner = this->m_hWnd ;
	browsinfo.pszDisplayName=displayname ;
	browsinfo.lpszTitle=_T("Open a directory with DVR video data");
	browsinfo.ulFlags = BIF_USENEWUI |BIF_BROWSEINCLUDEFILES  ;

	pidl=SHBrowseForFolder(&browsinfo);
	if( pidl ) {
		if( SHGetPathFromIDList( pidl, displayname) ) {
            m_numdvr = 1 ;
            m_dvrlist = new dvrlistitem[m_numdvr] ;
            m_dvrlist[0].dvrname = displayname ;
            m_dvrlist[0].dvrtype = 2 ;      // a directory
            m_dvrlist[0].idx = 0 ;
            m_dvrlist[0].feature = 0 ;
            EndDialog( IDOK );
		}
		CoTaskMemFree((LPVOID)pidl);
		return ;
	}
	*/

	ShowWindow(m_hWnd, SW_SHOW);
}

void SelectDvr::OnLbnDblclkListDvrserver()
{
    int cursel = SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DVRSERVER),  LB_GETCURSEL, 0, 0 );
    if( SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DVRSERVER), LB_GETSEL, cursel, 0)>0 ) {
        OnOK();
	}
}

void SelectDvr::OnBnClickedButtonCleandisk()
{
	decoder dec ;
    WaitCursor waitcursor ;
	int sel = SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DVRSERVER),  LB_GETCURSEL, 0, 0 ) ; 
	if( sel>=0 &&
		SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DVRSERVER),  LB_GETSEL, sel, 0 ) > 0 ) 
    {
        dvrlistitem * dvritem = (dvrlistitem *)SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DVRSERVER),  LB_GETITEMDATA, sel, 0 ) ;
        if( (dvritem->feature & PLYFEATURE_CLEAN)!=0 && dvritem->dvrtype==0 && dvritem->idx>=0 ) {
            if( dec.open(dvritem->idx, m_opentype )>0 ) {
		        if( dec.cleandata(0,NULL,NULL)>=0 ) {
                    delete dvritem ;
                    SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DVRSERVER),  LB_DELETESTRING, sel, 0 ) ;
		        }
		        dec.close();
	        }
		}
	}
}

