// dvrplayer.cpp : DVR player object
//

#include "stdafx.h"

#include <WindowsX.h>

#include "ply301.h"
#include "dvrnet.h"
#include "dvrsetup.h"

static SOCKET cfgsocket ;

int GetSystemSetup(void * buf, int bufsize)
{
	return net_GetSystemSetup(cfgsocket, buf, bufsize);
}

int SetSystemSetup(void * buf, int bufsize)
{
	return net_SetSystemSetup(cfgsocket, buf, bufsize);
}

int GetChannelSetup(void * buf, int bufsize, int channel)
{
	return net_GetChannelSetup(cfgsocket, buf, bufsize, channel );
}

int SetChannelSetup(void * buf, int bufsize, int channel)
{
	return net_SetChannelSetup( cfgsocket, buf, bufsize, channel );
}

int DVRGetChannelState(struct channelstatus * cs, int nchannel)
{
	int res = net_DVRGetChannelState(cfgsocket, cs, nchannel*sizeof(struct channelstatus) );
	return res/sizeof(struct channelstatus) ;
}

// return 0: error, n: number of int return in version
int DVR_Version(int * version )
{
	int v[4] ;
	if( net_dvrversion(cfgsocket, v ) ) {
		version[0]=v[0];
		version[1]=v[1];
		version[2]=v[2];
		version[3]=v[3];
		return 4 ;
	}
	return 0;
}

int DVRGetTimezone( TIME_ZONE_INFORMATION * tzi )
{
	return net_DVRGetTimezone(cfgsocket, tzi);
}

int DVRSetTimezone( TIME_ZONE_INFORMATION * tzi )
{
	return net_DVRSetTimezone(cfgsocket, tzi);
}

int DVRGetLocalTime(SYSTEMTIME * st )
{
	return net_DVRGetLocalTime(cfgsocket, st);
}

int DVRSetLocalTime(SYSTEMTIME * st )
{
	return net_DVRSetLocalTime(cfgsocket, st);
}

int DVRGetSystemTime( SYSTEMTIME * st )
{
	return net_DVRGetSystemTime(cfgsocket, st);
}

int DVRSetSystemTime( SYSTEMTIME * st )
{
	return net_DVRSetSystemTime(cfgsocket, st);
}

// Get time zone info
// return buffer size returned, return 0 for error
int DVR2ZoneInfo( char ** zoneinfo)
{
	return net_DVR2ZoneInfo( cfgsocket, zoneinfo) ;
}

// Set DVR time zone
//   timezone: timezone name to set
// return 1:success, 0:fail
int DVR2SetTimeZone(char * timezone)
{
	return net_DVR2SetTimeZone(cfgsocket, timezone);
}

// Get DVR time zone
// return buffer size returned, return 0 for error
//    timezone: DVR timezone name 
int DVR2GetTimeZone( char ** timezone )
{
	return net_DVR2GetTimeZone( cfgsocket, timezone );
}

// get dvr (version2) time
//    st: return dvr time
//    system: 0-get localtime, 1-get system time
// return: 0: fail, 1: success
int DVR2GetTime(SYSTEMTIME * st, int system)
{
	return net_DVR2GetTime(cfgsocket, st, system );
}

// set dvr (version2) time
//    st: return dvr time
//    system: 0-set localtime, 1-set system time
// return: 0: fail, 1: success
int DVR2SetTime(SYSTEMTIME * st, int system)
{
	return net_DVR2SetTime(cfgsocket, st, system );
}


// Kill DVR application, if Reboot != 0 also reset DVR
int DVRKill(int Reboot )
{
	return net_DVRKill(cfgsocket, Reboot);
}

// DIALOG ITEM value
#define DLGITEMVAR string VAR_GT_STR ;
#define GETITEMSTR(id,str) (GetDlgItemText( m_hWnd, id, str.tcssize(512), 512 ))
#define GETITEMTXT(id,txt) (GetDlgItemText( m_hWnd, id, VAR_GT_STR.tcssize(512), 512), strcpy(txt, VAR_GT_STR))
#define GETITEMINT(id,i)   (i=GetDlgItemInt(m_hWnd, id, NULL, TRUE ))
#define SETITEMTXT(id,str) (VAR_GT_STR=str, SetDlgItemText( m_hWnd, id, VAR_GT_STR ))
#define SETITEMINT(id,i)   SetDlgItemInt(m_hWnd, id, i, TRUE )
#define GETITEMCHECK(id, ck)  ( ck=IsDlgButtonChecked(m_hWnd,id) )
#define SETITEMCHECK(id, check)  ( CheckDlgButton(m_hWnd, id, check) )

#define WRITEVAR(var) fwrite(&(var), 1, sizeof(var), fh)
#define READVAR(var)  fread( &(var), 1, sizeof(var), fh)

class SystemPage * pSystemPage ;
PropSheet * PropSheet::m_curpropsheet ;

SystemPage::SystemPage():
  PropSheetPage(IDD_SYSTEM_PAGE)
{
    pSystemPage=this ;

    memset( &sys, 0, sizeof(sys));
    GetSystemSetup(&sys, sizeof(struct system_stru)) ;
}

  BOOL SystemPage::OnInitDialog() 
{
//	if( !g_noptz ) {
		ShowWindow( GetDlgItem(m_hWnd, IDC_PTZ_BOX), SW_HIDE);
		ShowWindow( GetDlgItem(m_hWnd, IDC_PTZ_ENABLE), SW_HIDE);
		ShowWindow( GetDlgItem(m_hWnd, IDC_PTZ_TXT1), SW_HIDE);
		ShowWindow( GetDlgItem(m_hWnd, IDC_PTZ_PORT), SW_HIDE);
		ShowWindow( GetDlgItem(m_hWnd, IDC_PTZ_TXT2), SW_HIDE);
		ShowWindow( GetDlgItem(m_hWnd, IDC_PTZ_BAUDRATE), SW_HIDE);
		ShowWindow( GetDlgItem(m_hWnd, IDC_PTZ_TXT3), SW_HIDE);
		ShowWindow( GetDlgItem(m_hWnd, IDC_PTZ_PROTOCOL), SW_HIDE);
//	}
   		ShowWindow( GetDlgItem(m_hWnd, IDC_DVRUSERMAN), SW_HIDE);

    PutData();
	return TRUE;  
}

void SystemPage::PutData() 
{
    DLGITEMVAR ;
    string str ;
    int i;
    if( !IsWindow(m_hWnd) ) 
    {
        return;
    }

    SETITEMINT(IDC_ALARMNUMBER, sys.alarmnum);
	SETITEMINT(IDC_BREAKSIZE, sys.breakSize/(1024*1024) );
	SETITEMINT(IDC_BREAKTIME, sys.breakTime);
	SETITEMINT(IDC_CAMERANUMBER, sys.cameranum);
	SETITEMINT(IDC_SENSORNUMBER, sys.sensornum);
	SETITEMINT(IDC_MINDISKSPACE, sys.minDiskSpace/(1024*1024));

    if( sys.breakMode ) {
        SETITEMCHECK(IDC_BREAKBYTIME, 1 ) ;
    }
    else {
        SETITEMCHECK(IDC_BREAKBYSIZE, 1 ) ;
    }
	
    SETITEMINT(IDC_SHUTDOWNDELAY, sys.shutdowndelay);
	SETITEMINT(IDC_EVMPOSTLOCK, sys.eventmarker_postlock);
	SETITEMINT(IDC_EVMPRELOCK, sys.eventmarker_prelock);
	SETITEMCHECK(IDC_PTZ_ENABLE, sys.ptz_en);
	SETITEMINT(IDC_PTZ_BAUDRATE, sys.ptz_baudrate);
    ComboBox_SetCurSel(GetDlgItem(m_hWnd, IDC_PTZ_PORT), sys.ptz_port);
    ComboBox_SetCurSel(GetDlgItem(m_hWnd, IDC_PTZ_PROTOCOL), sys.ptz_protocol);
    SETITEMCHECK(IDC_VIDEO_ENCRYPT, sys.videoencrpt_en );

    SETITEMTXT(IDC_AUTODISK, sys.autodisk);
	SETITEMTXT(IDC_IOMODULE, sys.productid);
	SETITEMTXT(IDC_SERVERNAME, sys.ServerName);
    m_videopassword = "********" ;
	SETITEMTXT(IDC_EDIT_VIDEOPASSWORD, m_videopassword);
    ListBox_AddString(GetDlgItem(m_hWnd, IDC_EVENTMARKER), str );
	if( sys.eventmarker_enable == 0 ) {
		sys.eventmarker = 0 ;
	}
    ListBox_ResetContent(GetDlgItem(m_hWnd, IDC_EVENTMARKER));
    for( i=0; i<sys.sensornum; i++ ) {
        str = sys.sensorname[i] ;
        ListBox_AddString(GetDlgItem(m_hWnd, IDC_EVENTMARKER), str );
    }
    for( i=0; i<sys.sensornum; i++ ) {
        if(sys.eventmarker&(1<<i)) {
            ListBox_SetSel(GetDlgItem(m_hWnd, IDC_EVENTMARKER), TRUE, i );
        }
    }
}

