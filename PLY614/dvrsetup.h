// DvrSetup.h

#ifndef __DVRSETUP_H__
#define __DVRSETUP_H__

#include "ply614.h"
#include "dvrnet.h"

// SystemPage.h : header file
//
struct system_stru {
//	char IOMod[80]  ;
	char productid[8] ;
	char serial[24] ;
	char id1[24] ;
	char id2[24] ;
	char ServerName[80] ;
	int cameranum ;
	int alarmnum ;
	int sensornum ;
	int breakMode ;
	int breakTime ;
	int breakSize ;
	int minDiskSpace ;
	int shutdowndelay ;
	char autodisk[32] ;
	char sensorname[16][32];
	int sensorinverted[16];
	int eventmarker_enable ;
	int eventmarker ;
	int eventmarker_prelock ;
	int eventmarker_postlock ;
	char ptz_en ;
	char ptz_port ;			// COM1: 0
	char ptz_baudrate ;		// times of 1200
	char ptz_protocol ;		// 0: PELCO "D"   1: PELCO "P"
	int  videoencrpt_en ;	// enable video encryption
	char videopassword[32] ;
	char res[88];
};

/////////////////////////////////////////////////////////////////////////////
// CSystemPage dialog

class CSystemPage : public CPropertyPage
{
	DECLARE_DYNCREATE(CSystemPage)

// Construction
public:
	CSystemPage();
	~CSystemPage();

	void Save(FILE * fh) ;
	void Load(FILE * fh) ;
	void Set();

	struct system_stru sys ;

// Dialog Data
	//{{AFX_DATA(CSystemPage)
	enum { IDD = IDD_SYSTEM_PAGE };
	CListBox	m_eventmarker;
	int		m_alarmnum;
	CString	m_autodisk;
	int		m_breaksize;
	int		m_breaktime;
	int		m_cameranum;
	CString	m_iomodule;
	int		m_sensornum;
	int		m_mindiskspace;
	int		m_breakmode;
	CString	m_servername;
	int		m_shutdowndelay;
	int		m_evmpostlock;
	int		m_evmprelock;
	BOOL	m_ptz_enable;
	int		m_ptz_baudrate;
	int		m_ptz_port;
	int		m_ptz_protocol;
	BOOL	m_videoencrypt;
	CString	m_videopassword;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CSystemPage)
	public:
	virtual void OnOK();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CSystemPage)
	virtual BOOL OnInitDialog();
	afx_msg void OnDvruserman();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};


// CameraPage.h : header file
//

struct  DvrChannel_attr {
	BOOL	Enable ;
	char    CameraName[64] ;
	int		Resolution;				// 0: CIF, 1: 2CIF, 2:DCIF, 3:4CIF
	int		RecMode;				// 0: Continue, 1: Trigger by sensor, 2: triger by motion, 3, trigger by sensor/motion, 4: Schedule
	int		PictureQuality;			// 0: lowest, 10: Highest
	int		FrameRate;
	int		Sensor[4] ;
	BOOL	SensorEn[4] ;
	BOOL	SensorOSD[4] ;
	int		PreRecordTime ;			// pre-recording time in seconds
	int		PostRecordTime ;		// post-recording time in seconds
	int		RecordAlarm ;			// Recording Signal
	BOOL	RecordAlarmEn ;
	int		RecordAlarmPat ;		// 0: OFF, 1: ON, 2: 0.5s Flash, 3: 2s Flash, 4, 4s Flash
	int		VideoLostAlarm ;		// Video signal lost alarm
	BOOL	VideoLostAlarmEn ;
	int		VideoLostAlarmPat ;
	int		MotionAlarm ;			// Motion Detect alarm
	BOOL	MotionAlarmEn ;
	int		MotionAlarmPat ;
	int		MotionSensitivity;		
	int		GPS_enable ;			// enable gps options
	int		ShowGPS ;
	int     GPSUnit ;				// GPS unit display, 0: English, 1: Metric
	BOOL    BitrateEn ;				// Enable Bitrate Control
	int		BitMode;				// 0: VBR, 1: CBR
	int		Bitrate;
	int		ptz_addr ;
	int		reserved[5] ;
} ;

/////////////////////////////////////////////////////////////////////////////
// CCameraPage dialog

class CCameraPage : public CPropertyPage
{
	DECLARE_DYNCREATE(CCameraPage)
	
// Construction
public:
	CCameraPage();
	~CCameraPage();

