// dvrclientDlg common
//      Put common code for dvrclient DLG here
//

// Common DvrclientDlg dialog member functions

// dvrclientDlg.cpp : implementation file
//

#include "stdafx.h"

#include <shellapi.h>
#include <commdlg.h>
#include <Shlobj.h>
#include <mshtml.h>
#include <urlmon.h>

#define _CRT_RAND_S
#include <stdlib.h>
#include <direct.h>
#include <process.h>
#include <malloc.h>
#include <memory.h>

#include "cwin.h"
#include "util.h"
#include "cstr.h"

#include "dvrclient.h"
#include "sliderbar.h"
#include "cliplist.h"
#include "dvrclientDlg.h"
#include "crypt.h"
#include "UsbPassword.h"

#ifdef APP_PWVIEWER_VOLUMECHECK
#include "VolumePass.h"
#endif

#ifdef JSON_SUPPORT
#include "cJSON.h" 
#endif

#ifdef IDD_DIALOG_SELECTCAMERA
#include "../common/SelectCamera.h"
#endif

// global variable
string  g_username ;
int     g_usertype ;	
int     g_MixSound ;
int     g_playstat ;

DvrclientDlg * g_maindlg = NULL;

int	 g_minitracksupport = 0 ;

// About dialog
class AboutDlg : public Dialog
{
protected:
    virtual int OnInitDialog();
	virtual int OnOK()
	{
		return EndDialog(IDOK);
	}
};

// DvrclientDlg dialog
DvrclientDlg::DvrclientDlg()
{
	int i;

	m_minxsize=600 ;
	m_minysize=400 ;
	m_screennum=4 ;			// default 2x2 screen
	m_ptz_savedcursor=NULL ;
	for(i=0;i<MAXSCREEN;i++)
		m_screen[i]=NULL ;
	m_focus=0;
	m_clientmode=CLIENTMODE_CLOSE;

	m_noupdtime = 0 ;
    m_appname = APPNAME ;
    m_reclight=0 ;
	m_startonlastday = 1;

    m_companylinkrect.left=0 ;
	m_companylinkrect.right=0 ;
	m_companylinkrect.top=0 ;
	m_companylinkrect.bottom=0 ;
	m_companylinkcursor=NULL ;

    m_bkbmp = NULL ;

    memset(&m_playertime, 0, sizeof(m_playertime));
    m_playertime.year = 1980 ;
    m_seektime = m_playertime ;

    m_zoom = 0 ;
#ifdef APP_PW_TABLET
	m_panelhide = 1 ;
#else
    m_panelhide = 0 ;
#endif

#ifdef  APP_PWII_FRONTEND
    m_pwbuttonstate = 0 ;               // PWii Buttons state
#endif 
    
#ifdef WMVIEWER_APP
    m_updateLocation=1 ;
#else 
    m_updateLocation=0;
#endif
    m_lati = m_longi=0.0;
	m_mapWindow = NULL ;

    m_cliplist = NULL ;

    m_playerrect.top = 0 ;
    m_playerrect.left = 230 ;
    m_playerrect.bottom = 500 ;
    m_playerrect.right = 700 ;

	// initialize decoder library
	decoder::initlibrary();

	CreateDlg(IDD_DVRCLIENT_DIALOG);
}

DvrclientDlg::~DvrclientDlg()
{

#ifdef NEWPAINTING
        if( m_bkbmp ) {
            delete m_bkbmp ;
        }
#endif

    if( m_mapWindow ) {
        m_mapWindow->Release();
    }

    if( m_cliplist ) {
        delete m_cliplist ;
    }

	CloseHandle(m_hmutx);

}

int PTZ_ctrl_id[]={
IDC_GROUP_PTZ,
IDC_SPIN_PTZ_PRESET,
IDC_PRESET_ID,
IDC_PTZ_GOTO,
IDC_PTZ_CLEAR,
IDC_PTZ_SET,
IDC_PTZ_TELE,
IDC_PTZ_WIDE,
IDC_PTZ_FAR,
IDC_PTZ_NEAR,
IDC_PTZ_OPEN,
IDC_PTZ_CLOSE,
IDC_STATIC_PRESETID,
0,
};

int PTZ_hide_ctrl_id[]={
IDC_STATIC_GROUP_CUSTOMCLIP,
IDC_TIMEBEGIN,
IDC_TIME_SELBEGIN,
IDC_TIMEEND,
IDC_TIME_SELEND,
IDC_MONTHCALENDAR,
IDC_SAVEFILE,
0,
};

int playback_ctrl_id[]={
IDC_SAVEFILE,
#ifdef  IDC_EXPORTJPG
IDC_EXPORTJPG,
#endif
IDC_QUICKCAP,
IDC_TIMEBEGIN,
IDC_TIMEEND,
IDC_TIME_SELBEGIN,
IDC_TIME_SELEND,
IDC_SLOW,
IDC_BACKWARD,
IDC_PLAY,
IDC_PAUSE,
IDC_FORWARD,
IDC_FAST,
IDC_STEP,
IDC_PREV,
IDC_NEXT,
IDC_COMBO_TIMERANGE,
IDC_MONTHCALENDAR,
IDC_SLIDER,
0,
};

// ctrl id on top control box
int top_ctrl_id[]={
    IDC_BUTTON_PLAY,
    IDC_DISCONNECT,
    IDC_SETUP,
    IDC_USERS,
    IDC_PREVIEW,
    IDC_PLAYBACK,
    IDC_PLAYFILE,
    IDC_EXIT,
    IDC_SMARTSERVER,
    IDC_STATIC_QUICKTOOLS,
    IDC_QUICKCAP,
    IDC_CAPTURE,
    IDC_STATIC_GROUP_CUSTOMCLIP,
    IDC_TIMEBEGIN,
    IDC_TIME_SELBEGIN,
    IDC_TIMEEND,
    IDC_TIME_SELEND,
    IDC_SAVEFILE,
    IDC_MONTHCALENDAR,
    IDC_GROUP_PTZ,
    IDC_PTZ_TELE,
    IDC_PTZ_WIDE,
    IDC_PTZ_FAR,
    IDC_PTZ_NEAR,
    IDC_PTZ_OPEN,
    IDC_PTZ_CLOSE,
    IDC_STATIC_PRESETID,
    IDC_PRESET_ID,
    IDC_SPIN_PTZ_PRESET,
    IDC_PTZ_SET,
    IDC_PTZ_CLEAR,
    IDC_PTZ_GOTO,
    IDC_STATIC_PLAYTIME,
    IDC_PREV,
    IDC_COMBO_TIMERANGE,
    IDC_NEXT,
#ifdef  IDC_EXPORTJPG
    IDC_EXPORTJPG,
#endif
#ifdef  IDC_BUTTON_TVSINFO
    IDC_BUTTON_TVSINFO,
#endif
#ifdef  IDC_EDIT_LOCATION
    IDC_EDIT_LOCATION,
#endif
#ifdef  IDC_STATIC_OID
	IDC_STATIC_OID,
	IDC_BUTTON_NEWOFFICERID,
	IDC_COMBO_OFFICERID,
#endif
#ifdef IDC_BUTTON_TAGEVENT
	IDC_BUTTON_TAGEVENT,
#endif
    0
}; 


screenrect screen1[1] =     {   { 0.0  , 0.0  , 1.0  , 1.0  } } ;
screenrect screen2_1[2] =   {   { 0.0  , 0.0  , 1.0  , 1.0/2}, 
                                { 0.0  , 1.0/2, 1.0  , 1.0/1} };
screenrect screen2_2[2] =   {   { 0.0  , 0.0  , 1.0/2, 1.0  }, 
                                { 1.0/2, 0.0  , 1.0  , 1.0  } };

screenrect screen3x1[3] =   {   { 0.0  , 0.0  , 1.0/3, 1.0  }, { 1.0/3, 0.0  , 2.0/3  , 1.0  }, { 2.0/3, 0.0  , 1.0  , 1.0  } };
screenrect screen4x1[4] =   {   { 0.0  , 0.0  , 1.0/4, 1.0  }, { 1.0/4, 0.0  , 2.0/4  , 1.0  },{ 2.0/4  , 0.0  , 3.0/4, 1.0  }, { 3.0/4, 0.0 , 1.0  , 1.0  } };
screenrect screen5x1[5] =   {   { 0.0  , 0.0  , 1.0/5, 1.0  }, { 1.0/5, 0.0  , 2.0/5  , 1.0  },{ 2.0/5  , 0.0  , 3.0/5, 1.0  }, { 3.0/5, 0.0 , 4.0/5  , 1.0  }, { 4.0/5, 0.0 , 1.0  , 1.0  } };

screenrect screen2x2[4] = {   {0.0, 0.0, 0.5, 0.5}, 
							{0.5, 0.0, 1.0, 0.5},
							{0.0, 0.5, 0.5, 1.0}, 
							{0.5, 0.5, 1.0, 1.0} };
screenrect screen3x3[9] = {   {0.0,   0.0,   0.333, 0.333}, 
							{0.333, 0.0,   0.666, 0.333},
							{0.666, 0.0,   1.0,   0.333}, 
							{0.0,   0.333, 0.333, 0.666},
							{0.333, 0.333, 0.666, 0.666}, 
							{0.666, 0.333, 1.0,   0.666},
							{0.0,   0.666, 0.333, 1.0},
							{0.333, 0.666, 0.666, 1.0},
							{0.666, 0.666, 1.0  , 1.0} };		

screenrect screen3x2[6] = {
	{0.0/3, 0.0/2, 1.0/3, 1.0/2 }, {1.0/3, 0.0/2, 2.0/3, 1.0/2 }, {2.0/3, 0.0/2, 3.0/3, 1.0/2 },
	{0.0/3, 1.0/2, 1.0/3, 2.0/2 }, {1.0/3, 1.0/2, 2.0/3, 2.0/2 }, {2.0/3, 1.0/2, 3.0/3, 2.0/2 } };

screenrect screen4x4[16] = { {0.0,  0.0,  0.25, 0.25 }, 
							{0.25, 0.0,  0.5 , 0.25 },
							{0.5 , 0.0,  0.75, 0.25 }, 
							{0.75, 0.0,  1.0,  0.25 },
							{0.0,  0.25, 0.25, 0.5  }, 
							{0.25, 0.25, 0.5,  0.5  },
							{0.5,  0.25, 0.75, 0.5  },
							{0.75, 0.25, 1.0,  0.5  },
							{0.0,  0.5,  0.25, 0.75 },
							{0.25, 0.5,  0.5,  0.75 }, 
							{0.5,  0.5,  0.75, 0.75 },
							{0.75, 0.5,  1.0,  0.75 }, 
							{0.0,  0.75, 0.25, 1.0  },
							{0.25, 0.75, 0.5,  1.0  },
							{0.5,  0.75, 0.75, 1.0  },
							{0.75, 0.75, 1.0,  1.0  } };							

screenrect screen1p7[8] = { {0.0,  0.0,  0.75, 0.75 }, 
							{0.75, 0.0,  1.0,  0.25 },
							{0.75, 0.25, 1.0,  0.5  },
							{0.75, 0.5,  1.0,  0.75 }, 
							{0.0,  0.75, 0.25, 1.0  },
							{0.25, 0.75, 0.5,  1.0  },
							{0.5,  0.75, 0.75, 1.0  },
							{0.75, 0.75, 1.0,  1.0  } };							

screenrect screen4x2[8] = { {0.0,  0.0,  0.25, 0.5 }, 
							{0.25, 0.0,  0.5,  0.5 },
							{0.5 , 0.0,  0.75, 0.5 },
							{0.75, 0.0,  1.0,  0.5 }, 
							{0.0,  0.5,  0.25, 1.0  },
							{0.25, 0.5,  0.5,  1.0  },
							{0.5,  0.5,  0.75, 1.0  },
							{0.75, 0.5,  1.0,  1.0  } };							

screenrect screen2x4[8] = { {0.0,  0.0,  0.5,  0.25 }, 
							{0.5,  0.0,  1.0,  0.25 },
							{0.0,  0.25, 0.5,  0.5  },
							{0.5,  0.25, 1.0,  0.5  }, 
							{0.0,  0.5,  0.5,  0.75 },
							{0.5,  0.5,  1.0,  0.75 }, 
							{0.0,  0.75, 0.5,  1.0  },
							{0.5,  0.75, 1.0,  1.0  } };							

screenrect screen4x3[12] = {{0.0,  0.0,    0.25, 1.0/3 }, 
							{0.25, 0.0,    0.5,  1.0/3 },
							{0.5 , 0.0,    0.75, 1.0/3 },
							{0.75, 0.0,    1.0,  1.0/3 }, 
							{0.0,  1.0/3,  0.25, 2.0/3 }, 
							{0.25, 1.0/3,  0.5,  2.0/3 },
							{0.5 , 1.0/3,  0.75, 2.0/3 },
							{0.75, 1.0/3,  1.0,  2.0/3 }, 
							{0.0,  2.0/3,  0.25, 1.0 }, 
							{0.25, 2.0/3,  0.5,  1.0 },
							{0.5 , 2.0/3,  0.75, 1.0 },
                            {0.75, 2.0/3,  1.0,  1.0 } } ; 

screenrect screen1p5[6] = {   {0.0,   0.0,   0.666, 0.666}, 
							{0.666, 0.0,   1.0,   0.333}, 
							{0.666, 0.333, 1.0,   0.666},
							{0.0,   0.666, 0.333, 1.0},
							{0.333, 0.666, 0.666, 1.0},
							{0.666, 0.666, 1.0  , 1.0} };							


screenrect screen5x5[25] = {{0.0,  0.0,  0.2,  0.2 }, {0.2, 0.0, 0.4 , 0.2 }, {0.4 , 0.0, 0.6, 0.2 }, {0.6, 0.0, 0.8,  0.2 }, {0.8, 0.0, 1.0,  0.2 },
							{0.0,  0.2,  0.2,  0.4 }, {0.2, 0.2, 0.4,  0.4 }, {0.4,  0.2, 0.6, 0.4 }, {0.6, 0.2, 0.8,  0.4 }, {0.8, 0.2, 1.0,  0.4 },
							{0.0,  0.4,  0.2,  0.6 }, {0.2, 0.4, 0.4,  0.6 }, {0.4,  0.4, 0.6, 0.6 }, {0.6, 0.4, 0.8,  0.6 }, {0.8, 0.4, 1.0,  0.6 },
							{0.0,  0.6,  0.2,  0.8 }, {0.2, 0.6, 0.4,  0.8 }, {0.4,  0.6, 0.6, 0.8 }, {0.6, 0.6, 0.8,  0.8 }, {0.8, 0.6, 1.0,  0.8 },
                            {0.0,  0.8,  0.2,  1.0 }, {0.2, 0.8, 0.4,  1.0 }, {0.4,  0.8, 0.6, 1.0 }, {0.6, 0.8, 0.8,  1.0 }, {0.8, 0.8, 1.0,  1.0 }
                            };	

screenrect screen6x6[36] = {{0.0/6, 0.0/6, 1.0/6, 1.0/6 },{1.0/6, 0.0/6, 2.0/6, 1.0/6 },{2.0/6, 0.0/6, 3.0/6, 1.0/6 },{3.0/6, 0.0/6, 4.0/6, 1.0/6 },{4.0/6, 0.0/6, 5.0/6, 1.0/6 },{5.0/6, 0.0/6, 6.0/6, 1.0/6 },
    						{0.0/6, 1.0/6, 1.0/6, 2.0/6 },{1.0/6, 1.0/6, 2.0/6, 2.0/6 },{2.0/6, 1.0/6, 3.0/6, 2.0/6 },{3.0/6, 1.0/6, 4.0/6, 2.0/6 },{4.0/6, 1.0/6, 5.0/6, 2.0/6 },{5.0/6, 1.0/6, 6.0/6, 2.0/6 },
							{0.0/6, 2.0/6, 1.0/6, 3.0/6 },{1.0/6, 2.0/6, 2.0/6, 3.0/6 },{2.0/6, 2.0/6, 3.0/6, 3.0/6 },{3.0/6, 2.0/6, 4.0/6, 3.0/6 },{4.0/6, 2.0/6, 5.0/6, 3.0/6 },{5.0/6, 2.0/6, 6.0/6, 3.0/6 },
							{0.0/6, 3.0/6, 1.0/6, 4.0/6 },{1.0/6, 3.0/6, 2.0/6, 4.0/6 },{2.0/6, 3.0/6, 3.0/6, 4.0/6 },{3.0/6, 3.0/6, 4.0/6, 4.0/6 },{4.0/6, 3.0/6, 5.0/6, 4.0/6 },{5.0/6, 3.0/6, 6.0/6, 4.0/6 },
                            {0.0/6, 4.0/6, 1.0/6, 5.0/6 },{1.0/6, 4.0/6, 2.0/6, 5.0/6 },{2.0/6, 4.0/6, 3.0/6, 5.0/6 },{3.0/6, 4.0/6, 4.0/6, 5.0/6 },{4.0/6, 4.0/6, 5.0/6, 5.0/6 },{5.0/6, 4.0/6, 6.0/6, 5.0/6 },
                            {0.0/6, 5.0/6, 1.0/6, 6.0/6 },{1.0/6, 5.0/6, 2.0/6, 6.0/6 },{2.0/6, 5.0/6, 3.0/6, 6.0/6 },{3.0/6, 5.0/6, 4.0/6, 6.0/6 },{4.0/6, 5.0/6, 5.0/6, 6.0/6 },{5.0/6, 5.0/6, 6.0/6, 6.0/6 }
                            };

screenrect screen_pancam[6] = {
	{0.0/3, 0.0/2, 1.0/3, 1.0/2 }, {1.0/3, 0.0/2, 2.0/3, 1.0/2 }, {2.0/3, 0.0/2, 3.0/3, 1.0/2 },
	                               {1.0/3, 1.0/2, 2.0/3, 2.0/2 }, {2.0/3, 1.0/2, 3.0/3, 2.0/2 }, 
	{0.0/3, 0.0/2, 0.0/3, 0.0/2 },
};

#if defined(PAL_SCREEN_SUPPORT) 
int g_ratiox=720 ;
int g_ratioy=576 ;
#else
int g_ratiox=4 ;
int g_ratioy=3 ;
#endif

struct screenmode screenmode_table[32] = {	// table of screen mode
#ifdef APP_PW_TABLET
	{ _T("Single"), 1, 1, 1, screen1, NULL, 2 },
#else
	{ _T("Single"), 1, 1, 1, screen1, NULL, 0 },
#endif
	{ _T("Vertical 2"), 2, 1, 2, screen2_1, NULL, 0 },
	{ _T("Horizontal 2"), 2, 2, 1, screen2_2, NULL, 0},
	{ _T("2x2"), 4, 2, 2, screen2x2, NULL, 0},
   	{ _T("3x1"), 3, 3, 1, screen3x1, NULL, 0},
	{ _T("4x1"), 4, 4, 1, screen4x1, NULL, 0},
	{ _T("3x2"), 6, 3, 2, screen3x2, NULL, 0},
    { _T("1+5"), 6, 1, 1, screen1p5, NULL, 0},
//	{ _T("5x1"), 5, 5, 1, screen5x1, NULL, 0},
	{ _T("1+7"), 8, 1, 1, screen1p7, NULL, 0 },
	{ _T("4x2"), 8, 4, 2, screen4x2, NULL, 0 },
	{ _T("2x4"), 8, 2, 4, screen2x4, NULL, 0 },
	{ _T("3x3"), 9, 3, 3, screen3x3, NULL, 0},
	{ _T("4x3"), 12, 4, 3, screen4x3, NULL, 0},
	{ _T("4x4"), 16, 4, 4, screen4x4, NULL, 0},
	{ _T("5x5"), 25, 5, 5, screen5x5, NULL, 0},
	{ _T("6x6"), 36, 6, 6, screen6x6, NULL, 0},

	{NULL,0,0,0,NULL,NULL,0}
};

// const int screenmode_table_num = sizeof(screenmode_table)/sizeof(struct screenmode) ;
int screenmode_table_num = 32 ;

int g_screenmode = 33 ;		// automatic
int g_singlescreenmode = 0 ;

// Time range strings
#define TIMERANGE_ONEDAY    (_T("One Day View"))
#define TIMERANGE_ONEHOUR   (_T("One Hour View"))
#define TIMERANGE_15MIN     (_T("15 Minutes View"))
#define TIMERANGE_5MIN      (_T("5 Minutes View"))
#define TIMERANGE_WHOLEFILE (_T("Whole File View"))

BOOL CALLBACK DvrclientDlg::DvrEnumWindowsProc( HWND hwnd, LPARAM lParam )
{
	if (hwnd == (HWND)lParam) {
		return TRUE;
	}

	HANDLE tag = GetProp(hwnd, _T(APPNAME));

	if (tag == (HANDLE)0x55aa) {
        // found another instance.

		COPYDATASTRUCT cpd;
		cpd.dwData = 0x33613431;
		cpd.lpData = NULL;
		cpd.cbData = 0;

        // Check cmdline open
		string arg1;
		LPWSTR *szArglist;
        int nArgs;

        szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
        if( szArglist ) {
            if( nArgs>1 ) {
				arg1 = szArglist[1] ;
                cpd.lpData = (void *)(char *)arg1;
                cpd.cbData = strlen((char *)arg1) + 1 ;
            }
            LocalFree(szArglist);
        }

		if (SendMessage(hwnd, WM_COPYDATA, (WPARAM)(HWND)lParam, (LPARAM)&cpd)) {
			return FALSE;
		}
    }

    return TRUE ;
}

static BOOL CALLBACK _EnumThreadWndProc(HWND   hwnd, LPARAM lParam) 
{
	if (hwnd != (HWND)lParam) {
		Dialog * dlg = (Dialog *)Window::getWindow(hwnd);
		if (dlg && dlg->isDialog()) {
			SetForegroundWindow(hwnd);
			return FALSE;
		}
	}
	return TRUE;
}


int DvrclientDlg::OnCopyData( PCOPYDATASTRUCT pcd )
{
    if( pcd->dwData == 0x33613431) {
		if (g_maindlg != NULL) {

			// bring it to forground
			SetForegroundWindow(m_hWnd);

			string pfile = (char *)pcd->lpData;
			if (!pfile.isempty()) {

#ifndef GW_ENABLEDPOPUP
#define GW_ENABLEDPOPUP     6
#endif

				HWND hpop = GetWindow(m_hWnd, GW_ENABLEDPOPUP);
				if (hpop != NULL) {
					Dialog * d = (Dialog *)Window::getWindow(hpop);
					if (d && d->isDialog()) {
						d->EndDialog();
					}
				}
				Playfile(pfile);
			}
		}
		else {
			EnumThreadWindows(GetCurrentThreadId(), _EnumThreadWndProc, (LPARAM)m_hWnd);
		}
    }
	

	SetWindowLongPtr(m_hWnd, DWLP_MSGRESULT, TRUE);
    return TRUE ;
}

