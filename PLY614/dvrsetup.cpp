// dvrplayer.cpp : DVR player object
//

#include "stdafx.h"
#include <process.h>
#include "ply614.h"
#include "dvrnet.h"
#include "dvrsetup.h"

static int stringcpy( char * dest, LPCTSTR str )
{
	int i=0 ;
	while( str[i] ) {
		dest[i]=(char)str[i] ;
		i++ ;
	}
	dest[i]='\0' ;
	return i ;
}

static int stringncpy( char * dest, LPCTSTR str, int n )
{
	int i=0 ;
	while( str[i] && i<(n-1) ) {
		dest[i]=(char)str[i] ;
		i++ ;
	}
	dest[i]='\0' ;
	return i ;
}

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

/////////////////////////////////////////////////////////////////////////////
// CSystemPage property page

IMPLEMENT_DYNCREATE(CSystemPage, CPropertyPage)

class CSystemPage * pSystemPage ;

CSystemPage::CSystemPage() : CPropertyPage(CSystemPage::IDD)
{
	//{{AFX_DATA_INIT(CSystemPage)
	m_alarmnum = 0;
	m_autodisk = _T("");
	m_breaksize = 0;
	m_breaktime = 0;
	m_cameranum = 0;
	m_iomodule = _T("");
	m_sensornum = 0;
	m_mindiskspace = 0;
	m_breakmode = -1;
	m_servername = _T("");
	m_shutdowndelay = 0;
	m_evmpostlock = 0;
	m_evmprelock = 0;
	m_ptz_enable = FALSE;
	m_ptz_baudrate = -1;
	m_ptz_port = -1;
	m_ptz_protocol = -1;
	m_videoencrypt = FALSE;
	m_videopassword = _T("");
	//}}AFX_DATA_INIT

	pSystemPage=this ;

	if( GetSystemSetup(&sys, sizeof(struct system_stru)) ) {
		m_cameranum = sys.cameranum ;
		m_alarmnum = sys.alarmnum ;
		m_sensornum = sys.sensornum ;
		m_iomodule = sys.productid ;
		m_breakmode = sys.breakMode ;
		m_breaksize = sys.breakSize/(1024*1024) ;
		m_breaktime = sys.breakTime ;
		m_mindiskspace = sys.minDiskSpace/(1024*1024) ;
		m_autodisk = sys.autodisk ;
		m_servername = sys.ServerName ;
		m_shutdowndelay = sys.shutdowndelay ;
//		m_evmenable = sys.eventmarker_enable ;
		m_evmpostlock = sys.eventmarker_postlock  ;
		m_evmprelock = sys.eventmarker_prelock ;
		m_ptz_enable = sys.ptz_en;
		m_ptz_baudrate = sys.ptz_baudrate;
		m_ptz_port = sys.ptz_port;
		m_ptz_protocol = sys.ptz_protocol;
		m_videopassword = _T("********") ;
		m_videoencrypt = sys.videoencrpt_en ;
	}
	else {
		memset(&sys, 0, sizeof(sys));
	}
}

CSystemPage::~CSystemPage()
{
}

void CSystemPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSystemPage)
	DDX_Control(pDX, IDC_EVENTMARKER, m_eventmarker);
	DDX_Text(pDX, IDC_ALARMNUMBER, m_alarmnum);
	DDX_Text(pDX, IDC_AUTODISK, m_autodisk);
	DDX_Text(pDX, IDC_BREAKSIZE, m_breaksize);
	DDX_Text(pDX, IDC_BREAKTIME, m_breaktime);
	DDX_Text(pDX, IDC_CAMERANUMBER, m_cameranum);
	DDX_CBString(pDX, IDC_IOMODULE, m_iomodule);
	DDX_Text(pDX, IDC_SENSORNUMBER, m_sensornum);
	DDX_Text(pDX, IDC_MINDISKSPACE, m_mindiskspace);
	DDX_Radio(pDX, IDC_BREAKBYSIZE, m_breakmode);
	DDX_Text(pDX, IDC_SERVERNAME, m_servername);
	DDX_Text(pDX, IDC_SHUTDOWNDELAY, m_shutdowndelay);
	DDV_MinMaxInt(pDX, m_shutdowndelay, 0, 5000);
	DDX_Text(pDX, IDC_EVMPOSTLOCK, m_evmpostlock);
	DDX_Text(pDX, IDC_EVMPRELOCK, m_evmprelock);
	DDX_Check(pDX, IDC_PTZ_ENABLE, m_ptz_enable);
	DDX_CBIndex(pDX, IDC_PTZ_BAUDRATE, m_ptz_baudrate);
	DDX_CBIndex(pDX, IDC_PTZ_PORT, m_ptz_port);
	DDX_CBIndex(pDX, IDC_PTZ_PROTOCOL, m_ptz_protocol);
	DDX_Check(pDX, IDC_VIDEO_ENCRYPT, m_videoencrypt);
	DDX_Text(pDX, IDC_EDIT_VIDEOPASSWORD, m_videopassword);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CSystemPage, CPropertyPage)
	//{{AFX_MSG_MAP(CSystemPage)
	ON_BN_CLICKED(IDC_DVRUSERMAN, OnDvruserman)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSystemPage message handlers

BOOL CSystemPage::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();
	
	// TODO: Add extra initialization here
//	if( !g_noptz ) {
		GetDlgItem(IDC_PTZ_BOX)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_PTZ_ENABLE)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_PTZ_TXT1)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_PTZ_PORT)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_PTZ_TXT2)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_PTZ_BAUDRATE)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_PTZ_TXT3)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_PTZ_PROTOCOL)->ShowWindow(SW_HIDE);
