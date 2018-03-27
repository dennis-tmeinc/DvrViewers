
// dvrnet.h
// DVR 301 net protocol

#ifndef __DVRNET_H__
#define __DVRNET_H__

#include <winsock2.h>
#include <ws2tcpip.h>

#include "PLY301.h"

// test zeus6
// #define PORT_TMEDVR		15114

#define PORT_TMEDVR		15111
#define REQDVR		0xa782
#define DVRSVR		0x6953
#define REQDVREX	0x7986a348
#define DVRSVREX	0x95349b63
#define REQSHUTDOWN 0x5d26f7a3

#define NETDELAY (3)

/////////////////////////////////////////////////////////////////////////////
// CClient

enum reqcode_type { REQOK =	1, 
    REQREALTIME, REQREALTIMEDATA, 
    REQCHANNELINFO, REQCHANNELDATA,
    REQSWITCHINFO, REQSHAREINFO, 
    REQPLAYBACKINFO, REQPLAYBACK,
    REQSERVERNAME,
    REQAUTH, REQGETSYSTEMSETUP, 
    REQSETSYSTEMSETUP, REQGETCHANNELSETUP,
    REQSETCHANNELSETUP, REQKILL, 
    REQSETUPLOAD, REQGETFILE, REQPUTFILE,
    REQHOSTNAME, REQRUN,
    REQRENAME, REQDIR, REQFILE, 
    REQGETCHANNELSTATE, REQSETTIME, REQGETTIME,
    REQSETLOCALTIME, REQGETLOCALTIME, 
    REQSETSYSTEMTIME, REQGETSYSTEMTIME,
    REQSETTIMEZONE, REQGETTIMEZONE,
    REQFILECREATE, REQFILEREAD, 
    REQFILEWRITE, REQFILESETPOINTER, REQFILECLOSE,
    REQFILESETEOF, REQFILERENAME, REQFILEDELETE,
    REQDIRFINDFIRST, REQDIRFINDNEXT, REQDIRFINDCLOSE,
    REQFILEGETPOINTER, REQFILEGETSIZE, REQGETVERSION, 
    REQPTZ, REQCAPTURE,
    REQKEY, REQCONNECT, REQDISCONNECT,
    REQCHECKID, REQLISTID, REQDELETEID, REQADDID, REQCHPASSWORD,
    REQSHAREPASSWD, REQGETTZIENTRY,
    REQECHO,
    
    REQ2BEGIN =	200, 
    REQSTREAMOPEN, REQSTREAMOPENLIVE, REQSTREAMCLOSE, 
    REQSTREAMSEEK, REQSTREAMGETDATA, REQSTREAMTIME,
    REQSTREAMNEXTFRAME, REQSTREAMNEXTKEYFRAME, REQSTREAMPREVKEYFRAME,
    REQSTREAMDAYINFO, REQSTREAMMONTHINFO, REQLOCKINFO, REQUNLOCKFILE, REQSTREAMDAYLIST,
    REQOPENLIVE,
    REQ2ADJTIME,
    REQ2SETLOCALTIME, REQ2GETLOCALTIME, REQ2SETSYSTEMTIME, REQ2GETSYSTEMTIME,
    REQ2GETZONEINFO, REQ2SETTIMEZONE, REQ2GETTIMEZONE,
    REQSETLED,
    REQSETHIKOSD,                                               // Set OSD for hik's cards
    REQ2GETCHSTATE,
    REQ2GETSETUPPAGE,
    REQ2GETSTREAMBYTES,
    REQ2GETSTATISTICS,
    REQ2KEYPAD,
    REQ2PANELLIGHTS,
    REQ2GETJPEG,
    
    REQ3BEGIN = 300,
    REQSENDDATA,
    REQGETDATA,
    REQCHECKKEY,
    REQUSBKEYPLUGIN, // plug-in a new usb key. (send by plug-in autorun program)
    