BOOL DvrclientDlg::OnInitDialog()
{
	int i ;

	// set main window ICON
	HICON hIcon = LoadIcon(ResInst(), MAKEINTRESOURCE(IDR_MAINFRAME));
	SendMessage(m_hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
	SendMessage(m_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

	// IDM_ABOUTBOX must be in the system command range.
	HMENU hSysMenu = ::GetSystemMenu(m_hWnd, FALSE);
	if (hSysMenu != NULL)
	{
		::AppendMenu(hSysMenu, MF_SEPARATOR, 0, NULL);
		::AppendMenu(hSysMenu, MF_STRING, IDM_ABOUTBOX, TMPSTR("&About %s", (char *)m_appname));
	}

	// to supress no disk error message, when calling FindFirstFile() on empty drive
	SetErrorMode(SEM_FAILCRITICALERRORS);

	SetProp(m_hWnd, _T(APPNAME), (HANDLE)0x55aa);

	m_hmutx = CreateMutex(NULL, 0, TMPSTR( "mutex:%s", APPNAME) );
	if (m_hmutx == NULL) {
		EndDialog(IDCANCEL);
		return TRUE;
	}

	if (WaitForSingleObject(m_hmutx, 10) != WAIT_OBJECT_0) {
		// test if another instance is running?
		if (EnumWindows(DvrEnumWindowsProc, (LPARAM)(HANDLE)m_hWnd) == 0) {
			EndDialog(IDCANCEL);
			return TRUE;
		}
	}

#if defined( APP_TVS )
	HWND hbtuser = GetDlgItem(m_hWnd, IDC_USERS);
	if( hbtuser )
		ShowWindow(hbtuser,SW_HIDE);

	struct tvskey_data * tvskey ;
	tvskey = tvs_readtvskey();
	if( tvskey ) {
		
#ifdef APP_PWVIEWER_VOLUMECHECK
		static struct volumecheck_type {
			GUID volume_check_tag ;	
			char username[256] ;
			char password[256] ;
			DWORD VolumeSerialNumber ;
		} volumecheck = {
			// {7745A171-3D20-4A15-AF81-8DBF66F30302}
			{ 0x7745a171, 0x3d20, 0x4a15, { 0xaf, 0x81, 0x8d, 0xbf, 0x66, 0xf3, 0x3, 0x2 } },
			"",
			"",
			0
		} ;

		if( volumecheck.VolumeSerialNumber != 0 ) {

			string app ;
			GetModuleFileName(NULL, app.tcssize(512), 511 );

			string volumePath ;
			GetVolumePathName( app, volumePath.tcssize(512), 511 );

			DWORD  SerialNumber = 0 ;
			GetVolumeInformation( volumePath, 
				NULL, 0,
				&SerialNumber,
				NULL,
				NULL,
				NULL, 0 );
			if( SerialNumber == volumecheck.VolumeSerialNumber ) {
				VolumePass vpdialog ;
				vpdialog.m_title = "Authentication" ;
				vpdialog.m_message = "Please enter username and password." ;
				for( int retry=0; retry<50; retry++ ) {
					if( retry>20 || vpdialog.DoModal( IDD_DIALOG_VOLUMEPASSWORD, m_hWnd ) != IDOK ) {
						EndDialog(IDCANCEL);
						return FALSE ;
					}
					else {
						// check username, password
						unsigned char k256_user[256] ;
						unsigned char k256_pass[256] ;
						key_256( vpdialog.m_username, k256_user);
						key_256( vpdialog.m_password, k256_pass);

						if( memcmp( k256_user, volumecheck.username, 256)==0 &&
							memcmp( k256_pass, volumecheck.password, 256)==0 ) {
							break ;
						}
						else {
							vpdialog.m_message = "Authentication error! please retry.";
							continue ;
						}
					}
				}
			}
			else {
				MessageBox( m_hWnd, _T("Wrong Media, application quit!"), _T("Error"), MB_OK ) ;
				EndDialog(IDCANCEL);
				return FALSE ;
			}
		}

#endif

		UsbPasswordDlg dlgusbpassword ;

		if( tvskey->usbid[0]=='M' && tvskey->usbid[1]=='F' ) {
			g_usertype = IDADMIN ;
            dlgusbpassword.m_key = tvskey->mfpassword ;
		}
		else if( tvskey->usbid[0]=='I' && tvskey->usbid[1]=='N' ) { 
			g_usertype = IDINSTALLER ;
            dlgusbpassword.m_key = tvskey->inpassword ;
		}
		else if( tvskey->usbid[0]=='P' && tvskey->usbid[1]=='L' ) { 
			g_usertype = IDPOLICE ;
			ShowWindow(GetDlgItem(m_hWnd, IDC_SETUP),SW_HIDE);
            dlgusbpassword.m_key = tvskey->plpassword ;
		}
		else if( tvskey->usbid[0]=='I' && tvskey->usbid[1]=='S' ) { 
			g_usertype = IDINSPECTOR ;
			ShowWindow(GetDlgItem(m_hWnd, IDC_SETUP),SW_HIDE);
            dlgusbpassword.m_key = tvskey->ispassword ;
		}
		else if( tvskey->usbid[0]=='O' && tvskey->usbid[1]=='P' ) { 
			g_usertype = IDOPERATOR ;
			ShowWindow(GetDlgItem(m_hWnd, IDC_SETUP),SW_HIDE);
            dlgusbpassword.m_key = tvskey->oppassword ;
		}
		else {
			MessageBox(m_hWnd, _T("PLEASE INSERT USB KEY!"), NULL, MB_ICONERROR|MB_OK );
			EndDialog(IDCANCEL);
			return FALSE ;
		}
#ifndef APP_PWPLAYER
#ifndef APP_PW_TABLET
#ifndef APP_TVS_PR1ME
		if( dlgusbpassword.DoModal(IDD_DIALOG_USBPASSWORD )!=IDOK ) {
			EndDialog(IDCANCEL);
			return TRUE ;
		}
#endif
#endif
#endif

		decoder::settvskey(tvskey);
	}

#ifndef APP_TVS_PR1ME
	else {
		MessageBox(m_hWnd, _T("PLEASE INSERT USB KEY!"), NULL, MB_ICONERROR|MB_OK );
		EndDialog(IDCANCEL);
		return FALSE ;
	}
#endif

#elif defined( NOLOGIN )
	ShowWindow( GetDlgItem(m_hWnd, IDC_USERS), SW_HIDE );
	g_usertype=IDUSER ;
#else

	// Show login dialog
	if (isportableapp()) {
		ShowWindow(GetDlgItem(m_hWnd, IDC_USERS), SW_HIDE);
		g_usertype = IDUSER;
	}
	else {
		LoginDlg login(NULL);
		if (login.DoModal() != IDOK) {
			EndDialog(IDCANCEL);
			return TRUE;
		}
		g_usertype = login.m_usertype;
	}
#endif

	// calculate screen_table_num
	for(i=0; i<32; i++ ) {
		if( screenmode_table[i].numscreen == 0 ) {
			screenmode_table_num = i ;
			break ;
		}
	}

	m_ptz_cursor=(HCURSOR) LoadImage(ResInst(), MAKEINTRESOURCE( IDI_PTZBALL ), IMAGE_ICON, 0, 0, LR_SHARED );

	::SendDlgItemMessage(m_hWnd, IDC_SPIN_PTZ_PRESET, UDM_SETRANGE, 0, MAKELPARAM(1,32));
	::SetDlgItemText(m_hWnd, IDC_PRESET_ID, _T("1"));
	// Init time range strings
	::SendDlgItemMessage(m_hWnd, IDC_COMBO_TIMERANGE, CB_ADDSTRING, 0, (LPARAM)TIMERANGE_ONEDAY);
	::SendDlgItemMessage(m_hWnd, IDC_COMBO_TIMERANGE, CB_ADDSTRING, 0, (LPARAM)TIMERANGE_ONEHOUR);
	::SendDlgItemMessage(m_hWnd, IDC_COMBO_TIMERANGE, CB_ADDSTRING, 0, (LPARAM)TIMERANGE_15MIN);

#ifdef    WMVIEWER_APP
	::SendDlgItemMessage(m_hWnd, IDC_COMBO_TIMERANGE, CB_ADDSTRING, 0, (LPARAM)TIMERANGE_5MIN);
#endif

    m_clientmode=CLIENTMODE_CLOSE;

	CtrlSet();		// set default player control

//	EnableToolTips(TRUE);

	g_playstat=PLAY_STOP;

	// setup a UPDATE seconds timer
	m_update_time=1000 ;
    m_tbarupdate = 1 ;
    SetTimer(m_hWnd, TIMER_UPDATE, 10, NULL);
	
#if defined( APP_PW_TABLET ) || defined( APP_TVS ) 
	// replace SLIDER with bsliderbar
	DestroyWindow( GetDlgItem(m_hWnd, IDC_SLIDER) ) ;
	m_slider.Create( m_hWnd, IDC_SLIDER );
#else
    // attach slider
    m_slider.attach( GetDlgItem(m_hWnd, IDC_SLIDER) );
#endif

	// initialize speaker volume
    m_volume.Create( this->m_hWnd, IDC_SLIDER_VOLUME );

    DWORD status = 0 ;
// get preferred device ID
#define	DRVM_MAPPER_PREFERRED_GET (0x2000+21)
	m_hmx=NULL ;
	if( waveOutMessage((HWAVEOUT)WAVE_MAPPER, DRVM_MAPPER_PREFERRED_GET, (DWORD_PTR)&m_waveoutId, (DWORD_PTR)&status)
		== MMSYSERR_NOERROR ) {
		mixerOpen(&m_hmx, (UINT)m_waveoutId, (DWORD_PTR)m_hWnd, NULL, MIXER_OBJECTF_WAVEOUT|CALLBACK_WINDOW );
	}
	else {
		m_waveoutId = WAVE_MAPPER ;
	}

	SetVolume();

	// initialize tooltip
	int id ;
	TOOLINFO toolInfo ;
    HWND hwndTT = CreateWindowEx(NULL,
        TOOLTIPS_CLASS, NULL,
        WS_POPUP | TTS_BALLOON,		
        CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT,
        m_hWnd, NULL, AppInst(), NULL);
	for( id=1000; id<3000; id++ ) {		// this should cover all IDC number
		HWND hctl = ::GetDlgItem(m_hWnd, id);
		if( hctl ) {
			string idstr ;
			if( ::LoadString(ResInst(), id, idstr.tcssize(80), 80)>0 ) {
				// Associate the tooltip with the tool.
				memset(&toolInfo,0,sizeof(toolInfo));
				toolInfo.cbSize = sizeof(toolInfo);
				toolInfo.hwnd = m_hWnd;
				toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
				toolInfo.uId = (UINT_PTR)hctl;
				toolInfo.lpszText = LPSTR_TEXTCALLBACK;
				SendMessage(hwndTT, TTM_ADDTOOL, 0, (LPARAM)&toolInfo);
			}
		}
	}

	g_ratiox = reg_read("ratiox");
	g_ratioy = reg_read("ratioy");

	SetLayout();

    SYSTEMTIME monthrange[2] ;
    memset( monthrange, 0, sizeof(monthrange));
    monthrange[0].wYear = 2000 ;
    monthrange[0].wMonth = 1 ;
    monthrange[0].wDay = 1 ;
    monthrange[1].wYear = 2100 ;
    monthrange[1].wMonth = 1 ;
    monthrange[1].wDay = 1 ;
    ::SendDlgItemMessage( m_hWnd, IDC_MONTHCALENDAR, MCM_SETRANGE, GDTR_MAX|GDTR_MIN, (LPARAM)&monthrange );
    UpdateMonthcalendar();

#ifdef WMVIEWER_APP
    ::SetDlgItemText(m_hWnd, IDC_EDIT_LOCATION, _T(""));
#endif

	// speaker icon not display by control, 
	::ShowWindow( ::GetDlgItem(m_hWnd, IDC_PIC_SPEAKER_VOLUME), SW_HIDE );

    // Delayed minitrack init
    ::SetTimer(m_hWnd, TIMER_INITMINITRACK, 5000, NULL);

#ifdef APP_PW_TABLET
	// set to full screen mode
	SetWindowLong(m_hWnd, GWL_STYLE, WS_POPUP );
	ShowWindow(m_hWnd, SW_MAXIMIZE);

	m_volume.Hide();

	// auto bitmap buttons
	extern struct bmp_button_type bmp_buttons[] ;
	for( i=0; i<100&&bmp_buttons[i].id>0 ;i++ ) {
		AutoBitmapButton * bt = new AutoBitmapButton ;		// don't delete bt, let ondetach() do it
		bt->attach( GetDlgItem(m_hWnd, bmp_buttons[i].id) );
		bt->SetBitmap(bmp_buttons[i].img, bmp_buttons[i].img_sel );
	}

	ShowWindow(GetDlgItem(m_hWnd, IDC_BUTTON_CLOSE), SW_HIDE);
#endif

// try restore privous screen mode
	g_screenmode = reg_read( "screenmode");
	if( g_screenmode>=0 && g_screenmode<screenmode_table_num ) {
		SetScreenFormat();
#ifdef APP_PW_TABLET
		string chname ;
		for( i=0; i < MAXSCREEN; i++ ) {
			if( m_screen[i] != NULL ) {
				sprintf( chname.strsize(50), "screen%d", i );
				// restore previous closed channel
				m_screen[i]->m_xchannel = reg_read( chname );
			}
		}
#endif
	}
	else {
		// set screen mode, startup at 2x2 with 4 channel screen
#ifdef APP_PW_TABLET
		SetScreenFormat("Single");
#else
		m_screenmode=3 ;
		m_maxdecscreen = 4 ;
		SetScreenFormat();
#endif
	}

	// mark it initialized, player available
	g_maindlg = this;
	
#ifdef APP_PWII_FRONTEND
    SetTimer( m_hWnd, TIMER_AUTOSTART, 1000, NULL );
//    OnBnClickedButtonPlay();        //Start liveview 
#else
	int nPlay=0 ;

	// check portable autoplay file
	if( isportableapp() ) {
		string portableautoplay ;
		portableautoplay = "portableplay" ;
		getfilepath(portableautoplay);
		FILE * fport = fopen( portableautoplay, "r");
		if( fport!=NULL ) {
			string fname ;
			if( fscanf( fport, "%s", fname.strsize(512) )==1 ) {
				if( strchr( fname, '\\' )==NULL ) {
					Playfile(TMPSTR("%s\\..\\%s", getapppath(), (char *)fname));
				}
				else {
					Playfile(fname);
				}
				nPlay = 1;
			}
			fclose( fport );
		}
	}

	if (nPlay == 0) {
		// Check cmdline open
		LPWSTR *szArglist;
		int nArgs = 0 ;
		szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
		if (szArglist) {
			if (nArgs > 1) {
				Playfile(szArglist[1]);
				nPlay = 1;
			}
			LocalFree(szArglist);
		}
	}

#endif

    return TRUE;  // return TRUE  unless you set the focus to a control
}


// setup screen layout at system up
void DvrclientDlg::SetLayout()
{
	RECT rect ;
	int i ;
    int offset_x ;
    int x, y, w, h ;
    Bitmap * img ;

	// move PTZ controls
	int center_cal, center_ptz ;
    ::GetClientRect(::GetDlgItem(m_hWnd, IDC_MONTHCALENDAR), &rect);
    ::MapWindowPoints(::GetDlgItem(m_hWnd, IDC_MONTHCALENDAR), m_hWnd, (LPPOINT)&rect, 2);
	center_cal = ( rect.left + rect.right )/2 ;
    ::GetClientRect(::GetDlgItem(m_hWnd, IDC_GROUP_PTZ), &rect);
    ::MapWindowPoints(::GetDlgItem(m_hWnd, IDC_GROUP_PTZ), m_hWnd, (LPPOINT)&rect, 2);
	center_ptz = ( rect.left + rect.right ) /2 ;
    offset_x = center_cal - center_ptz ;
	i=0; 
	while( PTZ_ctrl_id[i] ) {
		HWND hwnd = ::GetDlgItem(m_hWnd, PTZ_ctrl_id[i] );
		if( hwnd ) {
            ::GetClientRect(::GetDlgItem(m_hWnd, PTZ_ctrl_id[i]), &rect);
            w = rect.right ;
            h = rect.bottom ;
            ::MapWindowPoints( ::GetDlgItem(m_hWnd, PTZ_ctrl_id[i]),m_hWnd, (LPPOINT)&rect, 2);
    		::MoveWindow( ::GetDlgItem(m_hWnd, PTZ_ctrl_id[i]), 
                rect.left+offset_x, rect.top, w, h,
                FALSE );
        }
        i++;
	}

    // setup ptz circle size (just size).
	img = loadbitmap(_T("PTZ_DE"));
	m_ptzcampass_rect.top = 0 ;
	m_ptzcampass_rect.bottom = img->GetHeight();
	m_ptzcampass_rect.left = 0 ;
	m_ptzcampass_rect.right = img->GetWidth() ;
	delete img ;

	// initialize panel controls, only to setup width
    ::GetClientRect(::GetDlgItem(m_hWnd, IDC_MONTHCALENDAR), &m_panelrect); // this is enough to do the job

    // initialize status icon rect, only to setup size
   	m_statusiconrect.top = 0 ;
	m_statusiconrect.left = 0 ;
    img = loadbitmap(_T("OPEN_DISCONNECT"));
    if( img ) {
        m_statusiconrect.bottom = img->GetHeight(); ;
        m_statusiconrect.right = img->GetWidth();
        delete img ;
    }
    else {
        m_statusiconrect.bottom = m_statusiconrect.top + 65 ;
        m_statusiconrect.right = m_statusiconrect.left + 65 ;
    }

	// initialize bottom controls, set bottom bar height, and line up buttons
    m_bottombar.top = 0 ;
    m_bottombar.left = 0 ;
    m_bottombar.right = 1 ;
    m_bottombar.bottom = 1 ;        
	x=0 ;
	y=100 ;
	w=0 ;
	for(i=0;btbar_ctls[i].id>0;i++) {
		if(btbar_ctls[i].xadj>-900) {
			x+=w+btbar_ctls[i].xadj ;
			w=0;
			if( btbar_ctls[i].imgname ) {
				img = loadbitmap( btbar_ctls[i].imgname );
				if( img ) {
					w = img->GetWidth();
					h = img->GetHeight();
					delete img ;
				}
			}
			if( w==0 ) {
				::GetClientRect(  ::GetDlgItem(m_hWnd, btbar_ctls[i].id), &rect );
                w = rect.right ;
				h = rect.bottom ;
			}
		}
        
		::MoveWindow( ::GetDlgItem(m_hWnd, btbar_ctls[i].id), 
			x, y-h/2, w+1, h, 
			FALSE );
        if( h>m_bottombar.bottom ) {
            m_bottombar.bottom = h ;
        }
    }
    m_bottombar.bottom+=2 ;         // only need to set this for height, onsize() would do the layout

#ifdef APP_PW_TABLET
	m_bottombar.bottom=120 ;
#endif

    ::GetWindowRect( m_hWnd, &rect);
	m_minxsize=rect.right-rect.left ;

    if( m_minxsize< (x+w+180) ) {
        m_minxsize = x+w+180 ;
    }

	m_minysize=rect.bottom-rect.top-30 ;
    ::MoveWindow( m_hWnd,
        rect.left, rect.top,
        m_minxsize+120, m_minysize, TRUE );

#ifdef APP_PWPLAYER
    m_companylinkrect.left = 0 ;
    m_companylinkrect.top = 0 ;
    img = loadbitmap( _T("PATROLWITNESS") ) ;
    if( img ) {
        m_companylinkrect.right = img->GetWidth();
        m_companylinkrect.bottom = img->GetHeight();
        delete img ;
    }
    else {
        m_companylinkrect.right = 100;
        m_companylinkrect.bottom = 50;
    }
#endif

}

// Clip List
void DvrclientDlg::DisplayClipList(int lockclip)
{
    WaitCursor waitcursor ;
    int i;
    if( m_screen[m_focus]->m_decoder==NULL ) {
        return ;
    }

    if( m_clientmode != CLIENTMODE_PLAYBACK &&
        m_clientmode != CLIENTMODE_PLAYFILE &&
        m_clientmode != CLIENTMODE_PLAYSMARTSERVER ) {
        return ;
    }

    if( m_cliplist ) {
        delete m_cliplist ;
        m_cliplist=NULL ;
    }

    char * tmpdir = getenv("TEMP");
    if( tmpdir==NULL ) {
        tmpdir = "D:\\tmp";
    }
    string clipdir;
	clipdir.printf( "%s\\%s", tmpdir, (char *)m_appname);
    mkdir(clipdir);

    Cliplist * cliplist = new Cliplist( this, m_screen[m_focus], lockclip, &m_playertime, (char *)clipdir ) ;

    if( cliplist->getlistsize() <= 0 ) {
        MessageBox(m_hWnd, _T("No clip available!"), NULL, MB_OK );
        delete cliplist ;
        return ;
    }
           
    for(i=0;i<m_screennum;i++) {
        if( i!=m_focus && m_screen[i] ) {
            m_screen[i]->Hide();
        }
    }
    m_screen[m_focus]->Move( 0,0, 1, 1 );

    OnBnClickedPause();

    //    cliplist->Show();
    //    cliplist->MsgLoop();
    //    delete cliplist ;
    //    cliplist->release();

    m_cliplist = cliplist ;

    OnSize( 0, 0, 0 );

//    for(i=0;btbar_ctls[i].id;i++){
//        ShowWindow(::GetDlgItem(m_hWnd, btbar_ctls[i].id), SW_HIDE);
//    }

    ::ShowWindow( m_slider.getHWND(), SW_HIDE );
	EnableWindow(GetDlgItem(m_hWnd, IDC_MONTHCALENDAR ),FALSE);
	EnableWindow(GetDlgItem(m_hWnd, IDC_PREV ),FALSE);
	EnableWindow(GetDlgItem(m_hWnd, IDC_NEXT ),FALSE);
	EnableWindow(GetDlgItem(m_hWnd, IDC_COMBO_TIMERANGE ),FALSE);

}

void DvrclientDlg::CloseClipList()
{
    int i;
    if( m_cliplist ) {
        if( m_cliplist->getHWND() != NULL ) {
            ::DestroyWindow( m_cliplist->getHWND() );
        }

        for(i=0;btbar_ctls[i].id;i++){
            ShowWindow(::GetDlgItem(m_hWnd, btbar_ctls[i].id), SW_SHOW);
        }
        ::ShowWindow( m_slider.getHWND(), SW_SHOW );
        EnableWindow(GetDlgItem(m_hWnd, IDC_MONTHCALENDAR ),TRUE);
        EnableWindow(GetDlgItem(m_hWnd, IDC_PREV ),TRUE);
        EnableWindow(GetDlgItem(m_hWnd, IDC_NEXT ),TRUE);
        EnableWindow(GetDlgItem(m_hWnd, IDC_COMBO_TIMERANGE ),TRUE);

        OnSize( 0, 0, 0 );

        SetScreenFormat();

        dvrtime dvrt ;
        if(  ((Cliplist*)m_cliplist)->getSelect( &dvrt ) ) {
            SeekTime(&dvrt);
        }
        else {
            SeekTime(&m_playertime);
        }
        CtrlSet();
//        this->OnBnClickedPlay();
        UpdateMap();

    }
}

#ifdef  NEWPAINTING
void DvrclientDlg::OnSize(UINT nType, int cx, int cy)
{
	int i, x, y, w, h ;
	int  off_x, off_y ;
	RECT rect ;
    int  sliderheight ;

    if( cx==0 ) {
        ::GetClientRect( m_hWnd, &rect);
        cx = rect.right ;
        cy = rect.bottom ;
    }

    // setup control layout,
    if( control_align == 1 ) {       // 0:left, 1:right
        // controls on right, video screen on left
        w = m_panelrect.right - m_panelrect.left ;
        m_panelrect.left = cx - w ;
        m_panelrect.right = cx ;
        m_playerrect.left = 0 ;
        m_playerrect.right = cx - w ;
    }
    else if( control_align == 2 ) {       // 0:left, 1:right, 2: bottom
        // controls on bottom, video screen on top
        w = cx ;
        m_panelrect.left = 0; 
        m_panelrect.right = w;

        m_playerrect.left = 0 ;
        m_playerrect.right = cx ;
    }
    else    
    {
        // controls on left, video screen on right
        w = m_panelrect.right - m_panelrect.left ;
        m_panelrect.left = 0 ;
		m_panelrect.right = w ;
        m_playerrect.left = w ;
        m_playerrect.right = cx ;
    }

    ::GetClientRect( m_slider.getHWND(), &rect);	
    sliderheight = rect.bottom - rect.top ;

    if(m_zoom) {
        m_panelrect.left += cx ;                // move out of sight
		m_panelrect.right += cx ;           
        m_panelrect.top = cy ;               
        m_panelrect.bottom = cy+1000 ;

        m_playerrect.top = 0 ;                  // full screen
        m_playerrect.bottom = cy ;
        m_playerrect.left = 0 ;
        m_playerrect.right = cx ;
    }
    else {
        if( control_align == 2 ) {       // 0:left, 1:right, 2: bottom
            if( m_panelhide ) {
                h = m_bottombar.bottom-m_bottombar.top ;
                m_panelrect.top = cy + 1000 ;
                m_panelrect.bottom = m_panelrect.top + h ;

                m_playerrect.top = 0 ;
                m_playerrect.bottom = m_bottombar.top - sliderheight ;	// adjust later
            }
            else {
                m_panelrect.bottom = cy-(m_bottombar.bottom-m_bottombar.top) ;
                m_panelrect.top = m_panelrect.bottom - 210 ;

                m_playerrect.top = 0 ;
                m_playerrect.bottom = m_panelrect.top - sliderheight ;	// adjust later
            }
        }
        else {
            if( m_panelhide ) {
                w = m_panelrect.right-m_panelrect.left ;
                m_panelrect.right = -w-50 ;
                m_panelrect.left = m_panelrect.right - w;

                m_playerrect.left = 0 ;
                m_playerrect.right = cx ;
                m_playerrect.top = 0 ;
                m_playerrect.bottom = m_bottombar.top - sliderheight ;	// adjust later
            }
            else {
                m_panelrect.top = 0 ;
                m_panelrect.bottom = cy-(m_bottombar.bottom-m_bottombar.top) ;

                m_playerrect.top = 0 ;
                m_playerrect.bottom = m_panelrect.bottom - sliderheight ;	// adjust later
            }
        }
    }

	// Move buttom bar
	m_bottombar.left = 0;
	m_bottombar.right = cx ;
    h = m_bottombar.bottom - m_bottombar.top ;
	m_bottombar.bottom = cy ;
    m_bottombar.top = m_bottombar.bottom - h ;

    if(m_zoom) {
        m_bottombar.top += h+100 ;
        m_bottombar.bottom += h+100 ;
    }

    // Move Control Panel
    if( control_align == 2 ) {       // 0:left, 1:right, 2: bottom
	    ::GetClientRect( ::GetDlgItem(m_hWnd, IDC_MONTHCALENDAR ), &rect);	
	    ::MapWindowPoints( ::GetDlgItem(m_hWnd, IDC_MONTHCALENDAR ), m_hWnd, (LPPOINT)&rect, 2);
        off_y = ( m_panelrect.bottom+m_panelrect.top)/2 - (rect.bottom+rect.top)/2 ;
        if( off_y ) {
            for(i=0;top_ctrl_id[i];i++){
                ::GetWindowRect( ::GetDlgItem(m_hWnd, top_ctrl_id[i]), &rect );
                w = rect.right-rect.left ;
                h = rect.bottom-rect.top ;
                ScreenToClient(m_hWnd, (LPPOINT)&rect);
                rect.top += off_y ;
                ::MoveWindow( ::GetDlgItem(m_hWnd, top_ctrl_id[i]), 
                    rect.left, rect.top, w, h, 
                    TRUE );
    //            ::InvalidateRect(::GetDlgItem(m_hWnd, top_ctrl_id[i]), NULL, FALSE );
            }


        }
    }
    else {
	    ::GetClientRect( ::GetDlgItem(m_hWnd, IDC_MONTHCALENDAR ), &rect);	
	    ::MapWindowPoints( ::GetDlgItem(m_hWnd, IDC_MONTHCALENDAR ), m_hWnd, (LPPOINT)&rect, 2);
        off_x = ( m_panelrect.left+m_panelrect.right)/2 - (rect.left+rect.right)/2 ;
        if( off_x ) {
            for(i=0;top_ctrl_id[i];i++){
                ::GetWindowRect( ::GetDlgItem(m_hWnd, top_ctrl_id[i]), &rect );
                w = rect.right-rect.left ;
                h = rect.bottom-rect.top ;
                ScreenToClient(m_hWnd, (LPPOINT)&rect);
                rect.left += off_x ;
                ::MoveWindow( ::GetDlgItem(m_hWnd, top_ctrl_id[i]), 
                    rect.left, rect.top, w, h, 
                    TRUE );
    //            ::InvalidateRect(::GetDlgItem(m_hWnd, top_ctrl_id[i]), NULL, FALSE );
            }
        }
    }
    
#ifdef APP_PWPLAYER
    w = m_companylinkrect.right - m_companylinkrect.left ;
    h = m_companylinkrect.bottom - m_companylinkrect.top ;
    m_companylinkrect.left = (m_panelrect.right + m_panelrect.left)/2-w/2 ;
    m_companylinkrect.right = m_companylinkrect.left+w ;
    m_companylinkrect.top = m_panelrect.top + 380 ;
    m_companylinkrect.bottom =  m_companylinkrect.top + h ;
#endif

    // Move bottom bar
	::GetClientRect( ::GetDlgItem(m_hWnd, btbar_refid), &rect);	
	::MapWindowPoints( ::GetDlgItem(m_hWnd, btbar_refid), m_hWnd, (LPPOINT)&rect, 2);
	if( btbar_align>=0 ) {
		off_x = m_bottombar.left - rect.left + btbar_align;
	}
	else {
		off_x = m_bottombar.right - rect.right + btbar_align;
	}
	off_y = (m_bottombar.top+m_bottombar.bottom)/2 - (rect.bottom+rect.top)/2 ;

	for(i=0;btbar_ctls[i].id;i++){
		::GetClientRect( ::GetDlgItem(m_hWnd, btbar_ctls[i].id), &rect );
		w = rect.right ;
		h = rect.bottom ;
		::MapWindowPoints( ::GetDlgItem(m_hWnd, btbar_ctls[i].id), m_hWnd, (LPPOINT)&rect, 2);
		rect.left += off_x ;
		rect.top +=off_y ;
		::MoveWindow( ::GetDlgItem(m_hWnd, btbar_ctls[i].id), 
				rect.left, rect.top, w, h, 
				TRUE );
	}

    /*
#ifdef  APP_PWII_FRONTEND  
    // move REC lights
    y = (m_bottombar.top + m_bottombar.bottom )/2 ;
    x = m_bottombar.left + 80 ;
    ::GetClientRect( ::GetDlgItem(m_hWnd, IDC_BUTTON_TM), &rect );
    w = rect.right ;
    h = rect.bottom ;
    ::MapWindowPoints( ::GetDlgItem(m_hWnd, IDC_BUTTON_TM), m_hWnd, (LPPOINT)&rect, 2);
	::MoveWindow( ::GetDlgItem(m_hWnd, IDC_BUTTON_TM),
			x, y-h/2,
            w, h, 
			TRUE );
    ::InvalidateRect(::GetDlgItem(m_hWnd, IDC_BUTTON_TM), NULL, FALSE );

    x+=w+8;
    ::GetClientRect( ::GetDlgItem(m_hWnd, IDC_BUTTON_REC), &rect );
    w = rect.right ;
    h = rect.bottom ;
    ::MapWindowPoints( ::GetDlgItem(m_hWnd, IDC_BUTTON_REC), m_hWnd, (LPPOINT)&rect, 2);
	::MoveWindow( ::GetDlgItem(m_hWnd, IDC_BUTTON_REC),
			x, y-h/2,
            w, h, 
			TRUE );
    ::InvalidateRect(::GetDlgItem(m_hWnd, IDC_BUTTON_REC), NULL, FALSE );
#endif
    */

#ifdef SPARTAN_APP
    w = m_bottombar.right-m_bottombar.left ;
    h = m_bottombar.bottom-m_bottombar.top ;
    if( m_bkbmp ) {
        if( m_bkbmp->GetWidth()<w || m_bkbmp->GetHeight()<h ) {
            delete m_bkbmp ;
            m_bkbmp = NULL ;
        }
    }
    if( m_bkbmp==NULL ) {
        m_bkbmp = new Gdiplus::Bitmap(w,h);
    }

    Graphics * gbk = new Graphics(m_bkbmp);
    Brush * bkbrush ;
    Bitmap * timg = loadbitmap( _T("PLAY_BK") );
    if( timg ) {
            bkbrush = new TextureBrush( timg );
    }
    else {
            bkbrush = new SolidBrush( Color(255,30,20,30) );
    }
    gbk->FillRectangle(bkbrush, 0, 0, m_bkbmp->GetWidth(), m_bkbmp->GetHeight());
    delete bkbrush ;
    if( timg ) {
       delete timg ;
    }
    delete gbk ;
#else
    w = m_bottombar.right-m_bottombar.left ;
    h = m_bottombar.bottom-m_bottombar.top ;
    if( m_bkbmp ) {
        if( m_bkbmp->GetWidth()<w || m_bkbmp->GetHeight()<h ) {
            delete m_bkbmp ;
            m_bkbmp = NULL ;
        }
    }
    if( m_bkbmp==NULL ) {
        Bitmap * timg = loadbitmap( _T("PLAY_BK") );
        if( timg ) {
            m_bkbmp = new Gdiplus::Bitmap(w+200,h);
#ifdef APP_PW_TABLET
            Graphics * gbk = new Graphics(m_bkbmp);
			gbk->SetInterpolationMode(InterpolationModeNearestNeighbor);
			gbk->DrawImage( timg, Rect(0,0,w+200,h), 0,0, timg->GetWidth(), timg->GetHeight(), UnitPixel);
//			gbk->DrawImage( timg, 0, 0, w, h );
            delete gbk ;
#else
            Graphics * gbk = new Graphics(m_bkbmp);
            Brush * bkbrush = new TextureBrush( timg );
            gbk->FillRectangle(bkbrush, 0, 0, w, h);
            delete bkbrush ;
            delete gbk ;
#endif
            delete timg ;
        }
    }
#endif      // SPARTAN_APP

	// adjust playerrect 
	m_playerrect.bottom = m_bottombar.top - sliderheight ;	// adjust later

	// move slider bar
#ifdef APP_PW_TABLET
	::MoveWindow( m_slider.getHWND(), 
        m_playerrect.left+50, m_playerrect.bottom, 
        m_playerrect.right-m_playerrect.left-100, sliderheight, 
		TRUE );
#else
    ::MoveWindow(m_slider.getHWND(), 
        m_playerrect.left, m_playerrect.bottom, 
        m_playerrect.right-m_playerrect.left, sliderheight, 
		TRUE );
#endif

#ifdef APP_PW_TABLET
	extern struct bt_layout playrect_ctls[] ;
	for( i=0; i<200&&playrect_ctls[i].id>0 ; i++ ) {
		int refx, refy ;
		if( playrect_ctls[i].posx<0 ) {
			refx = m_playerrect.right ;
		}
		else {
			refx = m_playerrect.left ;
		}
		if( playrect_ctls[i].posy<0 ) {
			refy = m_playerrect.bottom ;
		}
		else {
			refy = m_playerrect.top ;
		}
		MoveWindow(GetDlgItem(m_hWnd, playrect_ctls[i].id), refx+playrect_ctls[i].posx, refy+playrect_ctls[i].posy, playrect_ctls[i].w, playrect_ctls[i].h, false );
	}
#endif

    // move ptz campass circle 
    ::GetClientRect(::GetDlgItem(m_hWnd, IDC_GROUP_PTZ), &rect);
	::MapWindowPoints( ::GetDlgItem(m_hWnd, IDC_GROUP_PTZ),m_hWnd, (LPPOINT)&rect, 2);

    w = m_ptzcampass_rect.left - m_ptzcampass_rect.right ;
    h = m_ptzcampass_rect.bottom - m_ptzcampass_rect.top ;
	m_ptzcampass_rect.top = rect.top + 10 ;
	m_ptzcampass_rect.left = rect.left + (rect.right-rect.left-w)/2 ;
	m_ptzcampass_rect.right = m_ptzcampass_rect.left+w ;
	m_ptzcampass_rect.bottom = m_ptzcampass_rect.top+h ;

//	m_ptzcampass_rgn.CreateEllipticRgn(m_ptzcampass_rect.left,
//                                       m_ptzcampass_rect.top,
//									   m_ptzcampass_rect.right,
//									   m_ptzcampass_rect.bottom );

	POINT * ptzpoints ;
	ptzpoints = new POINT [200] ;
	for( i=0; i< 200; i++ ) {
        if( ptz_points[i*2]<0.0 ) {
            break;
        }
		ptzpoints[i].x = (int) (ptz_points[2*i]+0.5) + m_ptzcampass_rect.left ;
		ptzpoints[i].y = (int) (ptz_points[2*i+1]+0.5) + m_ptzcampass_rect.top ;
	}
    if( m_ptzcampass_rgn ) {
        DeleteObject( m_ptzcampass_rgn );
        m_ptzcampass_rgn = NULL ;
    }
	m_ptzcampass_rgn=::CreatePolygonRgn( ptzpoints, i, WINDING );
	delete ptzpoints ;

    // initialize status icon position
    m_panelrect.bottom = m_bottombar.top ;
    m_statusiconrect.bottom -= m_statusiconrect.top ;
	m_statusiconrect.right -= m_statusiconrect.left ;
    ::GetClientRect(::GetDlgItem( m_hWnd, IDC_BUTTON_PLAY ), &rect );
    ::MapWindowPoints(::GetDlgItem( m_hWnd, IDC_BUTTON_PLAY ), m_hWnd, (LPPOINT)&rect, 2 );  
    m_statusiconrect.top = rect.top ;
    m_statusiconrect.left = rect.right + 25 ;
	m_statusiconrect.bottom += m_statusiconrect.top ;
	m_statusiconrect.right += m_statusiconrect.left ;

    // move TIMERANGE combo, NEXT, PREV button
    if( control_align != 2 ) {       // 0:left, 1:right, 2: bottom    
        x = ( m_panelrect.left + m_panelrect.right ) /2 ;
        ::GetClientRect(m_slider.getHWND(), &rect );
        ::MapWindowPoints(m_slider.getHWND(), m_hWnd, (LPPOINT)&rect, 2 );  
        y = ( rect.top + rect.bottom )/2 ;
        ::GetClientRect(::GetDlgItem( m_hWnd, IDC_COMBO_TIMERANGE ), &rect );
        ::MapWindowPoints(::GetDlgItem( m_hWnd, IDC_COMBO_TIMERANGE ), m_hWnd, (LPPOINT)&rect, 2 );  
        w = rect.right - rect.left ;
        h = rect.bottom - rect.top ;
        ::MoveWindow(::GetDlgItem( m_hWnd, IDC_COMBO_TIMERANGE ), 
            x - w/2 , y - h/2 ,
            w, h, 
		    TRUE );

        off_x = x-w/2 ;
        ::GetClientRect(::GetDlgItem( m_hWnd, IDC_PREV ), &rect );
        ::MapWindowPoints(::GetDlgItem( m_hWnd, IDC_PREV ), m_hWnd, (LPPOINT)&rect, 2 );  
        off_x -= rect.right-rect.left+5 ;
        h =  rect.bottom-rect.top ;
        ::MoveWindow(::GetDlgItem( m_hWnd, IDC_PREV ), 
            off_x , y - h/2 ,
            rect.right-rect.left, h, 
		    TRUE );

        off_x = x+w/2 ;
        ::GetClientRect(::GetDlgItem( m_hWnd, IDC_NEXT ), &rect );
        ::MapWindowPoints(::GetDlgItem( m_hWnd, IDC_NEXT ), m_hWnd, (LPPOINT)&rect, 2 );  
        off_x += 5 ;
        h =  rect.bottom-rect.top ;
        ::MoveWindow(::GetDlgItem( m_hWnd, IDC_NEXT ), 
            off_x , y - h/2 ,
            rect.right-rect.left, h, 
		    TRUE );
    }

    // setup decoder screens
    if( m_cliplist==NULL || m_cliplist->getHWND()==NULL ) {     // not in clip listing mode
        SetScreenFormat();
    }
    else {
        ::GetClientRect( m_hWnd, &rect);	
        rect.left = m_playerrect.left ;
        rect.bottom = m_bottombar.top ;
        ::MoveWindow( m_cliplist->getHWND(), rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top, 
            TRUE );
    }

#ifdef APP_PW_TABLET
	// new bottom bar positioning
/*
	for( i=0; i<100 ; i++) {
		if( btbar_ctls[i].id == 0 ) break;
		::GetClientRect( GetDlgItem(m_hWnd, btbar_ctls[i].id), &rect);	
		if( btbar_ctls[i].xadj < 0 && btbar_ctls[i].xadj>-100000 ) {
			::MoveWindow( GetDlgItem(m_hWnd, btbar_ctls[i].id), 
				m_bottombar.right+btbar_ctls[i].xadj , 
				(m_bottombar.bottom+m_bottombar.top)/2 - rect.bottom/2 , 
				rect.right, 
				rect.bottom,
				0 ) ;
		}
		else if( btbar_ctls[i].xadj > 0 ) {
			::MoveWindow( GetDlgItem(m_hWnd, btbar_ctls[i].id), 
				m_bottombar.left + btbar_ctls[i].xadj , 
				(m_bottombar.bottom+m_bottombar.top)/2 - rect.bottom/2 , 
				rect.right, 
				rect.bottom,
				0 ) ;
		}
	}
*/

	extern struct bt_layout bottom_ctls[]  ;
	int bottom_h = m_bottombar.bottom - m_bottombar.top ;
	for( i=0; i<200&&bottom_ctls[i].id>0 ; i++ ) {
		int refx, refy ;
		if( bottom_ctls[i].posx<0 ) {
			refx = m_bottombar.right ;
		}
		else {
			refx = m_bottombar.left ;
		}
		refy = (m_bottombar.bottom + m_bottombar.top - bottom_ctls[i].h)/2 ;
		MoveWindow(GetDlgItem(m_hWnd, bottom_ctls[i].id), refx+bottom_ctls[i].posx, refy, bottom_ctls[i].w, bottom_ctls[i].h, false );
	}

#endif

	// calculate speaker icon pos
	::GetClientRect( ::GetDlgItem(m_hWnd, IDC_PIC_SPEAKER_VOLUME), &m_speakerrect );
	::MapWindowPoints( ::GetDlgItem(m_hWnd, IDC_PIC_SPEAKER_VOLUME), m_hWnd, (LPPOINT)&m_speakerrect, 2 );

//	InvalidateRect(m_hWnd, NULL, FALSE);
    RedrawWindow(m_hWnd, NULL, NULL, RDW_INVALIDATE|RDW_ALLCHILDREN );
}

#endif      // NEWPAINTING

#ifdef APP_PWII_FRONTEND

// ctrl id on top control box
int pw_hide_ctrl_id[]={
    IDC_STATIC_QUICKTOOLS,
    IDC_QUICKCAP,
    IDC_CAPTURE,
    IDC_STATIC_GROUP_CUSTOMCLIP,
    IDC_TIMEBEGIN,
    IDC_TIME_SELBEGIN,
    IDC_TIMEEND,
    IDC_TIME_SELEND,
    IDC_SAVEFILE,
#ifdef  IDC_EXPORTJPG
    IDC_EXPORTJPG,
#endif
#ifdef  IDC_BUTTON_TVSINFO
    IDC_BUTTON_TVSINFO,
#endif
    0 
};


#endif

// set controls on dialog  (show/hide controls)
void DvrclientDlg::CtrlSet()
{
	int ptz_show ;
	int playback_enable ;
	int playerfeature=0;
	int i ;
	HWND hwnd ;
	i=0; 

#ifdef APP_TVS 

#ifdef APP_PWPLAYER
	ShowWindow(GetDlgItem(m_hWnd, IDC_BUTTON_TVSINFO ), SW_HIDE);
#else
	if( m_clientmode == CLIENTMODE_LIVEVIEW || m_clientmode == CLIENTMODE_PLAYBACK ) 
	{
		EnableWindow(GetDlgItem(m_hWnd, IDC_BUTTON_TVSINFO ),TRUE);
	}
	else {
		EnableWindow(GetDlgItem(m_hWnd, IDC_BUTTON_TVSINFO ),FALSE);
	}

#ifdef  APP_PWVIEWER
		ShowWindow(GetDlgItem(m_hWnd, IDC_EXPORTJPG), SW_HIDE);
#endif

#endif

#ifdef  APP_PWII_FRONTEND 
    if( m_clientmode == CLIENTMODE_LIVEVIEW ) 
	{
        SetDlgItemText(m_hWnd, IDC_BUTTON_PLAY, _T("Playback") );
		ShowWindow(GetDlgItem(m_hWnd, IDC_BUTTON_CAM1 ), SW_SHOW);
		ShowWindow(GetDlgItem(m_hWnd, IDC_BUTTON_CAM2 ), SW_SHOW);
		ShowWindow(GetDlgItem(m_hWnd, IDC_BUTTON_TM ), SW_SHOW);
		ShowWindow(GetDlgItem(m_hWnd, IDC_BUTTON_LP ), SW_SHOW);
        SetTimer(m_hWnd, TIMER_RECLIGHT, 500, NULL );
	}
	else {
        SetDlgItemText(m_hWnd, IDC_BUTTON_PLAY, _T("Live") );
		ShowWindow(GetDlgItem(m_hWnd, IDC_BUTTON_CAM1 ), SW_HIDE);
		ShowWindow(GetDlgItem(m_hWnd, IDC_BUTTON_CAM2 ), SW_HIDE);
		ShowWindow(GetDlgItem(m_hWnd, IDC_BUTTON_TM ), SW_HIDE);
		ShowWindow(GetDlgItem(m_hWnd, IDC_BUTTON_LP ), SW_HIDE);
        KillTimer(m_hWnd, TIMER_RECLIGHT );
	}
#endif		//APP_PWII_FRONTEND

#endif

#ifdef  SPARTAN_APP
    ::ShowWindow( ::GetDlgItem(m_hWnd, IDC_QUICKCAP), SW_HIDE );
    ::ShowWindow( ::GetDlgItem(m_hWnd, IDC_STATIC_GROUP_QUICK_TOOL), SW_HIDE );
#endif

	// show / hide PTZ controls
	ptz_show = 0 ;
	if( m_clientmode == CLIENTMODE_LIVEVIEW ) {
		if( m_screen[m_focus] && ( m_screen[m_focus]->m_channelinfo.features & 3 )==3 ) {
			// show PTZ control
			ptz_show = 1;
		}
		else {
			// hide PTZ control
			ptz_show = PTZ_DEFAULT ;
		}
	}

 	playback_enable=0;
	if( m_clientmode == CLIENTMODE_PLAYBACK ||
		m_clientmode == CLIENTMODE_PLAYFILE ||
		m_clientmode == CLIENTMODE_PLAYSMARTSERVER ) {
		playback_enable=1;
	}
	i=0;
	while( playback_ctrl_id[i] ) {
		hwnd = GetDlgItem(m_hWnd, playback_ctrl_id[i] ) ;
		if( hwnd ) {
			EnableWindow(hwnd, playback_enable);
		}
		i++;
	}

	i=0; 
	while( PTZ_ctrl_id[i] ) {
		hwnd = GetDlgItem(m_hWnd, PTZ_ctrl_id[i] ) ;
		if( hwnd ) {
			ShowWindow(hwnd, ptz_show?SW_SHOW:SW_HIDE);
		}
		i++ ;
	}
	i=0; 
	while( PTZ_hide_ctrl_id[i] ) {
		hwnd = GetDlgItem(m_hWnd, PTZ_hide_ctrl_id[i] ) ;
		if( hwnd ) {
			ShowWindow(hwnd, ptz_show?SW_HIDE:SW_SHOW);
		}
		i++ ;
	}

	if( m_screen[m_focus] && m_screen[m_focus]->m_decoder ) {
		playerfeature=m_screen[m_focus]->m_decoder->m_playerinfo.features ;
	}

	// Setup button
#ifdef APP_TVS 
	if( ( g_usertype==IDADMIN || g_usertype==IDINSTALLER )
		&& 
		(	m_clientmode == CLIENTMODE_LIVEVIEW ||
			m_clientmode == CLIENTMODE_PLAYBACK ||
			m_clientmode == CLIENTMODE_PLAYSMARTSERVER ) &&
			(	playerfeature & PLYFEATURE_CONFIGURE ) ) {
#ifdef APP_TVS_NEW_ORLEANS
		EnableWindow(GetDlgItem(m_hWnd, IDC_SETUP ), FALSE);
#else
		EnableWindow(GetDlgItem(m_hWnd, IDC_SETUP ), TRUE);
#endif
	}
	else {
		EnableWindow(GetDlgItem(m_hWnd, IDC_SETUP ), FALSE);
	}
#else
	if( g_usertype==IDADMIN && 
		(	m_clientmode == CLIENTMODE_LIVEVIEW ||
			m_clientmode == CLIENTMODE_PLAYBACK ||
			m_clientmode == CLIENTMODE_PLAYSMARTSERVER ) &&
			(	playerfeature & PLYFEATURE_CONFIGURE ) ) {
		EnableWindow(GetDlgItem(m_hWnd, IDC_SETUP ), TRUE);
	}
	else {
		EnableWindow(GetDlgItem(m_hWnd,  IDC_SETUP ),FALSE);
	}
#endif

	// disconnect button
	if( m_clientmode == CLIENTMODE_CLOSE ) {
		EnableWindow(GetDlgItem(m_hWnd,  IDC_DISCONNECT ),FALSE);
	}
	else {
		EnableWindow(GetDlgItem(m_hWnd,  IDC_DISCONNECT ),TRUE);
	}

	// Capture button
	if( m_clientmode != CLIENTMODE_CLOSE &&
		(playerfeature & PLYFEATURE_CAPTURE) ) {
		EnableWindow(GetDlgItem(m_hWnd, IDC_CAPTURE ),TRUE);
	}
	else {
		EnableWindow(GetDlgItem(m_hWnd, IDC_CAPTURE ),FALSE);
	}
	
	if( m_clientmode == CLIENTMODE_CLOSE ) {
        ::SendDlgItemMessage(m_hWnd, IDC_COMBO_TIMERANGE, CB_SETCURSEL, 0, (LPARAM)0);
        m_slider.SetRange(0, 24*60*60-1 );
	}
	else if( m_clientmode == CLIENTMODE_PLAYFILE ) {
        i = ::SendDlgItemMessage(m_hWnd, IDC_COMBO_TIMERANGE, CB_FINDSTRING, 0, (LPARAM)TIMERANGE_WHOLEFILE);
        if(i==CB_ERR) {
            i=::SendDlgItemMessage(m_hWnd, IDC_COMBO_TIMERANGE, CB_ADDSTRING, 0, (LPARAM)TIMERANGE_WHOLEFILE);
           ::SendDlgItemMessage(m_hWnd, IDC_COMBO_TIMERANGE, CB_SETCURSEL, i, (LPARAM)0);
        }
//		EnableWindow(GetDlgItem(m_hWnd, IDC_COMBO_TIMERANGE ),FALSE);
//		EnableWindow(GetDlgItem(m_hWnd, IDC_PREV ),FALSE);
//		EnableWindow(GetDlgItem(m_hWnd, IDC_NEXT ),FALSE);
//		EnableWindow(GetDlgItem(m_hWnd, IDC_MONTHCALENDAR ),FALSE);
	}
    else {
        // to remove whole file view string
        i = ::SendDlgItemMessage(m_hWnd, IDC_COMBO_TIMERANGE, CB_FINDSTRING, 0, (LPARAM)TIMERANGE_WHOLEFILE);
        if(i!=CB_ERR) {
            ::SendDlgItemMessage(m_hWnd, IDC_COMBO_TIMERANGE, CB_DELETESTRING, i, 0 );
        }
//		EnableWindow(GetDlgItem(m_hWnd, IDC_COMBO_TIMERANGE ),TRUE);
//		EnableWindow(GetDlgItem(m_hWnd, IDC_PREV ),TRUE);
//		EnableWindow(GetDlgItem(m_hWnd, IDC_NEXT ),TRUE);
//		EnableWindow(GetDlgItem(m_hWnd, IDC_MONTHCALENDAR ),TRUE);
    }

	// Play/Pause button
#ifndef APP_PW_TABLET
	if( g_playstat == PLAY_PAUSE ) {
		ShowWindow(GetDlgItem(m_hWnd, IDC_PAUSE),SW_HIDE);
		ShowWindow(GetDlgItem(m_hWnd, IDC_PLAY),SW_SHOW);
		EnableWindow(GetDlgItem(m_hWnd, IDC_STEP),TRUE);
	}
	else {
		ShowWindow(GetDlgItem(m_hWnd, IDC_PLAY),SW_HIDE);
		ShowWindow(GetDlgItem(m_hWnd, IDC_PAUSE),SW_SHOW);
		EnableWindow(GetDlgItem(m_hWnd, IDC_STEP),FALSE);
	}
#endif

#ifdef APP_PWPLAYER
	ShowWindow(GetDlgItem(m_hWnd, IDC_MONTHCALENDAR ),SW_HIDE);
	ShowWindow(GetDlgItem(m_hWnd, IDC_MAP),SW_HIDE);
#else
#ifndef APP_PW_TABLET
	ShowWindow(GetDlgItem(m_hWnd, IDC_MAP),g_minitracksupport?SW_SHOW:SW_HIDE);
#endif
#endif

	if( m_screen[m_focus] && m_screen[m_focus]->m_decoder ) {
        string displaytitle ;
		displaytitle.printf( "%s - %s - %s",
			(LPCSTR)m_appname, 
            m_screen[m_focus]->m_decoder->getservername(), 
            m_screen[m_focus]->m_decoder->getserverinfo() );
		SetWindowText(m_hWnd, (LPCTSTR)displaytitle );
		decoder::g_minitrack_selectserver(m_screen[m_focus]->m_decoder);
	}
	else {
		SetWindowText(m_hWnd, (LPCTSTR)m_appname );
		decoder::g_minitrack_selectserver(NULL);		// select null server. (closed)
	}

#ifdef APP_PWII_FRONTEND
    i=0; 
    while( pw_hide_ctrl_id[i] ) {
        hwnd = GetDlgItem(m_hWnd, pw_hide_ctrl_id[i] ) ;
        if( hwnd ) {
            ShowWindow(hwnd, SW_HIDE);
        }
        i++ ;
    }
#endif


#ifdef  NOSAVECLIP
// special version do not allow save clip
    ::ShowWindow( ::GetDlgItem(m_hWnd, IDC_QUICKCAP), SW_HIDE );
    ::ShowWindow( ::GetDlgItem(m_hWnd, IDC_STATIC_GROUP_CUSTOMCLIP), SW_HIDE );
    ::ShowWindow( ::GetDlgItem(m_hWnd, IDC_TIMEBEGIN), SW_HIDE );
    ::ShowWindow( ::GetDlgItem(m_hWnd, IDC_TIMEEND), SW_HIDE );
    ::ShowWindow( ::GetDlgItem(m_hWnd, IDC_SAVEFILE), SW_HIDE );
    ::ShowWindow( ::GetDlgItem(m_hWnd, IDC_TIME_SELBEGIN), SW_HIDE );
    ::ShowWindow( ::GetDlgItem(m_hWnd, IDC_TIME_SELEND), SW_HIDE );
    ::ShowWindow( ::GetDlgItem(m_hWnd, IDC_STATIC_QUICKTOOLS), SW_HIDE );
    ::ShowWindow( ::GetDlgItem(m_hWnd, IDC_CAPTURE), SW_HIDE );
#endif

#ifdef APP_PW_TABLET
	// no support MAP & MIX AUDIO
    ::ShowWindow( ::GetDlgItem(m_hWnd, IDC_MAP), SW_HIDE );
    ::ShowWindow( ::GetDlgItem(m_hWnd, IDC_MIXAUDIO), SW_HIDE );


	// no slider volume, use volume window instead
	this->m_volume.Hide();

	struct {
		int id ;
		int show ;
	} playbutton[7]  = {
		{IDC_PLAY, SW_HIDE},
		{IDC_PAUSE, SW_HIDE},
		{IDC_SLOW,SW_HIDE},
		{IDC_BACKWARD,SW_HIDE},
		{IDC_FORWARD, SW_HIDE}, 
		{IDC_FAST,    SW_HIDE},
		{IDC_STEP,   SW_HIDE}
	};

	if( m_clientmode == CLIENTMODE_PLAYBACK || m_clientmode == CLIENTMODE_PLAYFILE  ) 
	{
		ShowWindow(GetDlgItem(m_hWnd, IDC_BUTTON_BCAL ), SW_SHOW);
		ShowWindow(GetDlgItem(m_hWnd, IDC_BUTTON_BPLAYLIVE ), SW_HIDE);
		ShowWindow(GetDlgItem(m_hWnd, IDC_BUTTON_BPLAYBACK ), SW_SHOW);
		i = ::SendDlgItemMessage(m_hWnd, IDC_COMBO_TIMERANGE, CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
		switch(i) {
		case 0 :
			ShowWindow( GetDlgItem(m_hWnd, IDC_BUTTON_BDAYRANGE ), SW_SHOW );
			ShowWindow( GetDlgItem(m_hWnd, IDC_BUTTON_BHOURRANGE ), SW_HIDE );
			ShowWindow( GetDlgItem(m_hWnd, IDC_BUTTON_BQUATRANGE ), SW_HIDE );
			break;
		case 1:
			ShowWindow( GetDlgItem(m_hWnd, IDC_BUTTON_BDAYRANGE ), SW_HIDE );
			ShowWindow( GetDlgItem(m_hWnd, IDC_BUTTON_BHOURRANGE ), SW_SHOW );
			ShowWindow( GetDlgItem(m_hWnd, IDC_BUTTON_BQUATRANGE ), SW_HIDE );
			break;
		case 2:
			ShowWindow( GetDlgItem(m_hWnd, IDC_BUTTON_BDAYRANGE ), SW_HIDE );
			ShowWindow( GetDlgItem(m_hWnd, IDC_BUTTON_BHOURRANGE ), SW_HIDE );
			ShowWindow( GetDlgItem(m_hWnd, IDC_BUTTON_BQUATRANGE ), SW_SHOW );
			break;
		}

		for( i=0; i<7; i++ ) {
			playbutton[i].show = SW_SHOW ;
		}
		if( g_playstat == PLAY_PAUSE ) {
			EnableWindow(GetDlgItem(m_hWnd, IDC_STEP),TRUE);
			playbutton[1].show = SW_HIDE ;
		}
		else {
			EnableWindow(GetDlgItem(m_hWnd, IDC_STEP),FALSE);
			playbutton[0].show = SW_HIDE ;
		}

	}
	else {

		ShowWindow( GetDlgItem(m_hWnd, IDC_BUTTON_BDAYRANGE ), SW_HIDE );
		ShowWindow( GetDlgItem(m_hWnd, IDC_BUTTON_BHOURRANGE ), SW_HIDE );
		ShowWindow( GetDlgItem(m_hWnd, IDC_BUTTON_BQUATRANGE ), SW_HIDE );

		ShowWindow(GetDlgItem(m_hWnd, IDC_BUTTON_BCAL ), SW_HIDE);
		ShowWindow(GetDlgItem(m_hWnd, IDC_BUTTON_BPLAYLIVE ), SW_SHOW);
		ShowWindow(GetDlgItem(m_hWnd, IDC_BUTTON_BPLAYBACK ), SW_HIDE);
	}
	for( i=0; i<7; i++ ) {
		ShowWindow( GetDlgItem(m_hWnd, playbutton[i].id ), playbutton[i].show );
	}
#endif

	if (isportableapp()) {
		ShowWindow(GetDlgItem(m_hWnd, IDC_USERS), SW_HIDE);
		EnableWindow(GetDlgItem(m_hWnd, IDC_BUTTON_PLAY), FALSE);
	}

	InvalidateRect(m_hWnd, &m_statusiconrect, FALSE);
}



/*
// set volume, 0 - 100
void DvrclientDlg::SetVolume()
{
	RECT speakerpic ;
	if( m_volume->GetPos()!=m_vvolume ) {
		// change speaker volume
		m_vvolume = m_volume->GetPos() ;
        waveOutSetVolume((HWAVEOUT)m_waveoutId, ((m_vvolume*655)<<16)+m_vvolume*655 );
		GetDlgItem(IDC_PIC_SPEAKER_VOLUME)->GetWindowRect(&speakerpic);
		ScreenToClient(&speakerpic);
		CImage img ;
		if( m_vvolume == 0 ) {
			loadimg( &img, _T("VOLUME_0") );
		}
		else if( m_vvolume<30 ) {
			loadimg( &img, _T("VOLUME_1") );
		}
		else if( m_vvolume<80 ) {
			loadimg( &img, _T("VOLUME_2") );
		}
		else {
			loadimg( &img, _T("VOLUME_3") );
		}
		CDC * pdc = GetDC();
		pdc->FillRect( &speakerpic, &m_bottombarbrush);
		img.AlphaBlend( pdc->GetSafeHdc(), speakerpic.left, speakerpic.top, 255, 0 );
		ReleaseDC(pdc);
	}
}
*/

// set volume, 0 - 100
void DvrclientDlg::SetVolume()
{
	RECT sprect ;
	if( m_volume.GetPos()!=m_vvolume ) {
		// change speaker volume
		m_vvolume = m_volume.GetPos() ;
        waveOutSetVolume((HWAVEOUT)m_waveoutId, ((m_vvolume*655)<<16)+m_vvolume*655 );

        // repaint volume indicator
        HWND hbox ;
        hbox = ::GetDlgItem(m_hWnd, IDC_PIC_SPEAKER_VOLUME) ;
        if( hbox ) {
            ::GetClientRect( hbox, &sprect );
            ::MapWindowPoints( hbox, m_hWnd, (LPPOINT)&sprect, 2 );
            InvalidateRect(m_hWnd, &sprect, FALSE);
        }
   		
/*
CImage img ;
		if( m_vvolume == 0 ) {
			loadimg( &img, _T("VOLUME_0") );
		}
		else if( m_vvolume<30 ) {
			loadimg( &img, _T("VOLUME_1") );
		}
		else if( m_vvolume<80 ) {
			loadimg( &img, _T("VOLUME_2") );
		}
		else {
			loadimg( &img, _T("VOLUME_3") );
		}
		CDC * pdc = GetDC();

//        m_bkimg.BitBlt( pdc->GetSafeHdc(), sprect.left, sprect.top, sprect.right-sprect.left, sprect.bottom-sprect.top, sprect.left, sprect.top );
	    pdc->FillRect( &sprect, &m_bottombarbrush);

        img.AlphaBlend( pdc->GetSafeHdc(), sprect.left, sprect.top, 255, 0 );
		ReleaseDC(pdc);
*/
    }
}


void DvrclientDlg::ScreenDblClk( Screen * player)
{
	FocusPlayer( player ) ;
    if( g_singlescreenmode ) {            // switch back from full screen mode
		g_singlescreenmode = 0 ;
	}
	else {								// switch to full screen mode (single screen mode)
		g_singlescreenmode = 1 ;
	}
	SetScreenFormat();
}

void DvrclientDlg::FocusPlayer( Screen * screen)
{
	// WaitCursor waitcursor ;
	int i;
    RECT rt  ;

	struct channel_info chinfo ;

    m_tbarupdate = 1;
    SetTimer(m_hWnd, TIMER_UPDATE, 10, NULL );

    m_slider.SetScreen(screen);
	
	screen->selectfocus();

    if( m_screen[m_focus] != screen ) {

		chinfo.size=sizeof(chinfo);
//		InvalidateRect(m_hWnd, &m_screct, FALSE);
		for(i=0; i<m_screennum ; i++ ) {
			rt = m_screenrect[i] ;
			InvalidateRect(m_hWnd, &rt, FALSE);
			InflateRect(&rt, -1, -1 );
			ValidateRect( m_hWnd, &rt ) ;
			if( m_screen[i]==screen ) {
				m_focus=i;
			}
			else if(m_screen[i]) {
				m_screen[i]->setaudio( g_MixSound ) ;
			}
		}
		CtrlSet();		// reset Ctrl set, may show/hide PTZ control
//		SetVolume();
	}
	screen->setaudio(1);
}

int DvrclientDlg::OnCancel()
{
    if( m_zoom ) {
        SetWindowLong(m_hWnd, GWL_STYLE, m_zoom );
        m_zoom = 0 ;
        ShowWindow(m_hWnd, SW_NORMAL);
        SetWindowPos(m_hWnd, HWND_TOP, 
            m_zoomrect.left, m_zoomrect.top, 
            m_zoomrect.right-m_zoomrect.left, m_zoomrect.bottom - m_zoomrect.top,
            0);
        OnSize(0, 0, 0);
    }
	return TRUE ;
}

void DvrclientDlg::OnActive( int active )
{
    if( m_zoom ) {
        if( active ) {
//            SetWindowPos(m_hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE );
        }
        else {
//            SetWindowPos(m_hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE );
        }
    }
}

void DvrclientDlg::SetZoom()
{
    if( m_zoom==0 ) {
        GetWindowRect(m_hWnd, &m_zoomrect);
        m_zoom = SetWindowLong(m_hWnd, GWL_STYLE, WS_POPUP );
        ShowWindow(m_hWnd, SW_MAXIMIZE);
    }
    else {
        OnCancel();
    }
}

// adjust screen format base on m_screenmode
void DvrclientDlg::SetScreenFormat()
{
	int i;
	RECT winrect ;
	int align = 0 ;

	if( g_screenmode>=0 && g_screenmode<screenmode_table_num ) {
		m_screenmode = g_screenmode ;
//		m_savescreenmode = m_screenmode ;
//		g_screenmode = screenmode ;
	}
	else {
		g_screenmode = screenmode_table_num ;	// automatic
		// automatic select screen mode
		m_screenmode = 0 ;

		// search exact screen mode first
		for(i=0;i<screenmode_table_num;i++) {
			if( screenmode_table[i].numscreen >= m_maxdecscreen ) {
				m_screenmode=i;
				break;
			}
		}

	}

	if( m_screenmode>=0 || m_screenmode<screenmode_table_num ){
		align =  screenmode_table[m_screenmode].align ;	// 0: center, 1: left, 2: right, 3: top, 4, bottom
	}

	if( m_screenmode<0 || m_screenmode>=screenmode_table_num || g_singlescreenmode ) {
		m_screenmode = 0 ;      // single screen
		align =  screenmode_table[0].align ;
	}

	m_screct=m_playerrect ;
	InflateRect(&m_screct, -1, -1 );

	// total screen number
	m_screennum = screenmode_table[m_screenmode].numscreen;

	int ratiox, ratioy ;
	if (g_ratiox <=0 || g_ratioy <= 0) {
		// default screen ratio 4:3
		ratiox = 4;
		ratioy = 3;
		// auto detect
		for (i = 0; i<m_screennum; i++) {
			if (m_screen[i] != NULL && m_screen[i]->isattached()) {
				if (m_screen[i]->m_channelinfo.yres >= 720) {
					// select wide (hd) ratio
					ratiox = 16;
					ratioy = 9;
					break;
				}
			}
		}
	}
	else if (g_ratiox == 10002) {		// native
		ratiox = 4;
		ratioy = 3;
		if (m_screen[m_focus] != NULL &&
			m_screen[m_focus]->isattached() &&
			m_screen[m_focus]->m_channelinfo.xres > 0 &&
			m_screen[m_focus]->m_channelinfo.yres > 0) {
			ratiox = m_screen[m_focus]->m_channelinfo.xres;
			ratioy = m_screen[m_focus]->m_channelinfo.yres;
		}
	}
	else {
		ratiox = g_ratiox;
		ratioy = g_ratioy;
	}
	

	// switch to all screen ration
	if (g_ratiox == 10001) {		// fill screen 
		ratiox = m_screct.right - m_screct.left;
		ratioy = m_screct.bottom - m_screct.top;
	}
	else {
		ratiox = screenmode_table[m_screenmode].xnum * ratiox;
		ratioy = screenmode_table[m_screenmode].ynum * ratioy;
	}

	winrect = m_screct ;
	int width = m_screct.right-m_screct.left ;
	int height = m_screct.bottom-m_screct.top ;
	if( height * ratiox > width * ratioy ) {
		InflateRect(&m_screct, 0, (width*ratioy/ratiox-height)/2 );
		if( align == 3 ) {			// top
			m_screct.bottom = winrect.top + (m_screct.bottom - m_screct.top) ;
			m_screct.top = winrect.top ;
		}
		else if( align == 4 ) {		// bottom
			m_screct.top = winrect.bottom - (m_screct.bottom - m_screct.top) ;
			m_screct.bottom = winrect.bottom ;
		}
	}
	else {
		InflateRect(&m_screct, (height*ratiox/ratioy-width)/2, 0 );
		if( align == 1 ) {			// left			
			m_screct.right = winrect.left + (m_screct.right - m_screct.left) ;
			m_screct.left = winrect.left ;
		}	
		else if( align == 2 ) {		// right
			m_screct.left = winrect.right - (m_screct.right - m_screct.left) ;
			m_screct.right = winrect.right ;
		}
	}

    if( g_singlescreenmode==0 && m_focus>=m_screennum) {
        m_focus=0 ;
    }
	width = m_screct.right-m_screct.left ;			// new full screen heidht
	height = m_screct.bottom-m_screct.top ;			// new full screen width
   
    // Hide screens
    for(i=0; i<MAXSCREEN; i++ ) {
        if( m_screen[i]!=NULL ) {
            if( g_singlescreenmode ) {
                if( i!=m_focus ) {
                    m_screen[i]->Hide();
                }
            }
            else if( i>=m_screennum ) {
				m_screen[i]->Hide();
            }
		}
    }

    // Move visible screens
    for( i=0; i<m_screennum; i++ ) {
        screenrect * pscrect ;
        pscrect = &(screenmode_table[m_screenmode].screen[i]);
        m_screenrect[i].top = (int)((pscrect->top)*height + m_screct.top ) ;
        m_screenrect[i].bottom = (int)((pscrect->bottom)*height + m_screct.top ) ;
        m_screenrect[i].left = (int)((pscrect->left)*width + m_screct.left ) ;
        m_screenrect[i].right = (int)((pscrect->right)*width + m_screct.left ) ;
        winrect=m_screenrect[i] ;
        InflateRect(&winrect, -1, -1 );
        
		if( m_screen[i]==NULL ) {
			m_screen[i]=new Screen(m_hWnd) ;
		}

		// move ;
        if( g_singlescreenmode ) {
            m_screenrect[m_focus] = m_screenrect[i] ;
			m_screen[m_focus]->Move( winrect.left, winrect.top, winrect.right-winrect.left, winrect.bottom-winrect.top );
			break ;
        }
		else {
			m_screen[i]->Move( winrect.left, winrect.top, winrect.right-winrect.left, winrect.bottom-winrect.top );
		}
    }
	
	// Show screens
    for(i=0; i<MAXSCREEN; i++ ) {
        if( m_screen[i]!=NULL ) {
            if( g_singlescreenmode ) {
                if( i==m_focus ) {
                    m_screen[i]->Show();
				}
            }
            else if( i<m_screennum ) {
                m_screen[i]->Show();
			}
        }
    }

	if(  m_screen[m_focus] )
		FocusPlayer( m_screen[m_focus] ) ;

    InvalidateRect(m_hWnd, &m_playerrect, 1);

}

void DvrclientDlg::SetScreenFormat(char * screenname)
{
	int i ;
	string scn ;
	for(i=0; i<screenmode_table_num; i++) {
		scn = screenmode_table[i].screenname ;
		if( strcmp( screenname, scn )==0 ) {
			break;
		}
	}
	g_singlescreenmode=0;
	g_screenmode = i ;
	SetScreenFormat();
}

double ptz_points[] = { 
             119.50,92.50,
             126.25,79.25,
             147.50,62.50,
             127.00,47.00,
             119.75,31.75,
             120.75,17.00,
             106.75,18.00,
             95.00,11.50,
             78.75,7.50,
             74.00,1.25,
             69.50,7.25,
             55.00,10.75,
             42.50,17.50,
             27.00,17.00,
             28.50,32.00,
             21.00,47.50,
             0.25,62.75,
             21.75,79.50,
             29.00,93.50,
             26.75,111.00,
             46.00,108.50,
             56.75,114.00,
             68.00,116.50,
             74.75,125.00,
             81.00,116.25,
             94.25,113.25,
             102.50,108.75,
             121.00,111.50,
             -1.0,-1.0
};

void DvrclientDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		AboutDlg dlgAbout;
		dlgAbout.DoModal(IDD_ABOUTBOX, m_hWnd);
	}
}

void DvrclientDlg::OnClose()
{
	ClosePlayer();
	EndDialog(IDCANCEL);
}

void DvrclientDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
	lpMMI->ptMinTrackSize.x=m_minxsize ;
	lpMMI->ptMinTrackSize.y=m_minysize ;
}