//	}

	int i, sel;
	if( sys.eventmarker_enable == 0 ) {
		sys.eventmarker = 0 ;
	}
	for( i=0; i<sys.sensornum ; i++ ) {
		CString portnum ;
		portnum.Format(_T("%d"), i+1);
		sel = m_eventmarker.AddString(portnum);
		if( sys.eventmarker & (1<<i) ) {
			m_eventmarker.SetSel(sel) ;
		}
	}
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CSystemPage::OnOK() 
{
	// TODO: Add your specialized code here and/or call the base class

	UpdateData();
//	sys.cameranum = m_cameranum;
//	sys.alarmnum = m_alarmnum;
//	sys.sensornum = m_sensornum;
//	sys.IOMod = m_iomodule;
	stringcpy( sys.ServerName, m_servername ) ;
	sys.breakMode = m_breakmode;
	sys.breakSize = (1024*1024)*m_breaksize;
	sys.breakTime = m_breaktime;
	sys.minDiskSpace = (1024*1024)*m_mindiskspace ;
	stringcpy( sys.autodisk, m_autodisk) ;

	sys.shutdowndelay = m_shutdowndelay ;

	sys.eventmarker=0 ;
	for( int i=0; i<sys.sensornum ; i++ ) {
		if( m_eventmarker.GetSel(i)>0 ) {
			sys.eventmarker |= 1<<i ;
		}
	}
	if( sys.eventmarker == 0 ) {
		sys.eventmarker_enable = 0 ;
	}
	else {
		sys.eventmarker_enable = 1 ;
	}
	sys.eventmarker_postlock = m_evmpostlock;
	sys.eventmarker_prelock = m_evmprelock ;
	sys.ptz_en = m_ptz_enable ;
	sys.ptz_baudrate = m_ptz_baudrate ;
	sys.ptz_port = m_ptz_port ;
	sys.ptz_protocol = m_ptz_protocol ;
	sys.videoencrpt_en = m_videoencrypt ;
	if( m_videopassword != _T("********") ) {
		memset(sys.videopassword, 0, sizeof(sys.videopassword));
		stringncpy(sys.videopassword, m_videopassword, sizeof(sys.videopassword) );
	}

	CPropertyPage::OnOK();
}


#define WRITEVAR(var) fwrite(&(var), 1, sizeof(var), fh)
#define READVAR(var)  fread( &(var), 1, sizeof(var), fh)

void CSystemPage::Save(FILE * fh) 
{
	int tag = 0x5633 ;
	long pos = ftell(fh);
	char tmpstr[256] ;
	pos = (pos+0x100) & 0xffffff00 ;
	fseek( fh, pos, SEEK_SET );

	UpdateData();

	WRITEVAR(tag);
	stringcpy(tmpstr, m_servername);
	WRITEVAR(tmpstr);
	WRITEVAR(m_breakmode);
	WRITEVAR(m_breaksize);
	WRITEVAR(m_breaktime);
	WRITEVAR(m_mindiskspace);
	stringcpy(tmpstr, m_autodisk);
	WRITEVAR(tmpstr);
	WRITEVAR(m_shutdowndelay);

	DWORD eventmarker=0 ;
	for( int i=0; i<sys.sensornum ; i++ ) {
		if( m_eventmarker.GetSel(i)>0 ) {
			eventmarker |= 1<<i ;
		}
	}

	WRITEVAR(eventmarker);
	WRITEVAR(m_evmpostlock);
	WRITEVAR(m_evmprelock);
	WRITEVAR(m_ptz_enable);
	WRITEVAR(m_ptz_port);
	WRITEVAR(m_ptz_baudrate);
	WRITEVAR(m_ptz_protocol);

	WRITEVAR(m_videoencrypt);
	if( m_videopassword != _T("********") ) {
		memset(sys.videopassword, 0, sizeof(sys.videopassword));
		stringncpy(sys.videopassword, m_videopassword, sizeof(sys.videopassword));
	}
	WRITEVAR( sys.videopassword ); 
}

void CSystemPage::Load(FILE * fh) 
{
	int tag  ;
	long pos = ftell(fh);
	char tmpstr[256] ;
	pos = (pos+0x100) & 0xffffff00 ;
	fseek( fh, pos, SEEK_SET );

	UpdateData();

	READVAR(tag);
	if( tag!=0x5633 ) return ;

	READVAR(tmpstr);
	m_servername = tmpstr ;
	READVAR(m_breakmode);
	READVAR(m_breaksize);
	READVAR(m_breaktime);
	READVAR(m_mindiskspace);
	READVAR(tmpstr);
	m_autodisk=tmpstr ;
	READVAR(m_shutdowndelay);

	DWORD eventmarker ;
	READVAR(eventmarker) ;
	for( int i=0; i<sys.sensornum ; i++ ) {
		m_eventmarker.SetSel(i, (eventmarker & (1<<i))!=0 );
	}
	READVAR(m_evmpostlock);
	READVAR(m_evmprelock);
	READVAR(m_ptz_enable);
	READVAR(m_ptz_port);
	READVAR(m_ptz_baudrate);
	READVAR(m_ptz_protocol);

	READVAR(m_videoencrypt);
	READVAR(sys.videopassword);
	m_videopassword = _T("********") ;

	UpdateData(FALSE);
}

void CSystemPage::Set() 
{
	SetSystemSetup(&sys, sizeof(struct system_stru)) ;
}

void CSystemPage::OnDvruserman() 
{
	// TODO: Add your control notification handler code here
//	CDvruserman dvruserman ;
//	dvruserman.DoModal();
}


/////////////////////////////////////////////////////////////////////////////
// CCameraPage property page

IMPLEMENT_DYNCREATE(CCameraPage, CPropertyPage)

CCameraPage::CCameraPage() : CPropertyPage(CCameraPage::IDD)
{
	//{{AFX_DATA_INIT(CCameraPage)
	m_AlarmMotionEn = FALSE;
	m_AlarmRecEn = FALSE;
	m_AlarmVSigLostEn = FALSE;
	m_CamEn = FALSE;
	m_FrameRate = 0;
	m_MotionSensitiveStr = _T("");
	m_CameraName = _T("");
	m_PostRecTime = 0;
	m_PreRecTime = 0;
	m_trigen1 = FALSE;
	m_trigen2 = FALSE;
	m_trigen3 = FALSE;
	m_trigen4 = FALSE;
	m_trigosd1 = FALSE;
	m_trigosd2 = FALSE;
	m_trigosd3 = FALSE;
	m_trigosd4 = FALSE;
	m_GPS = FALSE;
	m_bitrate = 0;
	m_bitrateEn = FALSE;
	m_ptz_addr = 0;
	//}}AFX_DATA_INIT
	cam_attr = new struct DvrChannel_attr [pSystemPage->sys.cameranum] ;
	for( int i=0; i<pSystemPage->sys.cameranum ; i++ ) {
		memset(&cam_attr[i], 0, sizeof( struct DvrChannel_attr ) );
		GetChannelSetup(&cam_attr[i], sizeof( struct DvrChannel_attr), i );
	}
	Modified=FALSE ;
}

CCameraPage::~CCameraPage()
{
	delete [] cam_attr ;
}

void CCameraPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CCameraPage)
	DDX_Control(pDX, IDC_TRIGPORT4, m_trigport4);
	DDX_Control(pDX, IDC_TRIGPORT3, m_trigport3);
	DDX_Control(pDX, IDC_TRIGPORT1, m_trigport1);
	DDX_Control(pDX, IDC_TRIGPORT2, m_trigport2);
	DDX_Control(pDX, IDC_RECORDMODE, m_RecordMode);
	DDX_Control(pDX, IDC_QUALITY, m_Quality);
	DDX_Control(pDX, IDC_PICTUREFORMAT, m_Resolution);
	DDX_Control(pDX, IDC_MOTSENSITIVE, m_MotionSensitive);
	DDX_Control(pDX, IDC_ALARMVIDEOLOSTPORT, m_AlarmVSigLostPort);
	DDX_Control(pDX, IDC_ALARMVIDEOLOSTPAT, m_AlarmVSigLostPat);
	DDX_Control(pDX, IDC_ALARMRECPORT, m_AlarmRecPort);
	DDX_Control(pDX, IDC_ALARMRECPAT, m_AlarmRecPat);
	DDX_Control(pDX, IDC_ALARMMOTIONPORT, m_AlarmMotionPort);
	DDX_Control(pDX, IDC_ALARMMOTIONPAT, m_AlarmMotionPat);
	DDX_Control(pDX, IDC_TAB_CAMERA, m_Tab);
	DDX_Check(pDX, IDC_ALARMMOTION, m_AlarmMotionEn);
	DDX_Check(pDX, IDC_ALARMREC, m_AlarmRecEn);
	DDX_Check(pDX, IDC_ALARMVIDEOLOST, m_AlarmVSigLostEn);
	DDX_Check(pDX, IDC_CAMENABLE, m_CamEn);
	DDX_Text(pDX, IDC_FRAMERATE, m_FrameRate);
	DDX_Text(pDX, IDC_MOTIONSENSTR, m_MotionSensitiveStr);
	DDX_Text(pDX, IDC_NAME, m_CameraName);
	DDX_Text(pDX, IDC_POSTREC, m_PostRecTime);
	DDX_Text(pDX, IDC_PREREC, m_PreRecTime);
	DDX_Check(pDX, IDC_TRIGEN1, m_trigen1);
	DDX_Check(pDX, IDC_TRIGEN2, m_trigen2);
	DDX_Check(pDX, IDC_TRIGEN3, m_trigen3);
	DDX_Check(pDX, IDC_TRIGEN4, m_trigen4);
	DDX_Check(pDX, IDC_TRIGOSD1, m_trigosd1);
	DDX_Check(pDX, IDC_TRIGOSD2, m_trigosd2);
	DDX_Check(pDX, IDC_TRIGOSD3, m_trigosd3);
	DDX_Check(pDX, IDC_TRIGOSD4, m_trigosd4);
	DDX_Check(pDX, IDC_GPS, m_GPS);
	DDX_Text(pDX, IDC_BITRATE, m_bitrate);
	DDX_Check(pDX, IDC_ENABLE_BITRATE, m_bitrateEn);
	DDX_Text(pDX, IDC_PTZ_ADDR, m_ptz_addr);
	DDV_MinMaxInt(pDX, m_ptz_addr, -1, 128);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CCameraPage, CPropertyPage)
	//{{AFX_MSG_MAP(CCameraPage)
	ON_NOTIFY(TCN_SELCHANGE, IDC_TAB_CAMERA, OnSelchangeTabCamera)
	ON_NOTIFY(NM_RELEASEDCAPTURE, IDC_MOTSENSITIVE, OnReleasedcaptureMotsensitive)
	ON_BN_CLICKED(IDC_ENABLE_BITRATE, OnEnableBitrate)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CCameraPage message handlers

BOOL CCameraPage::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();
	// TODO: Add extra initialization here

//	if( g_noptz ) {
		GetDlgItem(IDC_PTZ_ADDR)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_PTZ_BOX)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_SPIN_PTZ_ADDR)->ShowWindow(SW_HIDE);
//	}

	if(!cam_attr[0].GPS_enable ){
		this->GetDlgItem(IDC_GPS)->ShowWindow(SW_HIDE);
		this->GetDlgItem(IDC_GPSBOX)->ShowWindow(SW_HIDE);
		this->GetDlgItem(IDC_GPSDISP)->ShowWindow(SW_HIDE);
		this->GetDlgItem(IDC_GPSENGLISH)->ShowWindow(SW_HIDE);
		this->GetDlgItem(IDC_GPSMETRIC)->ShowWindow(SW_HIDE);
	}

	CString TabTitle ;
	int i;
	for( i=0 ; i<pSystemPage->sys.cameranum; i++ ) {
		TabTitle.Format(_T("Camera%d"), i+1);
		m_Tab.InsertItem(i, TabTitle);
	}

	CRect clientrect ;
	GetClientRect(&clientrect); 
	m_Tab.MoveWindow(&clientrect);

	((CSpinButtonCtrl *)GetDlgItem(IDC_SPIN_PTZ_ADDR))->SetRange32( -1, 128 );

	// init sensor related controls
	for( i=0; i<pSystemPage->sys.sensornum ; i++ ) {
		CString portnum ;
		portnum.Format(_T("%d"), i+1);
		m_trigport1.AddString(portnum);
		m_trigport2.AddString(portnum);
		m_trigport3.AddString(portnum);
		m_trigport4.AddString(portnum);
	}

	// init alarm related controls
	for( i=0; i<pSystemPage->sys.alarmnum ; i++ ) {
		CString alarmnum ;
		alarmnum.Format(_T("%d"), i+1);
		m_AlarmVSigLostPort.AddString(alarmnum);
		m_AlarmRecPort.AddString(alarmnum);
		m_AlarmMotionPort.AddString(alarmnum);
	}

	m_Quality.SetRange(0, 10);
	m_MotionSensitive.SetRange(0, 6);

	tabindex = 0 ;
	m_Tab.SetCurSel(0);
	PutData();
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CCameraPage::OnSelchangeTabCamera(NMHDR* pNMHDR, LRESULT* pResult) 
{
	// TODO: Add your control notification handler code here

	int tab = m_Tab.GetCurSel();
	if( tab!=tabindex ) {
		GetData();
		tabindex = tab ;
		PutData();
	}
	
	*pResult = 0;
}

void CCameraPage::OnOK() 
{
	// TODO: Add your specialized code here and/or call the base class
	tabindex = m_Tab.GetCurSel();
	GetData();
	Modified = TRUE ;
	CPropertyPage::OnOK();
}

