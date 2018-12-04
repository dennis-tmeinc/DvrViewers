// dvrclientDlg.h : header file
//

#pragma once

#include "../common/cwin.h"
#include "../common/cstr.h"
#include "../common/util.h"

#include <Windows.h>
#include <mmsystem.h>
#include <MsHTML.h>

#include "dvrclient.h"
#include "Screen.h"
#include "sliderbar.h"
#include "Volumebar.h"
#include "User.h"
#include "SelectDvr.h"
#include "decoder.h"
#include "CopyProgress.h"
#include "bitmapbutton.h"

#if defined( APP_PW_TABLET ) || defined( APP_TVS ) 
#include "bsliderbar.h"
#include "bitmapbutton.h"
#include "volume.h"
#endif

#define MAXSCREEN (6*6)

#define MAXUSER	1000
#define IDUSER	101
#define IDADMIN	102
#define IDINSTALLER	103
#define IDPOLICE	104
#define IDINSPECTOR	105
#define IDOPERATOR  106

#define PLAY_STOP	  0
#define PLAY_PAUSE	  1
#define PLAY_SLOW_MAX 90
#define PLAY_SLOW	  99
#define PLAY_PLAY     100
#define PLAY_FAST	  101
#define PLAY_FAST_MAX 110

#define CLIENTMODE_CLOSE	(0)
#define CLIENTMODE_LIVEVIEW	(1)
#define CLIENTMODE_PLAYBACK (2)
#define CLIENTMODE_PLAYFILE	(3)
#define CLIENTMODE_PLAYSMARTSERVER  (4)

// notify main window to reset screen format
#define WM_SETSCREENFORMAT	(WM_APP + 201)

// DvrclientDlg dialog
class DvrclientDlg : public Dialog
{
// Construction
public:
	DvrclientDlg();	// standard constructor
    ~DvrclientDlg();

// Implementation
protected:

	HANDLE m_hmutx;					// application mutex 

	int   m_minxsize, m_minysize ;
	int   m_screenmode ;
	int   m_screennum ;
	int   m_maxdecscreen ;					// maximum available decoder screen
	int   m_focus ;
	int   m_ptzbuttonpressed ;
	struct dvrtime m_playertime ;
	struct dvrtime m_seektime ;             // remember seeking time (used for timebar updating)
	int   m_clientmode ;					// 0: preview, 1: other(playback)
    int   m_noupdtime ;
    string m_appname ;
    int   m_tbarupdate ;
	string m_clippath ;
	int   m_startonlastday;					// to start play on last day

//	CImage  m_ptzcircle ;
	TCHAR   m_ptzres[16] ;
	RECT    m_ptzcampass_rect ;
	HRGN    m_ptzcampass_rgn ;
	HCURSOR m_ptz_cursor ;
	HCURSOR m_ptz_savedcursor ;

//	RECT  fullscreen ;						// full screen area
    RECT  m_screenrect[MAXSCREEN] ;         // each screen 
	RECT  m_statusiconrect ;
    RECT  m_panelrect ;                     // panel area
    int   m_panelhide ;                     // hide panel ;
    RECT  m_playerrect ;                    // player screen area
    RECT  m_screct ;                        // where video screen
	RECT  m_bottombar ;						// bottom bar contain speed control and company logo
    RECT  m_appnamerect ;                   // Area show APP name
    RECT  m_companylogorect ;               // Area show company logo
	RECT  m_companylinkrect ;               // spartanviewer has a company link area
	RECT  m_speakerrect ;					// speaker ICON rect

    RECT  m_zoomrect ;                      // save rect before zoom (full screen)
    int   m_zoom ;

	HCURSOR	m_companylinkcursor ;

    Screen * m_screen[MAXSCREEN] ;
    int     m_reclight;                 // PW FRONT END

    // volume controls
	VolumeBar m_volume;
	int m_vvolume ;						// cached volume value
	int m_waveoutId ;					// waveout device id
	HMIXER m_hmx ;						// mixer handle

	int m_update_time ;					// miliseconds to update time bar
	int m_playfile_beginpos;
	int m_playfile_endpos;
    Bitmap * m_bkbmp ;

#if defined( APP_PW_TABLET ) || defined( APP_TVS ) 
	BSliderBar m_slider ;
#else
    SliderBar m_slider ;
#endif

#ifdef  APP_PWII_FRONTEND
    int m_pwbuttonstate ;               // PWii Buttons state
#endif 