void DvrclientDlg::OnBnClickedExit()
{
	OnClose();
}

DWORD DvrclientDlg::GetDaystateMonthcalendar(int year, int month)
{
	DWORD daystate ;
	struct dvrtime daytime ;

	daytime.year = 	year ;
	daytime.month = month ;
	daytime.hour =0 ;
	daytime.min = 0 ;
	daytime.second = 0 ;
	daytime.millisecond = 0 ;
	daytime.tz = 0 ;

	daystate=0 ;

    if( m_clientmode == CLIENTMODE_CLOSE || m_clientmode == CLIENTMODE_LIVEVIEW	) {
        return 0;
    }
    else {
        for( daytime.day=1; daytime.day<=31; daytime.day++) {
            if( m_screen[m_focus]->getdayinfo(&daytime) ) {
                daystate|= 1<<(daytime.day-1) ;
            }
        }
    }
	return daystate ;
}

void DvrclientDlg::UpdateMonthcalendar()
{
    SYSTEMTIME timeFrom[3] ;
	int i ;
    int nCount=::SendDlgItemMessage(m_hWnd, IDC_MONTHCALENDAR, MCM_GETMONTHRANGE, GMR_DAYSTATE, (LPARAM)&timeFrom);
	LPMONTHDAYSTATE pDayState = new MONTHDAYSTATE[nCount];
    int year=timeFrom[0].wYear;
    int month=timeFrom[0].wMonth ;
	for( i=0; i<nCount; i++) {
		pDayState[i]=(MONTHDAYSTATE)GetDaystateMonthcalendar(year, month);
		month++;
		if( month>12 ){
			month=1 ;
			year++;
		}
	}
    ::SendDlgItemMessage(m_hWnd, IDC_MONTHCALENDAR, MCM_SETDAYSTATE, nCount, (LPARAM)pDayState);
	delete [] pDayState;
	::SetTimer(m_hWnd, TIMER_UPDATE, 100, NULL );		// start update timer
}

void DvrclientDlg::OnMcnGetdaystateMonthcalendar(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMDAYSTATE pDayState = reinterpret_cast<LPNMDAYSTATE>(pNMHDR);

	*pResult = 0;

	int i, iMax;
	int year, month ;
	iMax=pDayState->cDayState;

	year = pDayState->stStart.wYear ;
	month = pDayState->stStart.wMonth ;

	for(i=0;i<iMax;i++)
	{
		pDayState->prgDayState[i] = (MONTHDAYSTATE) GetDaystateMonthcalendar(year, month) ;
		if( ++month>12 ) {
			month=1 ;
			year++ ;
		}
	}
}

void DvrclientDlg::OnMcnSelchangeMonthcalendar(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMSELCHANGE pSelChange = reinterpret_cast<LPNMSELCHANGE>(pNMHDR);
    m_noupdtime=10 ;         // stop update time for a while
	*pResult = 0;
}

void DvrclientDlg::OnMcnSelectMonthcalendar(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMSELCHANGE pSelChange = reinterpret_cast<LPNMSELCHANGE>(pNMHDR);

	SYSTEMTIME caltime ;
	
    m_noupdtime=0 ;     // enable update time 

    ::SendDlgItemMessage( m_hWnd, IDC_MONTHCALENDAR, MCM_GETCURSEL, 0, (LPARAM)&caltime );

    if( caltime.wDay == m_playertime.day &&
        caltime.wMonth == m_playertime.month &&
        caltime.wYear == m_playertime.year )
    {
        return ;                // same day
    }

    // seek to new date
    dvrtime t ;
    memset( &t, 0, sizeof(t));
    t.year = caltime.wYear ;
    t.month = caltime.wMonth ;
    t.day = caltime.wDay ;
    SeekTime( &t );
	*pResult = 0;
}

void DvrclientDlg::OnNMReleasedcaptureMonthcalendar(NMHDR *pNMHDR, LRESULT *pResult)
{
	*pResult = 0;
}

void DvrclientDlg::ClosePlayer()
{
	int i;
	
	if( m_cliplist!=NULL && m_cliplist->getHWND()!=NULL ) {     // in clip listing mode
        CloseClipList();
        delete m_cliplist ;
        m_cliplist=NULL ;
    }

	m_focus=0;
	for(i=0;i<MAXSCREEN;i++) {
		if( m_screen[i] ) {
			m_screen[i]->DetachDecoder();
		}
	}
	decoder::g_close() ;
    m_playertime.year = 1970 ;
    m_seektime.year = 1979 ;
	m_clientmode=CLIENTMODE_CLOSE ;
	// reset start from last day
	m_startonlastday = 1;

	CtrlSet();
#ifdef IDC_EDIT_LOCATION
    if( m_updateLocation ) {
        ::SetDlgItemText(m_hWnd, IDC_EDIT_LOCATION, _T(""));
    }
#endif
    if( m_mapWindow ) {
        VARIANT_BOOL vb ;
        m_mapWindow->get_closed(&vb);
        if( vb==VARIANT_FALSE ) {
            m_mapWindow->close();
        }
        m_mapWindow->Release();
        m_mapWindow=NULL ;
    }

	ReleaseMap();

#ifdef 	IDC_COMBO_OFFICERID
	SendMessage( GetDlgItem(m_hWnd, IDC_COMBO_OFFICERID),  CB_RESETCONTENT, 0, 0);
#endif
}

void DvrclientDlg::StartPlayer()
{
	int i, j, ch ;

#ifdef APP_TVS
	if( tvs_checktvskey() == 0 ) {
		// key removed?
		ShowWindow(m_hWnd, SW_HIDE);
		ClosePlayer();
		MessageBox(m_hWnd, _T("USB Key removed, application Quit!"),NULL,MB_OK );
		EndDialog(IDCANCEL);
		return ;
	}
#endif

#ifdef IDD_DIALOG_SELECTCAMERA
    
    SelectCamera scamera ;

	int r;
    j=0;
    for(i=0; i<MAXDECODER && j<MAXSCREEN; i++ ) {
		if( g_decoder[i].isopen() ) {
			for( ch=0; ch<g_decoder[i].getchannel(); ch++ ) {
                channel_info ci ;
                ci.size = sizeof(ci);
				ci.features = 0 ;
                ci.cameraname[0]=0 ;
				if (g_decoder[i].getchannelinfo(ch, &ci) >= 0) {
					if ((ci.features & 1) != 0) {
						if (ci.cameraname[0] == 0) {
							sprintf(ci.cameraname, "camera%d", ch + 1);
						}
						scamera.m_cameraname[j].printf("%s - %s", g_decoder[i].getservername(), ci.cameraname);
						scamera.m_decoder[j] = i;
						scamera.m_channel[j] = ch;
						scamera.m_used[j] = 1;
						scamera.m_selected[j] = 1;			// from 2015-12-22, to be all selected by default
						j++;
					}
				}
				if (j >= MAXSCREEN) {
					break;
				}
			}
        }
    }

    if(j>0 && scamera.DoModal(IDD_DIALOG_SELECTCAMERA, m_hWnd)==IDOK ) {
        j=0 ;
        for(i=0; i<MAXSCREEN; i++ ) {
            if( scamera.m_selected[i] ) {
                if( j<MAXSCREEN ) {
                    if( scamera.m_decoder[i]>= 0 && scamera.m_channel[i]>=0 ) {
                        if( m_screen[j]==NULL ) {
                            m_screen[j]=new Screen(m_hWnd);
                        }
                        if( m_screen[j]->AttachDecoder(&g_decoder[scamera.m_decoder[i]], scamera.m_channel[i] )>=0 ) {
                            j++ ;
                        }
                    }
                }
                else {
                    break;
                }
            }
        }
    }

#else

	j=0 ;
    for(i=0; i<MAXDECODER && j<MAXSCREEN; i++ ) {
        if( g_decoder[i].isopen() ) {
            for( ch=0; ch<g_decoder[i].getchannel(); ch++ ) {
                if( j<MAXSCREEN ) {
					if (m_screen[j] != NULL) {
						delete m_screen[j];
					}
                    m_screen[j]=new Screen(m_hWnd);
                    if( m_screen[j]->AttachDecoder(&g_decoder[i], ch)>=0 ) {
                        j++ ;
                    }
                }
                else {
                    break;
                }
            }
        }
    }

#endif

	// to restore prevously saved channel 
	for(i=0; i<MAXSCREEN; i++ ) {
		if( m_screen[i] !=NULL ) {
			if( m_screen[i]->m_xchannel>=0 && m_screen[i]->isVisible() && m_screen[i]->m_decoder!=NULL ) {
				m_screen[i]->AttachDecoder(m_screen[i]->m_decoder, m_screen[i]->m_xchannel);
			}
			m_screen[i]->m_xchannel=-1 ;
		}
    }

	m_maxdecscreen = j;
	decoder::g_checkpassword();
	g_singlescreenmode=0 ;
    m_focus=0;
	SetScreenFormat();
	g_playstat = PLAY_PLAY ;
    m_playertime.year = 1970 ;
    m_seektime.year = 1979 ;
	if( j<=0 ) {
		ClosePlayer();
	}
    else {

		// seek to last day with videos, if possible, 
		if(m_startonlastday == 1 && m_clientmode == CLIENTMODE_PLAYBACK && m_screen[0] != NULL && m_screen[0]->m_decoder != NULL ) {
			dvrtime dt ;
			dt = time_now();
			dt.hour = 0 ;
			dt.min = 0 ;
			dt.second = 0 ;
			int * daylist = new int [2000] ;		// 2000 days  
			int dsize = m_screen[0]->m_decoder->getdaylist(daylist, 2000) ;
			if( dsize >= 0 ) {
				if( dsize>0 ) {
					dsize--;
					dt.year = daylist[dsize]/10000 ;
					dt.month = (daylist[dsize]%10000)/100 ;
					dt.day = daylist[dsize]%100 ;
					decoder::g_seek( &dt ) ;
				}
			}
			else {
				dt = time_now();
				dt.hour = 0 ;
				dt.min = 0 ;
				dt.second = 0 ;
				// up to 10 yrs backward search
				for( int i=0; i<355*10; i++ ) {
					if( m_screen[0]->getdayinfo( &dt ) ) {
						decoder::g_seek( &dt ) ;
						break;
					}
					time_add( dt, -(24*3600) ) ;	// backward a day
				}
			}
			delete [] daylist ;
		}

    	decoder::g_play();
    }
    if( m_screen[0] ) {
        m_focus=1 ;
		this->FocusPlayer(m_screen[0]);
	}
   	::SetTimer(m_hWnd, TIMER_UPDATE, 10, NULL );		// start update timer
#ifdef IDC_EDIT_LOCATION
    if( m_updateLocation ) {
        ::SetDlgItemText(m_hWnd, IDC_EDIT_LOCATION, _T(""));
        ::SetTimer(m_hWnd, TIMER_UPDATEMAP, 2000, NULL );		// start location update timer
    }
#endif
	UpdateMonthcalendar();      // to update day info (bold days that has video)
}