void SystemPage::GetData() 
{
    DLGITEMVAR  ;
    int iv ;
    int i;
    if( !IsWindow(m_hWnd) ) 
    {
        return;
    }

    GETITEMINT(IDC_ALARMNUMBER, sys.alarmnum);
	GETITEMINT(IDC_BREAKSIZE, iv );
    sys.breakSize = iv * 1024 * 1024 ;
	GETITEMINT(IDC_BREAKTIME, sys.breakTime);
	GETITEMINT(IDC_CAMERANUMBER, sys.cameranum);
	GETITEMINT(IDC_SENSORNUMBER, sys.sensornum);
	GETITEMINT(IDC_MINDISKSPACE, iv );
    sys.minDiskSpace = iv *(1024*1024) ;
    GETITEMCHECK(IDC_BREAKBYTIME, sys.breakMode); // fix this
    GETITEMINT(IDC_SHUTDOWNDELAY, sys.shutdowndelay);
	GETITEMINT(IDC_EVMPOSTLOCK, sys.eventmarker_postlock);
	GETITEMINT(IDC_EVMPRELOCK, sys.eventmarker_prelock);
	GETITEMCHECK(IDC_PTZ_ENABLE, sys.ptz_en);
	GETITEMINT(IDC_PTZ_BAUDRATE, sys.ptz_baudrate);
    sys.ptz_port=ComboBox_GetCurSel(GetDlgItem(m_hWnd, IDC_PTZ_PORT));
    sys.ptz_protocol = ComboBox_GetCurSel(GetDlgItem(m_hWnd, IDC_PTZ_PROTOCOL));
    GETITEMCHECK(IDC_VIDEO_ENCRYPT, sys.videoencrpt_en );

    GETITEMTXT(IDC_AUTODISK, sys.autodisk);
	GETITEMTXT(IDC_IOMODULE, sys.productid);
	GETITEMTXT(IDC_SERVERNAME, sys.ServerName);
	GETITEMTXT(IDC_EDIT_VIDEOPASSWORD, m_videopassword);
    if( strcmp( m_videopassword, "********" )!=0 ) {
        strcpy( sys.videopassword, m_videopassword );
    }

    GETITEMCHECK(IDC_VIDEO_ENCRYPT, sys.videoencrpt_en );

    sys.eventmarker = 0 ;
    for( i=0; i<sys.sensornum; i++ ) {
        if(ListBox_GetSel(GetDlgItem(m_hWnd, IDC_EVENTMARKER), i)) {
            sys.eventmarker|=(1<<i);
        }
    }
    if( sys.eventmarker ) {
        sys.eventmarker_enable = 1;
    }
    else {
        sys.eventmarker_enable = 0;
    }
}

void SystemPage::OnKillActive() 
{
    GetData();
}

void SystemPage::Save(FILE * fh) 
{
	int tag = 0x5638 ;
	long pos = ftell(fh);
	pos = (pos+0x100) & 0xffffff00 ;
	fseek( fh, pos, SEEK_SET );
    WRITEVAR(tag);
	WRITEVAR(sys);
}

void SystemPage::Load(FILE * fh) 
{
	int tag  ;
	long pos = ftell(fh);
	pos = (pos+0x100) & 0xffffff00 ;
	fseek( fh, pos, SEEK_SET );
	READVAR(tag);
	if( tag!=0x5638 ) return ;
    READVAR( sys );
}

void SystemPage::Set() 
{
	SetSystemSetup(&sys, sizeof(struct system_stru)) ;
}

/////////////////////////////////////////////////////////////////////////////
// CameraPage

CameraPage::CameraPage() :       
    PropSheetPage(IDD_CAMERA_PAGE) 
{
    cam_attr = new struct DvrChannel_attr [pSystemPage->sys.cameranum] ;
	for( int i=0; i<pSystemPage->sys.cameranum ; i++ ) {
		memset(&cam_attr[i], 0, sizeof( struct DvrChannel_attr ) );
		GetChannelSetup(&cam_attr[i], sizeof( struct DvrChannel_attr), i );
	}
    tabindex=0;
}

CameraPage::~CameraPage()
{
	delete [] cam_attr ;
}

BOOL CameraPage::OnInitDialog() 
{
    string str ;
	int i;

//	if( g_noptz ) {
    ShowWindow( GetDlgItem(m_hWnd, IDC_PTZ_ADDR), SW_HIDE);
    ShowWindow( GetDlgItem(m_hWnd, IDC_PTZ_BOX), SW_HIDE);
    ShowWindow( GetDlgItem(m_hWnd, IDC_SPIN_PTZ_ADDR), SW_HIDE);
//	}

    m_Tab.attach( GetDlgItem(m_hWnd, IDC_TAB_CAMERA ));

	for( i=0 ; i<pSystemPage->sys.cameranum; i++ ) {
        TCITEM tci ;
        tci.mask=TCIF_TEXT ;
        sprintf( str.strsize(64), "Camera%d", i+1 );
        tci.mask=TCIF_TEXT ;
        tci.pszText = str ;
        TabCtrl_InsertItem(m_Tab.getHWND(), i, &tci ) ;
	}

   	tabindex = 0 ;
    TabCtrl_SetCurSel( m_Tab.getHWND(), tabindex );

	RECT clientrect ;
	GetClientRect(m_hWnd, &clientrect); 
    MoveWindow( m_Tab.getHWND(), 
        clientrect.left,
        clientrect.top, 
        clientrect.right - clientrect.left ,
        clientrect.bottom - clientrect.top,
        TRUE );

    SendMessage( GetDlgItem( m_hWnd, IDC_QUALITY), TBM_SETRANGEMIN, FALSE, 0 );
    SendMessage( GetDlgItem( m_hWnd, IDC_QUALITY), TBM_SETRANGEMAX, TRUE, 10 );

    SendMessage( GetDlgItem( m_hWnd, IDC_MOTSENSITIVE), TBM_SETRANGEMIN, FALSE, 0 );
    SendMessage( GetDlgItem( m_hWnd, IDC_MOTSENSITIVE), TBM_SETRANGEMAX, TRUE, 6 );

    // init droplists
    // video formats

    struct cb_initstr {
        int id ;
        const char * initstr ;
    } cb_init[] = {
        { IDC_PICTUREFORMAT, "352x240" },
        { IDC_PICTUREFORMAT, "704x240" },
        { IDC_PICTUREFORMAT, "528x320" },
        { IDC_PICTUREFORMAT, "704x480" },
        { IDC_RECORDMODE, "Continue" },
        { IDC_RECORDMODE, "Trigger by sensor" },
        { IDC_RECORDMODE, "Trigger by motion" },
        { IDC_RECORDMODE, "Trigger by sensor/motion" },
        { IDC_ALARMRECPAT, "OFF" },
        { IDC_ALARMRECPAT, "ON" },
        { IDC_ALARMRECPAT, "0.5s Flash" },
        { IDC_ALARMRECPAT, "1s Flash" },
        { IDC_ALARMRECPAT, "2s Flash" },
        { IDC_ALARMRECPAT, "4s Flash" },
        { IDC_ALARMRECPAT, "8s Flash" },
        { IDC_ALARMVIDEOLOSTPAT, "0.5s Flash" },
        { IDC_ALARMVIDEOLOSTPAT, "1s Flash" },
        { IDC_ALARMVIDEOLOSTPAT, "2s Flash" },
        { IDC_ALARMVIDEOLOSTPAT, "4s Flash" },
        { IDC_ALARMVIDEOLOSTPAT, "8s Flash" },
        { IDC_ALARMMOTIONPAT, "0.5s Flash" },
        { IDC_ALARMMOTIONPAT, "1s Flash" },
        { IDC_ALARMMOTIONPAT, "2s Flash" },
        { IDC_ALARMMOTIONPAT, "4s Flash" },
        { IDC_ALARMMOTIONPAT, "8s Flash" },
        { 0, NULL } 
    };
    // init combobox string
    for( i=0; i<200; i++ ) {
        if( cb_init[i].id==0 ) break;
        str = cb_init[i].initstr ;
        ComboBox_AddString( GetDlgItem(m_hWnd, cb_init[i].id), str );
    }

    PutData();

	return TRUE; 
}