    // map window
    IHTMLWindow2 * m_mapWindow ;
    int   m_maperror ;
    float m_lati, m_longi;              // gps location
    int   m_updateLocation ;            // for WMVIewer ;

    // Cliplist window
    Window * m_cliplist ;

public:

    void SetZoom();
    int  IsZoom(){ return m_zoom; } 
	void CtrlSet();						// set button status (gray out...)
	void SetLayout();					// setup bottons layout on system up
	void SetVolume() ;					// set volume, 0 - 100
	void SetScreenFormat();
	void SetScreenFormat(char * screenname);
	void Campass(int x, int y);
	void FocusPlayer( Screen * player);
	void StartPlayer();						// start player
	void ClosePlayer();						// close all player window
    void OpenDvr(int playmode, int clientmode, int autoopen=FALSE, char * autoserver=NULL);
	void ScreenDblClk( Screen * player);
	void Seek();
    void SetPlayTime(dvrtime *dvrt);
	DWORD GetDaystateMonthcalendar(int year, int month);
	void UpdateMonthcalendar();
	int DVR_PTZMsg( DWORD msg, DWORD param );
	void SeekTime( struct dvrtime * dtime );	// seek to specified time
	void SetPlaymode( int clientmode )		// set client playing mode
	{
		if( clientmode>0 && clientmode<=CLIENTMODE_PLAYSMARTSERVER ) {
			m_clientmode = clientmode ;
		}
		else {
			m_clientmode = 0 ;
			ClosePlayer();
		}
	}
	void SnapShot( void * param );
	void ExportjpgThread( void * param ) ;
    Bitmap * getbkimg() {
        return m_bkbmp ;
    }
    void getBottombar(RECT * rbot) {
        *rbot = m_bottombar ;
    }

    // Display a location map


    void UpdateMap();
	void ReleaseMap();
    void DisplayMap() ;
	int  getLocation(float &lat, float &lng);

	void UpdateMap_x();
	void DisplayMap_x();

    // Clip List
    void DisplayClipList(int lockclip);
    void CloseClipList();

   	int Playfile(LPCTSTR filename) ;
    int PlayRemote(LPCTSTR rhost, int playmode);

	// change Time Range
	int  GetTimeRange();
	void SetTimeRange( int tirange );

	// move this to public, so slider bar can access them
	void OnBnClickedPrev();
	void OnBnClickedNext();

	void setSingleScreen(int channel);

	int GetSaveClipFilename( string &filename );

	// Generated message map functions
protected:
    static BOOL CALLBACK DvrEnumWindowsProc( HWND hwnd, LPARAM lParam );

	virtual int OnCancel();
	virtual BOOL OnInitDialog();

	BOOL PreProcessMessage(MSG* pMsg);
	void OnSysCommand(UINT nID, LPARAM lParam);
	void OnPaint();
	void OnToolTipNotify( NMHDR * pNMHDR, LRESULT * pResult );
	void OnClose();
    void OnActive( int active );
	void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
	void OnBnClickedExit();
	void OnBnClickedPreview();
	void OnDtnDatetimechangeTimeSelbegin(NMHDR *pNMHDR, LRESULT *pResult);
	void OnDtnDatetimechangeTimeSelend(NMHDR *pNMHDR, LRESULT *pResult);
	void OnSize(UINT nType, int cx, int cy);
	void OnNMCustomdrawSlider(NMHDR *pNMHDR, LRESULT *pResult);
	void OnRButtonUp(UINT nFlags, int x, int y);
	BOOL OnMouseMove(UINT nFlags, int x, int y);
	BOOL OnMouseWheel(int delta, UINT keys, int x, int y);
	void OnLButtonDown(UINT nFlags, int x, int y);
	void OnLButtonUp(UINT nFlags, int x, int y);
	void OnCaptureChanged(HWND hWnd);
	void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
	void OnDestroy();
	void OnBnClickedPlayback();
	void OnBnClickedSmartserver();
	void OnBnClickedPlayfile();
	void OnBnClickedSetup();
	void OnBnClickedCapture();
	void OnBnClickedSavefile();
	void OnBnClickedQuickcap();
	void OnBnClickedTimebegin();
	void OnBnClickedTimeend();
	void OnBnClickedMixaudio();
	void OnBnClickedSlow();
	void OnBnClickedBackward();
	void OnBnClickedPause();
	void OnBnClickedPlay();
	void OnBnClickedForward();
	void OnBnClickedFast();
	void OnBnClickedUsers();
	void OnBnClickedPtzSet();
	void OnBnClickedPtzClear();
	void OnBnClickedPtzGoto();
	void OnMcnGetdaystateMonthcalendar(NMHDR *pNMHDR, LRESULT *pResult);
	void OnMcnSelchangeMonthcalendar(NMHDR *pNMHDR, LRESULT *pResult);
    void OnMcnSelectMonthcalendar(NMHDR *pNMHDR, LRESULT *pResult);
	void OnNMReleasedcaptureMonthcalendar(NMHDR *pNMHDR, LRESULT *pResult);
	void OnTimer(UINT_PTR nIDEvent);
	void OnCbnSelchangeComboTimerange();
	void OnCbnSelchangeComboOfficerId();