void DvrclientDlg::OpenDvr(int playmode, int clientmode, int autoopen, char * autoserver)
{
	int i, j;
    int dvrerror ;

    SelectDvr selectdvr(playmode);
    if( autoopen ) {
        selectdvr.m_autoopen = TRUE ;
        selectdvr.m_autoopen_server = autoserver ;
    }
	if( selectdvr.DoModal(selectdvr.IDD, m_hWnd)==IDOK ) {
		WaitCursor waitcursor ;
		ClosePlayer();
		j=0;
        for(i=0; i<selectdvr.m_numdvr && j<MAXDECODER; i++ ) {
            dvrerror = -1 ;
            if( selectdvr.m_dvrlist[i].dvrtype == 0 ) {         // dvrlist
                dvrerror = g_decoder[j].open(selectdvr.m_dvrlist[i].idx, playmode);
            }
            else if(  selectdvr.m_dvrlist[i].dvrtype == 1 ) {   // ip address
                dvrerror = g_decoder[j].openremote( selectdvr.m_dvrlist[i].dvrname, playmode);
            }
            else if(  selectdvr.m_dvrlist[i].dvrtype == 2 ) {   // directory
                dvrerror = g_decoder[j].openharddrive( selectdvr.m_dvrlist[i].dvrname);
            }
            if( dvrerror>0 ) {
                // check if it is a smartserver
                if( g_decoder[0].getfeatures() & PLYFEATURE_SMARTSERVER) {
                    g_decoder[0].initsmartserver();             // do smartserver init
                }
                j++ ;
            }
            else {
                g_decoder[j].close();
            }
        }
		m_clientmode=clientmode;
		StartPlayer();

#ifdef APP_PWII_FRONTEND
		// get Officer ID list
		if( g_decoder[0].isopen() ) {
			int i;
			string ** idlist = new string * [20]  ;
			for( i=0; i<20; i++ ) idlist[i]=NULL ;
			int l = g_decoder[0].getofficeridlist( idlist, 20 );
			for( i=0; i<l; i++ ) {
				if( idlist[i] ) {
					SendMessage( GetDlgItem(m_hWnd, IDC_COMBO_OFFICERID),  CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)( *idlist[i] ));
					delete idlist[i] ;
				}
			}
			delete idlist ;
			SendMessage( GetDlgItem(m_hWnd, IDC_COMBO_OFFICERID),  CB_SETCURSEL , 0, 0 );
		}
#endif

	}
}

// pwz5, offerid changed
void DvrclientDlg::OnCbnSelchangeComboOfficerId()
{
#ifdef APP_PWII_FRONTEND
	string officerid ;
	int sel = SendMessage( GetDlgItem(m_hWnd, IDC_COMBO_OFFICERID),  CB_GETCURSEL, 0, 0 );
	if( sel >= 0 ) {
		SendMessage( GetDlgItem(m_hWnd, IDC_COMBO_OFFICERID),  CB_GETLBTEXT, (WPARAM)sel, (LPARAM)(LPCTSTR)(officerid.tcssize(200)) );
		if( g_decoder[0].isopen() ) {
			g_decoder[0].setofficerid( (char *)officerid );
		}
	}
#endif
}


void DvrclientDlg::OnBnClickedPreview()
{
    OpenDvr(PLY_PREVIEW, CLIENTMODE_LIVEVIEW);
}

void DvrclientDlg::OnBnClickedPlayback()
{
    OpenDvr(PLY_PLAYBACK, CLIENTMODE_PLAYBACK);
}

void DvrclientDlg::OnBnClickedSmartserver()
{
    OpenDvr(PLY_SMARTSERVER, CLIENTMODE_PLAYSMARTSERVER);
}

// play DVR file
int DvrclientDlg::Playfile(LPCTSTR filename)
{
    int res = 0 ;
	WaitCursor waitcursor ;
	ClosePlayer();
    string fname(filename) ;
	char * ext=strrchr(fname,'.');
    if( g_decoder[0].openfile(fname) >= 0 ) {
//		m_clientmode=CLIENTMODE_PLAYFILE;
        if( ext!=NULL && stricmp(ext,".dpl")==0 ) {
		}
		m_startonlastday = 0;				// don't start from last day
		m_clientmode=CLIENTMODE_PLAYBACK;
        m_playfile_beginpos = 0 ;
        m_playfile_endpos = 0 ;
		StartPlayer();
		res = 1;
	}
	else {
        // try decode json play list
#ifdef JSON_SUPPORT
        if( ext!=NULL && stricmp(ext,".dpl")==0 ) {
            FILE * fdpl = fopen( fname, "r");
            if( fdpl ) {    
				char * dplbuf = new char [8000];
                int r = fread(dplbuf, 1, 7999, fdpl );
				if (r > 0) {
					dplbuf[r] = 0;
					char * jsonstr = strstr(dplbuf, "\n\n");
					if (jsonstr) {
						cJSON *j_root = cJSON_Parse(jsonstr);
						if (j_root) {
							cJSON *j_server = cJSON_GetObjectItem(j_root, "server");
							if (j_server) {
								cJSON * jitem = cJSON_GetObjectItem(j_server, "protocol");
								string protocol;
								protocol = jitem->valuestring;
								if (strcmp(protocol, "dvr") == 0) {
									cJSON * host = cJSON_GetObjectItem(j_server, "host");
									string rhost(host->valuestring);
									// connect to dvr directly
									res = PlayRemote(rhost, PLY_PREVIEW);
								}
							}
						}
					}
				}
				delete [] dplbuf;
                fclose( fdpl );
            }
        }
#endif
	}
    if( res == 0 ) {
		MessageBox(m_hWnd, _T("Can't play this file."), NULL, MB_OK);
    }
    return res ;
}

int DvrclientDlg::PlayRemote(LPCTSTR rhost, int playmode)
{
    int res = 0 ;
    string host( rhost );
    if( g_decoder[0].openremote( host, playmode ) >= 0 ) {
        if( playmode == PLY_PREVIEW ) {
            SetPlaymode(CLIENTMODE_LIVEVIEW) ;
        }
        else {
            SetPlaymode(CLIENTMODE_PLAYBACK) ;
        }
	    StartPlayer();
	    res = 1;
    }
    return res ;
}

void DvrclientDlg::OnBnClickedPlayfile()
{
    OPENFILENAME ofn ;
    string filename ;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof (OPENFILENAME) ;
    ofn.nMaxFile = 512 ;
    ofn.lpstrFile = filename.tcssize(ofn.nMaxFile) ;
    ofn.lpstrFilter=_T("DVR Files\0*.DVR;*.26?\0All files\0*.*\0\0");
    ofn.nFilterIndex=1;
    ofn.hInstance = ResInst();
    ofn.hwndOwner = m_hWnd ;
	if( GetOpenFileName(&ofn) ) {
		Playfile( ofn.lpstrFile ) ;
    }
}

void DvrclientDlg::OnBnClickedSetup()
{
	int res=DVR_ERROR ;
	if(m_screen[m_focus] && m_screen[m_focus]->m_decoder) {
		res=m_screen[m_focus]->m_decoder->configureDVR();
		if( res<0 ) {
			MessageBox(m_hWnd, _T("Setup Error!"), NULL, MB_OK);
		}
	}
}

void DvrclientDlg::OnBnClickedDisconnect()
{
	ClosePlayer();
}

#ifdef DVRVIEWER_APP
#include "SaveClipDialog.h"
#include "saveclipdialog2.h"
#endif

int DvrclientDlg::GetSaveClipFilename( string &filename )
{
	return 0 ;
}

void DvrclientDlg::OnBnClickedSavefile()
{
	dvrtime begintime, endtime ;
    SYSTEMTIME st ;
    ::SendDlgItemMessage(m_hWnd, IDC_TIME_SELBEGIN, DTM_GETSYSTEMTIME, 0, (LPARAM)&st);
    time_timedvr(&st, &begintime);
    ::SendDlgItemMessage(m_hWnd, IDC_TIME_SELEND, DTM_GETSYSTEMTIME, 0, (LPARAM)&st);
    time_timedvr(&st, &endtime);

	if( m_screen[m_focus]->m_decoder != NULL && time_compare( endtime, begintime)>0 ) {
		int res = 0 ;
		int copyPlayer = 0 ;
        string  filename ;
        
		int len = endtime-begintime ;
		if( m_clippath.isempty() ) {
			GetCurrentDirectory( 512, m_clippath.tcssize(520));
		}

		string initname;
		initname.printf( "%s\\%s_%04d%02d%02d_%02d%02d%02d_%d.DVR",
			(char *)m_clippath,
            m_screen[m_focus]->m_decoder->m_playerinfo.servername,
            begintime.year, begintime.month, begintime.day,
            begintime.hour, begintime.min, begintime.second,
            len );

		int support_save_v2 = m_screen[m_focus]->m_decoder->support_api("savedvrfile2");
		string channels;
		int    encrypted = false;
		string password;

#ifdef DVRVIEWER_APP
#ifdef	IDD_DIALOG_MAKECLIP
		if (support_save_v2) {
			SaveClipDialog2 dlg(m_screen[m_focus]->m_decoder);
			dlg.m_clipfilename = initname;
			if (dlg.DoModal(IDD_DIALOG_MAKECLIP2, m_hWnd) == IDOK) {
				filename = dlg.m_clipfilename;
				copyPlayer = dlg.m_saveplayer;

				encrypted = dlg.m_encrypted;
				channels = dlg.m_channels;
				password = dlg.m_password;

				res = 1;
			}
		}
		else {
			SaveClipDialog dlg;
			dlg.m_clipfilename = initname;
			if (dlg.DoModal(IDD_DIALOG_MAKECLIP, m_hWnd) == IDOK) {
				filename = dlg.m_clipfilename;
				copyPlayer = dlg.m_saveplayer;
				res = 1;
			}
		}
#endif
#else
        OPENFILENAME ofn ;
        memset(&ofn, 0, sizeof(ofn));
        ofn.lStructSize = sizeof (OPENFILENAME) ;
        ofn.nMaxFile = 512 ;
        ofn.lpstrFile = filename.tcssize(ofn.nMaxFile) ;
        // set initialized file name (dvrname_date_time_lengh)
        wcscpy(ofn.lpstrFile, initname );
        // ofn.lpstrFilter=_T("DVR Files (*.DVR)\0*.DVR\0AVI Files (*.AVI)\0*.AVI\0All files\0*.*\0\0") ;  // all .avi support
		ofn.lpstrFilter = _T("DVR Files (*.DVR)\0*.DVR\0AVI Files (*.AVI)\0*.AVI\0MP4 Files (*.MP4)\0*.MP4\0All files\0*.*\0\0");  // all .avi support
		ofn.Flags=0;
        ofn.nFilterIndex=1;
        ofn.hInstance = AppInst();
        ofn.hwndOwner = m_hWnd ;
        ofn.lpstrDefExt=_T("DVR");
        ofn.Flags=OFN_OVERWRITEPROMPT ;
		res = GetSaveFileName(&ofn) ;
#endif

        if( res ) {

			// save previous clip path
			m_clippath = filename;
			char * fp;
			char * rs;
			fp = (char *)m_clippath;
			rs = strrchr(fp, '\\');
			if (rs) {
				*(rs) = 0;
			}

			struct dup_state dupstate ;
            string fpath = filename ;

			memset(&dupstate, 0, sizeof(dupstate));

			if (support_save_v2) {
				char * passwd = NULL;
				if (encrypted) {
					passwd = (char *)password;
				}
				res = m_screen[m_focus]->m_decoder->savedvrfile2(&begintime, &endtime, fpath, DUP_ALLFILE | DUP_BACKGROUND, &dupstate, channels, passwd);
			}
			else {
				res = m_screen[m_focus]->m_decoder->savedvrfile(&begintime, &endtime, fpath, DUP_ALLFILE | DUP_BACKGROUND, &dupstate);
			}
			if( res<0 ) {
				MessageBox(m_hWnd, _T("Can't save DVR file."), NULL, MB_OK);
			}
			else if( res==0 ) {

				// to copy portable player
				if (copyPlayer) {
					WaitCursor waitcursor;
					copyportablefiles(fpath, NULL);
				}

                string title ;
				title.printf( "Saving file: %s", (LPCSTR)fpath);
				CopyProgress progress(title, &dupstate);
				progress.DoModal(IDD_DIALOG_COPY, m_hWnd);
				// on any case, we should wait until task finish
				while( dupstate.status==0 ) 
					Sleep(50);
#ifdef APP_PWPLAYER
				char * pwmsg = new char [4096] ;
				int i, n ;
				i=0 ;

				// event
				sprintf( &pwmsg[i], "event: clip\r\n" );
				i+=strlen(&pwmsg[i]) ;

				// server
				sprintf( &pwmsg[i], "server: %s\r\n", m_screen[m_focus]->m_decoder->getservername() );
				i+=strlen(&pwmsg[i]) ;

				// user_id
				if( tvskeydata ) {
					sprintf( &pwmsg[i], "user_id:  %s\r\n", tvskeydata->usbid );
					i+=strlen(&pwmsg[i]) ;
				}

				// system_time
                GetLocalTime(&st);
				sprintf( &pwmsg[i], "system_time:  %d-%02d-%02d %02d:%02d:%02d\r\n", 
                    st.wYear,
                    st.wMonth,
                    st.wDay,
                    st.wHour,
                    st.wMinute,
                    st.wSecond );
				i+=strlen(&pwmsg[i]) ;

				// file name
				sprintf( &pwmsg[i], "filename: %s\r\n", (LPSTR)fpath);
				i+=strlen(&pwmsg[i]) ;

				// start_time
				sprintf( &pwmsg[i], "start_time:  %d-%02d-%02d %02d:%02d:%02d\r\n", 
					begintime.year,
					begintime.month,
					begintime.day,
					begintime.hour,
					begintime.min,
					begintime.second );
				i+=strlen(&pwmsg[i]) ;

				// end_time
				sprintf( &pwmsg[i], "end_time:  %d-%02d-%02d %02d:%02d:%02d\r\n", 
					endtime.year,
					endtime.month,
					endtime.day,
					endtime.hour,
					endtime.min,
					endtime.second );
				i+=strlen(&pwmsg[i]) ;

				decoder::g_minitrack_sendpwevent(pwmsg, i+1) ;
				delete pwmsg ;
#endif
			}
			else {		// sucess
				MessageBox(m_hWnd, _T("DVR file saved."), NULL, MB_OK);
			}
		}
	}
	else {
		MessageBox(m_hWnd, _T("Please Select Correct Begin/End Time!"), NULL, MB_OK);
	}
}

void DvrclientDlg::OnBnClickedQuickcap()
{
    SYSTEMTIME st ;
    dvrtime begintime, endtime ;

    begintime = m_playertime ; 
    endtime = m_playertime ;
    time_add(begintime, -30) ;
    time_add(endtime, 30) ;

    time_dvrtime(&begintime, &st);
    ::SendDlgItemMessage(m_hWnd, IDC_TIME_SELBEGIN, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&st);
    time_dvrtime(&endtime, &st);
    ::SendDlgItemMessage(m_hWnd, IDC_TIME_SELEND, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&st);
    OnBnClickedSavefile();
}

void DvrclientDlg::OnBnClickedTimebegin()
{
	SYSTEMTIME systime ;
    time_dvrtime(&m_playertime, &systime );
    ::SendDlgItemMessage(m_hWnd, IDC_TIME_SELBEGIN, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&systime);
    LRESULT r ;
    OnDtnDatetimechangeTimeSelbegin(NULL, &r);
}

void DvrclientDlg::OnBnClickedTimeend()
{
	SYSTEMTIME systime ;
    time_dvrtime(&m_playertime, &systime );
    ::SendDlgItemMessage(m_hWnd, IDC_TIME_SELEND, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&systime);
    LRESULT r ;
    OnDtnDatetimechangeTimeSelend(NULL, &r);
}

void DvrclientDlg::OnBnClickedMixaudio()
{
	g_MixSound=!g_MixSound;
	if( m_screen[ m_focus ] ) {
		FocusPlayer( m_screen[ m_focus ] ) ;
	}
}

void DvrclientDlg::OnBnClickedSlow()
{
	if( g_playstat==PLAY_PAUSE ) {
//		decoder::g_oneframebackword();
	}
	else {
		if( g_playstat>PLAY_SLOW || g_playstat<PLAY_SLOW_MAX ) {
			g_playstat=PLAY_SLOW ;
			decoder::g_slowforward(1);
		}
		else if( g_playstat>PLAY_SLOW_MAX ) {
			g_playstat--;
			decoder::g_slowforward(PLAY_SLOW-g_playstat+1);
		}
	}
}

void DvrclientDlg::SeekTime( struct dvrtime * dtime )
{
    m_playertime = *dtime ;
    m_seektime = *dtime ;
    decoder::g_seek(dtime);
    m_tbarupdate = 1 ;
	SetTimer(m_hWnd, TIMER_UPDATE, 1, NULL );
}

void DvrclientDlg::OnBnClickedForward()
{
	// Jump 20 seconds Forward
	struct dvrtime dtime ;
    dtime=m_playertime ;
    time_add(dtime, FORWARD_JUMP);
    SeekTime( &dtime );
}

void DvrclientDlg::OnBnClickedBackward()
{
	// Jump 20 seconds backward
    struct dvrtime dtime ;
    dtime=m_playertime ;
    time_add(dtime, -(BACKWARD_JUMP));
    SeekTime( &dtime );
}

void DvrclientDlg::OnBnClickedPause()
{
	g_playstat=PLAY_PAUSE ;
	decoder::g_pause();
	ShowWindow(GetDlgItem(m_hWnd, IDC_PAUSE),SW_HIDE);
	ShowWindow(GetDlgItem(m_hWnd, IDC_PLAY),SW_SHOW);
	EnableWindow(GetDlgItem(m_hWnd, IDC_STEP),TRUE);
}

void DvrclientDlg::OnBnClickedPlay()
{
    if( m_cliplist ) {
        CloseClipList();
        delete m_cliplist ;
        m_cliplist=NULL ;
        ::ShowWindow( m_slider.getHWND(), SW_SHOW );
        SetScreenFormat();
    }

	g_playstat=PLAY_PLAY ;
	decoder::g_play();
	ShowWindow(GetDlgItem(m_hWnd, IDC_PLAY),SW_HIDE);
	ShowWindow(GetDlgItem(m_hWnd, IDC_PAUSE),SW_SHOW);
	EnableWindow(GetDlgItem(m_hWnd, IDC_STEP),FALSE);
}

void DvrclientDlg::OnBnClickedFast()
{
	if( g_playstat!=PLAY_PAUSE ) {
		if( g_playstat<PLAY_FAST || g_playstat>PLAY_FAST_MAX ) {
			g_playstat=PLAY_FAST ;
			decoder::g_fastforward(1);
		}
		else if( g_playstat<PLAY_FAST_MAX ) {
			g_playstat++;
			decoder::g_fastforward(g_playstat-PLAY_FAST+1);
		}
	}
}

void DvrclientDlg::OnBnClickedStep()
{
	if( g_playstat==PLAY_PAUSE ) {
		decoder::g_oneframeforward();
	}
}

void DvrclientDlg::Seek()
{
    SYSTEMTIME st ;
	struct dvrtime stime ;
	int pos ;
    memset( &stime, 0, sizeof(stime));
    ::SendDlgItemMessage( m_hWnd, IDC_MONTHCALENDAR, MCM_GETCURSEL, 0, (LPARAM)&st );
    stime.year = st.wYear ;
    stime.month = st.wMonth ;
    stime.day = st.wDay ;
    pos=m_slider.GetPos();
    stime.hour = pos / 3600 ;
    stime.min = (pos%3600)/60 ;
    stime.second = pos%60 ;
    stime.millisecond=0 ;
    SeekTime( &stime );
}

void DvrclientDlg::OnBnClickedPrev()
{
    struct dvrtime dvrt ;
    int max = m_slider.GetRangeMax();
    int min = m_slider.GetRangeMin();
    dvrt=m_playertime ;
    dvrt.hour=min/3600 ; 
    dvrt.min=min%3600/60 ; 
    dvrt.second=min%60 ;
    time_add(dvrt, (min-max)) ;
    dvrt.second = 0 ;
    dvrt.millisecond=0 ;
    SeekTime(&dvrt);
}

void DvrclientDlg::OnBnClickedNext()
{
    struct dvrtime dvrt ;
    int max = m_slider.GetRangeMax();
    int min = m_slider.GetRangeMin();
    dvrt=m_playertime ;
    dvrt.hour=min/3600 ; 
    dvrt.min=min%3600/60 ; 
    dvrt.second=min%60 ;
    time_add(dvrt, (max-min+1)) ;
    dvrt.second=0 ;
    dvrt.millisecond=0 ;
    SeekTime(&dvrt);
}

void DvrclientDlg::OnBnClickedUsers()
{
#ifndef APP_TVS
#ifndef NOLOGIN 
	UserDlg userdlg ;
    userdlg.DoModal(IDD_DIALOG_USER, this->m_hWnd);
#endif
#endif
}

void DvrclientDlg::OnDtnDatetimechangeTimeSelbegin(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMDATETIMECHANGE pDTChange = reinterpret_cast<LPNMDATETIMECHANGE>(pNMHDR);

    *pResult = 0;
    SYSTEMTIME st ;
    struct dvrtime timebegin, timeend ;

    ::SendDlgItemMessage(m_hWnd, IDC_TIME_SELBEGIN, DTM_GETSYSTEMTIME, 0, (LPARAM)&st);
    time_timedvr(&st, &timebegin);
    ::SendDlgItemMessage(m_hWnd, IDC_TIME_SELEND, DTM_GETSYSTEMTIME, 0, (LPARAM)&st);
    time_timedvr(&st, &timeend);
	if( time_compare(timeend,timebegin)<0) {
        time_dvrtime(&timebegin, &st);
        ::SendDlgItemMessage(m_hWnd, IDC_TIME_SELEND, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&st);
	}
}

void DvrclientDlg::OnDtnDatetimechangeTimeSelend(NMHDR *pNMHDR, LRESULT *pResult)
{
   	LPNMDATETIMECHANGE pDTChange = reinterpret_cast<LPNMDATETIMECHANGE>(pNMHDR);

    *pResult = 0;
    SYSTEMTIME st ;
    struct dvrtime timebegin, timeend ;

    ::SendDlgItemMessage(m_hWnd, IDC_TIME_SELBEGIN, DTM_GETSYSTEMTIME, 0, (LPARAM)&st);
    time_timedvr(&st, &timebegin);
    ::SendDlgItemMessage(m_hWnd, IDC_TIME_SELEND, DTM_GETSYSTEMTIME, 0, (LPARAM)&st);
    time_timedvr( &st, &timeend);
	if( time_compare(timeend,timebegin)<0) {
        time_dvrtime(&timeend, &st);
        ::SendDlgItemMessage(m_hWnd, IDC_TIME_SELBEGIN, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&st);
	}
}

void DvrclientDlg::OnToolTipNotify( NMHDR * pNMHDR, LRESULT * pResult )
{
    TOOLTIPTEXT *pTTT = (TOOLTIPTEXT *)pNMHDR;
    UINT nID ;
	*pResult = 0 ;
    if (pTTT->uFlags & TTF_IDISHWND)
    {
        // idFrom is actually the HWND of the tool
        nID = ::GetDlgCtrlID((HWND)(pNMHDR->idFrom));
        string strTooltip ;
        if(nID == IDC_SLIDER)
        {
			int pos = m_slider.GetPos() ;
			strTooltip.printf("%02d:%02d:%02d", pos/3600, pos%3600/60, pos%60 );
            _tcscpy(pTTT->szText, strTooltip );
		}
        else if(nID == IDC_SLIDER_VOLUME )
        {
            int pos = ::SendDlgItemMessage(m_hWnd, nID, TBM_GETPOS, 0, 0);
			strTooltip.printf("%d", pos );
            _tcscpy(pTTT->szText, strTooltip );
		}
        else if(nID == IDC_BACKWARD ) {
			strTooltip.printf("Jump backward %d seconds", BACKWARD_JUMP );
            _tcscpy(pTTT->szText, strTooltip );
        }
        else if(nID == IDC_FORWARD ) {
			strTooltip.printf("Jump forward %d seconds", FORWARD_JUMP  );
            _tcscpy(pTTT->szText, strTooltip );
        }
        else if(nID == IDC_TIME_SELBEGIN ) {
            SYSTEMTIME st ;
            int c ;
            ::SendDlgItemMessage(m_hWnd, nID, DTM_GETSYSTEMTIME, 0, (LPARAM)&st);
            c = GetDateFormat( LOCALE_USER_DEFAULT, 
                DATE_LONGDATE, 
                &st, 
                NULL, 
                pTTT->szText,
                sizeof(pTTT->szText) );
            pTTT->szText[c-1]=' ' ;
            GetTimeFormat( LOCALE_USER_DEFAULT,
                0,
                &st,
                NULL,
                &pTTT->szText[c],
                sizeof(pTTT->szText)-c );
        }
        else if(nID == IDC_TIME_SELEND ) {
            SYSTEMTIME st ;
            int c ;
            ::SendDlgItemMessage(m_hWnd, nID, DTM_GETSYSTEMTIME, 0, (LPARAM)&st);
            c = GetDateFormat( LOCALE_USER_DEFAULT, 
                DATE_LONGDATE, 
                &st, 
                NULL, 
                pTTT->szText,
                sizeof(pTTT->szText) );
            pTTT->szText[c-1]=' ' ;
            GetTimeFormat( LOCALE_USER_DEFAULT,
                0,
                &st,
                NULL,
                &pTTT->szText[c],
                sizeof(pTTT->szText)-c );
        }
        else {
            ::LoadStringW(ResInst(), nID, strTooltip.tcssize(256), 256 );
            _tcscpy( pTTT->szText, strTooltip );
		}
    }
    return ;
}

void DvrclientDlg::OnNMCustomdrawSlider(NMHDR *pNMHDR, LRESULT *pResult)
{
}

/*
void DvrclientDlg::OnNMCustomdrawSlider(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMCUSTOMDRAW pNMCD = reinterpret_cast<LPNMCUSTOMDRAW>(pNMHDR);

#define BKCOLOR (RGB(0,115,220))
#define VIDEOCOLOR (RGB(255,180,0))
#define MARKCOLOR (RGB(248,80,0))

	int i;
	UINT SaveTextAlign ;

    // default result
	*pResult = CDRF_DODEFAULT ;

	if( pNMCD->dwDrawStage == CDDS_PREPAINT ) {
		*pResult = CDRF_NOTIFYITEMDRAW ;
	}
	else if( pNMCD->dwDrawStage == CDDS_ITEMPREPAINT && pNMCD->dwItemSpec==TBCD_THUMB ) {
		if( ::IsWindowEnabled(m_slider.getHWND()) ) {
            Image * pimg ;
            pimg = loadbitmap(_T("POINTER"));
            Graphics * g = new Graphics( pNMCD->hdc ) ;
            if( ::GetCapture() == m_slider.getHWND() ) {
                g->DrawImage(pimg, (INT)pNMCD->rc.left, pNMCD->rc.top );
            }
            else 
            {
                g->DrawImage(pimg, (INT)pNMCD->rc.left, pNMCD->rc.top );
            }
            delete pimg ;
            delete g ;
        }
        *pResult = CDRF_SKIPDEFAULT ;
	}
	else if( pNMCD->dwDrawStage == CDDS_ITEMPREPAINT && pNMCD->dwItemSpec==TBCD_CHANNEL ) {
		if( ::IsWindowEnabled(m_slider.getHWND()) ) {
			*pResult = CDRF_NOTIFYPOSTPAINT ;
		}
	}
	else if( pNMCD->dwDrawStage == CDDS_ITEMPOSTPAINT && pNMCD->dwItemSpec==TBCD_CHANNEL ) {
		RECT bkrect, ptrect ;
		m_slider.GetChannelRect( &bkrect );
		bkrect.bottom-=2 ;
		bkrect.top+=2;
		bkrect.left+=2;
		bkrect.right-=2 ;
		SetDCBrushColor(pNMCD->hdc, BKCOLOR);
		FillRect( pNMCD->hdc, &bkrect, (HBRUSH)GetStockObject(DC_BRUSH));

		bkrect.left+=3 ;
		bkrect.right-=3 ;
		ptrect=bkrect ;

		int ptwidth = bkrect.right - bkrect.left ;
		int SliderMax, SliderMin ;
        SliderMin = m_slider.GetRangeMin();
        SliderMax = m_slider.GetRangeMax();
		int poswidth =	SliderMax-SliderMin; // in seconds

		m_playfile_beginpos = SliderMax ;
		m_playfile_endpos = SliderMin ;

		int infosize ;
		struct cliptimeinfo * timeinfo ;
		infosize = m_screen[m_focus]->getcliptime( &m_playertime, &timeinfo );
		SetDCBrushColor(pNMCD->hdc, VIDEOCOLOR);
		for( i=0; i<infosize; i++ ) {
			// retieve begin and end time of .DVR file
			if( m_playfile_beginpos>timeinfo[i].on_time ){
				m_playfile_beginpos = timeinfo[i].on_time ;
			}
			if( m_playfile_endpos<timeinfo[i].off_time ) {
				m_playfile_endpos=timeinfo[i].off_time;
			}
			if( timeinfo[i].on_time>SliderMax ){
				break;
			}
			if( timeinfo[i].off_time<=SliderMin ) {
				continue ;
			}

			if( timeinfo[i].on_time<=timeinfo[i].off_time ) {
				ptrect.left = bkrect.left + (timeinfo[i].on_time-SliderMin) * ptwidth / poswidth ;
				ptrect.right = bkrect.left + (timeinfo[i].off_time-SliderMin+2) * ptwidth / poswidth ;
				if( ptrect.left<=bkrect.left ){
					ptrect.left=bkrect.left-3 ;
				}
				if( ptrect.right>=bkrect.right ) {
					ptrect.right=bkrect.right+3 ;
				}
				FillRect( pNMCD->hdc, &ptrect, (HBRUSH)GetStockObject(DC_BRUSH) );
				if( ptrect.right>=bkrect.right )
					break ;
			}
		}

		infosize = m_screen[m_focus]->getlocktime( &m_playertime, &timeinfo );
		SetDCBrushColor(pNMCD->hdc, MARKCOLOR);
		for( i=0; i<infosize; i++ ) {
			// retieve begin and end time of .DVR file
			if( m_playfile_beginpos>timeinfo[i].on_time ){
				m_playfile_beginpos = timeinfo[i].on_time ;
			}
			if( m_playfile_endpos<timeinfo[i].off_time ) {
				m_playfile_endpos=timeinfo[i].off_time;
			}

			if( timeinfo[i].on_time>SliderMax )
				break;
			if( timeinfo[i].off_time<=SliderMin ) 
				continue ;
			if( timeinfo[i].on_time<timeinfo[i].off_time ) {
				ptrect.left = bkrect.left + (timeinfo[i].on_time-SliderMin) * ptwidth / poswidth ;
				ptrect.right = bkrect.left + (timeinfo[i].off_time-SliderMin+1) * ptwidth / poswidth ;
				if( ptrect.left<=bkrect.left ){
					ptrect.left=bkrect.left-3 ;
				}
				if( ptrect.right>=bkrect.right ) {
					ptrect.right=bkrect.right+3 ;
				}
				FillRect( pNMCD->hdc, &ptrect, (HBRUSH)GetStockObject(DC_BRUSH) );
				if( ptrect.right>=bkrect.right )
					break ;
			}
		}

        // adjust playfile time
        if( m_clientmode==CLIENTMODE_PLAYFILE &&
            //                m_playfile_beginpos<m_playfile_endpos &&
			( m_playfile_beginpos != m_slider.GetRangeMin() ||
            m_playfile_endpos != m_slider.GetRangeMax() ) ) 
        {
            m_tbarupdate = 1 ;
            ::SetTimer(m_hWnd, TIMER_UPDATE, 10, NULL);
        }

// output tic text
		SetBkMode( pNMCD->hdc, TRANSPARENT );
		HFONT hft = CreateFont(
					-(ptrect.bottom-ptrect.top-1),
					0,
					0,
					0, 
					FW_THIN,			// Weight
					0, 0, 0, 
					DEFAULT_CHARSET,	// CharSet
					OUT_DEFAULT_PRECIS, // OutputPrecision
					CLIP_DEFAULT_PRECIS,
					CLEARTYPE_QUALITY, 
					FF_DONTCARE, NULL );
		HFONT oldhft = (HFONT)SelectObject( pNMCD->hdc, (HGDIOBJ)hft );
		SetTextColor(pNMCD->hdc, 0);
		SetROP2(pNMCD->hdc, R2_NOP);

		m_slider.GetChannelRect( &ptrect ) ;
        int slider_width = ptrect.right - ptrect.left ;
		int ticknum ;
		ticknum=slider_width / 120 ;
		if( ticknum>=12 )
			ticknum=12 ;
		else if( ticknum>=8 )
			ticknum=8 ;
		else if( ticknum>=6)
			ticknum=6 ;
		else if( ticknum>=4 )
			ticknum=4 ;

        string ticmark ;
        int pos = ptrect.left ;
        int ptime = SliderMin ;
        SaveTextAlign = SetTextAlign( pNMCD->hdc, TA_LEFT|TA_TOP );
        ticmark.printf( "%02d:%02d", ptime/3600, ptime%3600/60 );
		TextOut(  pNMCD->hdc, pos+3, ptrect.top, ticmark, 5 );
        SliderMax++ ;
        SetTextAlign( pNMCD->hdc, TA_CENTER|TA_TOP );
		for( i=2; i<=ticknum; i+=1 ) {
            pos += slider_width/ticknum ;
            ptime += (SliderMax-SliderMin)/ticknum ;
            ticmark.printf( "%02d:%02d", ptime/3600, ptime%3600/60 );
            TextOut( pNMCD->hdc, pos, ptrect.top, ticmark, 5 );
		}
		SetTextAlign( pNMCD->hdc, TA_RIGHT|TA_TOP );
        ticmark.printf( "%02d:%02d", SliderMax/3600, SliderMax%3600/60 );
		TextOut(  pNMCD->hdc, ptrect.right-3, ptrect.top, ticmark, 5 );

		SetTextAlign( pNMCD->hdc, SaveTextAlign );
		SelectObject( pNMCD->hdc, (HGDIOBJ)oldhft );
		DeleteObject(hft);
	}
}
*/