void CCameraPage::PutData() 
{

	this->m_CamEn = cam_attr[tabindex].Enable ;
	this->m_CameraName = cam_attr[tabindex].CameraName ;
	this->m_Resolution.SetCurSel( cam_attr[tabindex].Resolution );
	this->m_RecordMode.SetCurSel( cam_attr[tabindex].RecMode ) ;
	this->m_Quality.SetPos(10-cam_attr[tabindex].PictureQuality );
	this->m_FrameRate = cam_attr[tabindex].FrameRate ;
	this->m_trigport1.SetCurSel(  cam_attr[tabindex].Sensor[0] );
	this->m_trigport2.SetCurSel(  cam_attr[tabindex].Sensor[1] );
	this->m_trigport3.SetCurSel(  cam_attr[tabindex].Sensor[2] );
	this->m_trigport4.SetCurSel(  cam_attr[tabindex].Sensor[3] );
	this->m_trigen1 = cam_attr[tabindex].SensorEn[0] ;
	this->m_trigen2 = cam_attr[tabindex].SensorEn[1] ;
	this->m_trigen3 = cam_attr[tabindex].SensorEn[2] ;
	this->m_trigen4 = cam_attr[tabindex].SensorEn[3] ;
	this->m_trigosd1 = cam_attr[tabindex].SensorOSD[0] ;
	this->m_trigosd2 = cam_attr[tabindex].SensorOSD[1] ;
	this->m_trigosd3 = cam_attr[tabindex].SensorOSD[2] ;
	this->m_trigosd4 = cam_attr[tabindex].SensorOSD[3] ;
	this->m_PreRecTime = cam_attr[tabindex].PreRecordTime ;
	this->m_PostRecTime = cam_attr[tabindex].PostRecordTime ;
	this->m_AlarmRecEn = cam_attr[tabindex].RecordAlarmEn ;
	this->m_AlarmRecPort.SetCurSel( cam_attr[tabindex].RecordAlarm ); 
	this->m_AlarmRecPat.SetCurSel( cam_attr[tabindex].RecordAlarmPat );
	this->m_AlarmVSigLostEn = cam_attr[tabindex].VideoLostAlarmEn ;
	this->m_AlarmVSigLostPort.SetCurSel( cam_attr[tabindex].VideoLostAlarm ); 
	this->m_AlarmVSigLostPat.SetCurSel( cam_attr[tabindex].VideoLostAlarmPat );
	this->m_AlarmMotionEn = cam_attr[tabindex].MotionAlarmEn ;
	this->m_AlarmMotionPort.SetCurSel( cam_attr[tabindex].MotionAlarm ); 
	this->m_AlarmMotionPat.SetCurSel( cam_attr[tabindex].MotionAlarmPat );
	this->m_MotionSensitive.SetPos( cam_attr[tabindex].MotionSensitivity );
	this->m_MotionSensitiveStr.Format( _T(" %d "), m_MotionSensitive.GetPos() );
	this->m_GPS = cam_attr[tabindex].ShowGPS ;
	if( cam_attr[tabindex].GPSUnit == 0 ) {
		((CButton *)GetDlgItem(IDC_GPSENGLISH))->SetCheck(BST_CHECKED);
		((CButton *)GetDlgItem(IDC_GPSMETRIC))->SetCheck(BST_UNCHECKED);
	}
	else {
		((CButton *)GetDlgItem(IDC_GPSENGLISH))->SetCheck(BST_UNCHECKED);
		((CButton *)GetDlgItem(IDC_GPSMETRIC))->SetCheck(BST_CHECKED);
	}

	this->m_bitrateEn = cam_attr[tabindex].BitrateEn ;
	this->m_bitrate = cam_attr[tabindex].Bitrate ;
	if( cam_attr[tabindex].BitMode == 0 ) {
		((CButton *)GetDlgItem(IDC_VBR))->SetCheck(BST_CHECKED);
		((CButton *)GetDlgItem(IDC_CBR))->SetCheck(BST_UNCHECKED);
	}
	else {
		((CButton *)GetDlgItem(IDC_VBR))->SetCheck(BST_UNCHECKED);
		((CButton *)GetDlgItem(IDC_CBR))->SetCheck(BST_CHECKED);
	}

	if( m_bitrateEn ) {
		this->GetDlgItem(IDC_STATIC_CONTROLMODE)->EnableWindow( TRUE );
		this->GetDlgItem(IDC_VBR)->EnableWindow( TRUE );;
		this->GetDlgItem(IDC_CBR)->EnableWindow( TRUE );;
		this->GetDlgItem(IDC_STATIC_BITRATE)->EnableWindow( TRUE );;
		this->GetDlgItem(IDC_BITRATE)->EnableWindow( TRUE );;
	}
	else {
		this->GetDlgItem(IDC_STATIC_CONTROLMODE)->EnableWindow( FALSE );
		this->GetDlgItem(IDC_VBR)->EnableWindow( FALSE );
		this->GetDlgItem(IDC_CBR)->EnableWindow( FALSE );
		this->GetDlgItem(IDC_STATIC_BITRATE)->EnableWindow( FALSE );
		this->GetDlgItem(IDC_BITRATE)->EnableWindow( FALSE );
	}
	this->m_ptz_addr = cam_attr[tabindex].ptz_addr ;

	UpdateData(FALSE);
}