	BOOL OnEraseBkgnd(HDC hDC);
	void OnBnClickedDisconnect();
	LRESULT OnMixerMessage( WPARAM wParam, LPARAM lParam ) ;
	void OnBnClickedStep();
	void OnBnClickedButtonPlay();
	void OnBnClickedMap();
	BOOL OnDeviceChange( UINT nEventType, DWORD dwData );

#ifdef APP_TVS
//	virtual void OnBnClickedButtonTvslog();
	void OnBnClickedButtonTvsinfo();
#endif

#ifdef IDC_EXPORTJPG
	void OnBnClickedExportjpg();
#endif
	void OnHScroll(UINT nSBCode, UINT nPos, HANDLE hScrollBar);
	
#ifdef  APP_PWII_FRONTEND
    void OnBnClickedButtonRec();
    void OnBnClickedButtonRecall();
    
    void OnPWIIButton( int nIDCtl, int select );

    void OnBnClickedButtonTm();
    void OnBnClickedButtonLp();
    void OnBnClickedButtonRec1();
    void OnBnClickedButtonRec2();
	void OnBnClickedButtonNewOfficerId();
	void OnBnClickedButtonTagEvent();
#endif

#ifdef  APP_PW_TABLET
	void OnBnClickedBCovert();
	void OnBnClickedBRange( int Id );
	void OnBnClickedButtonBOfficerId();
	void OnBnClickedButtonBTagEvent();
	void OnBnClickedButtonBCal();

	void OnBnTrioview();
	void OnBnRearview();
	void OnBnHoodview();
	void OnBnAuxview();
	void OnBnPanLeft();
	void OnBnPanCenter();
	void OnBnPanRight();
#endif

#ifdef SPARTAN_APP
    void OnBnClickedScreen1();
	void OnBnClickedScreen2();
	void OnBnClickedScreen4();
#endif

	int OnDisplayChange();			// display resolution changed, (rotate)

    int OnCopyData( PCOPYDATASTRUCT pcd);

	void DisplayVolume( int x, int y );
		