void CameraPage::OnSelchangeTabCamera() 
{
    int tab = TabCtrl_GetCurSel( m_Tab.getHWND() );
	if( tab!=tabindex ) {
		GetData();
		tabindex = tab ;
		PutData();
	}
}

void CameraPage::OnKillActive() 
{
    tabindex = TabCtrl_GetCurSel( m_Tab.getHWND() );
	GetData();
}

void CameraPage::PutData() 
{
    DLGITEMVAR ;
    string str ;
    int i;
    if( !IsWindow(m_hWnd) ) 
    {
        return;
    }
    
    // init sensor related controls
    ComboBox_ResetContent(GetDlgItem( m_hWnd, IDC_TRIGPORT1));
    ComboBox_ResetContent(GetDlgItem( m_hWnd, IDC_TRIGPORT2));
    ComboBox_ResetContent(GetDlgItem( m_hWnd, IDC_TRIGPORT3));
    ComboBox_ResetContent(GetDlgItem( m_hWnd, IDC_TRIGPORT4));
	for( i=0; i<pSystemPage->sys.sensornum ; i++ ) {
        str = pSystemPage->sys.sensorname[i] ;
        ComboBox_AddString( GetDlgItem( m_hWnd, IDC_TRIGPORT1), str );
        ComboBox_AddString( GetDlgItem( m_hWnd, IDC_TRIGPORT2), str );
        ComboBox_AddString( GetDlgItem( m_hWnd, IDC_TRIGPORT3), str );
        ComboBox_AddString( GetDlgItem( m_hWnd, IDC_TRIGPORT4), str );
	}

	// init alarm related controls
    ComboBox_ResetContent(GetDlgItem( m_hWnd, IDC_ALARMRECPORT));
    ComboBox_ResetContent(GetDlgItem( m_hWnd, IDC_ALARMVIDEOLOSTPORT));
    ComboBox_ResetContent(GetDlgItem( m_hWnd, IDC_ALARMMOTIONPORT));
	for( i=0; i<pSystemPage->sys.alarmnum ; i++ ) {
        sprintf(str.strsize(20), "%d", i+1);
        ComboBox_AddString( GetDlgItem( m_hWnd, IDC_ALARMRECPORT), str );
        ComboBox_AddString( GetDlgItem( m_hWnd, IDC_ALARMVIDEOLOSTPORT), str );
        ComboBox_AddString( GetDlgItem( m_hWnd, IDC_ALARMMOTIONPORT), str );
	}

	if(!cam_attr[tabindex].GPS_enable ){
        ShowWindow( GetDlgItem(m_hWnd, IDC_GPS), SW_HIDE);
        ShowWindow( GetDlgItem(m_hWnd, IDC_GPSBOX), SW_HIDE);
        ShowWindow( GetDlgItem(m_hWnd, IDC_GPSDISP), SW_HIDE);
        ShowWindow( GetDlgItem(m_hWnd, IDC_GPSENGLISH), SW_HIDE);
        ShowWindow( GetDlgItem(m_hWnd, IDC_GPSMETRIC), SW_HIDE);
    }

    SendMessage( GetDlgItem( m_hWnd, IDC_QUALITY), TBM_SETPOS, TRUE, 10-cam_attr[tabindex].PictureQuality );

    ComboBox_SetCurSel(GetDlgItem( m_hWnd, IDC_TRIGPORT1), cam_attr[tabindex].Sensor[0]) ;
    ComboBox_SetCurSel(GetDlgItem( m_hWnd, IDC_TRIGPORT2), cam_attr[tabindex].Sensor[1]) ;
    ComboBox_SetCurSel(GetDlgItem( m_hWnd, IDC_TRIGPORT3), cam_attr[tabindex].Sensor[2]) ;
    ComboBox_SetCurSel(GetDlgItem( m_hWnd, IDC_TRIGPORT4), cam_attr[tabindex].Sensor[3]) ;

    ComboBox_SetCurSel(GetDlgItem( m_hWnd, IDC_ALARMRECPORT), cam_attr[tabindex].RecordAlarm) ;
    ComboBox_SetCurSel(GetDlgItem( m_hWnd, IDC_ALARMRECPAT), cam_attr[tabindex].RecordAlarmPat) ;
    ComboBox_SetCurSel(GetDlgItem( m_hWnd, IDC_ALARMVIDEOLOSTPORT), cam_attr[tabindex].VideoLostAlarm) ;
    ComboBox_SetCurSel(GetDlgItem( m_hWnd, IDC_ALARMVIDEOLOSTPAT), cam_attr[tabindex].VideoLostAlarmPat) ;
    ComboBox_SetCurSel(GetDlgItem( m_hWnd, IDC_ALARMMOTIONPORT), cam_attr[tabindex].MotionAlarm) ;
    ComboBox_SetCurSel(GetDlgItem( m_hWnd, IDC_ALARMMOTIONPAT), cam_attr[tabindex].MotionAlarmPat) ;

    SendMessage( GetDlgItem( m_hWnd, IDC_MOTSENSITIVE), TBM_SETPOS, TRUE, cam_attr[tabindex].MotionSensitivity );

	if( cam_attr[tabindex].GPSUnit == 0 ) {
        SETITEMCHECK(IDC_GPSENGLISH, BST_CHECKED );
        SETITEMCHECK(IDC_GPSMETRIC, BST_UNCHECKED ) ;
	}
	else {
        SETITEMCHECK(IDC_GPSENGLISH, BST_UNCHECKED );
        SETITEMCHECK(IDC_GPSMETRIC, BST_CHECKED ) ;
	}

    EnableWindow( GetDlgItem(m_hWnd, IDC_STATIC_CONTROLMODE), cam_attr[tabindex].BitrateEn );
    EnableWindow( GetDlgItem(m_hWnd, IDC_VBR), cam_attr[tabindex].BitrateEn );
    EnableWindow( GetDlgItem(m_hWnd, IDC_CBR), cam_attr[tabindex].BitrateEn );
    EnableWindow( GetDlgItem(m_hWnd, IDC_STATIC_BITRATE), cam_attr[tabindex].BitrateEn );
    EnableWindow( GetDlgItem(m_hWnd, IDC_BITRATE), cam_attr[tabindex].BitrateEn );

    ComboBox_SetCurSel(GetDlgItem( m_hWnd, IDC_PICTUREFORMAT), cam_attr[tabindex].Resolution) ;
    ComboBox_SetCurSel(GetDlgItem( m_hWnd, IDC_RECORDMODE), cam_attr[tabindex].RecMode) ;

    SETITEMCHECK(IDC_CAMENABLE, cam_attr[tabindex].Enable);
    SETITEMTXT(IDC_NAME,            cam_attr[tabindex].CameraName );
    SETITEMINT(IDC_FRAMERATE,       cam_attr[tabindex].FrameRate );
    SETITEMINT(IDC_MOTSENSITIVE,    cam_attr[tabindex].MotionSensitivity );
    SETITEMINT(IDC_MOTIONSENSTR,    cam_attr[tabindex].MotionSensitivity );
	SETITEMINT(IDC_POSTREC, cam_attr[tabindex].PreRecordTime);
	SETITEMINT(IDC_PREREC, cam_attr[tabindex].PostRecordTime );
	SETITEMCHECK(IDC_ALARMMOTION, cam_attr[tabindex].MotionAlarmEn);
	SETITEMCHECK(IDC_ALARMREC, cam_attr[tabindex].RecordAlarmEn);
	SETITEMCHECK(IDC_ALARMVIDEOLOST, cam_attr[tabindex].VideoLostAlarmEn);
	SETITEMCHECK(IDC_TRIGEN1, cam_attr[tabindex].SensorEn[0]);
	SETITEMCHECK(IDC_TRIGEN2, cam_attr[tabindex].SensorEn[1]);
	SETITEMCHECK(IDC_TRIGEN3, cam_attr[tabindex].SensorEn[2]);
	SETITEMCHECK(IDC_TRIGEN4, cam_attr[tabindex].SensorEn[3]);
	SETITEMCHECK(IDC_TRIGOSD1, cam_attr[tabindex].SensorOSD[0]);
	SETITEMCHECK(IDC_TRIGOSD2, cam_attr[tabindex].SensorOSD[1]);
	SETITEMCHECK(IDC_TRIGOSD3, cam_attr[tabindex].SensorOSD[2]);
	SETITEMCHECK(IDC_TRIGOSD4, cam_attr[tabindex].SensorOSD[3]);
	SETITEMCHECK(IDC_GPS,  cam_attr[tabindex].ShowGPS);
    if( cam_attr[tabindex].BitMode == 0 ) {
        Button_SetCheck( GetDlgItem(m_hWnd,IDC_VBR), BST_CHECKED ) ;
        Button_SetCheck( GetDlgItem(m_hWnd,IDC_CBR), BST_UNCHECKED ) ;
	}
	else {
        Button_SetCheck( GetDlgItem(m_hWnd,IDC_VBR), BST_UNCHECKED ) ;
        Button_SetCheck( GetDlgItem(m_hWnd,IDC_CBR), BST_CHECKED ) ;
	}
	SETITEMINT(IDC_BITRATE, cam_attr[tabindex].Bitrate);
	SETITEMCHECK(IDC_ENABLE_BITRATE, cam_attr[tabindex].BitrateEn);
	SETITEMINT(IDC_PTZ_ADDR, cam_attr[tabindex].ptz_addr);

}