void CCameraPage::GetData() 
{
	UpdateData();
	cam_attr[tabindex].Enable = m_CamEn ;
	stringcpy( cam_attr[tabindex].CameraName , m_CameraName ) ;
	cam_attr[tabindex].Resolution = m_Resolution.GetCurSel();
	cam_attr[tabindex].RecMode = m_RecordMode.GetCurSel();
	cam_attr[tabindex].PictureQuality = 10-m_Quality.GetPos ();
	cam_attr[tabindex].FrameRate = m_FrameRate  ;
	cam_attr[tabindex].Sensor[0] = m_trigport1.GetCurSel() ;
	cam_attr[tabindex].Sensor[1] = m_trigport2.GetCurSel() ;
	cam_attr[tabindex].Sensor[2] = m_trigport3.GetCurSel() ;
	cam_attr[tabindex].Sensor[3] = m_trigport4.GetCurSel() ;
	cam_attr[tabindex].SensorEn[0] = m_trigen1 ;
	cam_attr[tabindex].SensorEn[1] = m_trigen2 ;
	cam_attr[tabindex].SensorEn[2] = m_trigen3 ;
	cam_attr[tabindex].SensorEn[3] = m_trigen4 ;
	cam_attr[tabindex].SensorOSD[0] = m_trigosd1 ;
	cam_attr[tabindex].SensorOSD[1] = m_trigosd2 ;
	cam_attr[tabindex].SensorOSD[2] = m_trigosd3 ;
	cam_attr[tabindex].SensorOSD[3] = m_trigosd4 ;
	cam_attr[tabindex].PreRecordTime = m_PreRecTime ;
	cam_attr[tabindex].PostRecordTime = m_PostRecTime ;
	cam_attr[tabindex].RecordAlarmEn = m_AlarmRecEn ;
	cam_attr[tabindex].RecordAlarm = m_AlarmRecPort.GetCurSel();
	cam_attr[tabindex].RecordAlarmPat = m_AlarmRecPat.GetCurSel();
	cam_attr[tabindex].VideoLostAlarmEn = m_AlarmVSigLostEn ;
	cam_attr[tabindex].VideoLostAlarm = m_AlarmVSigLostPort.GetCurSel();
	cam_attr[tabindex].VideoLostAlarmPat = m_AlarmVSigLostPat.GetCurSel();
	cam_attr[tabindex].MotionAlarmEn = m_AlarmMotionEn ;
	cam_attr[tabindex].MotionAlarm = m_AlarmMotionPort.GetCurSel() ;
	cam_attr[tabindex].MotionAlarmPat = m_AlarmMotionPat.GetCurSel();
	cam_attr[tabindex].MotionSensitivity = m_MotionSensitive.GetPos();
	cam_attr[tabindex].ShowGPS = m_GPS ;
	cam_attr[tabindex].GPSUnit = ((CButton *)GetDlgItem(IDC_GPSENGLISH))->GetCheck()!=BST_CHECKED ;

	cam_attr[tabindex].BitrateEn = m_bitrateEn ;
	cam_attr[tabindex].Bitrate = m_bitrate ;
	cam_attr[tabindex].BitMode = ((CButton *)GetDlgItem(IDC_VBR))->GetCheck()!=BST_CHECKED ;
	cam_attr[tabindex].ptz_addr = m_ptz_addr ;
}

void CCameraPage::OnReleasedcaptureMotsensitive(NMHDR* pNMHDR, LRESULT* pResult) 
{
	// TODO: Add your control notification handler code here
	UpdateData();
	m_MotionSensitiveStr.Format( _T(" %d "), m_MotionSensitive.GetPos() );
	UpdateData(FALSE);	
	*pResult = 0;
}

void CCameraPage::Save(FILE * fh) 
{
	int tag = 0x5433 ;
	long pos = ftell(fh);
	pos = (pos+0x100) & 0xffffff00 ;
	fseek( fh, pos, SEEK_SET );
	WRITEVAR(tag);

	WRITEVAR( pSystemPage->sys.cameranum );

	int i ;
	if( IsWindow(m_hWnd) ) {
		tabindex = m_Tab.GetCurSel();
		GetData();
	}

	for( i=0; i<pSystemPage->sys.cameranum; i++ ) {
		WRITEVAR( cam_attr[i] );
	}
}

void CCameraPage::Load(FILE * fh) 
{
	int tag ;
	long pos = ftell(fh);
	pos = (pos+0x100) & 0xffffff00 ;
	fseek( fh, pos, SEEK_SET );

	READVAR(tag);
	if( tag!=0x5433 ) return ;
	Modified = TRUE ;

	int cameranum, readnum ;
	READVAR( cameranum );

	readnum = pSystemPage->sys.cameranum ;
	if( readnum>cameranum ) readnum = cameranum ;
	for( int i=0; i<readnum ; i++ ) {
		READVAR( cam_attr[i] );
	}

	fseek( fh, pos+sizeof(tag)+sizeof(cameranum)+cameranum*sizeof(cam_attr[0]), SEEK_SET) ;

	if( IsWindow(m_hWnd) ) {
		tabindex = m_Tab.GetCurSel();
		PutData();
	}
}

void CCameraPage::OnEnableBitrate() 
{
	// TODO: Add your control notification handler code here
	if( ((CButton *)GetDlgItem(IDC_ENABLE_BITRATE))->GetCheck()==BST_CHECKED  ) {
		this->GetDlgItem(IDC_STATIC_CONTROLMODE)->EnableWindow( TRUE );
		this->GetDlgItem(IDC_VBR)->EnableWindow( TRUE );;
		this->GetDlgItem(IDC_CBR)->EnableWindow( TRUE );;
		this->GetDlgItem(IDC_STATIC_BITRATE)->EnableWindow( TRUE );;
		this->GetDlgItem(IDC_BITRATE)->EnableWindow( TRUE );;
	}
	else {
		this->GetDlgItem(IDC_STATIC_CONTROLMODE)->EnableWindow( FALSE );
		this->GetDlgItem(IDC_VBR)->EnableWindow( FALSE );
		this->GetDlgItem(IDC_CBR)->EnableWindow( FALSE );
		this->GetDlgItem(IDC_STATIC_BITRATE)->EnableWindow( FALSE );
		this->GetDlgItem(IDC_BITRATE)->EnableWindow( FALSE );
	}
}

void CCameraPage::Set()
{
	if( Modified ) {
	  for( int i=0; i<pSystemPage->sys.cameranum; i++ ) {
		SetChannelSetup(&cam_attr[i], sizeof( struct DvrChannel_attr ), i);
	  }
	}
}


/////////////////////////////////////////////////////////////////////////////
// CSensorPage property page

IMPLEMENT_DYNCREATE(CSensorPage, CPropertyPage)

CSensorPage::CSensorPage() : CPropertyPage(CSensorPage::IDD)
{
	//{{AFX_DATA_INIT(CSensorPage)
	m_SensorName = _T("");
	m_inverted = FALSE;
	//}}AFX_DATA_INIT
	tabindex = 0 ;
	m_SensorName = pSystemPage->sys.sensorname[0] ;
	m_inverted = pSystemPage->sys.sensorinverted[0] ;
}

CSensorPage::~CSensorPage()
{
}

void CSensorPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSensorPage)
	DDX_Control(pDX, IDC_TABSENSOR, m_Tab);
	DDX_Text(pDX, IDC_SENSORNAME, m_SensorName);
	DDX_Check(pDX, IDC_INVERTED, m_inverted);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CSensorPage, CPropertyPage)
	//{{AFX_MSG_MAP(CSensorPage)
	ON_NOTIFY(TCN_SELCHANGE, IDC_TABSENSOR, OnSelchangeTabsensor)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSensorPage message handlers