BOOL DvrclientDlg::PreProcessMessage(MSG* pMsg)
{
	BOOL res = 0;
	if(	pMsg->message == WM_KEYDOWN ) {
        if( pMsg->wParam == VK_F4 ) {       // display or close map
            DisplayMap();
        }
        else if( pMsg->wParam == VK_F5 ) {       // refresh camera
            int i;
            for(i=0; i<m_screennum ; i++ ) {
                if( m_screen[i] && m_screen[i]->isattached() ) {
                    m_screen[i]->AttachDecoder( 
                        m_screen[i]->m_decoder, 
                        m_screen[i]->m_channel );
                }
            }
        }
        else if( pMsg->wParam == VK_F6 ) {      // context menu
            if( m_screen[m_focus] && m_screen[m_focus]->isVisible() ) {
                PostMessage( m_screen[m_focus]->getHWND(), WM_RBUTTONUP, (WPARAM)0, (LPARAM)MAKELONG(40, 30));
            }
        }
		else if( pMsg->wParam == VK_F7 ) {	// change focused camera
			if( g_singlescreenmode==0 ) {
                int focus=m_focus+1 ;
				if( focus>=m_screennum ) {
					focus=0 ;
				}
				if( m_screen[focus] ) {
					FocusPlayer( m_screen[focus] );
				}
			}
		}
        else if( pMsg->wParam == VK_F8 ) {	// Rotate screen format
            g_singlescreenmode=0 ;
            g_screenmode++ ;
			if( g_screenmode >= screenmode_table_num ) {
				g_screenmode=0 ;
			}
			SetScreenFormat();
		}
        else if( pMsg->wParam == VK_F9 ) {	// full screen (one screen)
            if( m_screen[m_focus] && m_screen[m_focus]->isVisible() ) {
                PostMessage( m_screen[m_focus]->getHWND(), WM_LBUTTONDBLCLK, (WPARAM)1, (LPARAM)0);
            }
		}
        else if( pMsg->wParam == VK_F11 ) {	// to hide control panel
            m_panelhide = ! m_panelhide ;
            OnSize(0, 0, 0);
		}
		else if( pMsg->wParam == VK_MEDIA_PLAY_PAUSE ) {	
            if( m_clientmode>=CLIENTMODE_PLAYBACK ) {
                if(g_playstat==PLAY_PLAY) {
                    OnBnClickedPause();
                }
                else if( g_playstat==PLAY_PAUSE ) {
                    OnBnClickedPlay();
                }
            }
        }
        else if( pMsg->wParam == VK_MEDIA_STOP ) {
            if( m_clientmode!=CLIENTMODE_CLOSE ) {
                ClosePlayer();
            }
		}
        else if( pMsg->wParam == VK_MEDIA_PREV_TRACK ) {	
            OnBnClickedPrev();
		}
		else if( pMsg->wParam == VK_MEDIA_NEXT_TRACK ) {	
            OnBnClickedNext();
		}
		else if (pMsg->wParam == VK_LEFT ) {	// left
			if (m_screen[m_focus] && (m_screen[m_focus]->is_zoomin() || (GetKeyState(VK_SHIFT)&0x8000))) {
				m_screen[m_focus]->ZoomPTZ(-1, 0, 0);
				res = 1;
			}
		}
		else if (pMsg->wParam == VK_RIGHT) {
			if (m_screen[m_focus] && (m_screen[m_focus]->is_zoomin() || (GetKeyState(VK_SHIFT) & 0x8000))) {
				m_screen[m_focus]->ZoomPTZ(1, 0, 0);
				res = 1;
			}
		}
		else if (pMsg->wParam == VK_UP) {		// up
			if (m_screen[m_focus] && (m_screen[m_focus]->is_zoomin() || (GetKeyState(VK_SHIFT) & 0x8000))) {
				m_screen[m_focus]->ZoomPTZ(0, -1, 0);
				res = 1;
			}
		}
		else if (pMsg->wParam == VK_DOWN) {		// down
			if (m_screen[m_focus] && (m_screen[m_focus]->is_zoomin() || (GetKeyState(VK_SHIFT) & 0x8000))) {
				m_screen[m_focus]->ZoomPTZ(0, 1, 0);
				res = 1;
			}
		}
		else if (pMsg->wParam == VK_ADD || pMsg->wParam == VK_OEM_PLUS) {		// +
			if (m_screen[m_focus] && (m_screen[m_focus]->is_zoomin() || (GetKeyState(VK_SHIFT) & 0x8000))) {
				m_screen[m_focus]->ZoomPTZ(0, 0, 1);
				res = 1;
			}
		}
		else if (pMsg->wParam == VK_SUBTRACT || pMsg->wParam == VK_OEM_MINUS) {		// -
			if (m_screen[m_focus] && (m_screen[m_focus]->is_zoomin() || (GetKeyState(VK_SHIFT) & 0x8000))) {
				m_screen[m_focus]->ZoomPTZ(0, 0, -1);
				res = 1;
			}
		}
		else if ((int)pMsg->wParam == (int)'R' ) {		// rotate hot key
			if (m_screen[m_focus]) {
				m_screen[m_focus]->rotate();
				res = 1;
			}
		}

	}
	return res;
}

#define PTZ_PANRIGHT	(0x00020000)
#define PTZ_PANLEFT		(0x00040000)
#define PTZ_TILTUP		(0x00080000)
#define PTZ_TILTDOWN	(0x00100000)
#define PTZ_ZOOMTELE	(0x00200000)
#define PTZ_ZOOMWIDE	(0x00400000)
#define PTZ_FOCUSFAR	(0x00800000)
#define PTZ_FOCUSNEAR	(0x01000000)
#define PTZ_IRISOPEN	(0x02000000)
#define PTZ_IRISCLOSE	(0x04000000)
#define PTZ_CAMERAON	(0x88000000)
#define PTZ_CAMERAOFF	(0x08000000)

#define PTZ_CAMERAON	(0x88000000)
#define PTZ_CAMERAOFF	(0x08000000)
#define PTZ_AUTOSCAN	(0x90000000)
#define PTZ_AUTOSCANOFF	(0x10000000)

// extended command set
#define PTZ_SETPRESET			0x00030000
#define PTZ_CLEARPRESET			0x00050000
#define PTZ_GOTOPRESET			0x00070000

// more extended command set
#define PTZ_FLIP		(0x00070021)
#define PTZ_ZEROPOS		(0x00070022)
#define PTZ_RESET		(0x000f0000)
#define PTZ_ZONESTART	(0x00110000)
#define PTZ_ZONEEND		(0x00130000)
#define PTZ_ZONESCANON	(0x001b0000)
#define PTZ_ZONESCANOFF	(0x001d0000)
#define PTZ_PATTERNSTART (0x001f0000)
#define PTZ_PATTERNSTOP (0x00210000)
#define PTZ_RUNPATTERN	(0x00230000)
#define PTZ_ZOOMSPEED	(0x00250000)
#define PTZ_FOCUSSPEED	(0x00270000)
#define PTZ_AUTOFOCUSON	(0x002b0001)
#define PTZ_AUTOFOCUSOFF (0x002b0002)
#define PTZ_AUTOIRISON	(0x002d0001)
#define PTZ_AUTOIRISOFF (0x002d0002)

#define ABS( x )  ((x)>0?(x):-(x))

int DvrclientDlg::DVR_PTZMsg( DWORD msg, DWORD param )
{
	if( m_screen[m_focus] && m_screen[m_focus]->m_decoder && m_screen[m_focus]->isattached() ) {
		struct Ptz_Cmd_Buffer buf ;
		buf.channel = m_screen[m_focus]->m_channel ;
		buf.cmd = msg ;
		buf.param = param ;
		return m_screen[m_focus]->m_decoder->senddvrdata(PROTOCOL_PTZ, (void *)&buf, sizeof(buf) );
	}
	return -1;
}

#define CAMPASSCENTER	8
#define CAMPASSEDGE    (CAMPASSCENTER+63)
// draw PTZ campass
void DvrclientDlg::Campass( int x, int y )
{
	DWORD ptzcmd = 0 ;
	int xoffset, yoffset ;
	int signx, signy ;
	xoffset = x - (m_ptzcampass_rect.left+m_ptzcampass_rect.right)/2 ;
	yoffset = y - (m_ptzcampass_rect.top+m_ptzcampass_rect.bottom)/2 ;

	if( xoffset>=0 ) {
		signx=1 ;
	}
	else {
		signx=-1 ;
		xoffset=-xoffset ;
	}

	if( yoffset>=0 ) {
		signy=1 ;
	}
	else {
		signy=-1 ;
		yoffset=-yoffset ;
	}

	if( xoffset>CAMPASSEDGE || yoffset>CAMPASSEDGE ) {
		if( xoffset > yoffset ) {
			yoffset = CAMPASSEDGE * yoffset / xoffset ; 
			xoffset = CAMPASSEDGE ;
		}
		else {
			xoffset = CAMPASSEDGE * xoffset / yoffset ;
			yoffset = CAMPASSEDGE ;
		}
	}

	TCHAR res[8] = _T("PTZ_OO") ;

	if( xoffset > CAMPASSCENTER ) {
		xoffset -= CAMPASSCENTER ;
		if( signx>0 ){
			ptzcmd |= PTZ_PANRIGHT ;
			res[5] = 'R' ;
		}
		else{
			ptzcmd |= PTZ_PANLEFT ;
			res[5] = 'L' ;
		}
	}

	if( yoffset > CAMPASSCENTER ) {
		yoffset -= CAMPASSCENTER ;
		if( signy>0 ){
			ptzcmd |= PTZ_TILTDOWN ;
			res[4]='D' ;
		}
		else {
			ptzcmd |= PTZ_TILTUP ;
			res[4]='U' ;
		}
	}

	if( _tcscmp(res, m_ptzres)!=0 ) {
		Bitmap *ptzcampass ;
		ptzcampass = loadbitmap( res );
		Graphics * g = new Graphics(m_hWnd);
		Region * reg = new Region(m_ptzcampass_rgn);
		g->SetClip(reg);
		g->DrawImage( ptzcampass, (INT)m_ptzcampass_rect.left, m_ptzcampass_rect.top );
		delete reg ;
		delete g ;
		_tcscpy(m_ptzres,res );
	}

//	ptz_run=1 ;
//	int channel = HikPlayer[focus]->nChannel ;
	if( ptzcmd == 0 ) {
		DVR_PTZMsg( 0, 0 );
	}
	else {
		ptzcmd += xoffset*256 + yoffset ;
		DVR_PTZMsg( ptzcmd, 0 );
	}
}

void DvrclientDlg::OnLButtonDown(UINT nFlags, int x, int y)
{
	if( IsWindowVisible(GetDlgItem(m_hWnd, IDC_GROUP_PTZ)) ) {
		if( ::PtInRegion((HRGN)m_ptzcampass_rgn, x, y) ) {		
			SetCapture(m_hWnd);
			m_ptz_savedcursor = SetCursor(m_ptz_cursor);
			Campass( x, y ) ;
		}
	}

#ifdef APP_PW_TABLET
	// click on speaker icon to pop up volume control window
	HWND hbox = ::GetDlgItem(m_hWnd, IDC_PIC_SPEAKER_VOLUME) ;
    if( hbox ) {
		RECT sprect ;
        ::GetClientRect( hbox, &sprect );
        ::MapWindowPoints( hbox, m_hWnd, (LPPOINT)&sprect, 2 );
		if( x>=sprect.left && x<=sprect.right && y>=sprect.top && y<=sprect.bottom ) {
			DisplayVolume(sprect.left-10, sprect.top-10) ;
			return ;
		}
    }

	// check clik on slider prev/next
	if( m_clientmode == CLIENTMODE_PLAYBACK && y>=m_playerrect.bottom && y<m_bottombar.top ) {
		if( x>=m_playerrect.left  && x<m_playerrect.left+50 ) {
			OnBnClickedPrev() ;
		}
		else if( x>=m_playerrect.right-50  && x<m_playerrect.right ) {
			OnBnClickedNext() ;
		}
	}
#endif
}

struct ptz_extcmd_stru {
	int	id ;
	DWORD cmd ;
} ptz_ext_cmd[] = {
	{ID_PTZ_CAMERAON,	PTZ_CAMERAON},
	{ID_PTZ_CAMERAOFF,	PTZ_CAMERAOFF},
	{ID_PTZ_AUTOSCANON,	PTZ_AUTOSCAN},
	{ID_PTZ_AUTOSCANOFF, PTZ_AUTOSCANOFF},
	{ID_PTZ_FLIP,		PTZ_FLIP},
	{ID_PTZ_ZEROPANPOSITION,	PTZ_ZEROPOS},
	{ID_PTZ_RESET,		PTZ_RESET},
	{ID_ZONESTART_1, PTZ_ZONESTART+1},
	{ID_ZONESTART_2, PTZ_ZONESTART+2},
	{ID_ZONESTART_3, PTZ_ZONESTART+3},
	{ID_ZONESTART_4, PTZ_ZONESTART+4},
	{ID_ZONESTART_5, PTZ_ZONESTART+5},
	{ID_ZONESTART_6, PTZ_ZONESTART+6},
	{ID_ZONESTART_7, PTZ_ZONESTART+7},
	{ID_ZONESTART_8, PTZ_ZONESTART+8},
	{ID_ZONEEND_1,	PTZ_ZONEEND+1},
	{ID_ZONEEND_2,	PTZ_ZONEEND+2},
	{ID_ZONEEND_3,	PTZ_ZONEEND+3},
	{ID_ZONEEND_4,	PTZ_ZONEEND+4},
	{ID_ZONEEND_5,	PTZ_ZONEEND+5},
	{ID_ZONEEND_6,	PTZ_ZONEEND+6},
	{ID_ZONEEND_7,	PTZ_ZONEEND+7},
	{ID_ZONEEND_8,	PTZ_ZONEEND+8},
	{ID_PTZ_ZONESCANON,	PTZ_ZONESCANON},
	{ID_PTZ_ZONESCANOFF, PTZ_ZONESCANOFF},
	{ID_PTZ_PATTERNSTART, PTZ_PATTERNSTART},
	{ID_PTZ_PATTERNSTOP, PTZ_PATTERNSTOP},
	{ID_PTZ_RUNPATTERN,	PTZ_RUNPATTERN},
	{ID_SETZOOMSPEED_0,	PTZ_ZOOMSPEED+0},
	{ID_SETZOOMSPEED_1,	PTZ_ZOOMSPEED+1},
	{ID_SETZOOMSPEED_2,	PTZ_ZOOMSPEED+2},
	{ID_SETZOOMSPEED_3,	PTZ_ZOOMSPEED+3},
	{ID_SETFOCUSSPEED_0, PTZ_FOCUSSPEED+0},
	{ID_SETFOCUSSPEED_1, PTZ_FOCUSSPEED+1},
	{ID_SETFOCUSSPEED_2, PTZ_FOCUSSPEED+2},
	{ID_SETFOCUSSPEED_3, PTZ_FOCUSSPEED+3},
	
	{ID_PTZ_AUTOFOCUSON, PTZ_AUTOFOCUSON },
	{ID_PTZ_AUTOFOCUSOFF, PTZ_AUTOFOCUSOFF },
	{ID_PTZ_AUTOIRISON, PTZ_AUTOIRISON },
	{ID_PTZ_AUTOIRISOFF, PTZ_AUTOIRISOFF },
	{ID_PATTERNSTART_1, PTZ_PATTERNSTART+1 },
	{ID_PATTERNSTART_2, PTZ_PATTERNSTART+2 },
	{ID_PATTERNSTART_3, PTZ_PATTERNSTART+3 },
	{ID_PATTERNSTART_4, PTZ_PATTERNSTART+4 },
	{ID_PATTERNSTART_5, PTZ_PATTERNSTART+5 },
	{ID_PATTERNSTART_6, PTZ_PATTERNSTART+6 },
	{ID_PATTERNSTART_7, PTZ_PATTERNSTART+7 },
	{ID_PATTERNSTART_8, PTZ_PATTERNSTART+8 },
	{ID_RUNPATTERN_1, PTZ_RUNPATTERN+1},
	{ID_RUNPATTERN_2, PTZ_RUNPATTERN+2},
	{ID_RUNPATTERN_3, PTZ_RUNPATTERN+3},
	{ID_RUNPATTERN_4, PTZ_RUNPATTERN+4},
	{ID_RUNPATTERN_5, PTZ_RUNPATTERN+5},
	{ID_RUNPATTERN_6, PTZ_RUNPATTERN+6},
	{ID_RUNPATTERN_7, PTZ_RUNPATTERN+7},
	{ID_RUNPATTERN_8, PTZ_RUNPATTERN+8},

	{0,0}
};

void DvrclientDlg::OnRButtonUp(UINT nFlags, int x, int y)
{
	if( IsWindowVisible(GetDlgItem(m_hWnd, IDC_GROUP_PTZ)) && ::PtInRegion((HRGN)m_ptzcampass_rgn, x, y) ) 
	{
		HMENU hMenu1, hMenu2 ;
		hMenu1 = LoadMenu(AppInst(), MAKEINTRESOURCE(IDR_MENU_POPUP)); 
		hMenu2 = GetSubMenu(hMenu1, 0);

		POINT point;
		point.x=x ;
		point.y=y ;
		ClientToScreen(m_hWnd, &point );
		int cmd=TrackPopupMenu(hMenu2,  TPM_RIGHTBUTTON  | TPM_RETURNCMD ,
			x, y, 
			0, m_hWnd, NULL );
		DestroyMenu(hMenu1);
		if( cmd>0 ) {
			for( int i=0; i<100; i++ ) {
				if(ptz_ext_cmd[i].id==0) break;
				if(ptz_ext_cmd[i].id==cmd) {
					DVR_PTZMsg( ptz_ext_cmd[i].cmd, 0 );
					break;
				}
			}
		}
	}
}

void DvrclientDlg::OnLButtonUp(UINT nFlags, int x, int y)
{
	if( GetCapture()==m_hWnd ) {
		ReleaseCapture();
		m_ptzres[0]='\0';
		InvalidateRect(m_hWnd, &m_ptzcampass_rect, 0);
		// stop camera
		DVR_PTZMsg( 0, 0 );
	}
#ifdef APP_PWPLAYER
	else if( m_companylinkcursor!=NULL ) {
		openhtmldlg( "www.patrolwitness.com", "/" );
	}
#endif
}

BOOL DvrclientDlg::OnMouseMove(UINT nFlags, int x, int y)
{
	if( (nFlags&1) && 
	    GetCapture()==m_hWnd ) {
		Campass( x, y );
		return TRUE;
	}
#ifdef APP_PWPLAYER
    else if( x >= m_companylinkrect.left &&
		x <  m_companylinkrect.right &&
		y >= m_companylinkrect.top &&
		y < m_companylinkrect.bottom )
	{
		if( m_companylinkcursor == NULL ) {
			m_companylinkcursor = (HCURSOR)::SetClassLong(m_hWnd, GCL_HCURSOR, (LONG)LoadCursor(NULL, IDC_HAND ) ) ;
		}
	}
	else if( m_companylinkcursor!=NULL ) {
		::SetClassLong(m_hWnd, GCL_HCURSOR, (LONG)m_companylinkcursor ) ;
		m_companylinkcursor = NULL ;
	}
#endif

    if( m_noupdtime > 0 ) {
        m_noupdtime = 0 ;               // Eanble calender update
    }

#ifdef IDC_BUTTON_CLOSE
	if( nFlags == 0 ) {
//		ShowWindow(GetDlgItem(m_hWnd, IDC_BUTTON_CLOSE), SW_SHOW);
	}
#endif
	return FALSE;
}

BOOL DvrclientDlg::OnMouseWheel(int delta, UINT keys, int x, int y)
{
	POINT pt;
	pt.x = x;
	pt.y = y;
	ScreenToClient(m_hWnd, &pt);
	HWND hChild = GetCapture();
	if (hChild == NULL) {
		hChild = ChildWindowFromPoint(m_hWnd, pt);
	}
	if (hChild && hChild != m_hWnd ) {
		Window *w = Window::getWindow(hChild);
		if ( w != NULL) {
			if( w== m_cliplist )
				SendMessage(hChild, WM_MOUSEWHEEL, MAKELONG(keys, delta), MAKELONG(x, y));
			else {
				for (int s = 0; s < m_screennum; s++) {
					if (w == m_screen[s]) {
						SendMessage(hChild, WM_MOUSEWHEEL, MAKELONG(keys, delta), MAKELONG(x, y));
						break;
					}
				}
			}
			return TRUE;
		}
	}
	return FALSE;
}

void DvrclientDlg::OnCaptureChanged(HWND hWnd)
{
	if( m_ptz_savedcursor!=NULL ) {
		SetCursor(m_ptz_savedcursor);
		m_ptz_savedcursor=NULL ;
	}
}

struct play_button_t {
	int id ;
	TCHAR * img ;
	TCHAR * img_sel ;
} play_buttons [] = {
	{ IDC_PLAY, _T("PLAY_PLAY"), _T("PLAY_PLAY_SEL") },
	{ IDC_PAUSE, _T("PLAY_PAUSE"), _T("PLAY_PAUSE_SEL") },
	{ IDC_SLOW, _T("PLAY_SLOW"), _T("PLAY_SLOW_SEL") },
	{ IDC_BACKWARD, _T("PLAY_BACKWARD"), _T("PLAY_BACKWARD_SEL") },
	{ IDC_FORWARD, _T("PLAY_FORWARD"), _T("PLAY_FORWARD_SEL") },
	{ IDC_FAST, _T("PLAY_FAST"), _T("PLAY_FAST_SEL") },
	{ IDC_STEP, _T("PLAY_STEP"), _T("PLAY_STEP_SEL") },

#ifdef  APP_PW_TABLET
	{ IDC_BUTTON_BDAYRANGE, _T("PW_BDAYRANGE"), _T("PW_BDAYRANGE") },
	{ IDC_BUTTON_BHOURRANGE, _T("PW_BHOURRANGE"), _T("PW_BHOURRANGE") },
	{ IDC_BUTTON_BQUATRANGE, _T("PW_BQUATRANGE"), _T("PW_BQUATRANGE") },
	{ IDC_BUTTON_BCAL, _T("PW_BCAL"), _T("PW_BCAL") },
	{ IDC_BUTTON_BPLAYLIVE, _T("PW_BPLAYLIVE"), _T("PW_BPLAYLIVE") },
	{ IDC_BUTTON_BPLAYBACK, _T("PW_BPLAYBACK"), _T("PW_BPLAYBACK") },
#endif

	{ 0, NULL }
} ;


#ifdef SPARTAN_APP
struct spartan_button_t {
	int id ;
	TCHAR * img ;
} spartan_buttons [] = {
	{ IDC_SPARTAN_1, _T("PLAY_SPARTAN_1") },
	{ IDC_SPARTAN_2, _T("PLAY_SPARTAN_2") },
	{ IDC_SPARTAN_4, _T("PLAY_SPARTAN_4") },
	{ 0, NULL }
} ;
#endif

struct ptz_button_t {
	int id ;
	int msg ;
	TCHAR * img ;
	TCHAR * img_sel ;
} ptz_buttons[] = {
	{ IDC_PTZ_TELE, PTZ_ZOOMTELE, _T("ZOOMTELE_U"),_T("ZOOMTELE_D") },
	{ IDC_PTZ_WIDE, PTZ_ZOOMWIDE, _T("ZOOMWIDE_U"),_T("ZOOMWIDE_D") },
	{ IDC_PTZ_FAR,  PTZ_FOCUSFAR, _T("FOCUSFAR_U"),_T("FOCUSFAR_D") },
	{ IDC_PTZ_NEAR, PTZ_FOCUSNEAR, _T("FOCUSNEAR_U"),_T("FOCUSNEAR_D") },
	{ IDC_PTZ_OPEN, PTZ_IRISOPEN, _T("IRISOPEN_U"),_T("IRISOPEN_D") },
	{ IDC_PTZ_CLOSE, PTZ_IRISCLOSE, _T("IRISCLOSE_U"),_T("IRISCLOSE_D") },
	{0,0,NULL,NULL}
};

struct frontend_button_t {
	int id ;
	TCHAR * img ;
	TCHAR * img_sel ;
	TCHAR * img_hot ;
	TCHAR * img_trans ;
} front_end_buttons [] = {
#ifdef  APP_PWII_FRONTEND 
	{ IDC_BUTTON_TM, _T("PW_TM"), _T("PW_TM_SEL"), _T("PW_TM"), _T("PW_TM") },
	{ IDC_BUTTON_LP, _T("PW_LP"), _T("PW_LP_SEL"), _T("PW_LP"), _T("PW_LP") },
	{ IDC_BUTTON_CAM1, _T("PW_CAM1"), _T("PW_CAM1_SEL"), _T("PW_CAM1_LIGHT"), _T("PW_CAM1_TRANS") },
	{ IDC_BUTTON_CAM2, _T("PW_CAM2"), _T("PW_CAM2_SEL"), _T("PW_CAM2_LIGHT"), _T("PW_CAM2_TRANS") },
#endif
	{0,NULL,NULL,NULL,NULL}
};

void DvrclientDlg::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	int res ;
	int i;
	Graphics * g = new Graphics(lpDrawItemStruct->hDC) ;
	Image * img ;
	ColorMatrix colorMatrix = {
		1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.3f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 1.0f
	};
	if( lpDrawItemStruct->CtlType == ODT_BUTTON ) {
		int w = lpDrawItemStruct->rcItem.right - lpDrawItemStruct->rcItem.left ;
		int h = lpDrawItemStruct->rcItem.bottom - lpDrawItemStruct->rcItem.top ;
        if( m_bkbmp ) {
			POINT pt ;
			pt.x=lpDrawItemStruct->rcItem.left ;
			pt.y=lpDrawItemStruct->rcItem.top ;
			::MapWindowPoints(::GetDlgItem(m_hWnd, nIDCtl), m_hWnd, &pt, 1 );
            pt.x-=m_bottombar.left ;
            pt.y-=m_bottombar.top ;
            g->DrawImage( m_bkbmp, 
                lpDrawItemStruct->rcItem.left, lpDrawItemStruct->rcItem.top,
                pt.x, pt.y, w, h, UnitPixel );
        }
		else {
			Color color ;
			color.SetFromCOLORREF( BOTTOMBARCOLOR );
			Brush * bkbrush = new SolidBrush( color );
			g->FillRectangle( bkbrush, 
				lpDrawItemStruct->rcItem.left, 
				lpDrawItemStruct->rcItem.top, 
				w, 
				h );
			delete bkbrush ;
		}

		int hit = 0 ;
		// test ptz buttons
		if( !hit ) {
			for( i=0; i<20 && ptz_buttons[i].id!=0; i++ ) {
				if( ptz_buttons[i].id == nIDCtl ) {
					RECT rt ;
					::GetClientRect(::GetDlgItem(m_hWnd, nIDCtl), &rt);
					if( lpDrawItemStruct->itemState & ODS_SELECTED ) {
						img = loadbitmap( ptz_buttons[i].img_sel );
						g->DrawImage( img, rt.left, rt.top, rt.right, rt.bottom );
						delete img ;
						res = DVR_PTZMsg( ptz_buttons[i].msg, 0 );
						m_ptzbuttonpressed=1;
					}
					else {
						img = loadbitmap( ptz_buttons[i].img );
						g->DrawImage( img, rt.left, rt.top, rt.right, rt.bottom );
						delete img ;
						if( m_ptzbuttonpressed ) {
							DVR_PTZMsg( 0, 0 );
							m_ptzbuttonpressed=0;
						}
					}
					hit=1;
					break;
				}
			}
		}

		// test player buttons
		if( !hit ) {
			for( i=0; i<20 && play_buttons[i].id!=0; i++ ) {
				if( play_buttons[i].id == nIDCtl ) {
					RECT rt ;
					::GetClientRect(::GetDlgItem(m_hWnd, nIDCtl), &rt);
					if( lpDrawItemStruct->itemState & ODS_SELECTED  ) {	// selected
						img = loadbitmap( play_buttons[i].img_sel );
						g->DrawImage( img, rt.left, rt.top, rt.right, rt.bottom );
//						g->DrawImage( img, 0, 0 );
						delete img ;
					}
					else {
						if(  lpDrawItemStruct->itemState & (ODS_GRAYED|ODS_DISABLED) ) {
    						img = loadbitmap( play_buttons[i].img );
							w = img->GetWidth();
							h = img->GetHeight();
							colorMatrix.m[3][3] = 0.3 ;		// alpha
							ImageAttributes  imageAttributes;
							imageAttributes.SetColorMatrix(
								&colorMatrix, 
								ColorMatrixFlagsDefault,
								ColorAdjustTypeBitmap);
							g->DrawImage(
								img, 
								Rect(rt.left, rt.top, rt.right, rt.bottom),  // destination rectangle 
								0, 0, w, h,        // source rectangle 
								UnitPixel,
								&imageAttributes);
    						delete img ;
						}
						else {		// normal
							img = loadbitmap( play_buttons[i].img );
//							g->DrawImage( img, 0, 0 );
							g->DrawImage( img, rt.left, rt.top, rt.right, rt.bottom );
							delete img ;
						}
					}
					hit=1;
					break;
				}
			}
		}

#ifdef  APP_PWII_FRONTEND 
		if( !hit ) {
			for( i=0; i<20 && front_end_buttons[i].id!=0; i++ ) {
				if( front_end_buttons[i].id == nIDCtl ) {
					RECT rt ;
					::GetClientRect(::GetDlgItem(m_hWnd, nIDCtl), &rt);
					if( lpDrawItemStruct->itemState & ODS_SELECTED  ) {	// selected
						img = loadbitmap( front_end_buttons[i].img_sel );
//						g->DrawImage( img, 0, 0 );
						g->DrawImage( img, rt.left, rt.top, rt.right, rt.bottom );
						delete img ;
                        //OnPWIIButton( nIDCtl, 1 );
					}
					else {
						if(  lpDrawItemStruct->itemState & (ODS_GRAYED|ODS_DISABLED) ) {
    						img = loadbitmap( front_end_buttons[i].img );
							w = img->GetWidth();
							h = img->GetHeight();
							colorMatrix.m[3][3] = 0.3 ;		// alpha
							ImageAttributes  imageAttributes;
							imageAttributes.SetColorMatrix(
								&colorMatrix, 
								ColorMatrixFlagsDefault,
								ColorAdjustTypeBitmap);
							g->DrawImage(
								img, 
								Rect(rt.left, rt.top, rt.right, rt.bottom),  // destination rectangle 
								0, 0, w, h,        // source rectangle 
								UnitPixel,
								&imageAttributes);
    						delete img ;
						}
						else {		// normal
                            //OnPWIIButton( nIDCtl, 0 );
                            img=NULL ;
							string hot ;
							GetDlgItemText(m_hWnd, nIDCtl, hot.tcssize(10), 10 );
							if( ((TCHAR *)hot)[0] == '1' ) {
								// hot 
								hot = front_end_buttons[i].img_hot ;
							}
							else if( ((TCHAR *)hot)[0] == '2' ) {
								// trans
								hot = front_end_buttons[i].img_trans ;
							}
							else {	// no
								hot = front_end_buttons[i].img ;
							}
							img = loadbitmap( hot );
                            if( img ) {
//                                g->DrawImage( img, 0, 0 );
								g->DrawImage( img, rt.left, rt.top, rt.right, rt.bottom );
                                delete img ;
                            }
						}
					}
					hit=1;
					break;
				}
			}
		}
#endif

#ifdef SPARTAN_APP
		// test spartans buttons
		if( !hit ) {
			for( i=0; i<20 && spartan_buttons[i].id!=0; i++ ) {
				if( spartan_buttons[i].id == nIDCtl ) {
					img = loadbitmap( spartan_buttons[i].img );
					if( lpDrawItemStruct->itemState & ODS_SELECTED ) {
						g->DrawImage( img, 0, 0 );
					}
					else {
						w = img->GetWidth();
						h = img->GetHeight();
						colorMatrix.m[3][3] = 0.6 ;		// alpha
						ImageAttributes  imageAttributes;
						imageAttributes.SetColorMatrix(
							&colorMatrix, 
							ColorMatrixFlagsDefault,
							ColorAdjustTypeBitmap);
						g->DrawImage(
							img, 
							Rect(0, 0, w, h),  // destination rectangle 
							0, 0, w, h,        // source rectangle 
							UnitPixel,
							&imageAttributes);
					}
					hit=1;
					break;
				}
			}
		}
#endif
		// other button
		if( !hit ) {
			switch( nIDCtl ) {

			// mixaudio
			case IDC_MIXAUDIO :
				if( g_MixSound ) {
					img = loadbitmap( _T("AUDIO_MIXALL") );
				}
				else {
					img = loadbitmap( _T("AUDIO_MIXSINGLE") );
				}
				if( lpDrawItemStruct->itemState & ODS_SELECTED ) {
					g->DrawImage( img, 1, 1 );
				}
				else {
					g->DrawImage( img, 0, 0 );
				}
				delete img ;
				break;

			// mini track map
			case IDC_MAP :
				if( g_minitrack_up ) {
					img = loadbitmap( _T("MAP_DISABLE") );
				}
				else {
					img = loadbitmap( _T("MAP_ENABLE") );
				}
				if( lpDrawItemStruct->itemState & ODS_SELECTED ) {
					g->DrawImage( img, 1, 1 );
				}
				else {
					g->DrawImage( img, 0, 0 );
				}
				delete img ;
				break;

			default :
				break;
			}
		}
	}
	delete g ;
}