    REQ5BEGIN = 500,                                            // For eagle32 system
    REQNFILEOPEN,
    REQNFILECLOSE,
    REQNFILEREAD,
    
    REQMAX
};

enum anscode_type { ANSERROR =1, ANSOK, 
    ANSREALTIMEHEADER, ANSREALTIMEDATA, ANSREALTIMENODATA,
    ANSCHANNELHEADER, ANSCHANNELDATA, 
    ANSSWITCHINFO,
    ANSPLAYBACKHEADER, ANSSYSTEMSETUP, ANSCHANNELSETUP, ANSSERVERNAME,
    ANSGETCHANNELSTATE,
    ANSGETTIME, ANSTIMEZONEINFO, ANSFILEFIND, ANSGETVERSION, ANSFILEDATA,
    ANSKEY, ANSCHECKID, ANSLISTID, ANSSHAREPASSWD, ANSTZENTRY,
    ANSECHO,
    
    ANS2BEGIN =	200, 
    ANSSTREAMOPEN, ANSSTREAMDATA,ANSSTREAMTIME,
    ANSSTREAMDAYINFO, ANSSTREAMMONTHINFO, ANSLOCKINFO, ANSSTREAMDAYLIST,
    ANS2RES1, ANS2RES2, ANS2RES3,                        	// reserved, don't use
    ANS2TIME, ANS2ZONEINFO, ANS2TIMEZONE, ANS2CHSTATE, ANS2SETUPPAGE, ANS2STREAMBYTES,
    ANS2JPEG,
    
    ANS3BEGIN = 300,
    ANSSENDDATA,
    ANSGETDATA,
    
    ANS5BEGIN = 500,					// file base copying
    ANSNFILEOPEN,
    ANSNFILEREAD,
    
    ANSMAX
};

struct Req_type {
	int	reqcode ;		// Request code
	int data ;			// Request parameter, ex: channel number
	int reqsize ;		// Request data size
} ;

struct Answer_type {
	int anscode ;		// Answer code
	int data ;		    // Answer single value data
	int anssize ;		// Answer data size
} ;

#define MAX_ANSSIZE		(1000000)	// Maximum recving size

#ifdef _DEBUG
#define RECV_TIMEOUT	(10000000)	// Recving time out
#else
#define RECV_TIMEOUT	(10000000)	// Recving time out
#endif

extern USHORT g_dvr_port ;
extern int g_lan_detect ;
extern int g_net_watchdog ;
extern int g_remote_support ;