void CameraPage::GetData() 
{
    DLGITEMVAR ;
    string str ;
    if( !IsWindow(m_hWnd) ) 
    {
        return;
    }
	GETITEMCHECK(IDC_CAMENABLE, cam_attr[tabindex].Enable);
	GETITEMTXT(IDC_NAME, cam_attr[tabindex].CameraName);
    GETITEMINT(IDC_FRAMERATE, cam_attr[tabindex].FrameRate);
    cam_attr[tabindex].MotionSensitivity = 
        SendMessage( GetDlgItem( m_hWnd, IDC_MOTSENSITIVE), TBM_GETPOS, 0, 0);
    SETITEMINT( IDC_MOTIONSENSTR,  cam_attr[tabindex].MotionSensitivity ) ;
    cam_attr[tabindex].Sensor[0] = ComboBox_GetCurSel(GetDlgItem( m_hWnd, IDC_TRIGPORT1)) ;
    cam_attr[tabindex].Sensor[1] = ComboBox_GetCurSel(GetDlgItem( m_hWnd, IDC_TRIGPORT2)) ;
    cam_attr[tabindex].Sensor[2] = ComboBox_GetCurSel(GetDlgItem( m_hWnd, IDC_TRIGPORT3)) ;
    cam_attr[tabindex].Sensor[3] = ComboBox_GetCurSel(GetDlgItem( m_hWnd, IDC_TRIGPORT4)) ;
	GETITEMCHECK(IDC_TRIGEN1, cam_attr[tabindex].SensorEn[0]);
	GETITEMCHECK(IDC_TRIGEN2, cam_attr[tabindex].SensorEn[1]);
	GETITEMCHECK(IDC_TRIGEN3, cam_attr[tabindex].SensorEn[2]);
	GETITEMCHECK(IDC_TRIGEN4, cam_attr[tabindex].SensorEn[3]);

    GETITEMCHECK(IDC_TRIGOSD1, cam_attr[tabindex].SensorOSD[0]);
	GETITEMCHECK(IDC_TRIGOSD2, cam_attr[tabindex].SensorOSD[1]);
	GETITEMCHECK(IDC_TRIGOSD3, cam_attr[tabindex].SensorOSD[2]);
	GETITEMCHECK(IDC_TRIGOSD4, cam_attr[tabindex].SensorOSD[3]);

    GETITEMINT(IDC_POSTREC, cam_attr[tabindex].PreRecordTime);
	GETITEMINT(IDC_PREREC, cam_attr[tabindex].PostRecordTime);

	GETITEMCHECK(IDC_ALARMREC, cam_attr[tabindex].RecordAlarmEn);
    cam_attr[tabindex].RecordAlarm = ComboBox_GetCurSel(GetDlgItem( m_hWnd, IDC_ALARMRECPORT)) ;
    cam_attr[tabindex].RecordAlarmPat = ComboBox_GetCurSel(GetDlgItem( m_hWnd, IDC_ALARMRECPAT)) ;

    GETITEMCHECK(IDC_ALARMVIDEOLOST, cam_attr[tabindex].VideoLostAlarmEn);
    cam_attr[tabindex].VideoLostAlarm = ComboBox_GetCurSel(GetDlgItem( m_hWnd, IDC_ALARMVIDEOLOSTPORT)) ;
    cam_attr[tabindex].VideoLostAlarmPat = ComboBox_GetCurSel(GetDlgItem( m_hWnd, IDC_ALARMVIDEOLOSTPAT)) ;
	
    GETITEMCHECK(IDC_ALARMMOTION, cam_attr[tabindex].MotionAlarmEn);
    cam_attr[tabindex].MotionAlarm = ComboBox_GetCurSel(GetDlgItem( m_hWnd, IDC_ALARMMOTIONPORT)) ;
    cam_attr[tabindex].MotionAlarmPat = ComboBox_GetCurSel(GetDlgItem( m_hWnd, IDC_ALARMMOTIONPAT)) ;

	GETITEMCHECK(IDC_GPS, cam_attr[tabindex].ShowGPS);
    GETITEMCHECK( IDC_GPSMETRIC, cam_attr[tabindex].GPSUnit); 
    GETITEMCHECK(IDC_ENABLE_BITRATE, cam_attr[tabindex].BitrateEn);
    cam_attr[tabindex].BitMode = Button_GetCheck( GetDlgItem(m_hWnd,IDC_VBR) ) != BST_CHECKED ;
	GETITEMINT(IDC_BITRATE, cam_attr[tabindex].Bitrate);

	GETITEMINT(IDC_PTZ_ADDR, cam_attr[tabindex].ptz_addr);
    cam_attr[tabindex].Resolution = ComboBox_GetCurSel(GetDlgItem( m_hWnd, IDC_PICTUREFORMAT)) ;
    cam_attr[tabindex].RecMode = ComboBox_GetCurSel(GetDlgItem( m_hWnd, IDC_RECORDMODE)) ;
    cam_attr[tabindex].PictureQuality = 10-
        SendMessage( GetDlgItem( m_hWnd, IDC_QUALITY), TBM_GETPOS, 0, 0) ;
}

void CameraPage::OnReleasedcaptureMotsensitive() 
{
    int sen ;
    sen = SendMessage( GetDlgItem(m_hWnd, IDC_MOTSENSITIVE), TBM_GETPOS, 0, 0 );
    SETITEMINT( IDC_MOTIONSENSTR, sen );
}

void CameraPage::Save(FILE * fh) 
{
    int i;
	int tag = 0x5434 ;
	long pos = ftell(fh);
	pos = (pos+0x100) & 0xffffff00 ;
	fseek( fh, pos, SEEK_SET );
	WRITEVAR(tag);
    for(i=0;i<pSystemPage->sys.cameranum;i++) {
	    WRITEVAR( cam_attr[i] );
    }
}

void CameraPage::Load(FILE * fh) 
{
    int i;
	int tag ;
	long pos = ftell(fh);
	pos = (pos+0x100) & 0xffffff00 ;
	fseek( fh, pos, SEEK_SET );
	READVAR(tag);
	if( tag!=0x5434 ) return ;
	for( i=0; i<pSystemPage->sys.cameranum ; i++ ) {
		READVAR( cam_attr[i] );
	}
}

void CameraPage::OnEnableBitrate() 
{
    int en ;
    int ben ;
    GETITEMCHECK( IDC_ENABLE_BITRATE, ben );
    if( ben  ) {
        en=TRUE ;
    }
    else {
        en=FALSE ;
    }
    EnableWindow( GetDlgItem(m_hWnd, IDC_STATIC_CONTROLMODE), en );
    EnableWindow( GetDlgItem(m_hWnd, IDC_VBR), en );
    EnableWindow( GetDlgItem(m_hWnd, IDC_CBR), en );
    EnableWindow( GetDlgItem(m_hWnd, IDC_STATIC_BITRATE), en );
    EnableWindow( GetDlgItem(m_hWnd, IDC_BITRATE), en );
}

void CameraPage::Set()
{
  for( int i=0; i<pSystemPage->sys.cameranum; i++ ) {
	SetChannelSetup(&cam_attr[i], sizeof( struct DvrChannel_attr ), i);
  }
}


/////////////////////////////////////////////////////////////////////////////
// SensorPage message handlers