void DvrclientDlg::OnBnClickedPtzSet()
{
	int presetid ;
	presetid=GetDlgItemInt(m_hWnd, IDC_PRESET_ID, NULL, TRUE  );
	if( presetid < 1 || presetid > 255 ) return ;
	DVR_PTZMsg( PTZ_SETPRESET, presetid );
}

void DvrclientDlg::OnBnClickedPtzClear()
{
	int presetid ;
	presetid=GetDlgItemInt(m_hWnd,  IDC_PRESET_ID, NULL, TRUE );
	if( presetid < 1 || presetid > 255 ) return ;
	DVR_PTZMsg( PTZ_CLEARPRESET, presetid );
}

void DvrclientDlg::OnBnClickedPtzGoto()
{
	int presetid ;
	presetid=GetDlgItemInt(m_hWnd, IDC_PRESET_ID, NULL, TRUE );
	if( presetid < 1 || presetid > 255 ) return ;
	DVR_PTZMsg(PTZ_GOTOPRESET, presetid );
}

void DvrclientDlg::OnDestroy()
{
    if( m_cliplist ) {
        delete m_cliplist ;
        m_cliplist=NULL ;
    }

	// save ratio x/y
	reg_save("ratiox", g_ratiox);
	reg_save("ratioy", g_ratioy);

	reg_save( "screenmode", g_screenmode );

	int i;
	for(i=0;i<MAXSCREEN;i++) {
		string chname;
		chname.printf( "screen%d", i );
		if( m_screen[i] ) {
			reg_save( (char *)chname, m_screen[i]->m_channel );
			delete m_screen[i] ;
			m_screen[i]=NULL ;
		}
		else {
			reg_save( (char *)chname, -1 );
		}
	}

	decoder::releaselibrary();
}

void DvrclientDlg::SetPlayTime(dvrtime *dvrt)
{
    SYSTEMTIME st ;
    
    if( dvrt->day != m_playertime.day ||
        dvrt->month != m_playertime.month ||
        dvrt->year != m_playertime.year ) 
    {
        m_tbarupdate = 1 ;
    }

    // don't update time if actual player time is earlier then seeked time
    if( time_compare(*dvrt, m_seektime)>=0 ) {
        m_playertime = *dvrt ;
    }
    else {
        m_playertime = m_seektime ;
    }

    // update calendor
    ::SendDlgItemMessage( m_hWnd, IDC_MONTHCALENDAR, MCM_GETCURSEL, 0, (LPARAM)&st );
    if( st.wYear != m_playertime.year ||
        st.wMonth!=m_playertime.month ||
        st.wDay!=m_playertime.day ) 
    {
        if( m_noupdtime>0 ) {
            m_noupdtime -- ;
        }
        else {
            st.wYear = m_playertime.year ;
            st.wMonth = m_playertime.month ;
            st.wDay = m_playertime.day ;
            st.wHour = 0 ;
            ::SendDlgItemMessage( m_hWnd, IDC_MONTHCALENDAR, MCM_SETCURSEL, TRUE, (LPARAM)&st );
        }
        m_tbarupdate=1 ;
    }

    // update time slider
	if( ::IsWindowVisible( m_slider.getHWND() )  &&
        ::IsWindowEnabled(m_slider.getHWND()) ) 
    {
        int npos ;
        int omin, omax ;
        omin = m_slider.GetRangeMin();
        omax = m_slider.GetRangeMax();
        npos = m_playertime.hour * 3600 + m_playertime.min * 60 + m_playertime.second ;
        if( m_clientmode==CLIENTMODE_PLAYFILE ) {
            // adjust time bar to full file length when play .DVR file
            if(omin != m_slider.m_lowfilepos || omax !=  m_slider.m_highfilepos || m_tbarupdate )
            {
                omin = m_slider.m_lowfilepos ;
                omax = m_slider.m_highfilepos ;
                m_tbarupdate = 1 ;
                SetTimer(m_hWnd, TIMER_UPDATE, 10, NULL );
            }
        }
        else if( npos<omin || npos>omax || m_tbarupdate ) {
            int range = ::SendDlgItemMessage(m_hWnd, IDC_COMBO_TIMERANGE, CB_GETCURSEL, 0, (LPARAM)0);
            if( range==1 ) {									// one hour range
                omax = npos % 3600 ;
                omin = npos - omax ;
                omax = omin + 3599 ;
            }
            else if( range==2 ) {                                   // 15 minutes
                omax = npos % 900 ;
                omin = npos - omax ;
                omax = omin + 899 ;
            }
            else if( range==3 ) {                                   // 5 minutes
                omax = npos % 300 ;
                omin = npos - omax ;
                omax = omin + 299 ;
            }
            else {          										// (include range==0 ) oneday range
                omin = 0 ;
                omax = 24*3600 - 1 ;
            }
            m_tbarupdate = 1 ;
        }
        if( m_tbarupdate ) {
            m_slider.SetRange( omin, omax );

            // also update clip start/end date
/* 
            ::SendDlgItemMessage(m_hWnd, IDC_TIME_SELBEGIN, DTM_GETSYSTEMTIME, 0, (LPARAM)&st);
            st.wDayOfWeek=0 ;
            st.wYear = m_playertime.year ;
            st.wMonth = m_playertime.month ;
            st.wDay = m_playertime.day ;
            ::SendDlgItemMessage(m_hWnd, IDC_TIME_SELBEGIN, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&st);
            ::SendDlgItemMessage(m_hWnd, IDC_TIME_SELEND, DTM_GETSYSTEMTIME, 0, (LPARAM)&st);
            st.wDayOfWeek=0 ;
            st.wYear = m_playertime.year ;
            st.wMonth = m_playertime.month ;
            st.wDay = m_playertime.day ;
            ::SendDlgItemMessage(m_hWnd, IDC_TIME_SELEND, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&st);
*/
        }
        m_slider.SetPos(&m_playertime);
    }
    m_tbarupdate = 0 ;

    // update Display play time
    ::SetDlgItemText(m_hWnd, IDC_STATIC_PLAYTIME, 
		TMPSTR("%04d-%02d-%02d %02d:%02d:%02d",
			m_playertime.year,
			m_playertime.month,
			m_playertime.day,
			m_playertime.hour,
			m_playertime.min,
			m_playertime.second));
}

void DvrclientDlg::OnTimer(UINT_PTR nIDEvent)
{
    SYSTEMTIME st ;
    dvrtime dvrt ;
    memset(&dvrt,0,sizeof(dvrt));
	if( nIDEvent==TIMER_UPDATE ) {
        SetTimer(m_hWnd, TIMER_UPDATE, m_update_time, NULL );
		if(m_clientmode==CLIENTMODE_CLOSE) {
			GetLocalTime(&st);
			SetPlayTime(time_timedvr(&st, &dvrt));
		}
		else if(m_clientmode==CLIENTMODE_LIVEVIEW && g_playstat == PLAY_PLAY ) {
			if( decoder::g_getlivetime(&dvrt)>=0 ) {
	            SetPlayTime(&dvrt)  ;
			}
		}
		else if( (g_playstat != PLAY_STOP) &&
			decoder::g_getcurrenttime(&dvrt)>=0 ) 
        {
            SetPlayTime(&dvrt)  ;
        }
	}
	else if( nIDEvent==TIMER_SEEK ) {
		KillTimer(m_hWnd, TIMER_SEEK);
		Seek();
	}
	else if( nIDEvent==TIMER_STEP ) {
		if( g_playstat==PLAY_PAUSE && GetCapture() == GetDlgItem(m_hWnd, IDC_STEP) ) {
			decoder::g_oneframeforward();
			SetTimer(m_hWnd, TIMER_STEP, 250, NULL);
		}
		else {
			KillTimer(m_hWnd, TIMER_STEP);
		}
	}
	else if( nIDEvent==TIMER_INITMINITRACK ) {
		KillTimer(m_hWnd, TIMER_INITMINITRACK);
		decoder::g_minitrack_init_interface();	// delayed minitrack interface init. (2008-11-25)
    }
    else if( nIDEvent==TIMER_UPDATEMAP ) {
        UpdateMap();
    }
#ifdef  APP_PWII_FRONTEND 
   	else if( nIDEvent==TIMER_RECLIGHT ) {
        int i ;

        int rec1=0, rec2=0 ;
        for(i=0; i<m_screennum ; i++ ) {
            if( m_screen[i] && m_screen[i]->m_decoder && m_screen[i]->isattached() ) {
                int rec = 0 ;

#ifdef APP_PW_TABLET
				string status ;
//				rec = m_screen[i]->m_decoder->getsystemstate(status);
				if( rec>0 ) {
					Graphics * g = new Graphics( m_hWnd, FALSE );
					Gdiplus::Font        font(TEXT("Sans"), 24, FontStyleRegular, UnitPixel);
					Gdiplus::PointF      pointF((float)m_playerrect.left+8.0, (float)m_playerrect.bottom - 350);
					Gdiplus::SolidBrush  solidBrush(Color(255, 255, 255, 255));
					Gdiplus::SolidBrush  stringBrush(Color(255, 0, 0, 255));
					g->FillRectangle( &solidBrush, m_playerrect.left+8, m_playerrect.bottom - 350, 380, 180 );
					g->DrawString(status, -1, &font, pointF, &stringBrush);
					delete g ;
				}
#endif

				rec = m_screen[i]->m_decoder->getrecstate();
#ifdef APP_PW_TABLET
				// Zoom Cam
				int tranrec = rec>>16 ;

				if( (tranrec ^ rec) & 1 ) {
					rec1 = 2 ;		// trans
				}
				else {
					rec1 = rec&1 ;
				}

				if( (tranrec ^ rec) & (1<<4) ) {
					rec2 = 2 ;		// trans
				}
				else {
					rec2 = (rec>>4)&1 ;
				}

#else
                if( rec&1 )
                   rec1 = 1;
                if( rec&2 )
                   rec2 = 1 ;
#endif
                break;
            }
        }

		char *hstr[] = {
			"0", "1", "2" } ;
        string hot ;
		GetDlgItemText(m_hWnd, IDC_BUTTON_CAM1, hot.tcssize(10), 10 );
		if( strcmp( hot, hstr[rec1] )!=0 ) {
			hot = hstr[rec1] ;
			SetDlgItemText(m_hWnd, IDC_BUTTON_CAM1, hot );
		}
		
		GetDlgItemText(m_hWnd, IDC_BUTTON_CAM2, hot.tcssize(10), 10 );
		if( strcmp( hot, hstr[rec2] )!=0 ) {
			hot = hstr[rec2] ;
			SetDlgItemText(m_hWnd, IDC_BUTTON_CAM2, hot );
		}
		        
        SetTimer(m_hWnd, TIMER_RECLIGHT, 1000, NULL );

	}
    else if( nIDEvent==TIMER_AUTOSTART ) {
  		KillTimer(m_hWnd, TIMER_AUTOSTART);
        OnBnClickedButtonPlay();
    }
#endif
    
}


int  DvrclientDlg::getLocation(float &lat, float &lng)
{
	string gpslocstr;
	if (m_screen[m_focus] && m_screen[m_focus]->m_decoder && m_screen[m_focus]->m_decoder->getlocation(gpslocstr.strsize(500), 500) > 0) {
		int l = strlen(gpslocstr);
		if (l > 32) {
			float lati, longi, kmh, direction;
			char  latiD, longiD, cdirection;
			sscanf(((char *)gpslocstr) + 12, "%9f%c%10f%c%f%c%6f",
				&lati, &latiD, &longi, &longiD, &kmh, &cdirection, &direction);
			if (lati > 0.001 && longi > 0.001) {
				if (latiD == 'S') {
					lati = -lati;
				}
				if (longiD == 'W') {
					longi = -longi;
				}
				lat = lati;
				lng = longi;
				return 1;
			}
		}
	}
	return 0;
}


#include "WebWin.h"
static WebWindow * _mapwin = NULL;

void DvrclientDlg::DisplayMap()
{
	HRESULT hr;

	// open the new map window
	if (_mapwin && _mapwin->getHWND() == NULL) {
		delete _mapwin;
		_mapwin = NULL;
	}

	if (_mapwin == NULL) {
		string url;
		string exefilename;

		GetModuleFileName(AppInst(), exefilename.tcssize(512), 512);

		if (reg_read("map") == 1) {
			url.printf("res://%s/MAPBING_HTML", (char *)exefilename);
		}
		else {
			url.printf("res://%s/MAPGOOGLE_HTML", (char *)exefilename);
		}

		// for testing
		// url = "file:///D:\\proj\\DVR\\DVRCLIENT_V3\\Viewers\\common\\map.html";
		// url = "http://html5test.com/";


		_mapwin = new WebWindow();

		float lat, lng;
		if (getLocation( lat, lng)) {
			_mapwin->setInitloc(lat, lng);
		}

		_mapwin->DisplayPage(url);
	}
	else {
		_mapwin->show();
	}

	::SetTimer(m_hWnd, TIMER_UPDATEMAP, 100, NULL);		// start map update timer
	m_lati = m_longi = 0.0;        // reset saved location, so map would update next time

}

void DvrclientDlg::ReleaseMap() 
{
	if (_mapwin) {
		delete _mapwin;
		_mapwin = NULL;
	}
}

void DvrclientDlg::UpdateMap()
{
	if (m_updateLocation || _mapwin) {
		// Show GPS location
		float lati, longi;
		if (getLocation( lati, longi )){
			if (m_lati != lati || m_longi != longi) {
				m_lati = lati; m_longi = longi;
				if (_mapwin) {
					if (_mapwin->getHWND() == NULL) {
						delete _mapwin;
						_mapwin = NULL;
					}
					else if (_mapwin->ExecScript(TMPSTR("setpos(%.6f,%06f)", lati, longi))) {
						::SetTimer(m_hWnd, TIMER_UPDATEMAP, 2000, NULL);		// start map update timer
					}
				}
#ifdef WMVIEWER_APP
				::SetDlgItemText(m_hWnd, IDC_EDIT_LOCATION, TMPSTR("%.6f,%.6f", lati, longi));
#endif
			}
		}

#ifdef WMVIEWER_APP
		else {
			// clean gps location text
			::SetDlgItemText(m_hWnd, IDC_EDIT_LOCATION, _T(""));
			m_lati = -200.0;
		}
#endif

	}
	else {
		::KillTimer(m_hWnd, TIMER_UPDATEMAP);
	}
}



class MapWindow : public Window
{
protected:
	virtual LRESULT OnDestroy() {
		return TRUE ;
	}
    
	virtual LRESULT WndProc(UINT message, WPARAM wParam, LPARAM lParam) {
		int res = 0 ;
		switch( message ) {
		case WM_DESTROY :
			res = OnDestroy();
			break;
		default:
			res = DefWndProc(message, wParam, lParam);
			break;
		}
        return res;
    }
};

static MapWindow * mapwin ;

void DvrclientDlg::DisplayMap_x()
{
    HRESULT hr ;
    if( m_mapWindow ) {
        VARIANT_BOOL vb ;
        hr=m_mapWindow->get_closed(&vb);
        if( hr==S_OK && vb==VARIANT_FALSE ) {
            return ;
        }
        m_mapWindow->Release();
        m_mapWindow=NULL ;
    }

    // map window is closed or not opened

    // open the new map window
    if (SUCCEEDED(OleInitialize(NULL)))
    {

        HINSTANCE hinstMSHTML = LoadLibrary(TEXT("MSHTML.DLL"));

        typedef HRESULT STDAPICALLTYPE SHOWMODELESSHTMLDIALOG_FN (HWND hwndParent, IMoniker *pmk, VARIANT *pvarArgIn, VARIANT* pvarOptions, IHTMLWindow2 ** ppWindow);

        if (hinstMSHTML ){
            string strShowModellessHTML="ShowModelessHTMLDialog";
            SHOWMODELESSHTMLDIALOG_FN * pfnShowModelessHTMLDialog;
            pfnShowModelessHTMLDialog =
                (SHOWMODELESSHTMLDIALOG_FN*)GetProcAddress(hinstMSHTML, strShowModellessHTML);
            if (pfnShowModelessHTMLDialog)
            {
                string url ;
                string exefilename;

                GetModuleFileName(AppInst(), exefilename.tcssize(512), 512); 
				
				if( reg_read("map") == 1 ) {
					url.printf( "res://%s/MAP_HTML", (char *)exefilename);
				}
				else {
					url.printf( "res://%s/MAPBING_HTML", (char *)exefilename);
				}
				// for testing
				url = "file:///D:\\proj\\DVR\\DVRCLIENT_V3\\Viewers\\common\\mapbing.html";

				url = "http://html5test.com/";


                IMoniker *pURLMoniker;
				IUri *pUri;
				BSTR bstrURL = SysAllocString(url);

				CreateUri(url, Uri_CREATE_CANONICALIZE, 0, &pUri);


                //CreateURLMoniker(NULL, bstrURL, &pURLMoniker);
				//CreateURLMonikerEx(NULL, bstrURL, &pURLMoniker, URL_MK_UNIFORM);
				CreateURLMonikerEx2(NULL, pUri, &pURLMoniker, URL_MK_UNIFORM);

                if (pURLMoniker)
                {
                    VARIANT argument ;
                    VariantInit(&argument);
                    argument.vt = VT_BSTR ;
                    argument.bstrVal = NULL ;

                    string gpslocation ;
                    if( m_screen[m_focus]->m_decoder && m_screen[m_focus]->m_decoder->getlocation( gpslocation.strsize(200), 200)>0 ) {
                        int l=strlen(gpslocation);
                        if( l>32 ) {
                            float lati,longi,kmh,direction;
                            char  latiD, longiD, cdirection; 
                            sscanf( ((char *)gpslocation)+12, "%9f%c%10f%c%f%c%6f", 
                                &lati, &latiD, &longi, &longiD, &kmh, &cdirection, &direction );
                            if( lati>1.0 || longi>1.0 ) {
                                if( latiD=='S') {
                                    lati=-lati;
                                }
                                if( longiD=='W' ) {
                                    longi=-longi ;
                                }
                                string arg ;
								arg.printf( "{\"lati\":\"%f\",\"longi\":\"%f\"}", lati, longi );
                                argument.bstrVal = SysAllocString( arg );
                            }
                        }
                    }
                    if( argument.bstrVal == NULL ) {
                        argument.bstrVal = SysAllocString( _T("{\"lati\":\"40.757701\",\"longi\":\"-73.985766\"}") );
                    }

                    VARIANT option;
                    VariantInit(&option);
                    option.vt = VT_BSTR ;
					option.bstrVal = (BSTR)SysAllocString(_T("dialogHeight:12cm;dialogWidth:12cm;resizable:yes;"));
                    hr=(*pfnShowModelessHTMLDialog)(getHWND(), pURLMoniker, &argument,
                        &option, &m_mapWindow);

                    pURLMoniker->Release();
                    VariantClear(&option);
                    VariantClear(&argument);
                    if( m_mapWindow ) {

						/*  Test map on child window
                                
                        IHTMLDocument2 * doc ;
                        hr=m_mapWindow->get_document(&doc);
                        HWND hmap = GetLastActivePopup( m_hWnd );
                        if( hmap ) {
							doc->Release();
                            LONG lStyle = GetWindowLong(hmap, GWL_STYLE);
                            lStyle |= WS_CHILDWINDOW;        //remove the CHILD style
                            lStyle &= ~WS_CAPTION;            //Add the POPUP style
                            lStyle &= ~WS_POPUP;            //Add the POPUP style
                            lStyle |= WS_VISIBLE;        //Add the VISIBLE style
                            lStyle &= ~WS_SYSMENU;        //Add the SYSMENU to have a close button
                        
                            //Now set the Modified Window Style
                            SetWindowLong(hmap, GWL_STYLE, lStyle);  
                            SetParent(hmap, m_hWnd );
//                          MoveWindow(hmap, 100, 100, 200, 200, 1 );

							if( mapwin==NULL ) {
								mapwin = new MapWindow ;
							}

							mapwin->attach( hmap );

                        }


						*/

						/*
                        if( hr==S_OK && doc!=NULL ) {
                            int retry ;
                            BSTR rdy = NULL ;
                            for( retry=0; retry<100; retry++) {
                                hr = doc->get_readyState( &rdy );
                                if( rdy && wcscmp(rdy, _T("complete"))==0 ) {
                                    break ;
                                }
                                Sleep(30);
                            }
                            if( rdy ) {
                                SysFreeString(rdy);
                            }
                            doc->Release();
                        }
						*/

                        ::SetTimer(m_hWnd, TIMER_UPDATEMAP, 2000, NULL );		// start map update timer
                        m_lati = m_longi = 0.0 ;        // reset saved location, so map would update next time
                    }
                }
                SysFreeString(bstrURL);
            }

            FreeLibrary(hinstMSHTML);
        }
    }
    return ;
}


void DvrclientDlg::UpdateMap_x()
{
	static int maperror = 0;

	HRESULT hr;
	int nogps = 1;
	if (m_updateLocation || m_mapWindow) {
		// Show GPS location
		if (m_screen[m_focus] && m_screen[m_focus]->m_decoder) {
			char gpslocation[300];
			if (m_screen[m_focus]->m_decoder->getlocation(gpslocation, 299)>0) {
				int l = strlen(gpslocation);
				if (l>32) {
					float lati, longi, kmh, direction;
					char  latiD, longiD, cdirection;
					sscanf(((char *)gpslocation) + 12, "%9f%c%10f%c%f%c%6f",
						&lati, &latiD, &longi, &longiD, &kmh, &cdirection, &direction);
					if (lati>1.0 || longi>1.0) {
						if (latiD == 'S') {
							lati = -lati;
						}
						if (longiD == 'W') {
							longi = -longi;
						}
						nogps = 0;
						if (m_lati != lati || m_longi != longi) {
							m_lati = lati; m_longi = longi;
							if (m_mapWindow) {
								// check map window is running
								IHTMLDocument2 * doc;
								hr = m_mapWindow->get_document(&doc);
								if (hr == S_OK && doc != NULL) {
									BSTR rdy = NULL;
									hr = doc->get_readyState(&rdy);
									if (rdy && wcscmp(rdy, _T("complete")) == 0) {
										VARIANT var;
										string script;
										var.vt = VT_EMPTY;
										script.printf("setvalue(%.6f,%06f)", lati, longi);
										hr = m_mapWindow->execScript(SysAllocString(script), SysAllocString(_T("JavaScript")), &var);
										if (hr == S_OK) {
											maperror = 0;
										}
									}
									if (rdy) {
										SysFreeString(rdy);
									}
									hr = doc->Release();
								}
								else {
									VARIANT var;
									string script;
									var.vt = VT_EMPTY;
									script.printf("setvalue(%.6f,%06f)", lati, longi);
									hr = m_mapWindow->execScript(SysAllocString(script), SysAllocString(_T("JavaScript")), &var);
									if (hr == S_OK) {
										maperror = 0;
									}
								}
								if (maperror++>3) {
									maperror = 0;
									m_mapWindow->Release();
									m_mapWindow = NULL;
								}
							}
#ifdef WMVIEWER_APP
							::SetDlgItemText(m_hWnd, IDC_EDIT_LOCATION, TMPSTR("%.6f,%.6f", lati, longi));
#endif
						}
					}
				}
			}
		}
	}
	else {
		::KillTimer(m_hWnd, TIMER_UPDATEMAP);
	}
#ifdef WMVIEWER_APP
	if (nogps && m_lati>-180) {
		// clean gps location text
		::SetDlgItemText(m_hWnd, IDC_EDIT_LOCATION, _T(""));
		m_lati = -200.0;
	}
#endif
}




void DvrclientDlg::OnCbnSelchangeComboTimerange()
{
    m_tbarupdate = 1 ;
    SetTimer(m_hWnd, TIMER_UPDATE, 10, NULL);
}

BOOL DvrclientDlg::OnEraseBkgnd(HDC hDC)
{
    // don't erase bkgnd
	return TRUE ;
}

LRESULT DvrclientDlg::OnMixerMessage( WPARAM wParam, LPARAM lParam )
{
	DWORD volume ;
	waveOutGetVolume( (HWAVEOUT)m_waveoutId, &volume);
	volume = (volume&0x0ffff)/655 ;
	if( volume != m_vvolume ) {
		// update volume control
		m_vvolume = volume ;
		m_volume.SetPos( m_vvolume );
        HWND hbox = ::GetDlgItem(m_hWnd, IDC_PIC_SPEAKER_VOLUME) ;
        if( hbox ) {
			RECT sprect ;
            ::GetClientRect( hbox, &sprect );
            ::MapWindowPoints( hbox, m_hWnd, (LPPOINT)&sprect, 2 );
            InvalidateRect(m_hWnd, &sprect, FALSE);
        }
	}
	return 0;
}

void DvrclientDlg::OnBnClickedMap()
{
	if( g_minitrack_up ) {
		decoder::g_minitrack_stop();
	}
	else {
		if( m_screen[m_focus] && m_screen[m_focus]->m_decoder ) {
			decoder::g_minitrack_start(m_screen[m_focus]->m_decoder);	// start with decoder running
		}
		else {
			decoder::g_minitrack_start(NULL);				// start with no decoder running
		}
	}
}

#ifndef APP_PWII_FRONTEND 

void DvrclientDlg::OnBnClickedButtonPlay()
{
	HMENU hMenu1, hMenu2 ;
	RECT winrect ;
	hMenu1 = ::LoadMenu(ResInst(), MAKEINTRESOURCE(IDR_MENU_POPUP)); 
	hMenu2 = ::GetSubMenu(hMenu1, 3);

#ifdef APP_TVS
	if( g_usertype == IDINSTALLER ) {
		DeleteMenu( hMenu2, ID_PLAYSOURCE_PLAYARCH, MF_BYCOMMAND);
		DeleteMenu( hMenu2, ID_PLAYSOURCE_PLAYFILE, MF_BYCOMMAND);
	}
	else if( g_usertype == IDINSPECTOR ) {
		DeleteMenu( hMenu2, ID_PLAYSOURCE_PLAYARCH, MF_BYCOMMAND);
		DeleteMenu( hMenu2, ID_PLAYSOURCE_PLAYFILE, MF_BYCOMMAND);
	}
#endif


#ifdef NOPLAYARCHIVE
	DeleteMenu( hMenu2, ID_PLAYSOURCE_PLAYARCH, MF_BYCOMMAND);
	DeleteMenu( hMenu2, ID_PLAYSOURCE_PLAYFILE, MF_BYCOMMAND);
#endif

#ifdef NOPLAYLIVE
	DeleteMenu( hMenu2, ID_PLAYSOURCE_LIVEVIEW, MF_BYCOMMAND);
#endif

#ifdef NOPLAYSMARTSERVER
	DeleteMenu( hMenu2, ID_PLAYSOURCE_PLAYSMARTSERVER, MF_BYCOMMAND);
#endif

	::GetWindowRect(::GetDlgItem(m_hWnd,IDC_BUTTON_PLAY), &winrect) ;
	int cmd=TrackPopupMenu(hMenu2,  TPM_RIGHTBUTTON  | TPM_RETURNCMD ,
		winrect.left, winrect.bottom+1, 
		0, m_hWnd, NULL );
	DestroyMenu(hMenu1);
	switch (cmd) {
		case ID_PLAYSOURCE_LIVEVIEW :
			OnBnClickedPreview();
			break;
		case ID_PLAYSOURCE_PLAYARCH :
			OnBnClickedPlayback();
			break;
		case ID_PLAYSOURCE_PLAYFILE :
			OnBnClickedPlayfile();
			break;
		case ID_PLAYSOURCE_PLAYSMARTSERVER :
			OnBnClickedSmartserver();
			break;
	}
}

#else

// for PW front end
void DvrclientDlg::OnBnClickedButtonPlay()
{
	int i;
    int res ;
    string modname ;
    char dvrname[MAX_PATH] ;
    string dvrserverfile = "dvrserver" ;
    if( getfilepath(dvrserverfile) ) {
        FILE * fsvr = fopen( dvrserverfile, "r" );
        dvrname[0]=0;
        if( fsvr ) {
            fscanf(fsvr, "%s", dvrname);
            fclose( fsvr );
        }
        // check current state

		// save previous open channel state
		int x_channel[MAXSCREEN] ;
		for( i=0; i<MAXSCREEN; i++ ) {
			if( m_screen[i] && m_screen[i]->m_decoder == g_decoder && m_screen[i]->m_channel >= 0 ) {
				x_channel[i] = m_screen[i]->m_channel ;
				 m_screen[i]->m_xchannel = m_screen[i]->m_channel ;
			}
			else {
				x_channel[i] = -1 ;
			}
		}

        if( m_clientmode != CLIENTMODE_LIVEVIEW ) {
            // start auto live view
            OpenDvr(PLY_PREVIEW, CLIENTMODE_LIVEVIEW, TRUE, dvrname);
        }
        else {
            // start auto play back
            OpenDvr(PLY_PLAYBACK, CLIENTMODE_PLAYBACK, TRUE, dvrname);
        }

		// try restore previous channels
//		for( i=0; i<MAXSCREEN; i++ ) {
//			if( m_screen[i] && m_screen[i]->isVisible() && x_channel[i]>=0 ) {
//				m_screen[i]->AttachDecoder( g_decoder, x_channel[i] );
//			}
//		}

    }
}

#endif      // PW_FRONTEND

void DvrclientDlg::OnHScroll(UINT nSBCode, UINT nPos, HANDLE hScrollBar)
{
	if( hScrollBar == m_slider.getHWND() ) {
        if( nSBCode != TB_THUMBPOSITION && nSBCode != TB_ENDTRACK ) {
            SetTimer(m_hWnd, TIMER_SEEK, 20, NULL );
            SetTimer(m_hWnd, TIMER_UPDATE, 2000, NULL );    // delay TIMER_UPDATE
        }
    }
    else if( hScrollBar == m_volume.getHWND() ) {
        SetVolume();
    }
}

#if (defined(DVRVIEWER_APP) ||defined(SPARTAN_APP) )

#include "capture.h"

void DvrclientDlg::OnBnClickedCapture()
{
    string capturefilename ;
    string deffilename ;
    string tmpdir = getenv("TEMP");
	struct dvrtime dvrt ;
    dvrt = m_playertime ;       // default time
	decoder * dec = NULL ;

	if( strlen(tmpdir)==0 )
		tmpdir=getenv("TMP");
	if( strlen(tmpdir)==0 )
		tmpdir=".";
	capturefilename.printf( "%s\\dvrcap.bmp", (char*)tmpdir );

	int res=DVR_ERROR ;

#ifdef CAPTUREALL
	int i;
	CImage  * capimg ;
	int capfiles ;
	string homedir = getenv("USERPROFILE");
	if( strlen(homedir)<=0 ) {
		homedir="C:";
	}

	deffilename.printf("%s\\DVR_Snap_Shot", (LPCSTR)homedir );
	_mkdir( deffilename );
	capfiles = 0 ;
	for( i=0; i<MAXSCREEN; i++ ) {
		if(m_screen[i]!=NULL && m_screen[i]->m_decoder && m_screen[i]->m_visible ) {
			WaitCursor waitcursor ;
			res=m_screen[i]->m_decoder->capture( m_screen[i]->m_channel, capturefilename );
			if( res>=0 ) {
				capimg = new CImage ;
				capimg->Load( (LPCTSTR)capturefilename );
				
				if( dec!=m_screen[i]->m_decoder ) {
					dec = m_screen[i]->m_decoder ;
					dec->getcurrenttime( &dvrt );
				}

				sprintf(deffilename.strsize(MAX_PATH),"%s\\DVR_Snap_Shot\\DVR_%s_%s_%04d%02d%02d_%02d%02d%02d.JPG", 
					(LPCSTR)homedir, m_screen[i]->m_decoder->m_playerinfo.servername,
					m_screen[i]->m_channelinfo.cameraname,
					dvrt.year, dvrt.month, dvrt.day,
					dvrt.hour, dvrt.min, dvrt.second 
					);

				if( capimg->Save( (LPCTSTR)deffilename )==S_OK ) {
					capfiles++;
				}
				delete capimg ;
				remove(capturefilename);
			}
		}
	}
	if( capfiles>0 ) {
		sprintf(capturefilename.strsize(MAX_PATH),"%d image(s) captured and saved on \"%s\\DVR_Snap_Shot\".", capfiles, (LPCSTR)homedir );
		MessageBox((LPCTSTR)capturefilename );
	}

#else
    if( m_cliplist && m_cliplist->getHWND() != NULL ) {
        res = ((Cliplist *)m_cliplist)->getClipname( capturefilename );
         ((Cliplist *)m_cliplist)->getClipTime( &dvrt );
    }
	else if(m_screen[m_focus] && m_screen[m_focus]->m_decoder) {
		res=m_screen[m_focus]->m_decoder->capture( 
			m_screen[m_focus]->m_channel, capturefilename );
        m_screen[m_focus]->m_decoder->getcurrenttime( &dvrt );
	}
	if( res>=0 ) {
/*	
        CCapture capdlg ;
		capdlg.m_picturename=capturefilename;
		m_screen[m_focus]->m_decoder->getcurrenttime( &dvrt );
		sprintf(deffilename.strsize(MAX_PATH),"DVR_%s_%s_%04d%02d%02d_%02d%02d%02d.JPG", 
					m_screen[m_focus]->m_decoder->m_playerinfo.servername,
					m_screen[m_focus]->m_channelinfo.cameraname,
					dvrt.year, dvrt.month, dvrt.day,
					dvrt.hour, dvrt.min, dvrt.second 
					);
		capdlg.m_filename=deffilename;
		capdlg.DoModal();
		remove(capturefilename);
*/
        Capture capdlg ;
		capdlg.m_picturename=capturefilename;

//		sprintf(deffilename.strsize(MAX_PATH),"DVR_%s_%s_%04d%02d%02d_%02d%02d%02d.JPG", 
//					m_screen[m_focus]->m_decoder->m_playerinfo.servername,
//					m_screen[m_focus]->m_channelinfo.cameraname,
//					dvrt.year, dvrt.month, dvrt.day,
//					dvrt.hour, dvrt.min, dvrt.second 
//					);
		deffilename.printf("%02d%02d%04d_%02d%02d%02d_%s_%s.JPG",
					dvrt.month, dvrt.day, dvrt.year, 
					dvrt.hour, dvrt.min, dvrt.second,
					m_screen[m_focus]->m_decoder->m_playerinfo.servername,
					m_screen[m_focus]->m_channelinfo.cameraname
					);

        capdlg.m_filename=deffilename;
		capdlg.DoModal(IDD_DIALOG_CAPTURE,m_hWnd);
		remove(capturefilename);
    }
	else {
		MessageBox(m_hWnd, _T("No picture captured!"), NULL, MB_OK);
	}
#endif
}