    virtual int OnCommand( int Id, int Event ) {
		int res = TRUE ;
		switch( Id ) {
#ifdef IDC_COMBO_OFFICERID
		case IDC_COMBO_OFFICERID:
			if( Event == CBN_SELCHANGE )
				OnCbnSelchangeComboOfficerId();
			break;
#endif
		case IDC_COMBO_TIMERANGE:
			if( Event == CBN_SELCHANGE )
				OnCbnSelchangeComboTimerange();
			break;

		case IDC_BUTTON_PLAY: OnBnClickedButtonPlay(); break ;
		case IDC_CAPTURE: OnBnClickedCapture(); break ;
		case IDC_DISCONNECT: OnBnClickedDisconnect(); break ;
		case IDC_EXIT: OnBnClickedExit(); break ;
		case IDC_FAST: OnBnClickedFast(); break ;
		case IDC_FORWARD: OnBnClickedForward(); break ;
		case IDC_BACKWARD: OnBnClickedBackward(); break ;
		case IDC_MAP: OnBnClickedMap(); break ;
		case IDC_MIXAUDIO: OnBnClickedMixaudio(); break ;
		case IDC_NEXT: OnBnClickedNext(); break ;
		case IDC_PAUSE: OnBnClickedPause(); break ;
		case IDC_PLAY: OnBnClickedPlay(); break ;
		case IDC_PLAYBACK: OnBnClickedPlayback(); break ;
		case IDC_PLAYFILE: OnBnClickedPlayfile(); break ;
		case IDC_PREV: OnBnClickedPrev(); break ;
		case IDC_PREVIEW: OnBnClickedPreview(); break ;
		case IDC_PTZ_CLEAR: OnBnClickedPtzClear(); break ;
		case IDC_PTZ_GOTO: OnBnClickedPtzGoto(); break ;
		case IDC_PTZ_SET: OnBnClickedPtzSet(); break ;
		case IDC_QUICKCAP: OnBnClickedQuickcap(); break ;
		case IDC_SAVEFILE: OnBnClickedSavefile(); break ;
		case IDC_SETUP: OnBnClickedSetup(); break ;
		case IDC_SLOW: OnBnClickedSlow(); break;
		case IDC_SMARTSERVER:
			OnBnClickedSmartserver();
			break;
		case IDC_STEP:
			OnBnClickedStep();
			break;

		case IDC_TIMEBEGIN:
			OnBnClickedTimebegin();
			break;
		case IDC_TIMEEND:
			OnBnClickedTimeend();
			break;
		case IDC_USERS:
			OnBnClickedUsers();
			break;

#ifdef SPARTAN_APP
		case IDC_SPARTAN_1:
			OnBnClickedScreen1();
			break;
		case IDC_SPARTAN_2:
			OnBnClickedScreen2();
			break;
		case IDC_SPARTAN_4:
			OnBnClickedScreen4();
			break;
#endif

#ifdef  APP_PWII_FRONTEND

		case IDC_BUTTON_CAM1:
			OnBnClickedButtonRec1();
			break;
		case IDC_BUTTON_CAM2:
			OnBnClickedButtonRec2();
			break;
		case IDC_BUTTON_TM:
			OnBnClickedButtonTm();
			break;
		case IDC_BUTTON_LP:
			OnBnClickedButtonLp();
			break;
		case IDC_BUTTON_NEWOFFICERID:
			OnBnClickedButtonNewOfficerId();
			break;
		case IDC_BUTTON_TAGEVENT:
			OnBnClickedButtonTagEvent();
			break;
#endif

#ifdef APP_PW_TABLET
		case IDC_BUTTON_BOFFICER:
			OnBnClickedButtonBOfficerId();
			break;
		case IDC_BUTTON_BTAG:
			OnBnClickedButtonBTagEvent();
			break;
		case IDC_BUTTON_BCOVERT:
			OnBnClickedBCovert();
			break;
		case IDC_BUTTON_BPLAYLIVE:
		case IDC_BUTTON_BPLAYBACK:
			OnBnClickedButtonPlay();
			break;
		case IDC_BUTTON_BDAYRANGE:
		case IDC_BUTTON_BHOURRANGE:
		case IDC_BUTTON_BQUATRANGE:
			OnBnClickedBRange( Id );
			break;

		case IDC_BUTTON_BCAL:
			OnBnClickedButtonBCal();
			break;

		case IDC_BUTTON_TRIOVIEW:
			OnBnTrioview();
			break;
		case IDC_BUTTON_PANLEFT:
			OnBnPanLeft();
			break;
		case IDC_BUTTON_PANCENTER:
			OnBnPanCenter();
			break;
		case IDC_BUTTON_PANRIGHT:
			OnBnPanRight();
			break;
		case IDC_BUTTON_REARVIEW:
			OnBnRearview();
			break;
		case IDC_BUTTON_HOODVIEW:
			OnBnHoodview();
			break;
		case IDC_BUTTON_AUXVIEW:
			OnBnAuxview();
			break;

		case IDC_BUTTON_CLOSE:
			EndDialog();
			break;

#endif

#ifdef APP_TVS
#ifdef IDC_BUTTON_TVSINFO
		case IDC_BUTTON_TVSINFO:
			OnBnClickedButtonTvsinfo();
			break;
#endif
#endif
#ifdef IDC_EXPORTJPG
		case IDC_EXPORTJPG:
			OnBnClickedExportjpg();
			break;
#endif

#ifdef IDC_BUTTON_BVOLUME
		case IDC_BUTTON_BVOLUME :
			DisplayVolume();
			break;
#endif

		default:
			return Dialog::OnCommand( Id, Event );
		}
		return res ;
	}

