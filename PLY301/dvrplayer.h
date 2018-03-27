// ply301.h : main header file for the ply301 DLL
//

#pragma once

#ifndef __PLY301_H__
#define __PLY301_H__

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols

#define DLL_API __declspec(dllexport)

typedef void * HPLAYER;                       // player handle

// all functions return one of these error codes if it failed.
#define DVR_ERROR                 (-1)        // general error
#define DVR_ERROR_AUTH            (-2)        // username/password required
#define DVR_ERROR_FILEPASSWORD    (-3)        // file password required
#define DVR_ERROR_NOALLOW         (-4)        // operation not allowed

// finddevice flags
#define DVR_DEVICE_DVR		     (1)	 // Bit 0: DVR server (include ip camera, usb camera),
#define DVR_DEVICE_HARDDRIVE     (1<<1)  // Bit 1: local hard drive,
#define DVR_DEVICE_SMARTSERVER   (1<<2)  // Bit 2: smart server(Data server) ;

// duplcation flags
#define DUP_ALLFILE        (1)            // duplicate all files
#define DUP_LOCKFILE       (1<<1)         // duplicate locked video files
#define DUP_DVRDATA        (1<<2)         // duplicate other dvr data
#define DUP_BACKGROUND     (1<<3)         // duplicate run in background

// player features
#define PLYFEATURE_REALTIME    (1)        // support real time play (pre-view)
#define PLYFEATURE_PLAYBACK    (1<<1)     // support playback
#define PLYFEATURE_REMOTE      (1<<2)     // support remote playback
#define PLYFEATURE_HARDDRIVE   (1<<3)     // support localhard drive playback
#define PLYFEATURE_CONFIGURE   (1<<4)     // support DVR configuring
#define PLYFEATURE_CAPTURE     (1<<5)     // support picture capture
#define PLYFEATURE_GPSINFO     (1<<6)     // gps infomation available
#define PLYFEATURE_LOGINFO     (1<<7)     // log infomation available
#define PLYFEATURE_PTZ         (1<<8)     // PTZ support
#define PLYFEATURE_SMARTSERVER (1<<9)     // support smart server feature

// protocol type for send/receive DVR data
#define PROTOCOL_LOGINFO       (1)        // receiving log inforamtion
#define PROTOCOL_TIME          (2)        // send time info to DVR
#define PROTOCOL_TIME_BEGIN    (3)        // send a begining time
#define PROTOCOL_TIME_END      (4)        // send a ending time
#define PROTOCOL_PTZ           (5)        // send PTZ command
#define PROTOCOL_POWERCONTROL  (6)        // set DVR power status

// dvr open mode
#define PLY_PREVIEW				0
#define PLY_PLAYBACK			1
#define PLY_SMARTSERVER			2
#define PLY_PLAYFILE			3

// structures

// structure of player information
struct player_info {
    int    size ;                // size of this structure, must be set before call "getplayerinfo"
    int    type ;                // player type,
    int    playerversion[4] ;    // player DLL version, see note bellow.
    int    serverversion[4] ;    // DVR server version, if player is connecting to a network DVR server
    int    total_channel ;       // total channel of videos
    int    authenctype ;         // user password encryption type, 0 for no password required, 1 for plain text, other value for encrypted password
    int    videokeytype ;        // video file key encryption type, 0 for no password required, 1 for plain text password, other value are to be defined.
    int    timezone ;            // DVR timezone information, minutes from UTC.
    int    features ;            // bitmap of player and DVR server features.
    int    reserved[17] ;        // other fields may add in here later.
    char   servername[128] ;
    char   serverinfo[128] ;     // server information, (to show on user interface)
} ;

// note on version number
// verions are 4 numbers, 
//      version[0] - major number, 
//      version[1] - minor number, 
//      version[2] - year,
//      version[3] - month/date

// structure of channel information
struct channel_info {
    int    size ;
    int    features ;            // bit0: enabled, bit1: ptz support
    int    status ;              // bit0: signal lost, bit1: motion detected, bit2: recording, bit3: abnormal picture
    int    xres ;                // X resolutions
    int    yres ;                // Y resolutions
    char   cameraname[128] ;     // camera name
} ;

// structure of dvr playing time
struct dvrtime {
    int year;
    int month;
    int day;
    int hour;
    int min;
    int second;
    int millisecond;
    int tz;             // timezone information, minutes difference from UTC. (not used now, may use for time coordinating).
} ;

// structure of clip timing info
struct cliptimeinfo {
    int on_time ;       // seconds elapsed from begining time specified
    int off_time ;      // seconds elapsed from begining time specified
} ;

// structure used for background duplication
struct dup_state {
    int status;        //    <0: error, 0: not running, 1: success
    int cancel;        //    0: duplcation running, set to 1(non zero) to cancel duplicate  
    void (* dup_complete)();  // call back function when duplcate finished. set to NULL if you don't use it.
    int percent ;        //  percentage of duplicate complete
    int totoalkbytes ;   //  total kbytes to be copied
    int copykbytes ;     //  copied kbytes
    char msg[128] ;      //  displayable message if a user interface is used
    void * res1 ;         //  reserved communication data
} ;