#endif      // SPARTAN_APP || DVRVIEWER_APP


#ifdef APP_TVS

#include "../common/Capture4.h"

#define MAX_CAPTURE 4
void DvrclientDlg::OnBnClickedCapture()
{
    string capturefilename[MAX_CAPTURE] ;
	char * tmp = getenv("TEMP");
	char * homepath = getenv("USERPROFILE");
	struct dvrtime dvrt ;
	decoder * dec = NULL ;

	if( tmp==NULL )
		tmp=getenv("TMP");
	if( tmp==NULL )
		tmp=".";

#ifdef TVS_SNAPSHOT2

	int i, j, res;
	int highestCamID = 0; // 0, 1, 2, 3

	Capture4 capdlg ;
	capdlg.m_picturenumber = 0 ;

	// capture each available screens
	for (i = 0, j = 0; i < MAXSCREEN && j < MAX_CAPTURE; i++) {
		if(m_screen[i] && m_screen[i]->m_decoder && m_screen[i]->isattached() ){
            m_screen[i]->m_decoder->getcurrenttime( &dvrt );
			string capturefile;
            sprintf(capturefile.strsize(MAX_PATH),"%s\\dvrcap%d.bmp", tmp, i + 1 );
			res=m_screen[i]->m_decoder->capture( 
				m_screen[i]->m_channel, capturefile);

			if (res >= 0) {
				capdlg.m_picturename[j]= capturefile ;
				capdlg.m_cameraname[j] = m_screen[i]->m_channelinfo.cameraname ;
				capdlg.m_picturenumber++;
                highestCamID = i ;
//				if (m_screen[i]->m_channel > highestCamID) {
//					highestCamID = m_screen[i]->m_channel;
//				}
				j++;
			}
		}
	}

	if (capdlg.m_picturenumber>0) {
		m_screen[highestCamID]->m_decoder->getcurrenttime( &dvrt );
        sprintf(capdlg.m_filename.strsize(MAX_PATH),"%s_%s_%04d%02d%02d_%02d%02d%02d.JPG", 
            (LPCSTR)m_appname,
            m_screen[highestCamID]->m_decoder->m_playerinfo.servername,
            dvrt.year, dvrt.month, dvrt.day,
            dvrt.hour, dvrt.min, dvrt.second 
            );
		capdlg.m_capturetime = dvrt ;
//		capdlg.m_highestCamID = highestCamID;
		capdlg.ptvs_info = (struct tvs_info *)m_screen[highestCamID]->m_decoder->m_playerinfo.serverinfo ;
		capdlg.DoModal(IDD_DIALOG_CAPTURE, this->m_hWnd);
	} else {
		MessageBox(m_hWnd, _T("No picture captured!"),NULL,MB_OK);
	}

	for (i = 0; i < MAX_CAPTURE; i++) {
		if (!capdlg.m_picturename[i].isempty()) {
			remove(capdlg.m_picturename[i]);
		}
	}
#if 0
	if( res1>=0 || res2>=0 ) {
		int infoch ;
		CCapture capdlg ;
		capdlg.m_picturenumber = 0 ;
		if( res2 >= 0 ) {
			infoch = 1 ;
			capdlg.m_picturename[1]=capturefilename2;
			capdlg.m_cameraname[1] = m_screen[1]->m_channelinfo.cameraname ;

			capdlg.m_picturenumber++;
			if (m_screen[1]->m_channel > highestCamID) {
				highestCamID = m_screen[1]->m_channel;
			}
		}
		if( res1 >= 0 ) {
			infoch = 0 ;
			capdlg.m_picturename[0]=capturefilename1;
			capdlg.m_cameraname[0] = m_screen[0]->m_channelinfo.cameraname ;
			capdlg.m_picturenumber++;
			if (m_screen[0]->m_channel > highestCamID) {
				highestCamID = m_screen[0]->m_channel;
			}
		}
		m_screen[infoch]->m_decoder->getcurrenttime( &dvrt );
        sprintf(savefilename,"TVS_%s_%04d%02d%02d_%02d%02d%02d.JPG", 
					m_screen[infoch]->m_decoder->m_playerinfo.servername,
					dvrt.year, dvrt.month, dvrt.day,
					dvrt.hour, dvrt.min, dvrt.second 
					);
        capdlg.m_filename=savefilename;
		capdlg.m_capturetime = dvrt ;
		capdlg.ptvs_info = (struct tvs_info *)m_screen[infoch]->m_decoder->m_playerinfo.serverinfo ;
		capdlg.DoModal();
		remove(capturefilename1);
		remove(capturefilename2);
	}
	else {
		MessageBox(_T("No picture captured!"));
	}
#endif
#else

	sprintf(capturefilename1,"%s\\dvrcap.bmp", tmp );

	if(m_screen[m_focus] && m_screen[m_focus]->m_decoder) {
		res=m_screen[m_focus]->m_decoder->capture( 
			m_screen[m_focus]->m_channel, capturefilename1 );
	}
	if( res>=0 ) {
		CCapture capdlg ;
		capdlg.m_picturenumber = 1 ;
		capdlg.m_picturename[0]=capturefilename1;
		m_screen[m_focus]->m_decoder->getcurrenttime( &dvrt );
/*
sprintf_s(capturefilename1,MAX_PATH,"DVR_%s_%s_%04d%02d%02d_%02d%02d%02d.JPG", 
					m_screen[m_focus]->m_decoder->m_playerinfo.servername,
					m_screen[m_focus]->m_channelinfo.cameraname,
					dvrt.year, dvrt.month, dvrt.day,
					dvrt.hour, dvrt.min, dvrt.second 
					);
*/
        sprintf(savefilename,"DVR_%s_%s_%04d%02d%02d_%02d%02d%02d.JPG", 
					m_screen[m_focus]->m_decoder->m_playerinfo.servername,
					m_screen[m_focus]->m_channelinfo.cameraname,
					dvrt.year, dvrt.month, dvrt.day,
					dvrt.hour, dvrt.min, dvrt.second 
					);
        capdlg.m_filename=savefilename;
		capdlg.m_cameraname = m_screen[m_focus]->m_channelinfo.cameraname ;
		capdlg.m_capturetime = dvrt ;
		capdlg.ptvs_info = (struct tvs_info *)m_screen[m_focus]->m_decoder->m_playerinfo.serverinfo ;
		capdlg.DoModal();
		remove(capturefilename1);
	}
	else {
		MessageBox(_T("No picture captured!"));
	}
#endif	// TVS_DUALPICTURE
}

#include <dbt.h>

BOOL DvrclientDlg::OnDeviceChange( UINT nEventType, DWORD dwData )
{
    PDEV_BROADCAST_HDR lpdb = (PDEV_BROADCAST_HDR)dwData;
    if( nEventType == DBT_DEVICEREMOVECOMPLETE ) {
        if (lpdb -> dbch_devicetype == DBT_DEVTYP_VOLUME)
        {
			if( tvs_checktvskey() == 0 ) {
				// key removed?
				ShowWindow(m_hWnd, SW_HIDE);
				ClosePlayer();
				MessageBox(m_hWnd, _T("USB Key removed, application Quit!"), NULL, MB_OK );
				EndDialog(IDCANCEL);
			}
		}
	}
   return TRUE;
}

void DvrclientDlg::OnBnClickedButtonTvsinfo()
{
	HMENU hMenu1, hMenu2 ;
	RECT winrect ;
	hMenu1 = LoadMenu(ResInst(), MAKEINTRESOURCE(IDR_MENU_POPUP)); 
	hMenu2 = GetSubMenu(hMenu1, 4);

#ifdef APP_TVS
	if( g_usertype == IDADMIN ) {		// allow all
		;
	}
	else if( g_usertype == IDINSTALLER ) {
		DeleteMenu( hMenu2, ID_TVSINFORMATION_ACCESSHISTORY, MF_BYCOMMAND);
	}
	else if( g_usertype == IDPOLICE ) {
		DeleteMenu( hMenu2, ID_TVSINFORMATION_CONFIGURATIONREPORT, MF_BYCOMMAND);
		DeleteMenu( hMenu2, ID_TVSINFORMATION_SYSTEMLOG, MF_BYCOMMAND);
	}
	else if( g_usertype == IDOPERATOR ) {
		DeleteMenu( hMenu2, ID_TVSINFORMATION_SYSTEMLOG, MF_BYCOMMAND);
		DeleteMenu( hMenu2, ID_TVSINFORMATION_ACCESSHISTORY, MF_BYCOMMAND);
	}
	else if( g_usertype == IDINSPECTOR ) {
		DeleteMenu( hMenu2, ID_TVSINFORMATION_SYSTEMLOG, MF_BYCOMMAND);
		DeleteMenu( hMenu2, ID_TVSINFORMATION_ACCESSHISTORY, MF_BYCOMMAND);
	}
	else {
		DeleteMenu( hMenu2, ID_TVSINFORMATION_ACCESSHISTORY, MF_BYCOMMAND);
		DeleteMenu( hMenu2, ID_TVSINFORMATION_CONFIGURATIONREPORT, MF_BYCOMMAND);
		DeleteMenu( hMenu2, ID_TVSINFORMATION_SYSTEMLOG, MF_BYCOMMAND);
	}
#endif

	GetWindowRect(GetDlgItem(m_hWnd, IDC_BUTTON_TVSINFO),&winrect) ;
	int cmd=TrackPopupMenu(hMenu2,  TPM_RIGHTBUTTON  | TPM_RETURNCMD ,
		winrect.left, winrect.bottom+1, 
		0, m_hWnd, NULL );
	DestroyMenu(hMenu1);
	switch (cmd) {
		case ID_TVSINFORMATION_ACCESSHISTORY :
			if( (m_clientmode == CLIENTMODE_LIVEVIEW || m_clientmode == CLIENTMODE_PLAYBACK) &&
			m_screen[m_focus] && 
			m_screen[m_focus]->m_decoder ) 
			{
				openhtmldlg( m_screen[m_focus]->m_decoder->m_playerinfo.serverinfo, "/tvslog.html" );
			}	
			break;
		case ID_TVSINFORMATION_CONFIGURATIONREPORT :
			if( (m_clientmode == CLIENTMODE_LIVEVIEW || m_clientmode == CLIENTMODE_PLAYBACK) &&
			m_screen[m_focus] && 
			m_screen[m_focus]->m_decoder ) 
			{
				openhtmldlg( m_screen[m_focus]->m_decoder->m_playerinfo.serverinfo, "/cfgreport.html" );
			}	
			break;
		case ID_TVSINFORMATION_SYSTEMLOG :
			if( (m_clientmode == CLIENTMODE_LIVEVIEW || m_clientmode == CLIENTMODE_PLAYBACK) &&
			m_screen[m_focus] && 
			m_screen[m_focus]->m_decoder ) 
			{
				openhtmldlg( m_screen[m_focus]->m_decoder->m_playerinfo.serverinfo, "/dvrlog.html" );
			}	
			break;
#ifdef ID_TVSINFORMATION_GSENSORLOG
        case ID_TVSINFORMATION_GSENSORLOG :
            if( (m_clientmode == CLIENTMODE_LIVEVIEW || m_clientmode == CLIENTMODE_PLAYBACK) &&
			m_screen[m_focus] && 
			m_screen[m_focus]->m_decoder ) 
			{
				openhtmldlg( m_screen[m_focus]->m_decoder->m_playerinfo.serverinfo, "/gsensorlog.html" );
			}	
            break ;
#endif  // ID_TVSINFORMATION_GSENSORLOG
#ifdef ID_TVSINFORMATION_GPSLOG
        case ID_TVSINFORMATION_GPSLOG :
            if( (m_clientmode == CLIENTMODE_LIVEVIEW || m_clientmode == CLIENTMODE_PLAYBACK) &&
			m_screen[m_focus] && 
			m_screen[m_focus]->m_decoder ) 
			{
				openhtmldlg( m_screen[m_focus]->m_decoder->m_playerinfo.serverinfo, "/gpslog.html" );
			}	
            break ;
#endif // ID_TVSINFORMATION_GPSLOG
	}
}

#else   // TVS_APP

BOOL DvrclientDlg::OnDeviceChange( UINT nEventType, DWORD dwData )
{
   return TRUE;
}

#endif  //  TVS_App

#ifdef NEWPAINTING

void DvrclientDlg::OnPaint()
{
    PAINTSTRUCT ps;
    HDC hdc;
    int w,h;
    Bitmap * timg=NULL ;
    hdc = ::BeginPaint(m_hWnd, &ps);
    Gdiplus::Graphics *g = new Graphics(hdc);
//    RECT rect ;
    int redrawimg = 1 ;
    Color  color ;
    Brush * bkbrush ;

	RECT rect ;
	// Panel area ?
	if( IntersectRect( &rect, &m_panelrect, &ps.rcPaint ) ) {
		color.SetFromCOLORREF( GetSysColor(COLOR_3DFACE) );
		bkbrush = new SolidBrush( color );
		g->FillRectangle(bkbrush, (INT)m_panelrect.left, m_panelrect.top, m_panelrect.right-m_panelrect.left, m_panelrect.bottom-m_panelrect.top ) ;
		delete bkbrush ;

		if( m_clientmode == CLIENTMODE_CLOSE ) {
			timg = loadbitmap(_T("OPEN_DISCONNECT"));
		}
		else if( m_clientmode == CLIENTMODE_LIVEVIEW ) {
			timg = loadbitmap(_T("OPEN_LIVEVIEW"));
		}
		else if( m_clientmode == CLIENTMODE_PLAYBACK ) {
			timg = loadbitmap(_T("OPEN_ARCHIVE"));
		}
		else if( m_clientmode == CLIENTMODE_PLAYFILE ) {
			timg = loadbitmap(_T("OPEN_FILE"));
		}
		else if( m_clientmode == CLIENTMODE_PLAYSMARTSERVER ) {
			timg = loadbitmap(_T("OPEN_SMARTSERVER"));
		}
		if( timg ) {
			g->DrawImage( timg, (INT)m_statusiconrect.left, m_statusiconrect.top, timg->GetWidth(), timg->GetHeight() ) ;
			delete timg ;
			timg=NULL ;
		}
	
		// app logo
		timg = loadbitmap( _T("APPNAME")) ;
		if( timg ) {
			w = timg->GetWidth() ;
			h = timg->GetHeight() ;
			g->DrawImage( timg, m_panelrect.left, m_panelrect.top, w, h) ;
			delete timg;
			timg=NULL ;
		}
		    
		// draw PTZ campass
		if( ::IsWindowVisible( ::GetDlgItem(m_hWnd, IDC_GROUP_PTZ) ) ) {
			//    dc.SelectClipRgn(&m_ptzcampass_rgn);
			timg = loadbitmap( _T("PTZ_DE") );
			if( timg ) {
				g->DrawImage( timg, (INT)m_ptzcampass_rect.left, (INT)m_ptzcampass_rect.top);
				delete timg ;
				timg=NULL ;
			}
		}

#ifdef APP_PWPLAYER
		// draw patrolwitness logo
		timg = loadbitmap(_T("PATROLWITNESS"));
		if( timg ) {
			g->DrawImage( timg, (INT)m_companylinkrect.left, (INT)m_companylinkrect.top );
			delete timg ;
			timg = NULL ;
		}
#endif

	}

	// Player area
	if( IntersectRect( &rect, &m_playerrect, &ps.rcPaint ) ) {

#ifdef SPARTAN_APP
		timg = loadbitmap( _T("PLAY_BK") );
		if( timg ) {
			bkbrush = new TextureBrush( timg );
		}
		else {
			color.SetFromCOLORREF( SCREENBACKGROUNDCOLOR );
			bkbrush = new SolidBrush( color );
		}
#else
        color.SetFromCOLORREF( SCREENBACKGROUNDCOLOR );
        bkbrush = new SolidBrush( color );
#endif

//    g->ExcludeClip(Rect(m_screct.left, m_screct.top, m_screct.right-m_screct.left, m_screct.bottom-m_screct.top ));

		int i;
		for( i=0; i<m_screennum; i++) {
			g->ExcludeClip(Rect(m_screenrect[i].left, m_screenrect[i].top, m_screenrect[i].right-m_screenrect[i].left, m_screenrect[i].bottom-m_screenrect[i].top ));
		}
		g->FillRectangle(bkbrush, (INT)m_playerrect.left, m_playerrect.top, m_playerrect.right-m_playerrect.left, m_playerrect.bottom-m_playerrect.top ) ;
		delete bkbrush ;
		g->ResetClip();

		if( timg ) {
			delete timg ;
			timg = NULL ;
		}

    // draw video screen area
/*    color.SetFromCOLORREF( GetSysColor(COLOR_BTNFACE) );
    Pen pen(color);
    g->DrawLine( &pen, m_playerrect.left, m_playerrect.bottom, m_playerrect.left, m_playerrect.top );
    g->DrawLine( &pen, m_playerrect.left, m_playerrect.top, m_playerrect.right, m_playerrect.top );
    color.SetFromCOLORREF( GetSysColor(COLOR_3DSHADOW) );
    g->DrawLine( &pen, m_playerrect.right, m_playerrect.top, m_playerrect.right, m_playerrect.bottom );
    g->DrawLine( &pen, m_playerrect.right, m_playerrect.bottom, m_playerrect.left+1, m_playerrect.bottom );
*/

		Pen focuspen(Color(255, 248, 240, 16));
		Pen edgepen1(Color(255, 40, 40, 40 ));
		Pen edgepen2(Color(255, 60, 60, 60 ));

		int numscreen = screenmode_table[m_screenmode].numscreen ;

		for( i=0; i<numscreen; i++ ) {
			if( g_singlescreenmode || i==m_focus ) {
				g->DrawLine( &focuspen, (INT)m_screenrect[i].left, m_screenrect[i].bottom-1, m_screenrect[i].left, m_screenrect[i].top) ;
				g->DrawLine( &focuspen, (INT)m_screenrect[i].left, m_screenrect[i].top, m_screenrect[i].right-1, m_screenrect[i].top) ;
				g->DrawLine( &focuspen, (INT)m_screenrect[i].right-1, m_screenrect[i].top, m_screenrect[i].right-1, m_screenrect[i].bottom-1 );
				g->DrawLine( &focuspen, (INT)m_screenrect[i].right-1, m_screenrect[i].bottom-1, m_screenrect[i].left, m_screenrect[i].bottom-1 );
			}
			else {
				g->DrawLine( &edgepen1, (INT)m_screenrect[i].left, m_screenrect[i].bottom-1, m_screenrect[i].left, m_screenrect[i].top) ;
				g->DrawLine( &edgepen1, (INT)m_screenrect[i].left, m_screenrect[i].top, m_screenrect[i].right-1, m_screenrect[i].top) ;
				g->DrawLine( &edgepen2, (INT)m_screenrect[i].right-1, m_screenrect[i].top, m_screenrect[i].right-1, m_screenrect[i].bottom-1 );
				g->DrawLine( &edgepen2, (INT)m_screenrect[i].right-1, m_screenrect[i].bottom-1, m_screenrect[i].left, m_screenrect[i].bottom-1 );
			}
		}
	}

	// Bottom Bar
	if( IntersectRect( &rect, &m_bottombar, &ps.rcPaint ) ) {

		// load speaker icon first
		int volume=m_volume.GetPos();
		if( volume == 0 ) {
			timg = loadbitmap( _T("VOLUME_0") );
		}
		else if( volume<30 ) {
			timg = loadbitmap( _T("VOLUME_1") );
		}
		else if( volume<70 ) {
			timg = loadbitmap( _T("VOLUME_2") );
		}
		else {
			timg = loadbitmap( _T("VOLUME_3") );
		}

		// erase bottom bar background    
		if( m_bkbmp ) {
			//g->DrawImage( m_bkbmp, m_bottombar.left, m_bottombar.top, 0, 0, m_bottombar.right-m_bottombar.left, m_bottombar.bottom-m_bottombar.top, UnitPixel );
			//g->DrawImage( m_bkbmp, (INT)rect.left, (INT)rect.top, (INT)(rect.left-m_bottombar.left), (INT)(rect.top-m_bottombar.top), (INT)(rect.right-rect.left), (INT)(rect.bottom-rect.top), UnitPixel );
			// g->DrawImage( m_bkbmp, m_bottombar.left, m_bottombar.top, m_bottombar.right-m_bottombar.left, m_bottombar.bottom-m_bottombar.top );
			g->DrawImage( m_bkbmp, m_bottombar.left, m_bottombar.top );
		}
		else {
			color.SetFromCOLORREF( BOTTOMBARCOLOR );
			bkbrush = new SolidBrush( color );
			g->FillRectangle( bkbrush, (INT)rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top );
			delete bkbrush ;
		}

		if( timg ) {
			g->DrawImage( timg, (INT)m_speakerrect.left, (INT)m_speakerrect.top, m_speakerrect.right - m_speakerrect.left, m_speakerrect.bottom - m_speakerrect.top );
			delete timg ;
			timg=NULL ;
		}

		// draw company logo
		timg = loadbitmap( _T("COMPANYLOGO")) ;
		if( timg ) {
#ifdef APP_PW_TABLET
			w = timg->GetWidth() ;
			h = timg->GetHeight() ;

			int bh = m_bottombar.bottom - m_bottombar.top ;

			g->DrawImage( timg, Rect( m_bottombar.right - bh, m_bottombar.top, bh, bh), 0, 0, w, h, UnitPixel);

#else 
			g->DrawImage( timg, (INT)m_bottombar.left+5, (INT)m_bottombar.top+2 );
#endif
			delete timg;
			timg=NULL ;
		}
	}

#ifdef APP_PW_TABLET
	bkbrush = new SolidBrush( Color( 0x55, 0x36, 0x53 ) );
	// draw slider prev/next button
	RECT xrect ;
	xrect.left = m_playerrect.left ;
	xrect.right = 50 ;
	xrect.top = m_playerrect.bottom ;
	xrect.bottom = m_bottombar.top ;
	if( IntersectRect( &rect, &xrect, &ps.rcPaint ) ) {
		g->FillRectangle(bkbrush, (INT)xrect.left, xrect.top, xrect.right-xrect.left, xrect.bottom-xrect.top ) ;
		timg =  loadbitmap( _T("PW_SLIDER_PREV")) ;
		if( timg ) {
			g->DrawImage( timg, xrect.left, xrect.top, xrect.right-xrect.left, xrect.bottom - xrect.top ) ;
			delete timg ;
		}
	}

	xrect.left = m_playerrect.right-50 ;
	xrect.right = m_playerrect.right ;
	xrect.top = m_playerrect.bottom ;
	xrect.bottom = m_bottombar.top ;
	if( IntersectRect( &rect, &xrect, &ps.rcPaint ) ) {
		g->FillRectangle(bkbrush, (INT)xrect.left, xrect.top, xrect.right-xrect.left, xrect.bottom-xrect.top ) ;
		timg =  loadbitmap( _T("PW_SLIDER_NEXT")) ;
		if( timg ) {
			g->DrawImage( timg, xrect.left, xrect.top, xrect.right-xrect.left, xrect.bottom - xrect.top ) ;
			delete timg ;
		}
	}

	delete bkbrush ;

#endif

	delete g ;
	EndPaint(m_hWnd, &ps);
}

#endif

#ifdef SPARTAN_APP
void DvrclientDlg::OnBnClickedScreen1()
{
   		g_singlescreenmode = 0 ;
		g_screenmode = 0 ;
		SetScreenFormat();
}

void DvrclientDlg::OnBnClickedScreen2()
{
   		g_singlescreenmode = 0 ;
		g_screenmode = 1 ;
		SetScreenFormat();
}

void DvrclientDlg::OnBnClickedScreen4()
{
   		g_singlescreenmode = 0 ;
		g_screenmode = 3 ;
		SetScreenFormat();
}
#endif


#ifdef IDC_EXPORTJPG

struct _exportjpgst {
	DvrclientDlg * dlg ;
	struct dup_state dupstate ;
	struct dvrtime begint ;
	struct dvrtime endt ;
	struct dvrtime dvrt ;
	struct player_info plyinfo ;
	char   homepath[MAX_PATH] ;
} ;

void  DvrclientDlg::SnapShot( void * param )
{
#ifdef TVS_SNAPSHOT2		 
	struct _exportjpgst * expst = (struct _exportjpgst *) param	  ;
	Bitmap * snapshotimg ;
	HDC snapshotdc=0;
	int i, res;
	RECT picrect, rect ;
	int  txtxpos, txtypos ;
	struct tvs_info * ptvs_info ;
    string tbuf ;

	char * tmp = getenv("TEMP");
	if( tmp==NULL ) {
		tmp="C:\\TMP" ;
	}
	ptvs_info = (struct tvs_info *)(expst->plyinfo.serverinfo) ;

	snapshotimg=new Bitmap(1408, 1006);
	Graphics * g=new Graphics(snapshotimg);
	g->Clear(Color(255,255,255,255));

	if( m_screennum == 1 ) {
		picrect.left = 132 ;
		picrect.right = 1408-132 ;
		txtxpos = 380 ;
	}
	else if( m_screennum == 2 ) {
		if( screenmode_table[m_screenmode].ynum > screenmode_table[m_screenmode].xnum ) {
			picrect.left = 132 ;
			picrect.right = 1408-132 ;
		}
		else {
			picrect.left = 0 ;
			picrect.right = 1408 ;
		}
		txtxpos = 140 ;
	}
	else {
		picrect.left = 132 ;
		picrect.right = 1408-132 ;
		txtxpos = 140 ;
	}
	picrect.top = 0 ;
    int rx = screenmode_table[m_screenmode].xnum * g_ratiox ;
    int ry = screenmode_table[m_screenmode].ynum * g_ratioy ;
	picrect.bottom = ( picrect.right - picrect.left ) * ry / rx ;
	txtypos = picrect.bottom + 15 ;

	FontFamily  fontFamily(_T("Tahoma"));
	Font        font(&fontFamily, 30, FontStyleRegular, UnitPixel);
	SolidBrush  solidBrush(Color(255, 0, 0, 0));

	rect.left = 0 ;
		rect.right = snapshotimg->GetWidth() ;
		rect.top = 0 ;
		rect.bottom = snapshotimg->GetHeight() ;

		int  yinc, y ;
		int  l ;

		y = txtypos ;
		yinc = 28 ;

        l = sprintf(tbuf.strsize(128), "Date/Time : %02d/%02d/%04d %02d:%02d:%02d",
			expst->dvrt.month, 
			expst->dvrt.day, 
			expst->dvrt.year, 
			expst->dvrt.hour, 
			expst->dvrt.min, 
			expst->dvrt.second ) ; 
		g->DrawString(tbuf, -1, &font, PointF(txtxpos, y), &solidBrush);

		if( strcmp( ptvs_info->productid, "TVS" )==0 ) {
            l = sprintf(tbuf.strsize(128), "Medallion / VIN # : %s", ptvs_info->medallion );
			g->DrawString(tbuf, -1, &font, PointF(txtxpos+500, y), &solidBrush);
			y+=yinc ;

			l = sprintf(tbuf.strsize(128), "License # : %s", ptvs_info->lincenseplate );
			g->DrawString(tbuf, -1, &font, PointF(txtxpos, y), &solidBrush);
			//y+=yinc ;

			l = sprintf(tbuf.strsize(128), "Controller serial # : %s", ptvs_info->ivcs );
			g->DrawString(tbuf, -1, &font, PointF(txtxpos+500, y), &solidBrush);
		}
		y+=yinc ;

		for( i=0; i<m_screennum; i++ ) {
			Bitmap * img ;
			int draw=0 ;
			int l, t, w, h ;
			l = picrect.left ;
			t = picrect.top ;
			w = picrect.right-l ;
			h = picrect.bottom-t ;
			screenrect r = screenmode_table[m_screenmode].screen[i] ;
			l += r.left * w ;
			w  = w * (r.right-r.left);
			t += r.top * h ;
			h  = h * (r.bottom-r.top);

			img=NULL ;
			if( m_screen[i]!=NULL && m_screen[i]->m_decoder ) {
                sprintf(tbuf.strsize(MAX_PATH), "%s\\dvrcap_s.bmp", tmp );
				res=m_screen[i]->m_decoder->capture( m_screen[i]->m_channel, tbuf );
				if( res>=0 ) {
					img = Bitmap::FromFile(tbuf);
					if( img ) {
						g->DrawImage(img, l, t, w, h );
						draw=1 ;
						l=sprintf(tbuf.strsize(256), "Camera %d serial # : %s", i+1, m_screen[i]->m_channelinfo.cameraname);
						g->DrawString(tbuf, -1, &font, PointF(txtxpos + ((i % 2) ? 500 : 0), y), &solidBrush);
					}
				}
			}
			if( img==NULL ) {	// not painted
				// paint background pic
				img = loadbitmap(_T("BACKGROUND") );
				if( img ) {
					g->DrawImage(img, l, t, w, h );
				}
			}
			if( img ) {
				delete img ;
				img=NULL;
			}
			if( i%2 ) y+=yinc ;
		}
		if( i%2 ) y+=yinc ;

		// check installer shop name
		char * shopinfo ;
		char * shopname ;
		shopinfo = (char *) &(tvskeydata->size) ;
		shopinfo += tvskeydata->keyinfo_start ;
		shopname = strstr( shopinfo, "Shop Name" );
		if( shopname ) {
			char shopnameline[128] ;
			int ii ;
			// extract shop name from key file
			for(ii = 0 ; ii<127; ii++ ) {
				if( shopname[ii] == '\r' || shopname[ii] == '\n' || shopname[ii]==0) {
					break;
				}
				else {
					shopnameline[ii] = shopname[ii] ;
				}
			}
			shopnameline[ii]=0 ;
            l = sprintf(tbuf.strsize(256), "%s", shopnameline );
			g->DrawString(tbuf, -1, &font, PointF(txtxpos, y), &solidBrush);
			y+=yinc ;
		}

		static int serialno ;
		if( ++serialno>=1000 ) serialno=1 ;
		sprintf(expst->dupstate.msg, "%s_%04d%02d%02d_%02d%02d%02d%03d%03d.JPG",
			expst->plyinfo.servername,
			expst->dvrt.year,
			expst->dvrt.month, 
			expst->dvrt.day,
			expst->dvrt.hour,
			expst->dvrt.min, 
			expst->dvrt.second,
			expst->dvrt.millisecond, serialno);
        sprintf(tbuf.strsize(256), "%s\\%s",
			expst->homepath,
			expst->dupstate.msg );
		savebitmap(tbuf, snapshotimg);

		expst->dupstate.update = 1 ;
	
		delete g;
		delete snapshotimg ;

#endif	// TVS_SNAPSHOT2
}

void DvrclientDlg::ExportjpgThread( void * param )
{
	struct _exportjpgst * expst = (struct _exportjpgst *) param	  ;
	struct dvrtime t1 = expst->begint ;
	struct dvrtime t2 = t1 ;
	int count=0 ;
	int trynum=0 ;
	int tlength ;
	int frames = decoder::g_getdecframes();

	expst->dupstate.percent=1 ;
	OnBnClickedStep();
	Sleep(100);
    tlength = expst->endt - expst->begint ;

    while( decoder::g_getcurrenttime( &t1 ) >= 0 && tlength > 0 ) {
		if( expst->dupstate.cancel ) {
			break;
		}
		Sleep(10);
        expst->dupstate.percent = 100* ( t1 - expst->begint ) / tlength ;
		if( expst->dupstate.percent<=0 ) {
			expst->dupstate.percent=1 ;
		}
		expst->dvrt = t1 ;
		int f = decoder::g_getdecframes();
		if( f!=frames ) {
			trynum=0 ;
			t2=t1 ;
			count++ ;
			frames=f ;
			SnapShot(param);
			OnBnClickedStep();
		}
		else if( time_compare(t1, t2)!=0 ) {
			trynum=0 ;
			t2=t1 ;
			count++ ;
			SnapShot(param);
			OnBnClickedStep();
		}
		else {
			Sleep(100);
			count++ ;
			SnapShot(param);
			OnBnClickedStep();
			if( ++trynum > 30 ) {
				// too many retrys
				break ;
			}
		}
		if(time_compare(t1, expst->endt)>0) {
			break ;
		}
	}

    strncpy( expst->dupstate.msg, expst->homepath, sizeof(expst->dupstate.msg) );
	expst->dupstate.update =  1 ;

	// indicate end of copy
	expst->dupstate.status = 1 ;
}


void _exportjpgthread( void * param )
{
	struct _exportjpgst * expst = (struct _exportjpgst *) param ;
	expst->dlg->ExportjpgThread( param );
}

void DvrclientDlg::OnBnClickedExportjpg()
{
	dvrtime begintime, endtime ;
    SYSTEMTIME st ;
    ::SendDlgItemMessage(m_hWnd, IDC_TIME_SELBEGIN, DTM_GETSYSTEMTIME, 0, (LPARAM)&st);
    time_timedvr(&st, &begintime);
    ::SendDlgItemMessage(m_hWnd, IDC_TIME_SELEND, DTM_GETSYSTEMTIME, 0, (LPARAM)&st);
    time_timedvr(&st, &endtime);

    if( time_compare( endtime, begintime)>0)  {
		struct _exportjpgst expst ;
		memset(&expst, 0, sizeof(expst));
        expst.begint = begintime ;
        expst.endt = endtime ;
		expst.dlg = this ;
		expst.dvrt = expst.begint ;

        string homepath ;
        SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, homepath.tcssize(MAX_PATH) );
		sprintf( expst.homepath, "%s\\247Security", (LPCSTR)homepath );
		_mkdir( expst.homepath );
		sprintf( expst.homepath, "%s\\247Security\\SnapShots", (LPCSTR)homepath );
		_mkdir( expst.homepath );

		if(	m_screen[m_focus] && m_screen[m_focus]->m_decoder) {
			expst.plyinfo = m_screen[m_focus]->m_decoder->m_playerinfo ;
		}
		else {
			int i ;
			for(i=0; i<m_screennum; i++ ) {
				if(	m_screen[i] && m_screen[i]->m_decoder) {
					expst.plyinfo = m_screen[i]->m_decoder->m_playerinfo ;
					break;
				}
			}
			if(i>=m_screennum) {
				MessageBox(m_hWnd, _T("Can not export JPEG files!"),NULL, MB_OK);
				return ;
			}
		}

		struct dvrtime savetime ;
		decoder::g_getcurrenttime( &savetime );

//		OnBnClickedPause();
		decoder::g_seek(&expst.begint);
		OnBnClickedPause();

		_beginthread( _exportjpgthread, 0, &expst ) ;

		CopyProgress progress(_T("Export JPEG Files"), &expst.dupstate);
		progress.DoModal(IDD_DIALOG_COPY, m_hWnd);
		// on any case, we should wait until task finish
		while( expst.dupstate.status==0 ) 
			Sleep(50);

		decoder::g_seek(&savetime);
		OnBnClickedPlay();
	}
	else {
		MessageBox(m_hWnd, _T("Please Select Correct Begin/End Time!"), NULL, MB_OK );
	}	
}