	virtual int OnNotify( LPNMHDR pnmhdr ) {
		LRESULT lres=0 ;
		switch( pnmhdr->code ) {
			case TTN_NEEDTEXT: 
				OnToolTipNotify( pnmhdr, &lres ); 
				break;

			case DTN_DATETIMECHANGE: 
				if( pnmhdr->idFrom == IDC_TIME_SELBEGIN ) OnDtnDatetimechangeTimeSelbegin(pnmhdr, &lres) ; 
				if( pnmhdr->idFrom == IDC_TIME_SELEND) OnDtnDatetimechangeTimeSelend(pnmhdr, &lres) ; 
				break ;

			case MCN_GETDAYSTATE: 
				if( pnmhdr->idFrom == IDC_MONTHCALENDAR) OnMcnGetdaystateMonthcalendar(pnmhdr, &lres) ; 
				break ;

			case MCN_SELCHANGE: 
				if( pnmhdr->idFrom == IDC_MONTHCALENDAR) OnMcnSelchangeMonthcalendar(pnmhdr, &lres) ; 
				break ;

			case MCN_SELECT: 
				if( pnmhdr->idFrom == IDC_MONTHCALENDAR) OnMcnSelectMonthcalendar(pnmhdr, &lres) ;
				break ;

			case NM_CUSTOMDRAW: 
				if( pnmhdr->idFrom == IDC_SLIDER) {
					OnNMCustomdrawSlider(pnmhdr, &lres) ;
				}
				else {
					return FALSE ;
				}
				break ;

			case NM_RELEASEDCAPTURE: 
				if( pnmhdr->idFrom == IDC_MONTHCALENDAR) OnNMReleasedcaptureMonthcalendar(pnmhdr, &lres) ; 
				break ;
		
			default:
				return FALSE ;
		}
        SetWindowLongPtr(m_hWnd, DWLP_MSGRESULT, lres);
		return TRUE ;
	}

	// dialog box procedures
	virtual int DlgProc(UINT message, WPARAM wParam, LPARAM lParam)
	{
		int res = TRUE ;
		switch (message)
		{
		case MM_MIXM_LINE_CHANGE:
			return OnMixerMessage(wParam, lParam);

		case WM_CAPTURECHANGED:
            OnCaptureChanged((HWND)lParam);
			break ;

		case WM_CLOSE:
			OnClose();
			res = TRUE;
			break;

		case WM_DESTROY:
			OnDestroy();
			break;

		case WM_DEVICECHANGE:
			OnDeviceChange( wParam, lParam );
			break;

        case WM_ACTIVATE:
            OnActive( LOWORD(wParam) );
            break;

		case WM_DRAWITEM:
			OnDrawItem(wParam, (LPDRAWITEMSTRUCT)lParam);
			break;

		case WM_ERASEBKGND:
			res=OnEraseBkgnd((HDC)wParam);
			break;

		case WM_GETMINMAXINFO:
			OnGetMinMaxInfo((MINMAXINFO *)lParam);
			break;

		case WM_HSCROLL:
			OnHScroll( LOWORD(wParam), HIWORD(wParam), (HANDLE)lParam) ;
			break;

		case WM_LBUTTONDOWN:
			OnLButtonDown(wParam, MOUSE_POSX(lParam), MOUSE_POSY(lParam));
			break;

		case WM_LBUTTONUP:
			OnLButtonUp(wParam, MOUSE_POSX(lParam), MOUSE_POSY(lParam));
			break;

		case WM_RBUTTONUP:
			OnRButtonUp(wParam, MOUSE_POSX(lParam), MOUSE_POSY(lParam));
			break;

		case WM_MOUSEMOVE:
			res = OnMouseMove(wParam, MOUSE_POSX(lParam), MOUSE_POSY(lParam));
			break;

		case WM_MOUSEWHEEL:
			res = OnMouseWheel((int)(short int)HIWORD(wParam), LOWORD(wParam), MOUSE_POSX(lParam), MOUSE_POSY(lParam));
			break;

		case WM_PAINT:
			OnPaint();
			break;

		case WM_SIZE:
			OnSize( wParam, LOWORD(lParam), HIWORD(lParam));
			break;

		case WM_SYSCOMMAND:
			OnSysCommand(wParam, lParam);
			res = FALSE ;	// let system do the default
			break;

		case WM_TIMER:
			OnTimer(wParam);
			break;

        case WM_COPYDATA:
            res = OnCopyData((PCOPYDATASTRUCT) lParam);
            break ;

		case WM_DISPLAYCHANGE :
			res = OnDisplayChange();
			break;

		case WM_PARENTNOTIFY:
			res = FALSE ;
			break;

		case WM_SETSCREENFORMAT:
			SetScreenFormat();
			break;

		default:
			res = Dialog::DlgProc( message, wParam, lParam ) ;
		}

		return res ;
	}
};