BOOL CSensorPage::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();

	CString TabTitle ;
	for( int i=0 ; i<pSystemPage->sys.sensornum ; i++ ) {
		TabTitle.Format(_T("Sensor%d"), i+1);
		m_Tab.InsertItem(i, TabTitle);
	}

	CRect clientrect ;
	GetClientRect(&clientrect); 
	m_Tab.MoveWindow(&clientrect);

	tabindex = 0 ;
	m_Tab.SetCurSel(0);

	m_SensorName = pSystemPage->sys.sensorname[0] ;
	m_inverted = pSystemPage->sys.sensorinverted[0] ;
	UpdateData(FALSE);
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CSensorPage::OnSelchangeTabsensor(NMHDR* pNMHDR, LRESULT* pResult) 
{
	// TODO: Add your control notification handler code here
	
	int tab = m_Tab.GetCurSel();
	if( tab!=tabindex ) {
		UpdateData();
		stringcpy(pSystemPage->sys.sensorname[tabindex], m_SensorName) ;
		pSystemPage->sys.sensorinverted[tabindex]=m_inverted ;
		tabindex = tab ;
		m_inverted = pSystemPage->sys.sensorinverted[tabindex] ;
		m_SensorName = pSystemPage->sys.sensorname[tabindex] ;
		UpdateData(FALSE);
	}

	*pResult = 0;
	
}

void CSensorPage::OnOK() 
{
	// TODO: Add your specialized code here and/or call the base class
	UpdateData();
	stringcpy(pSystemPage->sys.sensorname[tabindex], m_SensorName) ;
	pSystemPage->sys.sensorinverted[tabindex]=m_inverted ;

	CPropertyPage::OnOK();
}

void CSensorPage::Save(FILE * fh)
{
	int tag = 0x5334 ;
	long pos = ftell(fh);
	pos = (pos+0x100) & 0xffffff00 ;
	fseek( fh, pos, SEEK_SET );
	WRITEVAR(tag);

	if( IsWindow(m_hWnd) )
		UpdateData();
	stringcpy(pSystemPage->sys.sensorname[tabindex], m_SensorName) ;
	pSystemPage->sys.sensorinverted[tabindex]=m_inverted ;
	WRITEVAR(pSystemPage->sys.sensorname);
	WRITEVAR(pSystemPage->sys.sensorinverted);
}


void CSensorPage::Load(FILE * fh)
{
	int tag ;
	long pos = ftell(fh);
	pos = (pos+0x100) & 0xffffff00 ;
	fseek( fh, pos, SEEK_SET );

	READVAR(tag);
	if( tag!=0x5334 ) return ;

	READVAR(pSystemPage->sys.sensorname);
	READVAR(pSystemPage->sys.sensorinverted);
	m_inverted = pSystemPage->sys.sensorinverted[tabindex] ;
	m_SensorName = pSystemPage->sys.sensorname[tabindex] ;

	if( IsWindow(m_hWnd) )
		UpdateData(FALSE);
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
// CStatusPage dialog

CStatusPage::CStatusPage(CWnd* pParent /*=NULL*/)
	: CPropertyPage(CStatusPage::IDD)
{
	//{{AFX_DATA_INIT(CStatusPage)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	m_timer = 0 ;
	tzchanged = 0;
}

void CStatusPage::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CStatusPage)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CStatusPage, CDialog)
	//{{AFX_MSG_MAP(CStatusPage)
	ON_WM_TIMER()
	ON_WM_DESTROY()
	ON_BN_CLICKED(IDC_SYN_TIME, OnSynTime)
	ON_CBN_SELCHANGE(IDC_TIMEZONE, OnSelchangeTimezone)
	ON_BN_CLICKED(IDC_DAYLIGHT, OnDaylight)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CStatusPage message handlers