#endif      // IDC_EXPORTJPG

enum e_keycode {
    VK_NULL,
    // pwii definition
    VK_EM   = 0xE0 ,    // EVEMT
    VK_LP,              // PWII, LP key (zoom in)
    VK_POWER,           // PWII, B/O
    VK_SILENCE,         // PWII, Mute
    VK_RECON,           // Turn on all record (force on)
    VK_RECOFF,          // Turn off all record    
    VK_C1,              // PWII, Camera1
    VK_C2,              // PWII, Camera2
    VK_C3,              // PWII, Camera3
    VK_C4,              // PWII, Camera4
    VK_C5,              // PWII, Camera5
    VK_C6,              // PWII, Camera6
    VK_C7,              // PWII, Camera7
    VK_C8               // PWII, Camera8
} ;


#ifdef  APP_PWII_FRONTEND
void DvrclientDlg::OnBnClickedButtonRec()
{
  	if( m_screen[m_focus] && m_screen[m_focus]->m_decoder && m_screen[m_focus]->isattached() ) {
        int channel = m_screen[m_focus]->m_channel ;
        int vk = VK_C1+channel ;
		m_screen[m_focus]->m_decoder->pwkeypad( vk, 1 );
        SetTimer(m_hWnd, TIMER_RECLIGHT, 50, NULL );
    }
	return ;
}

void DvrclientDlg::OnBnClickedButtonRecall()
{
    static int recon = 0 ;
    if( m_screen[m_focus] && m_screen[m_focus]->m_decoder && m_screen[m_focus]->isattached() ) {
        if( recon ) {
		    m_screen[m_focus]->m_decoder->pwkeypad( VK_RECOFF, 1 );
            recon=0 ;
        }
        else {
		    m_screen[m_focus]->m_decoder->pwkeypad( VK_RECON, 1 );
            recon=1 ;
        }
    }
	return ;
}

static struct pwii_button_map {
    int id ;
    int key ;
} pwii_bmap[] = {
    { IDC_BUTTON_CAM1, VK_C1 },
    { IDC_BUTTON_CAM2, VK_C2 },
    { IDC_BUTTON_TM, VK_EM },
    { IDC_BUTTON_LP, VK_LP },
    { 0, VK_NULL }
} ;

void DvrclientDlg::OnPWIIButton( int nIDCtl, int select )
{
    int i ;
    int zoom ;
    int vkey ;
    int bmap = 1 ;

    vkey = 0;
    for( i=0; i<20; i++ ) {
        if( pwii_bmap[i].id == 0 ) break;
        if( pwii_bmap[i].id == nIDCtl ) {
            vkey = pwii_bmap[i].key ;
            break;
        }
        bmap <<= 1 ;
    }
    if( vkey==0 ) return ;

	if( m_screen[m_focus] && m_screen[m_focus]->m_decoder && m_screen[m_focus]->isattached() ) {
        if( select ) {
            if( (m_pwbuttonstate & bmap) == 0 ) {
                m_pwbuttonstate ^= bmap ;
                m_screen[m_focus]->m_decoder->pwkeypad( vkey, 1 );
            }
        }
        else {
            if( (m_pwbuttonstate & bmap) != 0 ) {
                m_pwbuttonstate ^= bmap ;
                m_screen[m_focus]->m_decoder->pwkeypad( vkey, 0 );
				SetDlgItemText(m_hWnd, nIDCtl, _T("2") );		// trans state
            }
        }
    }
	return ;
}

void DvrclientDlg::OnBnClickedButtonTm()
{
    OnPWIIButton( IDC_BUTTON_TM, 1 ) ;
    OnPWIIButton( IDC_BUTTON_TM, 0 ) ;
}

void DvrclientDlg::OnBnClickedButtonLp()
{
    OnPWIIButton( IDC_BUTTON_LP, 1 ) ;
    OnPWIIButton( IDC_BUTTON_LP, 0 ) ;
}

void DvrclientDlg::OnBnClickedButtonRec1()
{
    OnPWIIButton( IDC_BUTTON_CAM1, 1 ) ;
    OnPWIIButton( IDC_BUTTON_CAM1, 0 ) ;
}

void DvrclientDlg::OnBnClickedButtonRec2()
{
    OnPWIIButton( IDC_BUTTON_CAM2, 1 ) ;
    OnPWIIButton( IDC_BUTTON_CAM2, 0 ) ;
}

// officer id dialog
class OfficerIdDialog : public Dialog {
public:
	string oid ;
protected:
	virtual int OnInitDialog()
    {
		SendDlgItemMessage( m_hWnd, IDC_EDIT_OFFICERID, EM_SETLIMITTEXT, (WPARAM)8, 0 );
		return TRUE ;
	}	
    virtual int OnOK()
    {
		GetDlgItemText( m_hWnd, IDC_EDIT_OFFICERID, (LPTSTR)oid.tcssize(100), 100);
        return EndDialog(IDOK);
    }
};

void DvrclientDlg::OnBnClickedButtonNewOfficerId()
{
	if( g_decoder[0].isopen() ) {
		OfficerIdDialog dofficerid ;
		if( dofficerid.DoModal( IDD_DIALOG_OFFICERID, m_hWnd )==IDOK ) {
			g_decoder[0].setofficerid( (char *)dofficerid.oid );

			// reload officer list to cb
			SendMessage( GetDlgItem(m_hWnd, IDC_COMBO_OFFICERID),  CB_RESETCONTENT , 0, 0 );
			int i;
			string ** idlist = new string * [20]  ;
			for( i=0; i<20; i++ ) idlist[i]=NULL ;
			int l = g_decoder[0].getofficeridlist( idlist, 20 );
			for( i=0; i<l; i++ ) {
				if( idlist[i] ) {
					SendMessage( GetDlgItem(m_hWnd, IDC_COMBO_OFFICERID),  CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)( *idlist[i] ));
					delete idlist[i] ;
				}
			}
			delete idlist ;
			SendMessage( GetDlgItem(m_hWnd, IDC_COMBO_OFFICERID),  CB_SETCURSEL , 0, 0 );

		}
	}
}

// tag event dialog
#include "tagevent.h"

void DvrclientDlg::OnBnClickedButtonTagEvent()
{
	if( g_decoder[0].isopen() ) {
		TagEventDialog tageventdlg ;
		tageventdlg.DoModal( IDD_DIALOG_TAGEVENT, m_hWnd );
	}
}

#ifdef APP_PW_TABLET

class BOfficerIdDialog : public Dialog 
{

protected:
	int OnInitDialog()
    {
		// get Officer ID list
		if( g_decoder[0].isopen() ) {
			int i;
			string ** idlist = new string * [20]  ;
			for( i=0; i<20; i++ ) idlist[i]=NULL ;
			int l = g_decoder[0].getofficeridlist( idlist, 20 );
			for( i=0; i<l; i++ ) {
				if( idlist[i] ) {
					SendMessage( GetDlgItem(m_hWnd, IDC_COMBO_BTOFFICERID),  CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)( *idlist[i] ));
					delete idlist[i] ;
				}
			}
			delete idlist ;
			SendMessage( GetDlgItem(m_hWnd, IDC_COMBO_BTOFFICERID),  CB_SETCURSEL , 0, 0 );
		}

		// move dialog on top / center of the window
		RECT mrect ;
		int  h, w ;
		GetWindowRect( m_hWnd, &mrect);
		h = mrect.bottom - mrect.top ;
		w = mrect.right - mrect.left ;
		MoveWindow(m_hWnd, mrect.left, 20, w, h, false );

        return TRUE;
    }
    virtual int OnOK()
    {
		string officerid ;
		if( g_decoder[0].isopen() ) {
			SendDlgItemMessage( m_hWnd, IDC_COMBO_BTOFFICERID, WM_GETTEXT, (WPARAM)80, (LPARAM)officerid.tcssize(80) );
			g_decoder[0].setofficerid( (char *)officerid );
		}
        return EndDialog(IDOK);
	}
};

void DvrclientDlg::OnBnClickedButtonBOfficerId()
{
	if( g_decoder[0].isopen() ) {
		BOfficerIdDialog boidialog ;
		boidialog.DoModal( IDD_DIALOG_B_OFFICERID, m_hWnd ) ;
	}
}

class BTagDialog : public Dialog 
{
protected:	

static char * trimtail( char * str )
{
	int l = strlen( str );
	while( l>0 ) {
		if( str[l-1] > ' ' ) break ;
		str[l-1] = 0 ;
		l-- ;
	}
	return str ;
}

	int m_recsize ;

	virtual int OnInitDialog()
    {
		string s ;
		string vlist ;
		char * p ;
		int i;

		// init tag list
		char * classifications [] = {
			"DUI",
			"Motor Assist",
			"Drug Seizure",
			"Suspicious Vehicle",
			"Domestic Calls",
			"Assault",
			"Evading",
			"Traffic Warning",
			"Traffic Citation",
			"Traffic Accident",
			"Felony",
			"Misdemeanor",
			NULL
		} ;

		for( i=0; i<50; i++ ) {
			if( classifications[i]==NULL ) break ;
			s = classifications[i] ;
			SendDlgItemMessage( m_hWnd, IDC_COMBO_BT_TAG, CB_ADDSTRING, (WPARAM)0, (LPARAM)(TCHAR *)s );
		}

		// init priority list
		char * prioritylist [] = {
			"Routine",
			"Critical",
			NULL
		} ;

		for( i=0; i<50; i++ ) {
			if( prioritylist[i]==NULL ) break ;
			s = prioritylist[i] ;
			SendDlgItemMessage( m_hWnd, IDC_COMBO_BT_PRIORITY, CB_ADDSTRING, (WPARAM)0, (LPARAM)(TCHAR *)s );
		}

		int recsize ;
		int vrisize = g_decoder[0].getvrilistsize( & recsize ) ;
		m_recsize = recsize ;
		vrisize *= recsize ;
		vrisize = g_decoder[0].getvrilist( vlist.strsize(vrisize+600), vrisize+600 ) ;
		if( vrisize <= 0 ) {
			EndDialog( IDCANCEL );
			return TRUE ;
		}

		int o ;
		int vr ;
		int vr_cur, vr_p, vr_pi , vr_n, vr_ni ;
		struct dvrtime curt  ;
		g_decoder[0].getcurrenttime( &curt );
		vr_cur = (curt.year-2000)*100000000 +
			(curt.month)*1000000 +
			(curt.day )*10000 +
			(curt.hour)*100 +
			(curt.min);
		vr_p = vr_n = 0 ;

		// Search matching vri
		for( o=0; o<vrisize; o+=recsize ) {
			strncpy( s.strsize(65), ((char *)vlist)+o, 64 );
			((char *)s)[64]=0 ;
			p=strchr( ((char *)s), '-' );
			if(p) {
				sscanf( p+1, "%d", &vr );
				if( vr<=vr_cur ) {
					if( vr_p == 0 ) {
						vr_p = vr ;
						vr_pi = o ;
					}
					else if( vr > vr_p ) {
						vr_p = vr ;
						vr_pi = o ;
					}
				}
				else {
					if( vr_n == 0 ) {
						vr_n = vr ;
						vr_ni = o ;
					}
					else if( vr < vr_n ) {
						vr_n = vr ;
						vr_ni = o ;
					}
				}
			}
		}

		if( vr_p ) {
			o=vr_pi ;
		} else if( vr_n ) {
			o=vr_ni ;
		}
		else {
			o= 0 ;
		}

		// now o is calculated VRI to be closed to current play time
		p = ((char *)vlist)+o ;

		char * xp = p ;

		// vri
		strncpy( s.strsize(65), p, 64 );
		((char *)s)[64]=0 ;
		trimtail(((char *)s));
		SetDlgItemText( m_hWnd, IDC_EDIT_BT_VRI,(TCHAR *)s );
		p+=64 ;

		// extra offierid from vri
		string oid ;
		char * xoid ;
		xoid=strchr((char *)s, '-') ;
		if( xoid ) {
			xoid=strchr(xoid+1, '-') ;
			if( xoid ) {
				oid=xoid+1 ;
			}
		}

		// classification
		strncpy( s.strsize(65), p, 32 );
		((char *)s)[32]=0 ;
		trimtail(((char *)s));
		SetDlgItemText( m_hWnd, IDC_COMBO_BT_TAG,(TCHAR *)s );
		p+=32 ;

		// case
		strncpy( s.strsize(65), p, 64 );
		((char *)s)[64]=0 ;
		trimtail(((char *)s));
		SetDlgItemText( m_hWnd, IDC_EDIT_BT_CASENUMBER,(TCHAR *)s );
		p+=64 ;

		// priority
		strncpy( s.strsize(65), p, 20 );
		((char *)s)[20]=0 ;
		trimtail(((char *)s));
		SetDlgItemText( m_hWnd, IDC_COMBO_BT_PRIORITY,(TCHAR *)s );
		p+=20 ;

		// officer id 
		if( strlen( oid )>0 ) {
			trimtail(((char *)oid));
			SetDlgItemText( m_hWnd, IDC_EDIT_BT_OFFICERID, (TCHAR *)oid );
		}
		p+=32 ;
		SetDlgItemText( m_hWnd, IDC_EDIT_BT_OFFICERID, (TCHAR *)oid );


		// Notes
		strncpy( s.strsize(260), p, 255 );
		((char *)s)[255]=0 ;
		trimtail(((char *)s));
		SetDlgItemText( m_hWnd, IDC_EDIT_BT_NOTES, (TCHAR *)s );


		// move dialog on top / center of the window
		RECT mrect ;
		int  h, w ;
		GetWindowRect( m_hWnd, &mrect);
		h = mrect.bottom - mrect.top ;
		w = mrect.right - mrect.left ;
		MoveWindow(m_hWnd, mrect.left, 20, w, h, false );

		return TRUE ;
	}	


	virtual int OnOK()
    {
		int l ;
		int si ;
		string s ;
		string vri ;
		char * p = vri.strsize( m_recsize+500 ) ;

		// get Text from dialog

		// vri
		si=64 ;
		GetDlgItemText( m_hWnd, IDC_EDIT_BT_VRI, (TCHAR *)s.tcssize(si+10), si+10 );
		l=strlen((char *)s);
		if( l>si ) l = si ;
		memset( p, ' ', si );
		strncpy( p, (char *)s, l );
		p+=si ;

		// classification
		si=32 ;
		GetDlgItemText( m_hWnd, IDC_COMBO_BT_TAG, (TCHAR *)s.tcssize(si+10), si+10 );
		l=strlen((char *)s);
		if( l>si ) l = si ;
		memset( p, ' ', si );
		strncpy( p, (char *)s, l );
		p+=si ;

		// case
		si=64 ;
		GetDlgItemText( m_hWnd, IDC_EDIT_BT_CASENUMBER, (TCHAR *)s.tcssize(si+10), si+10 );
		l=strlen((char *)s);
		if( l>si ) l = si ;
		memset( p, ' ', si );
		strncpy( p, (char *)s, l );
		p+=si ;


		// priority
		si=20 ;
		GetDlgItemText( m_hWnd, IDC_COMBO_BT_PRIORITY, (TCHAR *)s.tcssize(si+10), si+10 );
		l=strlen((char *)s);
		if( l>si ) l = si ;
		memset( p, ' ', si );
		strncpy( p, (char *)s, l );
		p+=si ;


		// officer id 
		si=32 ;
		memset( p, ' ', si );
		p+=si ;

		// Notes
		si=255 ;
		GetDlgItemText( m_hWnd, IDC_EDIT_BT_NOTES, (TCHAR *)s.tcssize(si+10), si+10 );
		l=strlen((char *)s);
		if( l>si ) l = si ;
		memset( p, ' ', si );
		strncpy( p, (char *)s, l );
		p+=si ;

		*p='\n' ; // end of record

		g_decoder[0].setvrilist( (char *)vri, m_recsize );

        return EndDialog(IDOK);
	}

};

void DvrclientDlg::OnBnClickedButtonBTagEvent()
{
	if( g_decoder[0].isopen() ) {
		BTagDialog btagdialog ;
		if( btagdialog.DoModal( IDD_DIALOG_BTAG, m_hWnd )==IDOK ) {
		}
	}
}

class bcalDialog : public Dialog 
{
public:
	Screen * sc ;
	struct dvrtime dvrt ;

protected:

	int verifyDate(struct dvrtime * dt)
	{
		// regulate date/time
		SYSTEMTIME SystemTime ;
		FILETIME   FileTime ;
		memset( &SystemTime, 0, sizeof( SystemTime ) );
		memset( &FileTime, 0, sizeof( FileTime ) );
		SystemTime.wYear = dt->year ;
		SystemTime.wMonth = dt->month ;
		SystemTime.wDay = dt->day ;
		SystemTimeToFileTime( &SystemTime, &FileTime );
		FileTimeToSystemTime( &FileTime, &SystemTime );
		return  dt->year==SystemTime.wYear &&
				dt->month==SystemTime.wMonth &&
				dt->day==SystemTime.wDay ;
	}

	void showDaylist()
	{
		string s ;
		int i ;
		int dayinfo ;
		int idx ;
		SendDlgItemMessage( m_hWnd, IDC_LIST_VDAY, LB_RESETCONTENT , 0, 0 );
		for( i=1; i<=31; i++ ) {
			dvrt.day = i ;
			if( verifyDate(&dvrt) && sc ) {
				dayinfo = sc->getdayinfo( &dvrt );
				if( dayinfo ) {
					SYSTEMTIME st;
					memset(&st, 0, sizeof(st));
					st.wYear = dvrt.year ;
					st.wMonth = dvrt.month ;
					st.wDay = dvrt.day ;
					GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &st,  _T(" MMM dd yyyy  '-'  ddd"), s.tcssize(200), 200, NULL );
					idx = SendDlgItemMessage( m_hWnd, IDC_LIST_VDAY, LB_ADDSTRING , 0, (LPARAM)(TCHAR *)s );
					SendDlgItemMessage( m_hWnd, IDC_LIST_VDAY, LB_SETITEMDATA, idx, (LPARAM)i );
				}
			}
		}
	}

	int OnInitDialog()
    {
		g_decoder[0].getcurrenttime( &dvrt );
		dvrt.day = 1;
		dvrt.hour = 0 ;
		dvrt.min = 0 ;
		dvrt.second = 0 ;
		showDaylist();

		// move dialog on top / center of the window
		RECT mrect ;
		int  h, w ;
		GetWindowRect( m_hWnd, &mrect);
		h = mrect.bottom - mrect.top ;
		w = mrect.right - mrect.left ;
		MoveWindow(m_hWnd, mrect.left, 20, w, h, false );

        return TRUE;
    }

    virtual int OnSPrev()
    {
		dvrt.month-- ;
		if( dvrt.month<=0 ) {
			dvrt.year-- ;
			dvrt.month = 12 ;
		}
		showDaylist();
        return TRUE;
	}

	virtual int OnSNext()
    {
		dvrt.month++ ;
		if( dvrt.month>12 ) {
			dvrt.year++ ;
			dvrt.month = 1 ;
		}
		showDaylist();
        return TRUE;
	}

    virtual int OnOK()
    {
		int i ;
		int sel ;
		dvrt.day = 1 ;
		sel = SendDlgItemMessage( m_hWnd, IDC_LIST_VDAY, LB_GETCURSEL, 0, 0 );
		if( sel>=0 ) {
			i = (int)SendDlgItemMessage( m_hWnd, IDC_LIST_VDAY, LB_GETITEMDATA, (WPARAM)sel, 0);
			dvrt.day = i ;
		}
        return EndDialog(IDOK);
	}

    virtual int OnCommand( int Id, int Event ) 
	{
        switch (Id)
        {
		case IDC_BUTTON_S_NEXT:
			return OnSNext();

		case IDC_BUTTON_S_PREV:
			return OnSPrev();
		
		case IDC_LIST_VDAY:
			if(Event == LBN_DBLCLK) {	// Double click on list
				return OnOK();
			}
			break;

		default:
			return Dialog::OnCommand( Id, Event );
        }
        return FALSE ;
    }

};

void DvrclientDlg::OnBnClickedButtonBCal()
{
	if( g_decoder[0].isopen() ) {
		bcalDialog bcal ;
		bcal.sc = m_screen[m_focus] ;
		if( bcal.DoModal( IDD_DIALOG_BCAL_SEARCH, m_hWnd )==IDOK ) {
			// seek to new date
			m_noupdtime=0 ;     // enable update time 
			SeekTime( &bcal.dvrt );
		}
	}
}

void DvrclientDlg::OnBnTrioview()
{
	int i;
	for(i=0; i<3; i++) {
		if(m_screen[i]) {
			if( m_screen[i]->m_decoder != g_decoder || m_screen[i]->m_channel != i ) {
				m_screen[i]->DetachDecoder();
			}
		}
	}
	SetScreenFormat("TRIOVIEW") ;
	for(i=0; i<3; i++) {
		if(m_screen[i]) {
			m_screen[i]->AttachDecoder( g_decoder,  i );
		}
	}
}

void DvrclientDlg::setSingleScreen(int channel)
{
	/*
	int i;
	for(i=0;i<MAXSCREEN;i++) {
		if( m_screen[i] )
			m_screen[i]->suspend();
	}
	*/
	SetScreenFormat("Single") ;
	m_screen[0]->AttachDecoder( g_decoder,  channel );
}

void DvrclientDlg::OnBnRearview()
{
	setSingleScreen(4);
}

void DvrclientDlg::OnBnHoodview()
{
	setSingleScreen(3);
}

void DvrclientDlg::OnBnAuxview()
{
	setSingleScreen(5);
}

void DvrclientDlg::OnBnPanLeft()
{
	setSingleScreen(0);
}

void DvrclientDlg::OnBnPanCenter()
{
	setSingleScreen(1);
}

void DvrclientDlg::OnBnPanRight()
{
	setSingleScreen(2);
}

#include "Covert.h"
void DvrclientDlg::OnBnClickedBCovert()
{
	int i ;
	Covertscreen cv(m_hWnd) ;
	
	if( m_clientmode == CLIENTMODE_LIVEVIEW ) {
		for(i=0;i<MAXSCREEN;i++) {
			if( m_screen[i] )
				m_screen[i]->suspend();
		}
	}
	else if( m_clientmode != CLIENTMODE_CLOSE ) {
		if( g_playstat==PLAY_PLAY ) {
			decoder::g_pause();
		}
	}

	// send covert command to PW
	if( m_screen[0] && m_screen[0]->m_decoder ) {
		 m_screen[0]->m_decoder->covert(1);
	}

	cv.run();
	// send un-covert command to PW
	if( m_screen[0] && m_screen[0]->m_decoder ) {
		 m_screen[0]->m_decoder->covert(0);
	}


	if( cv.result && MessageBox(m_hWnd, _T("Do you want to quit application?"), _T("Quit?"), MB_YESNO ) == IDYES ) {
		EndDialog(IDOK);
	}
	else {

		if( m_clientmode == CLIENTMODE_LIVEVIEW ) {
			for(i=0;i<MAXSCREEN;i++) {
				if( m_screen[i] )
					m_screen[i]->resume();
			}
		}
		else if( m_clientmode != CLIENTMODE_CLOSE ) {
			if( g_playstat==PLAY_PLAY ) {
				decoder::g_play();
			}
		}

	}
}

/*
void DvrclientDlg::OnBnClickedBRange( int id )
{
	int dayrange = 0 ;	// set time range 0: day, 1: hour , 2: quat
	if( id == IDC_BUTTON_BDAYRANGE ) {
		dayrange = 1 ;
		ShowWindow( GetDlgItem(m_hWnd, IDC_BUTTON_BDAYRANGE ), SW_HIDE );
		ShowWindow( GetDlgItem(m_hWnd, IDC_BUTTON_BHOURRANGE ), SW_SHOW );
	}
	if( id == IDC_BUTTON_BHOURRANGE ) {
		dayrange = 2 ;
		ShowWindow( GetDlgItem(m_hWnd, IDC_BUTTON_BHOURRANGE ), SW_HIDE );
		ShowWindow( GetDlgItem(m_hWnd, IDC_BUTTON_BQUATRANGE ), SW_SHOW );
	}
	if( id == IDC_BUTTON_BQUATRANGE ) {
		dayrange = 0 ;
		ShowWindow( GetDlgItem(m_hWnd, IDC_BUTTON_BQUATRANGE ), SW_HIDE );
		ShowWindow( GetDlgItem(m_hWnd, IDC_BUTTON_BDAYRANGE ), SW_SHOW );
	}

	::SendDlgItemMessage(m_hWnd, IDC_COMBO_TIMERANGE, CB_SETCURSEL, (WPARAM)dayrange, (LPARAM)0);
	m_tbarupdate = 1 ;
    SetTimer(m_hWnd, TIMER_UPDATE, 10, NULL);
}
*/

void DvrclientDlg::OnBnClickedBRange( int id )
{
	int dayrange ;	// set time range 0: day, 1: hour , 2: quat
	dayrange = GetTimeRange();
	dayrange++ ;
	if( dayrange>2 ) {
		dayrange = 0 ;
	}
	SetTimeRange( dayrange );
}

#endif

/*
void DvrclientDlg::OnBnClickedButtonRec2()
{
    int i ;
    for(i=0; i<m_screennum ; i++ ) {
        if( m_screen[i] && m_screen[i]->m_decoder && m_screen[i]->isattached() ) {
            m_screen[i]->m_decoder->pwkeypad( VK_C2, 1 );
            m_screen[i]->m_decoder->pwkeypad( VK_C2, 0 );
            break;
        }
    }
}
*/

#endif

int DvrclientDlg::GetTimeRange()
{
	return ::SendDlgItemMessage(m_hWnd, IDC_COMBO_TIMERANGE, CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
}

void DvrclientDlg::SetTimeRange( int tirange )
{
	::SendDlgItemMessage(m_hWnd, IDC_COMBO_TIMERANGE, CB_SETCURSEL, (WPARAM)tirange, (LPARAM)0);
	m_tbarupdate = 1 ;
    SetTimer(m_hWnd, TIMER_UPDATE, 10, NULL);
	CtrlSet();
}

void DvrclientDlg::DisplayVolume( int x, int y )
{
#ifdef APP_PW_TABLET
	VolumeWindow vw(m_hWnd, m_waveoutId, x, y ) ;
	vw.run();
#endif
}

int DvrclientDlg::OnDisplayChange()
{
	int i ;
	for(i=0;i<MAXSCREEN;i++) {
		if(	m_screen[i] ) 
			m_screen[i]->reattach();
	}
	return 0 ;
}

int AboutDlg::OnInitDialog()
{
	int i;
	string str ;
	int   libversion[4] ;
	int	  release_major = 1;
	int   release_minor = 101;
	LPVOID lpBuffer ;
	UINT uLen ;
	string appname="DvrViewer";
	string version="Version 3";
	string copyright="Copyright (C) 2012";
	string company="Toronto MicroElectronics Inc.";

#ifdef   APPNAME
    appname = APPNAME ;
#endif

    GetModuleFileName(NULL, str.tcssize(260), 259 );
    DWORD dwHandle;
    DWORD fvisize = GetFileVersionInfoSize( str, &dwHandle );
    char * pfvi = new char [fvisize] ;
    GetFileVersionInfo( str, dwHandle, fvisize, (LPVOID)pfvi );

    VS_FIXEDFILEINFO * pverinfo ;
    if( VerQueryValue( pfvi, _T("\\"), (LPVOID*)&pverinfo, &uLen ) ) {
        libversion[0]=(pverinfo->dwProductVersionMS)/0x10000 ;
        libversion[1]=(pverinfo->dwProductVersionMS)%0x10000 ;
        libversion[2]=(pverinfo->dwProductVersionLS)/0x10000 ;
        libversion[3]=(pverinfo->dwProductVersionLS)%0x10000 ;
        release_major = libversion[2]% 2000 ;
        release_minor = libversion[3] ;
		version.printf("Release number : %d.%d", release_major, release_minor);
    }

    struct LANGANDCODEPAGE {
        WORD wLanguage;
        WORD wCodePage;
    } *lpTranslate;

    // Read the list of languages and code pages.
    uLen = 0 ;
    VerQueryValue(pfvi, 
        _T("\\VarFileInfo\\Translation"),
        (LPVOID*)&lpTranslate,
        &uLen);

    WORD wlang, wcodepage ;
    if( uLen>=sizeof(struct LANGANDCODEPAGE) ) {
        wlang = lpTranslate->wLanguage ;
        wcodepage = lpTranslate->wCodePage ;
    }
    else {
        wlang = 9 ;             // English
        wcodepage = 0x4b0 ;     // US
    }

    // set appname from Intername
	str.printf( "\\StringFileInfo\\%04x%04x\\InternalName", wlang, wcodepage );
    if( VerQueryValue(pfvi, str, &lpBuffer, &uLen)) {
        appname=(LPCTSTR)lpBuffer ;
    }

	str.printf( "\\StringFileInfo\\%04x%04x\\CompanyName", wlang, wcodepage );
    if( VerQueryValue(pfvi, str, &lpBuffer, &uLen)) {
        company=(LPCTSTR)lpBuffer ;
    }

	str.printf( "\\StringFileInfo\\%04x%04x\\LegalCopyright", wlang, wcodepage );
    if( VerQueryValue(pfvi, str, &lpBuffer, &uLen)) {
        copyright=(LPCTSTR)lpBuffer ;
    }

	str.printf( "\\StringFileInfo\\%04x%04x\\OriginalFilename", wlang, wcodepage );
    if( VerQueryValue(pfvi, str, &lpBuffer, &uLen)) {
		string filename = (LPCTSTR)lpBuffer ;
		str.printf( "%s : %d, %d, %d, %d", (LPCSTR)filename, libversion[0], libversion[1], libversion[2], libversion[3] );
		SendMessage( GetDlgItem(m_hWnd, IDC_LISTLIBRARY), LB_ADDSTRING, 0, (LPARAM)(LPCTSTR)str ) ;
    }

	delete[] pfvi;

/*
    HRSRC hres = FindResource(NULL, MAKEINTRESOURCE(VS_VERSION_INFO), RT_VERSION);
	HGLOBAL hglo=LoadResource( NULL, hres );
	WORD * pver = (WORD *)LockResource( hglo );
	VS_FIXEDFILEINFO * pverinfo = (VS_FIXEDFILEINFO *) (pver+20);
	if( pverinfo->dwSignature ==  VS_FFI_SIGNATURE ) {
		VerQueryValue(pver, _T("\\StringFileInfo\\000904b0\\InternalName"), &lpBuffer, &uLen);
		if( uLen>0 ) {
            appname=(LPCTSTR)lpBuffer ;
        }
		libversion[0]=(pverinfo->dwProductVersionMS)/0x10000 ;
		libversion[1]=(pverinfo->dwProductVersionMS)%0x10000 ;
		libversion[2]=(pverinfo->dwProductVersionLS)/0x10000 ;
		libversion[3]=(pverinfo->dwProductVersionLS)%0x10000 ;
		release_major = libversion[2]% 2000 ;
		release_minor = libversion[3] ;
		sprintf( buf, "Release number : %d.%d", release_major, release_minor );
		version=buf ;
		VerQueryValue(pver, _T("\\StringFileInfo\\000904b0\\CompanyName"), &lpBuffer, &uLen);
		if( uLen>0 ) company=(LPCTSTR)lpBuffer ;
		VerQueryValue(pver, _T("\\StringFileInfo\\000904b0\\LegalCopyright"), &lpBuffer, &uLen);
		if( uLen>0 ) copyright=(LPCTSTR)lpBuffer ;

		// add exe version to version list
		VerQueryValue(pver, _T("\\StringFileInfo\\000904b0\\OriginalFilename"), &lpBuffer, &uLen);
		str = (LPCTSTR)lpBuffer ;
		sprintf( buf, "%s : %d, %d, %d, %d", (LPCSTR)str, libversion[0], libversion[1], libversion[2], libversion[3] );
		str = buf ;
		SendMessage( GetDlgItem(m_hWnd, IDC_LISTLIBRARY), LB_ADDSTRING, 0, (LPARAM)(LPCTSTR)str ) ;

	}
*/

	// add library version list
	for(i=0; i<decoder::g_getlibnum();i++) {
		decoder::g_getlibversion(i, libversion) ;
		str.printf( "%s : %d, %d, %d, %d",
			decoder::g_getlibname(i), 
			libversion[0], libversion[1], libversion[2], libversion[3] );
		SendMessage( GetDlgItem(m_hWnd, IDC_LISTLIBRARY), LB_ADDSTRING, 0, (LPARAM)(LPCTSTR)str ) ;
		libversion[2] %= 2000 ;
		if( libversion[2]>release_major || 
			( libversion[2]==release_major && libversion[3]>release_minor ) ) {
				release_major=libversion[2] ;
				release_minor=libversion[3] ;
				version.printf( "Release number : %d.%d", release_major, release_minor );
		}
	}

	// add extra extsvr version
	str.printf("%s\\extsvr.exe", getapppath());
	fvisize = GetFileVersionInfoSize(str, &dwHandle);
	if (fvisize > 0) {
		pfvi = new char[fvisize+100];
		if (GetFileVersionInfo(str, dwHandle, fvisize, (LPVOID)pfvi)) {
			if (VerQueryValue(pfvi, _T("\\"), (LPVOID*)&pverinfo, &uLen)) {
				libversion[0] = (pverinfo->dwProductVersionMS) / 0x10000;
				libversion[1] = (pverinfo->dwProductVersionMS) % 0x10000;
				libversion[2] = (pverinfo->dwProductVersionLS) / 0x10000;
				libversion[3] = (pverinfo->dwProductVersionLS) % 0x10000;
				SendMessage(GetDlgItem(m_hWnd, IDC_LISTLIBRARY), LB_ADDSTRING, 0, (LPARAM)(LPCTSTR)TMPSTR(
					"extsvr.exe : %d, %d, %d, %d", libversion[0], libversion[1], libversion[2], libversion[3]
				));

				libversion[2] %= 2000;
				if (libversion[2]>release_major ||
					(libversion[2] == release_major && libversion[3]>release_minor)) {
					release_major = libversion[2];
					release_minor = libversion[3];
					version.printf("Release number : %d.%d", release_major, release_minor);
				}

			}
		}
		delete[] pfvi;
	}

	str.printf( "About %s", (char *)appname);
	SetWindowText( m_hWnd, str );
	SetDlgItemText(m_hWnd, IDC_STATIC_APP, appname );
	SetDlgItemText(m_hWnd, IDC_STATIC_VERSION, version );
	SetDlgItemText(m_hWnd, IDC_STATIC_COPYRIGHT, copyright );
	SetDlgItemText(m_hWnd, IDC_STATIC_COMPANY, company );
	return TRUE;
}