extern DvrclientDlg * g_maindlg ;

// screen format structures

struct screenrect {
	double left, top, right, bottom ;
};

struct screenmode {
	TCHAR * screenname ;
	int numscreen ;		// number of screens
	int xnum, ynum ;	// screen ratio
	screenrect * screen ;
	int * channel ;		// default channel to open, NULL to use default
	int align ;			// 0: center, 1: left, 2: right, 3: top, 4, bottom
};

extern struct screenmode screenmode_table[] ;
extern int screenmode_table_num ;
extern int g_screenmode ;
extern int g_singlescreenmode ;				// 1: single screen mode
extern int g_ratiox ;                      // screen ratio (4:3) or (16:9)
extern int g_ratioy ;

#define PTZ_DEFAULT	0

#define TIMER_UPDATE 1
#define TIMER_SEEK	  2
#define TIMER_STEP	3
#define TIMER_INITMINITRACK 4 
#define TIMER_RECLIGHT 5
#define TIMER_AUTOSTART 6
#define TIMER_UPDATEMAP 7
#define TIMER_UPDATESCREENS 8

#ifndef SCREENBACKGROUNDCOLOR
#define	SCREENBACKGROUNDCOLOR	( RGB(0x72, 0x7c, 0x1a) )
#endif
#ifndef BOTTOMBARCOLOR
#define BOTTOMBARCOLOR			( RGB(0x83, 0x8d, 0x3c) )
#endif

#ifndef FORWARD_JUMP
#define FORWARD_JUMP   (20)
#endif  //FORWARD_JUMP

#ifndef BACKWARD_JUMP
#define BACKWARD_JUMP   (20)
#endif  //BACKWARD_JUMP

extern HWND g_mainwnd ;
extern DvrclientDlg * g_maindlg ;
extern int	 g_minitracksupport ;

extern string  g_username ;
extern int     g_usertype ;		// priviages
extern int     g_MixSound ;
extern int     g_playstat ;

extern int PTZ_ctrl_id[];
extern int PTZ_hide_ctrl_id[];
extern int playback_ctrl_id[];
extern double ptz_points[];

extern int top_ctrl_id[];
struct btbar_ctl {
	int id ;
	int xadj ;				// positive, offert to x=0, nagtive, offset to x=w
	TCHAR * imgname ;       // reference control size
	int showmask ;			// bit0: default, bit1: live mode, bit2: playbackmode
} ;

struct bt_layout {
	int id ;
	int posx ;			// positive, from ledt, negtive, from right
	int posy ;			// positive, from top,  negtive from bottom
	int w, h ;				// size
	int showmask ;			// bit0: default, bit1: live mode, bit2: playbackmode
} ;

struct bmp_button_type {
	int id ;
	TCHAR * img ;
	TCHAR * img_sel ;
};

extern btbar_ctl btbar_ctls[] ;
extern const int control_align  ;       // control layout, 0: left, 1: right ;
extern const int btbar_refid ;	        // Volue Slider as ref
extern const int btbar_align ;		    // >=0: left align, <0: right align