	void Set();
	void Save(FILE * fh) ;
	void Load(FILE * fh) ;

// Dialog Data
	//{{AFX_DATA(CCameraPage)
	enum { IDD = IDD_CAMERA_PAGE };
	CComboBox	m_trigport4;
	CComboBox	m_trigport3;
	CComboBox	m_trigport1;
	CComboBox	m_trigport2;
	CComboBox	m_RecordMode;
	CSliderCtrl	m_Quality;
	CComboBox	m_Resolution;
	CSliderCtrl	m_MotionSensitive;
	CComboBox	m_AlarmVSigLostPort;
	CComboBox	m_AlarmVSigLostPat;
	CComboBox	m_AlarmRecPort;
	CComboBox	m_AlarmRecPat;
	CComboBox	m_AlarmMotionPort;
	CComboBox	m_AlarmMotionPat;
	CTabCtrl	m_Tab;
	BOOL	m_AlarmMotionEn;
	BOOL	m_AlarmRecEn;
	BOOL	m_AlarmVSigLostEn;
	BOOL	m_CamEn;
	UINT	m_FrameRate;
	CString	m_MotionSensitiveStr;
	CString	m_CameraName;
	int		m_PostRecTime;
	int		m_PreRecTime;
	BOOL	m_trigen1;
	BOOL	m_trigen2;
	BOOL	m_trigen3;
	BOOL	m_trigen4;
	BOOL	m_trigosd1;
	BOOL	m_trigosd2;
	BOOL	m_trigosd3;
	BOOL	m_trigosd4;
	BOOL	m_GPS;
	UINT	m_bitrate;
	BOOL	m_bitrateEn;
	int		m_ptz_addr;
	//}}AFX_DATA

	int tabindex ;
	BOOL Modified;

	struct  DvrChannel_attr  * cam_attr ;

	void PutData() ;
	void GetData() ;

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CCameraPage)
	public:
	virtual void OnOK();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CCameraPage)
	virtual BOOL OnInitDialog();
	afx_msg void OnSelchangeTabCamera(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnReleasedcaptureMotsensitive(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnEnableBitrate();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

// SensorPage.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CSensorPage dialog

class CSensorPage : public CPropertyPage
{
	DECLARE_DYNCREATE(CSensorPage)

// Construction
public:
	CSensorPage();
	~CSensorPage();

	void Save(FILE * fh) ;
	void Load(FILE * fh) ;
	void Set(){};

// Dialog Data
	//{{AFX_DATA(CSensorPage)
	enum { IDD = IDD_SENSOR_PAGE };
	CTabCtrl	m_Tab;
	CString	m_SensorName;
	BOOL	m_inverted;
	//}}AFX_DATA

	int tabindex;

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CSensorPage)
	public:
	virtual void OnOK();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CSensorPage)
	virtual BOOL OnInitDialog();
	afx_msg void OnSelchangeTabsensor(NMHDR* pNMHDR, LRESULT* pResult);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};
// StatusPage.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CStatusPage dialog

class CStatusPage : public CPropertyPage
{
// Construction
public:
	CStatusPage(CWnd* pParent = NULL);   // standard constructor

	void Set(){};

// Dialog Data
	//{{AFX_DATA(CStatusPage)
	enum { IDD = IDD_DVRSTATUS_PAGE };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CStatusPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	void UpdateStatus();
	BOOL OnSetActive();
	BOOL OnKillActive();

	int	m_timer ;
	TIME_ZONE_INFORMATION tzi ;
	TIME_ZONE_INFORMATION localtzi ;
	int tzchanged ;
	int version[8] ;

	// Generated message map functions
	//{{AFX_MSG(CStatusPage)
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnDestroy();
	afx_msg void OnSynTime();
	virtual BOOL OnInitDialog();
	afx_msg void OnSettimezone();
	afx_msg void OnSelchangeTimezone();
	afx_msg void OnDaylight();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// DvrSetup

class DvrSetup : public CPropertySheet
{
	DECLARE_DYNAMIC(DvrSetup)

// Construction
public:
	DvrSetup(UINT nIDCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);
	DvrSetup(LPCTSTR pszCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);

// Attributes
public:
	CButton		BtSave;
	CButton		BtLoad;
	CButton		BtCommit;
	
	CPropertyPage * pSystemPage ;
	CPropertyPage * pCameraPage ;
	CPropertyPage * pSensorPage ;

    static int m_commit ;

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(DvrSetup)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~DvrSetup();

	// Generated message map functions
protected:
	//{{AFX_MSG(DvrSetup)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSaveSetup();
	afx_msg void OnLoadSetup();
	afx_msg void OnCommit();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

#endif //__DVRSETUP_H__