extern "C" {
DLL_API	int player_init() ;
DLL_API int player_deinit();
DLL_API	int finddevice(int flags);
DLL_API	HPLAYER opendevice(int index, int opentype ) ;
DLL_API	int initsmartserver( HPLAYER handle );
DLL_API	HPLAYER openremote(char * netname, int opentype ) ;
DLL_API	int scanharddrive(char * drives, char **servernames, int maxcount);
DLL_API	HPLAYER openharddrive( char * path ) ;
DLL_API	HPLAYER openfile( char * dvrfilename );
DLL_API	int close(HPLAYER handle);
DLL_API	int getplayerinfo(HPLAYER handle, struct player_info * pplayerinfo );
DLL_API	int setuserpassword( HPLAYER handle, char * username, void * password, int passwordsize );
DLL_API	int setvideokey( HPLAYER handle, void * key, int keysize );
DLL_API	int getchannelinfo(HPLAYER handle, int channel, struct channel_info * pchannelinfo );
DLL_API	int attach(HPLAYER handle, int channel, HWND hwnd );
DLL_API	int detach(HPLAYER handle);
DLL_API	int detachwindow(HPLAYER handle, HWND hwnd );
DLL_API	int play(HPLAYER handle);
DLL_API	int audioon(HPLAYER handle, int channel, int audioon);
DLL_API	int pause(HPLAYER handle);
DLL_API	int stop(HPLAYER handle);
DLL_API	int fastforward( HPLAYER handle, int speed );
DLL_API	int slowforward( HPLAYER handle, int speed );
DLL_API	int oneframeforward( HPLAYER handle );
DLL_API	int backward( HPLAYER handle, int speed );
DLL_API	int capture( HPLAYER handle, int channel, char * capturefilename );
DLL_API	int seek( HPLAYER handle, struct dvrtime * where );
DLL_API	int getcurrenttime( HPLAYER handle, struct dvrtime * where );
DLL_API	int getcliptimeinfo( HPLAYER handle, int channel, struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize);
DLL_API	int getclipdayinfo( HPLAYER handle, int channel, dvrtime * daytime ) ;
DLL_API	int getlockfiletimeinfo( HPLAYER handle, int channel, struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize);
DLL_API	int savedvrfile( HPLAYER handle, struct dvrtime * begintime, struct dvrtime * endtime, char * duppath, int flags, struct dup_state * dupstate);
DLL_API	int dupvideo(HPLAYER handle, struct dvrtime * begintime, struct dvrtime * endtime, char * duppath, int flags, struct dup_state * dupstate);
DLL_API	int configureDVR(HPLAYER handle);
DLL_API	int senddvrdata(HPLAYER handle, int protocol, void * sendbuf, int sendsize);
DLL_API	int recvdvrdata(HPLAYER handle, int protocol, void ** precvbuf, int * precvsize);
DLL_API	int freedvrdata(HPLAYER handle, void * dvrbuf);
}

HPLAYER net_openpreview(int index ) ;
int net_finddevice();

#define MAX_DVR_DEVICE	(50)

#define DVR_DEVICE_TYPE_NONE	(0)
#define DVR_DEVICE_TYPE_DVR		(1)
#define DVR_DEVICE_TYPE_HARDDRIVE	(2)

struct dvr_device {
	int type ;						// 0: no device, 1: network dvr, 2: harddrive
	char name[256] ;				// for network dvr, this is computer name
									// for harddrive, this is root directory
} ;

extern struct dvr_device g_dvr[MAX_DVR_DEVICE] ;	// array of DVR devices have been found
extern int    g_dvr_number ;			// number of DVR devices

class dvrstream ;

// a DVR player 
class dvrplayer {
protected:
	int m_servername[256] ;								// DVR server name.
	int m_dvrname[256] ;								// servername or directory root or DVR file name.
	int m_dvrtype ;										// DVR type, network server, smart server, file tree
	int m_opentype ;									// preview, playback, smart server

	int m_totalchannel ;

protected:
	dvrstream * newstream(int channel);					// create a new data stream base on open type

public:
// constructor
	dvrplayer() ;
	~dvrplayer() ;

// player open
	int openremote(char * netname, int opentype ) ;		// open DVR server over network, return 1:success, 0:fail.
	int openharddrive( char * path ) ;					// open hard driver (file tree), return 1:success, 0:fail.
	int openfile( char * dvrfilename );					// open .DVR file, return 1:success, 0:fail.

	int getplayerinfo( struct player_info * pplayerinfo );
	int setuserpassword( char * username, void * password, int passwordsize );
	int setvideokey( void * key, int keysize );
	int getchannelinfo( int channel, struct channel_info * pchannelinfo );
	int attach( int channel, HWND hwnd );
	int detach( );
	int detachwindow( HWND hwnd );
	int play();
	int audioon( int channel, int audioon);
	int pause();
	int stop();
	int fastforward( int speed );
	int slowforward( int speed );
	int oneframeforward();
	int backward( int speed );
	int capture( int channel, char * capturefilename );
	int seek( struct dvrtime * where );
	int getcurrenttime( struct dvrtime * where );
	int getcliptimeinfo( int channel, struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize);
	int getclipdayinfo( int channel, dvrtime * daytime ) ;
	int getlockfiletimeinfo( int channel, struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize);
	int savedvrfile( struct dvrtime * begintime, struct dvrtime * endtime, char * duppath, int flags, struct dup_state * dupstate);
	int dupvideo( struct dvrtime * begintime, struct dvrtime * endtime, char * duppath, int flags, struct dup_state * dupstate);
	int configureDVR( );
	int senddvrdata( int protocol, void * sendbuf, int sendsize);
	int recvdvrdata( int protocol, void ** precvbuf, int * precvsize);
	int freedvrdata(void * dvrbuf);

};

#endif	// __PLY301_H__