BOOL SensorPage::OnInitDialog() 
{
    int i;
	string str;
    
    m_Tab.attach( GetDlgItem( m_hWnd, IDC_TABSENSOR ) );
	for( i=0 ; i<pSystemPage->sys.sensornum; i++ ) {
        TCITEM tci ;
        tci.mask=TCIF_TEXT ;
        sprintf( str.strsize(50), "Sensor%d", i+1 );
        tci.mask=TCIF_TEXT ;
        tci.pszText = str ;
        TabCtrl_InsertItem(m_Tab.getHWND(), i, &tci ) ;
	}
    
	RECT clientrect ;
	GetClientRect(m_hWnd, &clientrect); 
    MoveWindow( m_Tab.getHWND(), 
        clientrect.left,
        clientrect.top, 
        clientrect.right - clientrect.left ,
        clientrect.bottom - clientrect.top,
        TRUE );

    tabindex = 0 ;
    TabCtrl_SetCurSel( m_Tab.getHWND(), tabindex );

    PutData();
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void SensorPage::GetData()
{
    DLGITEMVAR ;
    if( IsWindow(m_hWnd) ) {
        GETITEMTXT(IDC_SENSORNAME, pSystemPage->sys.sensorname[tabindex]);
        GETITEMCHECK(IDC_INVERTED, pSystemPage->sys.sensorinverted[tabindex]) ;
    }
}

void SensorPage::PutData()
{
    DLGITEMVAR ;
    if( IsWindow(m_hWnd) ) 
    {
        SETITEMTXT(IDC_SENSORNAME, pSystemPage->sys.sensorname[tabindex]);
        SETITEMCHECK(IDC_INVERTED, pSystemPage->sys.sensorinverted[tabindex]);
    }
}

void SensorPage::OnSelchangeTabsensor() 
{
    int tab = TabCtrl_GetCurSel( m_Tab.getHWND() );
	if( tab!=tabindex ) {
        GetData();
		tabindex = tab ;
        PutData();
	}
}

void SensorPage::OnKillActive() 
{
    GetData();
}

void SensorPage::Save(FILE * fh)
{
}

void SensorPage::Load(FILE * fh)
{
}


/////////////////////////////////////////////////////////////////////////////
// CStatusPage property page
struct tz_info {
	TCHAR tz_name[80];
	TIME_ZONE_INFORMATION tzi ;
} timezone_a[] = 
{

{ _T("(GMT-12:00) International Date Line West"), 
{ 720, 
L"Dateline Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"Dateline Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT-11:00) Midway Island, Samoa"),
{ 660, 
L"Samoa Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"Samoa Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT-10:00) Hawaii"),
{ 600, 
L"Hawaiian Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"Hawaiian Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT-09:00) Alaska"),
{ 540, 
L"Alaskan Standard Time", {0, 10, 0, 5, 2, 0, 0, 0 }, 0,
L"Alaskan Daylight Time", {0, 4, 0, 1, 2, 0, 0, 0 }, -60 }
},

{_T("(GMT-08:00) Pacific Time (US & Canada); Tijuana"),
{ 480, 
L"Pacific Standard Time", {0, 10, 0, 5, 2, 0, 0, 0 }, 0,
L"Pacific Daylight Time", {0, 4, 0, 1, 2, 0, 0, 0 }, -60 }
},

{_T("(GMT-07:00) Arizona"),
{ 420, 
L"US Mountain Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"US Mountain Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT-07:00) Chihuahua, La Paz, Mazatlan"),
{ 420, 
L"Mexico Standard Time 2", {0, 10, 0, 5, 2, 0, 0, 0 }, 0,
L"Mexico Daylight Time 2", {0, 4, 0, 1, 2, 0, 0, 0 }, -60 }
},

{_T("(GMT-07:00) Mountain Time (US & Canada)"),
{ 420, 
L"Mountain Standard Time", {0, 10, 0, 5, 2, 0, 0, 0 }, 0,
L"Mountain Daylight Time", {0, 4, 0, 1, 2, 0, 0, 0 }, -60 }
},

{_T("(GMT-06:00) Central America"),
{ 360, 
L"Central America Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"Central America Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT-06:00) Central Time (US & Canada)"),
{ 360, 
L"Central Standard Time", {0, 10, 0, 5, 2, 0, 0, 0 }, 0,
L"Central Daylight Time", {0, 4, 0, 1, 2, 0, 0, 0 }, -60 }
},

{_T("(GMT-06:00) Guadalajara, Mexico City, Monterrey"),
{ 360, 
L"Mexico Standard Time", {0, 10, 0, 5, 2, 0, 0, 0 }, 0,
L"Mexico Daylight Time", {0, 4, 0, 1, 2, 0, 0, 0 }, -60 }
},

{_T("(GMT-06:00) Saskatchewan"),
{ 360, 
L"Canada Central Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"Canada Central Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT-05:00) Bogota, Lima, Quito"),
{ 300, 
L"SA Pacific Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"SA Pacific Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT-05:00) Eastern Time (US & Canada)"),
{ 300, 
L"Eastern Standard Time", {0, 10, 0, 5, 2, 0, 0, 0 }, 0,
L"Eastern Daylight Time", {0, 4, 0, 1, 2, 0, 0, 0 }, -60 }
},

{_T("(GMT-05:00) Indiana (East)"),
{ 300, 
L"US Eastern Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"US Eastern Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT-04:00) Atlantic Time (Canada)"),
{ 240, 
L"Atlantic Standard Time", {0, 10, 0, 5, 2, 0, 0, 0 }, 0,
L"Atlantic Daylight Time", {0, 4, 0, 1, 2, 0, 0, 0 }, -60 }
},

{_T("(GMT-04:00) Caracas, La Paz"),
{ 240, 
L"SA Western Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"SA Western Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT-04:00) Santiago"),
{ 240, 
L"Pacific SA Standard Time", {0, 3, 6, 2, 0, 0, 0, 0 }, 0,
L"Pacific SA Daylight Time", {0, 10, 6, 2, 0, 0, 0, 0 }, -60 }
},

{_T("(GMT-03:30) Newfoundland"),
{ 210, 
L"Newfoundland Standard Time", {0, 10, 0, 5, 2, 0, 0, 0 }, 0,
L"Newfoundland Daylight Time", {0, 4, 0, 1, 2, 0, 0, 0 }, -60 }
},

{_T("(GMT-03:00) Brasilia"),
{ 180, 
L"E. South America Standard Time", {0, 2, 0, 2, 2, 0, 0, 0 }, 0,
L"E. South America Daylight Time", {0, 10, 0, 3, 2, 0, 0, 0 }, -60 }
},

{_T("(GMT-03:00) Buenos Aires, Georgetown"),
{ 180, 
L"SA Eastern Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"SA Eastern Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT-03:00) Greenland"),
{ 180, 
L"Greenland Standard Time", {0, 10, 0, 5, 2, 0, 0, 0 }, 0,
L"Greenland Daylight Time", {0, 4, 0, 1, 2, 0, 0, 0 }, -60 }
},

{_T("(GMT-02:00) Mid-Atlantic"),
{ 120, 
L"Mid-Atlantic Standard Time", {0, 9, 0, 5, 2, 0, 0, 0 }, 0,
L"Mid-Atlantic Daylight Time", {0, 3, 0, 5, 2, 0, 0, 0 }, -60 }
},

{_T("(GMT-01:00) Azores"),
{ 60, 
L"Azores Standard Time", {0, 10, 0, 5, 3, 0, 0, 0 }, 0,
L"Azores Daylight Time", {0, 3, 0, 5, 2, 0, 0, 0 }, -60 }
},

{_T("(GMT-01:00) Cape Verde Is."),
{ 60, 
L"Cape Verde Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"Cape Verde Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT) Casablanca, Monrovia"),
{ 0, 
L"Greenwich Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"Greenwich Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT) Greenwich Mean Time : Dublin, Edinburgh, Lisbon, London"),
{ 0, 
L"GMT Standard Time", {0, 10, 0, 5, 2, 0, 0, 0 }, 0,
L"GMT Daylight Time", {0, 3, 0, 5, 1, 0, 0, 0 }, -60 }
},

{_T("(GMT+01:00) Amsterdam, Berlin, Bern, Rome, Stockholm, Vienna "),
{ -60, 
L"W. Europe Standard Time", {0, 10, 0, 5, 3, 0, 0, 0 }, 0,
L"W. Europe Daylight Time", {0, 3, 0, 5, 2, 0, 0, 0 }, -60 }
},

{_T("(GMT+01:00) Belgrade, Bratislava, Budapest, Ljubljana, Prague"),
{ -60, 
L"Central Europe Standard Time", {0, 10, 0, 5, 3, 0, 0, 0 }, 0,
L"Central Europe Daylight Time", {0, 3, 0, 5, 2, 0, 0, 0 }, -60 }
},

{_T("(GMT+01:00) Brussels, Copenhagen, Madrid, Paris"),
{ -60, 
L"Romance Standard Time", {0, 10, 0, 5, 3, 0, 0, 0 }, 0,
L"Romance Daylight Time", {0, 3, 0, 5, 2, 0, 0, 0 }, -60 }
},

{_T("(GMT+01:00) Sarajevo, Skopje, Warsaw, Zagreb"),
{ -60, 
L"Central European Standard Time", {0, 10, 0, 5, 3, 0, 0, 0 }, 0,
L"Central European Daylight Time", {0, 3, 0, 5, 2, 0, 0, 0 }, -60 }
},

{_T("(GMT+01:00) West Central Africa"),
{ -60, 
L"W. Central Africa Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"W. Central Africa Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT+02:00) Athens, Beirut, Istanbul, Minsk"),
{ -120, 
L"GTB Standard Time", {0, 10, 0, 5, 3, 0, 0, 0 }, 0,
L"GTB Daylight Time", {0, 3, 0, 5, 2, 0, 0, 0 }, -60 }
},

{_T("(GMT+02:00) Bucharest  "),
{ -120, 
L"E. Europe Standard Time", {0, 10, 0, 5, 1, 0, 0, 0 }, 0,
L"E. Europe Daylight Time", {0, 3, 0, 5, 0, 0, 0, 0 }, -60 }
},

{_T("(GMT+02:00) Cairo"),
{ -120, 
L"Egypt Standard Time", {0, 9, 3, 5, 2, 0, 0, 0 }, 0,
L"Egypt Daylight Time", {0, 5, 5, 1, 2, 0, 0, 0 }, -60 }
},

{_T("(GMT+02:00) Harare, Pretoria"),
{ -120, 
L"South Africa Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"South Africa Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT+02:00) Helsinki, Kyiv, Riga, Sofia, Tallinn, Vilnius"),
{ -120, 
L"FLE Standard Time", {0, 10, 0, 5, 4, 0, 0, 0 }, 0,
L"FLE Daylight Time", {0, 3, 0, 5, 3, 0, 0, 0 }, -60 }
},

{_T("(GMT+02:00) Jerusalem"),
{ -120, 
L"Jerusalem Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"Jerusalem Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT+03:00) Baghdad"),
{ -180, 
L"Arabic Standard Time", {0, 10, 0, 1, 4, 0, 0, 0 }, 0,
L"Arabic Daylight Time", {0, 4, 0, 1, 3, 0, 0, 0 }, -60 }
},

{_T("(GMT+03:00) Kuwait, Riyadh"),
{ -180, 
L"Arab Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"Arab Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT+03:00) Moscow, St. Petersburg, Volgograd"),
{ -180, 
L"Russian Standard Time", {0, 10, 0, 5, 3, 0, 0, 0 }, 0,
L"Russian Daylight Time", {0, 3, 0, 5, 2, 0, 0, 0 }, -60 }
},

{_T("(GMT+03:00) Nairobi"),
{ -180, 
L"E. Africa Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"E. Africa Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT+03:30) Tehran"),
{ -210, 
L"Iran Standard Time", {0, 9, 2, 4, 2, 0, 0, 0 }, 0,
L"Iran Daylight Time", {0, 3, 0, 1, 2, 0, 0, 0 }, -60 }
},

{_T("(GMT+04:00) Abu Dhabi, Muscat"),
{ -240, 
L"Arabian Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"Arabian Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT+04:00) Baku, Tbilisi, Yerevan"),
{ -240, 
L"Caucasus Standard Time", {0, 10, 0, 5, 3, 0, 0, 0 }, 0,
L"Caucasus Daylight Time", {0, 3, 0, 5, 2, 0, 0, 0 }, -60 }
},

{_T("(GMT+04:30) Kabul"),
{ -270, 
L"Afghanistan Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"Afghanistan Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT+05:00) Ekaterinburg"),
{ -300, 
L"Ekaterinburg Standard Time", {0, 10, 0, 5, 3, 0, 0, 0 }, 0,
L"Ekaterinburg Daylight Time", {0, 3, 0, 5, 2, 0, 0, 0 }, -60 }
},

{_T("(GMT+05:00) Islamabad, Karachi, Tashkent"),
{ -300, 
L"West Asia Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"West Asia Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT+05:30) Chennai, Kolkata, Mumbai, New Delhi"),
{ -330, 
L"India Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"India Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT+05:45) Kathmandu"),
{ -345, 
L"Nepal Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"Nepal Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT+06:00) Almaty, Novosibirsk"),
{ -360, 
L"N. Central Asia Standard Time", {0, 10, 0, 5, 3, 0, 0, 0 }, 0,
L"N. Central Asia Daylight Time", {0, 3, 0, 5, 2, 0, 0, 0 }, -60 }
},

{_T("(GMT+06:00) Astana, Dhaka"),
{ -360, 
L"Central Asia Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"Central Asia Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT+06:00) Sri Jayawardenepura"),
{ -360, 
L"Sri Lanka Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"Sri Lanka Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT+06:30) Rangoon"),
{ -390, 
L"Myanmar Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"Myanmar Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT+07:00) Bangkok, Hanoi, Jakarta"),
{ -420, 
L"SE Asia Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"SE Asia Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT+07:00) Krasnoyarsk"),
{ -420, 
L"North Asia Standard Time", {0, 10, 0, 5, 3, 0, 0, 0 }, 0,
L"North Asia Daylight Time", {0, 3, 0, 5, 2, 0, 0, 0 }, -60 }
},

{_T("(GMT+08:00) Beijing, Chongqing, Hong Kong, Urumqi"),
{ -480, 
L"China Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"China Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT+08:00) Irkutsk, Ulaan Bataar"),
{ -480, 
L"North Asia East Standard Time", {0, 10, 0, 5, 3, 0, 0, 0 }, 0,
L"North Asia East Daylight Time", {0, 3, 0, 5, 2, 0, 0, 0 }, -60 }
},

{_T("(GMT+08:00) Kuala Lumpur, Singapore"),
{ -480, 
L"Malay Peninsula Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"Malay Peninsula Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT+08:00) Perth"),
{ -480, 
L"W. Australia Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"W. Australia Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT+08:00) Taipei"),
{ -480, 
L"Taipei Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"Taipei Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT+09:00) Osaka, Sappora, Tokyo"),
{ -540, 
L"Tokyo Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"Tokyo Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT+09:00) Seoul"),
{ -540, 
L"Korea Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"Korea Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT+09:00) Yakutsk"),
{ -540, 
L"Yakutsk Standard Time", {0, 10, 0, 5, 3, 0, 0, 0 }, 0,
L"Yakutsk Daylight Time", {0, 3, 0, 5, 2, 0, 0, 0 }, -60 }
},

{_T("(GMT+09:30) Adelaide"),
{ -570, 
L"Cen. Australia Standard Time", {0, 3, 0, 5, 3, 0, 0, 0 }, 0,
L"Cen. Australia Daylight Time", {0, 10, 0, 5, 2, 0, 0, 0 }, -60 }
},

{_T("(GMT+09:30) Darwin"),
{ -570, 
L"AUS Central Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"AUS Central Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT+10:00) Brisbane"),
{ -600, 
L"E. Australia Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"E. Australia Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT+10:00) Canberra, Melbourne, Sydney"),
{ -600, 
L"AUS Eastern Standard Time", {0, 3, 0, 5, 3, 0, 0, 0 }, 0,
L"AUS Eastern Daylight Time", {0, 10, 0, 5, 2, 0, 0, 0 }, -60 }
},

{_T("(GMT+10:00) Guam, Port Moresby"),
{ -600, 
L"West Pacific Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"West Pacific Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT+10:00) Hobart"),
{ -600, 
L"Tasmania Standard Time", {0, 3, 0, 5, 3, 0, 0, 0 }, 0,
L"Tasmania Daylight Time", {0, 10, 0, 1, 2, 0, 0, 0 }, -60 }
},

{_T("(GMT+10:00) Vladivostok"),
{ -600, 
L"Vladivostok Standard Time", {0, 10, 0, 5, 3, 0, 0, 0 }, 0,
L"Vladivostok Daylight Time", {0, 3, 0, 5, 2, 0, 0, 0 }, -60 }
},

{_T("(GMT+11:00) Magadan, Solomon Is., New Caledonia"),
{ -660, 
L"Central Pacific Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"Central Pacific Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT+12:00) Auckland, Wellington"),
{ -720, 
L"New Zealand Standard Time", {0, 3, 0, 3, 2, 0, 0, 0 }, 0,
L"New Zealand Daylight Time", {0, 10, 0, 1, 2, 0, 0, 0 }, -60 }
},

{_T("(GMT+12:00) Fiji, Kamchatka, Marshall Is."),
{ -720, 
L"Fiji Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"Fiji Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
},

{_T("(GMT+13:00) Nuku'alofa"),
{ -780, 
L"Tonga Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0,
L"Tonga Standard Time", {0, 0, 0, 0, 0, 0, 0, 0 }, 0 }
}

};

/////////////////////////////////////////////////////////////////////////////
// StatusPage

/////////////////////////////////////////////////////////////////////////////
// StatusPage message handlers
BOOL StatusPage::OnInitDialog() 
{

    DLGITEMVAR;
    int i;
    string str ;
	int sizetz ;

	memset(version, 0, sizeof(version));

	if( DVR_Version( version )>0 ) {
		sprintf(str.strsize(128), "%d.%d.%d.%d", version[0], version[1], version[2], version[3] );
        SETITEMTXT(IDC_VERSION, str );
	}

	for( i=pSystemPage->sys.cameranum; i<16; i++ ) {
        ShowWindow( GetDlgItem( m_hWnd, IDC_CHECK_SIG0+i), SW_HIDE );
        ShowWindow( GetDlgItem( m_hWnd, IDC_CHECK_REC0+i), SW_HIDE );
        ShowWindow( GetDlgItem( m_hWnd, IDC_CHECK_MOTION0+i), SW_HIDE );
	}

    HWND htz = GetDlgItem(m_hWnd, IDC_TIMEZONE);

	if( version[0]!=2 ) {
		sizetz = sizeof(timezone_a)/sizeof(struct tz_info) ;
		for( i=0; i< sizetz; i++ ) {
            ComboBox_AddString( htz, timezone_a[i].tz_name );
		}
		DVRGetTimezone(&tzi);

		GetTimeZoneInformation( &localtzi ) ;
		for( i=0; i< sizetz; i++ ) {
			if( timezone_a[i].tzi.Bias == localtzi.Bias &&
				wcscmp( timezone_a[i].tzi.StandardName, localtzi.StandardName )==0 )
            {
                SETITEMTXT(IDC_LOCALTIMEZONENAME, timezone_a[i].tz_name );
				break;
			}
		}
		if( i>= sizetz ) {
			for( i=0; i< sizetz; i++ ) {
				if( timezone_a[i].tzi.Bias == localtzi.Bias ){
                    SETITEMTXT(IDC_LOCALTIMEZONENAME, timezone_a[i].tz_name );
					break;
				}
			}
		}

        SETITEMCHECK( IDC_DAYLIGHT, tzi.DaylightBias != tzi.StandardBias ); 

		if( tzi.StandardDate.wMonth == 0 ) {
            ShowWindow( GetDlgItem( m_hWnd, IDC_DAYLIGHT ), SW_HIDE );
		}

		for( i=0; i< sizetz; i++ ) {
			if( timezone_a[i].tzi.Bias == tzi.Bias &&
				wcscmp( timezone_a[i].tzi.StandardName, tzi.StandardName )==0 )
            {
                ComboBox_SetCurSel(htz, i);
				break;
			}
		}
		if( i>= sizetz ) {
			for( i=0; i< sizetz; i++ ) {
				if( timezone_a[i].tzi.Bias == tzi.Bias ){
                    ComboBox_SetCurSel(htz, i);
					break;
				}
			}
		}
	}
	else {
		char * zoneinfo ;
		char * timezone ;
		char * p1, * p2 ;
//		tz->ResetContent();

		if( DVR2ZoneInfo( &zoneinfo )>0 ) {
			p1=zoneinfo ;
			while( *p1 != 0 ) {
				p2=strchr(p1, '\n');
				if( p2!=NULL ) {
					*p2=0 ;
                    str=p1 ;
                    ComboBox_AddString(htz, str);
				}
				else {
                    str=p1 ;
                    ComboBox_AddString(htz, str);
					break;
				}
				p1=p2+1 ;
			}
			delete zoneinfo ;
		}

		if( DVR2GetTimeZone( &timezone )>0 ) {
            str=timezone ;
            ComboBox_SelectString(htz, 0, str );
			delete timezone ;
		}
        ShowWindow( GetDlgItem( m_hWnd, IDC_DAYLIGHT ), SW_HIDE );
		if( GetTimeZoneInformation( &localtzi )==TIME_ZONE_ID_DAYLIGHT ) {
            str = localtzi.DaylightName ;
		}
		else {
            str = localtzi.StandardName ;
		}
        SETITEMTXT( IDC_LOCALTIMEZONENAME, str );
	}

	return TRUE; 
}

void StatusPage::OnSelchangeTimezone() 
{
	tzchanged = 1;
	if( version[0]==2 ) 
		return ;

    HWND htz = GetDlgItem(m_hWnd, IDC_TIMEZONE);

	int i=ComboBox_GetCurSel(htz);
	tzi=timezone_a[i].tzi ;
	if( tzi.StandardDate.wMonth == 0 ) {
        ShowWindow( GetDlgItem( m_hWnd, IDC_DAYLIGHT ), SW_HIDE );
    }
	else {
        ShowWindow( GetDlgItem( m_hWnd, IDC_DAYLIGHT ), SW_SHOW );
	}
}


void StatusPage::OnDaylight() 
{
	tzchanged = 1;
}

void StatusPage::UpdateStatus()
{
    DLGITEMVAR ;
    string str ;
	struct channelstatus cs[16] ;
	memset( cs, 0, sizeof(cs));

// return number of channels which status have been retrieved.
	int ch ;
	ch = DVRGetChannelState( cs, 16 );
	for( int i=0; i<ch; i++ ) {
        SETITEMCHECK(IDC_CHECK_SIG0+i, cs[i].sig );
        SETITEMCHECK(IDC_CHECK_REC0+i, cs[i].rec );
        SETITEMCHECK(IDC_CHECK_MOTION0+i, cs[i].mot );
	}
	SYSTEMTIME st ;
	if( version[0]==2 ) {
		if( DVR2GetTime( &st, 0 ) ) {
            sprintf( str.strsize(512), "%04d-%02d-%02d %02d:%02d:%02d",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond );
            SETITEMTXT( IDC_DVR_TIME, str );
		}
	}
	else {
		if( DVRGetLocalTime( &st ) ) {
            sprintf( str.strsize(512), "%04d-%02d-%02d %02d:%02d:%02d",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond );
            SETITEMTXT( IDC_DVR_TIME, str );
		}
	}

    time_t timer;
    struct tm* tm_info;
    time(&timer);
    tm_info = localtime(&timer);

    strftime(str.strsize(512), 25, "%Y-%m-%d %H:%M:%S", tm_info);
    SETITEMTXT( IDC_LOCALTIME, str );
}

void StatusPage::OnTimer(UINT nIDEvent) 
{
	UpdateStatus();	
}

void StatusPage::OnSetActive()
{
	m_timer = (int)SetTimer(m_hWnd, 977, 1000, NULL ) ;
	UpdateStatus();
}

void StatusPage::OnKillActive()
{
	if( m_timer!= 0 ) {
		KillTimer(m_hWnd, m_timer );	
		m_timer = 0 ;
	}
}

void StatusPage::OnSynTime() 
{
    DLGITEMVAR ;
    string str ;
	// Set DVR time zone
//	CComboBox * cb = (CComboBox *)GetDlgItem(IDC_TIMEZONE) ;
	if( tzchanged ) {
		int sel = ComboBox_GetCurSel( GetDlgItem(m_hWnd, IDC_TIMEZONE) );
		if( version[0]==2 ) {
			if( sel!=CB_ERR ) {
                GETITEMSTR( IDC_TIMEZONE, str );
				DVR2SetTimeZone(str);
//				int l = ComboBox_GetLBTextLen( GetDlgItem(m_hWnd, IDC_TIMEZONE), sel ) ;
//				TCHAR * tz=new TCHAR [l+2] ;
//				cb->GetLBText( sel, tz );
//				char * tzc=new char [l+2] ;
//				stringncpy(tzc,tz, l+2);
//				DVR2SetTimeZone(tzc);
//	            delete tzc ;
//				delete tz ;
			}
		}
		else {
			tzi = timezone_a[sel].tzi ;
/*			if( tzi.StandardDate.wMonth!=0 &&   
				((CButton *)GetDlgItem(IDC_DAYLIGHT))->GetCheck()==BST_UNCHECKED ) {
				tzi.DaylightBias = tzi.StandardBias ;
				tzi.DaylightDate = tzi.StandardDate ;
				memcpy( &(tzi.DaylightName), &(tzi.StandardName), sizeof(tzi.DaylightName));   
			}
*/
            int daylightcheck ;
            GETITEMCHECK( IDC_DAYLIGHT, daylightcheck );
			if( tzi.StandardDate.wMonth!=0 &&   
				daylightcheck==BST_UNCHECKED ) {
				tzi.DaylightBias = tzi.StandardBias ;
				tzi.DaylightDate = tzi.StandardDate ;
				memcpy( &(tzi.DaylightName), &(tzi.StandardName), sizeof(tzi.DaylightName));   
			}
            DVRSetTimezone(&tzi);
		}
		tzchanged=0 ;
	}

	SYSTEMTIME dvrt ;
	if( version[0]==2 ) {
		if( DVR2GetTime(&dvrt, 1)==0 ) {
			MessageBox( m_hWnd, _T("Set DVR time error!"), _T("Set DVR time"), MB_OK );
			return ;
		}
	}
	else {
		if( DVRGetSystemTime( &dvrt )==0 ){
			MessageBox( m_hWnd, _T("Set DVR time error!"), _T("Set DVR time"), MB_OK );
			return ;
		}
	}

    FILETIME  ftdvr, ftpc ;
    SystemTimeToFileTime( &dvrt, &ftdvr );
    GetSystemTimeAsFileTime(&ftpc);
	if(  CompareFileTime( &ftdvr, &ftpc )>0 ) {
		if( MessageBox( m_hWnd, _T("DVR time will be set backward, continue?"), _T("Set DVR time"), MB_YESNO) != IDYES ) {
			return ;
		}
	}

    GetSystemTime(&dvrt);
	if( version[0]==2 ) {
		if( DVR2SetTime( &dvrt, 1 ) ) {
			MessageBox(m_hWnd,  _T("DVR time synchronized!"), _T("Set DVR time"), MB_OK );
		}
		else {
			MessageBox(m_hWnd,  _T("Set DVR time error!"), _T("Set DVR time"), MB_OK );
		}
	}
	else {
		if( DVRSetSystemTime( &dvrt ) ) {
			MessageBox(m_hWnd,  _T("DVR time synchronized!"), _T("Set DVR time"), MB_OK );
		}
		else {
			MessageBox(m_hWnd,  _T("Set DVR time error!"), _T("Set DVR time"), MB_OK );
		}
	}
}

// helper function to findout the top level main window
static BOOL CALLBACK enumtopwinproc( HWND hwnd, LPARAM lParam )
{
    if( IsWindowVisible(hwnd) ) {
        if( hwnd == GetForegroundWindow() ) {
            *((HWND *)lParam)=hwnd ;
            return FALSE ;      // stop enum
        }
        else if( *((HWND *)lParam) == NULL ) {
            *((HWND *)lParam)=hwnd ;
        }
        else {
            // find out which one is on top
            HWND htop = *((HWND *)lParam) ;
            while( (htop=GetWindow(htop, GW_HWNDPREV))!=NULL ) {
                if( htop==hwnd ) {
                    *((HWND *)lParam)=hwnd ; ;
                    break;
                }
            }
        }
    }
    return TRUE ;
}

// find top level window
static HWND findtopwin() 
{
    HWND htopwin = NULL ;
    // find top level window
    EnumThreadWindows( GetCurrentThreadId(), enumtopwinproc, (LPARAM)&htopwin ) ;
    return htopwin ;
}


/////////////////////////////////////////////////////////////////////////////
// DvrSetup
DvrSetup::DvrSetup(LPCTSTR pszCaption)
    :PropSheet(pszCaption)
{
    this->m_psh.dwFlags  |= PSH_NOAPPLYNOW ;
    // try to find proper parent window
    m_psh.hwndParent = findtopwin() ;
    m_commit=1 ;
}


void DvrSetup::OnAttach()
{
    HWND hbt ;

    hbt = CreateWindow( _T("BUTTON"), _T("Reboot DVR (commit setup)"), WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX|WS_TABSTOP, 0, 0, 1, 1, m_hWnd, (HMENU)IDC_COMMITSETUP, AppInst(), NULL );
	BtCommit.attach( hbt );
    SETITEMCHECK( IDC_COMMITSETUP, BST_CHECKED );
    hbt = CreateWindow( _T("BUTTON"), _T("Load Config"), WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|WS_TABSTOP, 0, 0, 1, 1, m_hWnd, (HMENU)IDC_LOADSETUP, AppInst(), NULL );
	BtLoad.attach( hbt );
    hbt = CreateWindow( _T("BUTTON"), _T("Save Config"), WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|WS_TABSTOP, 0, 0, 1, 1, m_hWnd, (HMENU)IDC_SAVESETUP, AppInst(), NULL );
	BtSave.attach( hbt );
    HFONT hf ;
    hf = (HFONT)SendMessage(  GetDlgItem(m_hWnd, IDOK), WM_GETFONT, 0, 0 );
    if( hf ) {
        SendMessage( BtSave.getHWND(), WM_SETFONT, (WPARAM)hf, 0 );
        SendMessage( BtLoad.getHWND(), WM_SETFONT, (WPARAM)hf, 0 );
        SendMessage( BtCommit.getHWND(), WM_SETFONT, (WPARAM)hf, 0 );
    }
}

int DvrSetup::OnSize(int cx, int cy)
{
    POINT pt ;
	RECT r ;
    int width, height ;

    GetWindowRect( GetDlgItem(m_hWnd, IDOK), &r );
    width = r.right - r.left ;
    height = r.bottom - r.top ;
    pt.x = 20 ;
    pt.y = cy - 30 ;

    MoveWindow( BtSave.getHWND(), pt.x, pt.y, width, height, TRUE );
    pt.x += width + 10 ;

    MoveWindow( BtLoad.getHWND(), pt.x, pt.y, width, height, TRUE );
    pt.x += width + 10 ;

    MoveWindow( BtCommit.getHWND(), pt.x, pt.y, width+100, height, TRUE );

    return TRUE ;
}

void DvrSetup::OnSaveSetup() 
{
	FILE * fconfig ;
	string  filename ;

    OPENFILENAME ofn ;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof (OPENFILENAME) ;
    ofn.nMaxFile = 512 ;
    ofn.lpstrFile = filename.tcssize(ofn.nMaxFile) ;
    ofn.lpstrFilter=_T("DVR Configure Files\0*.DCF\0All files\0*.*\0\0");
    ofn.Flags=0;
    ofn.nFilterIndex=1;
    ofn.hInstance = AppInst();
    ofn.lpstrDefExt=_T("DCF");
    ofn.Flags=OFN_OVERWRITEPROMPT ;
    if( GetSaveFileName(&ofn) ) {
		fconfig = fopen( filename, "wb" );
		if( fconfig==NULL ) {
			MessageBox(m_hWnd, _T("Error!"), _T("ERROR!"), MB_OK );
			return ;
		}

        pSystemPage->GetData();
		pCameraPage->GetData();
		pSensorPage->GetData();

        pSystemPage->Save( fconfig );
		pCameraPage->Save( fconfig );
		pSensorPage->Save( fconfig );

        fclose(fconfig);
	}
}

void DvrSetup::OnLoadSetup()
{
	FILE * fconfig ;
	string  filename ;
    OPENFILENAME ofn ;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof (OPENFILENAME) ;
    ofn.nMaxFile = 512 ;
    ofn.lpstrFile = filename.tcssize(ofn.nMaxFile) ;
    ofn.lpstrFilter=_T("DVR Configure Files\0*.DCF\0All files\0*.*\0\0");
    ofn.Flags=0;
    ofn.nFilterIndex=1;
    ofn.lpstrDefExt=_T("DCF");
    ofn.hInstance = AppInst();
    if( GetOpenFileName(&ofn) ) {
		fconfig = fopen( filename, "rb" );
		if( fconfig==NULL ) {
			MessageBox(m_hWnd, _T("Error open file!"), _T("ERROR!"), MB_OK );
			return ;
		}

        pSystemPage->Load( fconfig );
		pCameraPage->Load( fconfig );
		pSensorPage->Load( fconfig );

        pSystemPage->PutData();
		pCameraPage->PutData();
		pSensorPage->PutData();

        fclose(fconfig);
	}
}

void DvrSetup::OnCommit()
{
       m_commit = Button_GetCheck(BtCommit.getHWND()) ;
}

// configure DVR server
// return 1 : success
//        0 : canceled
//       -1 : error
int DvrConfigure( char * servername )
{
	int res = 0 ;

	cfgsocket = net_connect( servername );
	if( cfgsocket==0 ) {
		return -1 ;
	}

	string Title ;
    sprintf(Title.strsize(256), "DVR Configure - %s", servername );
	DvrSetup ps (Title) ;

	SystemPage systempage;
	if( systempage.sys.cameranum==0) {
		net_close(cfgsocket);
		return -1 ;
	}
	string oldServerName(systempage.sys.ServerName) ; 
	CameraPage camerapage;
	SensorPage sensorpage ;
	StatusPage statuspage ;
	ps.addpage(&systempage);
	ps.pSystemPage = &systempage ;
	ps.addpage(&camerapage);
	ps.pCameraPage = &camerapage ;
	ps.addpage(&sensorpage);
	ps.pSensorPage = &sensorpage ;
	ps.addpage(&statuspage);
	if (ps.DoModal()==IDOK ) {
		camerapage.Set();
		systempage.Set();
		string newServerName(systempage.sys.ServerName) ;
		if( strcmp( newServerName, oldServerName)!=0 ) {
		}
		else if( ps.m_commit != 0 ) {
			DVRKill( TRUE );
		}
		res=1 ;
	};

	net_close(cfgsocket);
	return res ;
}