BOOL CStatusPage::OnInitDialog() 
{
	int i;
	char tmpname[256] ;
	int sizetz ;

	CDialog::OnInitDialog();

	// TODO: Add extra initialization here

	memset(version, 0, sizeof(version));

	if( DVR_Version( version )>0 ) {
		sprintf(tmpname, "%d.%d.%d.%d", version[0], version[1], version[2], version[3] );
		SetDlgItemText(IDC_VERSION, CString(tmpname));
	}

	for( i=pSystemPage->sys.cameranum; i<16; i++ ) {
		GetDlgItem(IDC_CHECK_SIG0+i)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_CHECK_REC0+i)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_CHECK_MOTION0+i)->ShowWindow(SW_HIDE);
	}

	CComboBox * tz = (CComboBox *)GetDlgItem(IDC_TIMEZONE);

	if( version[0]!=2 ) {
		sizetz = sizeof(timezone_a)/sizeof(struct tz_info) ;
		for( i=0; i< sizetz; i++ ) {
			tz->AddString( timezone_a[i].tz_name );
		}
		DVRGetTimezone(&tzi);

		GetTimeZoneInformation( &localtzi ) ;
		for( i=0; i< sizetz; i++ ) {
			if( timezone_a[i].tzi.Bias == localtzi.Bias &&
				wcscmp( timezone_a[i].tzi.StandardName, localtzi.StandardName )==0 ){
				GetDlgItem(IDC_LOCALTIMEZONENAME)->SetWindowText(timezone_a[i].tz_name );
				break;
			}
		}
		if( i>= sizetz ) {
			for( i=0; i< sizetz; i++ ) {
				if( timezone_a[i].tzi.Bias == localtzi.Bias ){
					GetDlgItem(IDC_LOCALTIMEZONENAME)->SetWindowText(timezone_a[i].tz_name );
					break;
				}
			}
		}

		if( tzi.DaylightBias == tzi.StandardBias  ){
			((CButton *)GetDlgItem(IDC_DAYLIGHT))->SetCheck(0);
		}
		else {
			((CButton *)GetDlgItem(IDC_DAYLIGHT))->SetCheck(1);
		}

		if( tzi.StandardDate.wMonth == 0 ) {
			GetDlgItem(IDC_DAYLIGHT)->ShowWindow(SW_HIDE);
		}

		for( i=0; i< sizetz; i++ ) {
			if( timezone_a[i].tzi.Bias == tzi.Bias &&
				wcscmp( timezone_a[i].tzi.StandardName, tzi.StandardName )==0 ){
				tz->SetCurSel(i);
				break;
			}
		}
		if( i>= sizetz ) {
			for( i=0; i< sizetz; i++ ) {
				if( timezone_a[i].tzi.Bias == tzi.Bias ){
					tz->SetCurSel(i);
					break;
				}
			}
		}
	}
	else {
		char * zoneinfo ;
		char * timezone ;
		char * p1, * p2 ;
		tz->ResetContent();
		if( DVR2ZoneInfo( &zoneinfo )>0 ) {
			p1=zoneinfo ;
			while( *p1 != 0 ) {
				p2=strchr(p1, '\n');
				if( p2!=NULL ) {
					*p2=0 ;
					tz->AddString( CString(p1) );
				}
				else {
					tz->AddString( CString(p1) );
					break;
				}
				p1=p2+1 ;
			}
			delete zoneinfo ;
		}

		if( DVR2GetTimeZone( &timezone )>0 ) {
			tz->SelectString(0, CString(timezone));
			delete timezone ;
		}
		GetDlgItem(IDC_DAYLIGHT)->ShowWindow(SW_HIDE);
		if( GetTimeZoneInformation( &localtzi )==TIME_ZONE_ID_DAYLIGHT ) {
			wcstombs( tmpname, localtzi.DaylightName, 40 );
		}
		else {
			wcstombs( tmpname, localtzi.StandardName, 40 );
		}
		GetDlgItem(IDC_LOCALTIMEZONENAME)->SetWindowText(CString(tmpname));
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CStatusPage::OnSelchangeTimezone() 
{
	tzchanged = 1;
	if( version[0]==2 ) 
		return ;
	CComboBox * tz = (CComboBox *)GetDlgItem(IDC_TIMEZONE);
	int i=tz->GetCurSel();
	tzi=timezone_a[i].tzi ;
	if( tzi.StandardDate.wMonth == 0 ) {
		GetDlgItem(IDC_DAYLIGHT)->ShowWindow(SW_HIDE);
	}
	else {
		GetDlgItem(IDC_DAYLIGHT)->ShowWindow(SW_SHOW);
	}
}


void CStatusPage::OnDaylight() 
{
	tzchanged = 1;
}

void CStatusPage::UpdateStatus()
{
	struct channelstatus cs[16] ;
	memset( cs, 0, sizeof(cs));

// return number of channels which status have been retrieved.
	int ch ;
	ch = DVRGetChannelState( cs, 16 );
	for( int i=0; i<ch; i++ ) {
		this->CheckDlgButton(IDC_CHECK_SIG0+i, cs[i].sig );
		this->CheckDlgButton(IDC_CHECK_REC0+i, cs[i].rec );
		this->CheckDlgButton(IDC_CHECK_MOTION0+i, cs[i].mot );
	}
	SYSTEMTIME st ;
	if( version[0]==2 ) {
		if( DVR2GetTime( &st, 0 ) ) {
			CTime dvrtime(st);
			SetDlgItemText(IDC_DVR_TIME, dvrtime.Format(_T("%Y-%m-%d  %H:%M:%S") ) );
		}
	}
	else {
		if( DVRGetLocalTime( &st ) ) {
			CTime dvrtime(st);
			SetDlgItemText(IDC_DVR_TIME, dvrtime.Format(_T("%Y-%m-%d  %H:%M:%S") ) );
		}
	}
	GetLocalTime(&st) ;
	CTime ltime(st);
	SetDlgItemText(IDC_LOCALTIME, ltime.Format(_T("%Y-%m-%d  %H:%M:%S") ) );
}

void CStatusPage::OnTimer(UINT nIDEvent) 
{
	// TODO: Add your message handler code here and/or call default
	UpdateStatus();	
	CDialog::OnTimer(nIDEvent);
}

BOOL CStatusPage::OnSetActive()
{
	BOOL result ;
	result = CPropertyPage::OnSetActive();

	m_timer = (int)SetTimer( 977, 1000, NULL ) ;
	UpdateStatus();
	
	return result ;
}

BOOL CStatusPage::OnKillActive()
{
	if( m_timer!= 0 ) {
		KillTimer( m_timer );	
		m_timer = 0 ;
	}
	return CPropertyPage::OnKillActive();
}


void CStatusPage::OnDestroy() 
{
	CDialog::OnDestroy();
	
	// TODO: Add your message handler code here
	if( m_timer!= 0 ) {
		KillTimer( m_timer );	
		m_timer = 0 ;
	}
}

void CStatusPage::OnSynTime() 
{
	// Set DVR time zone
	CComboBox * cb = (CComboBox *)GetDlgItem(IDC_TIMEZONE) ;
	if( tzchanged ) {
		int sel = cb->GetCurSel();
		if( version[0]==2 ) {
			if( sel!=CB_ERR ) {
				int l = cb->GetLBTextLen( sel ) ;
				TCHAR * tz=new TCHAR [l+2] ;
				cb->GetLBText( sel, tz );
				char * tzc=new char [l+2] ;
				stringncpy(tzc,tz, l+2);
				DVR2SetTimeZone(tzc);
				delete tzc ;
				delete tz ;
			}
		}
		else {
			tzi = timezone_a[sel].tzi ;
			if( tzi.StandardDate.wMonth!=0 &&   
				((CButton *)GetDlgItem(IDC_DAYLIGHT))->GetCheck()==BST_UNCHECKED ) {
				tzi.DaylightBias = tzi.StandardBias ;
				tzi.DaylightDate = tzi.StandardDate ;
				memcpy( &(tzi.DaylightName), &(tzi.StandardName), sizeof(tzi.DaylightName));   
			}
			DVRSetTimezone(&tzi);
		}
		tzchanged=0 ;
	}

	SYSTEMTIME st ;
	if( version[0]==2 ) {
		if( DVR2GetTime(&st, 1)==0 ) {
			MessageBox( _T("Set DVR time error!"), _T("Set DVR time"), MB_OK );
			return ;
		}
	}
	else {
		if( DVRGetSystemTime( &st )==0 ){
			MessageBox( _T("Set DVR time error!"), _T("Set DVR time"), MB_OK );
			return ;
		}
	}
	CTime dvrtime(st);
	GetSystemTime(&st);
	CTime now(st);
	if( now<dvrtime ) {
		if( MessageBox( _T("DVR time will be set backward, continue?"), _T("Set DVR time"), MB_YESNO) != IDYES ) {
			return ;
		}
	}

	if( version[0]==2 ) {
		if( DVR2SetTime( &st, 1 ) ) {
			MessageBox( _T("DVR time synchronized!"), _T("Set DVR time"), MB_OK );
		}
		else {
			MessageBox( _T("Set DVR time error!"), _T("Set DVR time"), MB_OK );
		}
	}
	else {
		if( DVRSetSystemTime( &st ) ) {
			MessageBox( _T("DVR time synchronized!"), _T("Set DVR time"), MB_OK );
		}
		else {
			MessageBox( _T("Set DVR time error!"), _T("Set DVR time"), MB_OK );
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
// DvrSetup

IMPLEMENT_DYNAMIC(DvrSetup, CPropertySheet)

DvrSetup::DvrSetup(UINT nIDCaption, CWnd* pParentWnd, UINT iSelectPage)
	:CPropertySheet(nIDCaption, pParentWnd, iSelectPage)
{
}

DvrSetup::DvrSetup(LPCTSTR pszCaption, CWnd* pParentWnd, UINT iSelectPage)
	:CPropertySheet(pszCaption, pParentWnd, iSelectPage)
{
}

DvrSetup::~DvrSetup()
{
}


BEGIN_MESSAGE_MAP(DvrSetup, CPropertySheet)
	//{{AFX_MSG_MAP(DvrSetup)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_COMMAND(IDC_SAVESETUP, OnSaveSetup)
	ON_COMMAND(IDC_LOADSETUP, OnLoadSetup)
	ON_BN_CLICKED(IDC_COMMITSETUP, OnCommit)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

int DvrSetup::m_commit;
 
/////////////////////////////////////////////////////////////////////////////
// DvrSetup message handlers

int DvrSetup::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CPropertySheet::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	BtCommit.Create(_T("Reboot DVR (commit setup)"), WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX|WS_TABSTOP, CRect(0,0,1,1), this, IDC_COMMITSETUP ); 
	BtLoad.Create( _T("Load Config"), WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|WS_TABSTOP, CRect(1,1,2,2), this, IDC_LOADSETUP );
	BtSave.Create( _T("Save Config"), WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|WS_TABSTOP, CRect(0,0,1,1), this, IDC_SAVESETUP );
	BtCommit.SetCheck(m_commit);
	return 0;
}

void DvrSetup::OnSize(UINT nType, int cx, int cy) 
{
	CPropertySheet::OnSize(nType, cx, cy);
	
	CRect r ;
	int width = 75;
	int height = 23 ;

	r.bottom = cy - 7 ;
	r.top = r.bottom - height ;
	r.right = cx - 170 ;

	r.left = r.right - width ;
	BtSave.MoveWindow( r );
	r.right = r.left - 10 ;
	r.left = r.right - width ;
	BtLoad.MoveWindow( r );

	r.left = 20 ;
	r.right = 300 ;
	BtCommit.MoveWindow( r );

	CWnd * OkButton=GetDlgItem(IDOK);
	if( OkButton==NULL )return ;
	BtSave.SetFont(OkButton->GetFont());
	BtLoad.SetFont(OkButton->GetFont());
	BtCommit.SetFont(OkButton->GetFont());
}

void DvrSetup::OnSaveSetup() 
{
	FILE * fconfig ;
	char filename[512] ;
	CFileDialog fileDialog(FALSE, _T(".DCF"), NULL, OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT, 
		_T("DVR Configure Files (*.DCF)|*.DCF|All files|*.*|\0\0"), this ) ;
	if( fileDialog.DoModal()==IDOK ) {
		stringcpy(filename, fileDialog.GetPathName());
		fconfig = fopen( filename, "wb");
		if( fconfig==NULL ) {
			MessageBox(_T("Error!"));
			return ;
		}
		((CSystemPage *)pSystemPage)->Save( fconfig );
		((CSensorPage *)pSensorPage)->Save( fconfig );
		((CCameraPage *)pCameraPage)->Save( fconfig );
		fclose(fconfig);
	}
	else {
		return ;
	}
}

void DvrSetup::OnLoadSetup()
{
	FILE * fconfig ;
	char filename[512] ;
	CFileDialog fileDialog(TRUE, _T(".DCF"), NULL, OFN_HIDEREADONLY, 
		_T("DVR Configure Files (*.DCF)|*.DCF|All files|*.*|\0\0"), this ) ;
	if( fileDialog.DoModal()==IDOK ) {
		stringncpy(filename, fileDialog.GetPathName(), 512);
		fconfig = fopen( filename, "rb");
		if( fconfig==NULL ) {
			MessageBox(_T("Error!"));
			return ;
		}
		((CSystemPage *)pSystemPage)->Load( fconfig );
		((CSensorPage *)pSensorPage)->Load( fconfig );
		((CCameraPage *)pCameraPage)->Load( fconfig );
		fclose( fconfig );
	}
	else {
		return ;
	}
	
}

void DvrSetup::OnCommit()
{
	m_commit = BtCommit.GetCheck();
}

// open html setup dialog
int openhtmldlg( char * servername, char * page )
{
	char url[200] ;
	TCHAR TURL[200] ;
	sprintf(url, "http://%s%s", servername, page);

	MultiByteToWideChar(CP_UTF8, 0, url, -1, TURL, 200);

	ShellExecute(NULL, _T("open"), TURL, NULL, NULL, SW_SHOWNORMAL );
	return 1;
}

// configure DVR server
// return 1 : success
//        0 : canceled
//       -1 : error
int DvrConfigure( char * servername )
{
	int res = 0 ;
	CString Title ;
	Title.Format( _T("DVR Configure - %s"), CString(servername) );

	cfgsocket = net_connect( servername );
	if( cfgsocket==0 ) {
		return -1 ;
	}

	char pagebuf[200] ;
	if( net_GetSetupPage(cfgsocket, pagebuf, sizeof( pagebuf ) ) ) {
		openhtmldlg( servername, pagebuf );
		net_close(cfgsocket);
		return 1 ;
	}

	DvrSetup ps (Title) ;
	ps.m_psh.dwFlags  |= PSH_NOAPPLYNOW ;
	CSystemPage systempage;
	if( systempage.sys.cameranum==0) {
		net_close(cfgsocket);
		return -1 ;
	}
	CString oldServerName(systempage.sys.ServerName) ; 
	CCameraPage camerapage;
	CSensorPage sensorpage ;
	CStatusPage statuspage ;
	ps.AddPage(&systempage);
	ps.pSystemPage = &systempage ;
	ps.AddPage(&camerapage);
	ps.pCameraPage = &camerapage ;
	ps.AddPage(&sensorpage);
	ps.pSensorPage = &sensorpage ;
	ps.AddPage(&statuspage);
	if (ps.DoModal()==IDOK ) {
		statuspage.Set();
		sensorpage.Set();
		camerapage.Set();
		systempage.Set();
		CString newServerName(systempage.sys.ServerName) ;
		if( newServerName != oldServerName) {
		}
		else if( ps.m_commit != 0 ) {
			DVRKill( TRUE );
		}
		res=1 ;
	};

	net_close(cfgsocket);
	return res ;
}