// DVR system setting structure
struct system_stru {
//	char IOMod[80]  ;
	char productid[8] ;
	char serial[24] ;
	char id1[24] ;
	char id2[24] ;
//
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

// DVR camera setting structure
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


struct channelstatus {
	int sig;            // signal lost
	int rec;            // recording
	int mot;            // motion
};

//CSocket DVRClient ;

struct sockad {
	int    len;
    union {
	    sockaddr addr;
        sockaddr_storage addrst ;
    };
};

// convert computer name (string) to sockad
// return 0 for success
int net_sockaddr(struct sockad * sad, const char * cpname, int port) ;

// convert sockad to computer name
// return 0 for success
int net_sockname(char * cpname, struct sockad * sad);

// return 1 if socket ready to read, 0 for time out
int net_wait(SOCKET sock, int timeout);

// clear received buffer
void net_clear(SOCKET sock);

// send data with clear receiving buffer
void net_send( SOCKET sock, void * buf, int bufsize );

// recv network data until buffer filled. 
// return 1: success
//		  0: failed
int net_recv( SOCKET sock, void * buf, int bufsize, int timeout=RECV_TIMEOUT );

int net_finddevice();

int net_shutdown( SOCKET s );

// connect to a DVR server
SOCKET net_connect( char * dvrserver, int port = 0 );

// close a connection
void net_close(SOCKET so);

// get server(player) information. 
//        setup ppi->servername, ppi->total_channel, ppi->serverversion
// return 1: success
//        0: failed
int net_getserverinfo( SOCKET so, struct player_info * ppi );

// return number of DVR video channels
// return 0: error, 1: success
int net_channelinfo( SOCKET so, struct channel_info * pci, int totalchannel);

// Get DVR server version
// parameter
//		so: socket
//		version: 4 int array 
// return 0: error, 1: success
int net_dvrversion(SOCKET so, int * version);

// Start play a preview channel
// return 1 : success
//        0 : failed
int net_preview_play( SOCKET so, int channel );

// Get data from preview stream
// return 1 : success
//        0 : failed
int net_preview_getdata( SOCKET so, char ** framedata, int * framesize );

// open a live stream
// return 0 : failed
//        >0 : stream handle ( should be == 2 )
int net_live_open( SOCKET so, int channel );

// Get a frame from live stream
// return:
//		-1 = error
//		frame type
int net_live_getframe( SOCKET so, char ** framedata, int * framesize );
// get DVR system time
// return 
//        1 success, DVR time in *t
//        0 failed
int net_getdvrtime(SOCKET so, struct dvrtime * t );

// get DVR system setup
int net_GetSystemSetup(SOCKET so, void * buf, int bufsize);

int net_SetSystemSetup(SOCKET so, void * buf, int bufsize);

int net_GetChannelSetup(SOCKET so, void * buf, int bufsize, int channel);

int net_SetChannelSetup(SOCKET so, void * buf, int bufsize, int channel);

int net_DVRGetChannelState(SOCKET so, void * buf, int bufsize );

int net_DVRGetTimezone( SOCKET so, TIME_ZONE_INFORMATION * tzi );

int net_DVRSetTimezone( SOCKET so, TIME_ZONE_INFORMATION * tzi );

int net_DVRGetLocalTime(SOCKET so, SYSTEMTIME * st );

int net_DVRSetLocalTime(SOCKET so, SYSTEMTIME * st );

int net_DVRGetSystemTime(SOCKET so, SYSTEMTIME * st );

int net_DVRSetSystemTime(SOCKET so, SYSTEMTIME * st );

// Get time zone info
// return buffer size returned, return 0 for error
int net_DVR2ZoneInfo( SOCKET so, char ** zoneinfo);

// Set DVR time zone
//   timezone: timezone name to set
// return 1:success, 0:fail
int net_DVR2SetTimeZone(SOCKET so, char * timezone);

// Get DVR time zone
// return buffer size returned, return 0 for error
//    timezone: DVR timezone name 
int net_DVR2GetTimeZone( SOCKET so, char ** timezone );

// get dvr (version2) time
//    st: return dvr time
//    system: 0-get localtime, 1-get system time
// return: 0: fail, 1: success
int net_DVR2GetTime(SOCKET so, SYSTEMTIME * st, int system);

// set dvr (version2) time
//    st: return dvr time
//    system: 0-set localtime, 1-set system time
// return: 0: fail, 1: success
int net_DVR2SetTime(SOCKET so, SYSTEMTIME * st, int system);

// Kill DVR application, if Reboot != 0 also reset DVR
int net_DVRKill(SOCKET so, int Reboot );

// get DVR setup page url (http setup version)
int net_GetSetupPage(SOCKET so, char * pagebuf, int bufsize);

// check key block. return 1 : success
int net_CheckKey(SOCKET so, char * keydata);

// get Key (TVS/PWII) access log data. Return 1: success, return 0: failed
int net_GetKeyLog(SOCKET so, char ** keylog, int * datalen) ;

// send keypad info to PWII
int	net_sendpwkeypad(SOCKET so, int keycode, int press );

// send data to DVR
int net_senddvrdata( SOCKET so, int protocol, void * sendbuf, int sendsize);

// receive (request) data from DVR
int net_recvdvrdata( SOCKET so, int protocol, void ** precvbuf, int * precvsize);

#endif		// __DVRNET_H__
