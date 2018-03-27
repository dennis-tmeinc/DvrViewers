// ply614.h : main header file for the ply614 DLL
//

#pragma once

#ifndef __PLY614_H__
#define __PLY614_H__

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols
#include <Wincrypt.h>
#include "tme_thread.h"
#include "player.h"

#define DLL_API __declspec(dllexport)

#if 1 // from Visual Studio templete
// Cply614App
// See ply614.cpp for the implementation of this class
//

class Cply614App : public CWinApp
{
public:
	Cply614App();

// Overrides
public:
	virtual BOOL InitInstance();

	DECLARE_MESSAGE_MAP()
};
#endif

#ifdef TEST_301
#define FILE_EXT "264"
#else 
#define FILE_EXT "265"
#endif

extern tme_object_t g_ipc;

typedef void * HPLAYER;                       // player handle

// all functions return one of these error codes if it failed.
#define DVR_ERROR_NONE            (0)		  // no error
#define DVR_ERROR                 (-1)        // general error
#define DVR_ERROR_AUTH            (-2)        // username/password required
#define DVR_ERROR_FILEPASSWORD    (-3)        // file password required
#define DVR_ERROR_NOALLOW         (-4)        // operation not allowed
#define DVR_ERROR_BUSY            (-5)        // finding device busy

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
#define	PROTOCOL_KEYDATA       (7)		 // send and check TVS/PWII key data to dvr
#define PROTOCOL_TVSKEYDATA	   (PROTOCOL_KEYDATA)

// dvr open mode
#define PLY_PREVIEW				0
#define PLY_PLAYBACK			1
#define PLY_SMARTSERVER			2

#define DVR_DEVICE_TYPE_NONE	(0)
#define DVR_DEVICE_TYPE_DVR		(1)
#define DVR_DEVICE_TYPE_HARDDRIVE	(2)
#define DVR_DEVICE_TYPE_DVRFILE	(3)
#define DVR_DEVICE_TYPE_SMARTSERVER (4)
#define DVR_DEVICE_TYPE_264FILE	(5)
#define DVR_DEVICE_TYPE_LOCALSERVER	(6)

#define MAX_DVR_DEVICE	(1024)
#define DVRNAME_LEN (256)


// structures

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

// tvs/pwii system will return this info in serverinfo field
struct serverinfo_detail {
	char serverip[48] ;
	char productid[8] ;
	char serial[24] ;
	char id1[24] ;
	char id2[24] ;
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
	int update ;		 //  need to update message 
    void * res1 ;         //  reserved communication data
} ;

/* gps support */
struct gpsinfo {
	int hour, min, second;
	double latitude, longitude, speed, angle;
};

extern "C" {
DLL_API	int player_init() ;
DLL_API int player_deinit();
DLL_API	int finddevice(int flags);
DLL_API int polldevice();
DLL_API	HPLAYER opendevice(int index, int opentype ) ;
DLL_API	int initsmartserver( HPLAYER handle );
DLL_API	HPLAYER openremote(char * netname, int opentype ) ;
DLL_API	int scanharddrive(char * drives, char **servernames, int maxcount);
DLL_API	HPLAYER openharddrive( char * path ) ;
DLL_API	HPLAYER openfile( char * dvrfilename );
DLL_API	int player_close(HPLAYER handle);
DLL_API	int getplayerinfo(HPLAYER handle, struct player_info * pplayerinfo );
DLL_API	int setuserpassword( HPLAYER handle, char * username, void * password, int passwordsize );
DLL_API	int setvideokey( HPLAYER handle, void * key, int keysize );
DLL_API	int getchannelinfo(HPLAYER handle, int channel, struct channel_info * pchannelinfo );
DLL_API	int attach(HPLAYER handle, int channel, HWND hwnd );
DLL_API	int detach(HPLAYER handle);
DLL_API	int detachwindow(HPLAYER handle, HWND hwnd );
DLL_API	int play(HPLAYER handle);
DLL_API	int audioon(HPLAYER handle, int channel, int audioon);
DLL_API int audioonwindow(HPLAYER handle, HWND hwnd, int audioon);
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
DLL_API	int detectdevice(char *ipname);
DLL_API	int pwkeypad(HPLAYER handle, int keycode, int press);
DLL_API	int getlocation(HPLAYER handle, char *locationbuffer, int buffersize);
DLL_API int setblurarea(HPLAYER handle, int channel, struct blur_area * ba, int blur_area_number );
DLL_API int clearblurarea(HPLAYER handle, int channel );
}

int dvrtime_diff(dvrtime * t1, dvrtime * t2);
int dvrtime_diffms(dvrtime * t1, dvrtime * t2);
void dvrtime_add(dvrtime * dvrt, int seconds);
void dvrtime_addmilisecond(dvrtime * dvrt, int miliseconds);
// compare two dvrtime
// return >0: t1>t2
//        0 : t1==t2
//       <0 : t1<t2
int dvrtime_compare(dvrtime * t1, dvrtime * t2);
void dvrtime_init(struct dvrtime * t, int year=2000, int month=1, int day=1, int hour=0, int minute=0, int second=0, int milisecond=0);
// get time_t from dvrtime
time_t dvrtime_mktime( struct dvrtime * dvrt );	
// get dvrtime from time_t
void dvrtime_localtime( struct dvrtime * dvrt, time_t tt ) ;

// convert time stamp unit to milliseconds
inline int tstamp2ms(int tstamp)
{
//	ms = tstamp * 1000 / 64
	return (tstamp*15 + tstamp*5/8);
}

#define LIST_EXPEND_STEP	(100)

// A list class for general use
//   List member can be any data type. If member <T> is class, it should implement operator = and default constructor
template <class T>
class list {
private:
	T    * m_ptr ;				// list data
	int    m_size ;		// how many items in list
	int    m_arraysize ;			// total size of the list

	// expand list to a new size
	void expand(int newsize) {
		int i;
		if( newsize>m_arraysize ) {
			newsize += LIST_EXPEND_STEP ;
			T * newptr = new T [newsize] ;
			for(i=0; i<m_size; i++ ) {
				newptr[i] = m_ptr[i] ;
			}
			if( m_arraysize>0 ) {
				delete [] m_ptr ;
			}
			m_arraysize=newsize ;
			m_ptr = newptr ;
		}
	}

	// quick sort helper
	void quicksort(int lo, int hi)
	{
		T swap ;
		int i, j ;
		i=(lo+hi)/2 ;

		// swap i, hi
		swap      = m_ptr[i] ;
		m_ptr[i]  = m_ptr[hi] ;
		m_ptr[hi] = swap ;

		i=lo; 
		j=hi-1;
		while( i<=j ) {
			while( i<=j && m_ptr[i]<m_ptr[hi] ) {
				i++ ;
			}
			while( j>i && (!(m_ptr[j]<m_ptr[hi])) ) {
				j-- ;
			}
			if(i<j) {
				swap    = m_ptr[i] ;
				m_ptr[i]= m_ptr[j] ;
				m_ptr[j]= swap ;
				i++; 
			}
			j--;
		}
		if( i<hi ) {
			swap = m_ptr[i] ;
			m_ptr[i]=m_ptr[hi] ;
			m_ptr[hi]=swap;
		}

		// sort hi half
		if( hi>i+1 ) {
			quicksort(i+1,hi) ;
		}
		// sort lo hafl
		if( lo<i-1 ) {
			quicksort(lo,i-1);
		}
	}

public:
	list() 
		:m_size(0)
		,m_arraysize(0)
		,m_ptr(NULL)
	{}

	// copy constructor
	list(list <T> & alist) {
		m_size = alist.m_size ;
		m_arraysize = m_size ;
		if( m_arraysize>0 ) {
			m_ptr = new T [m_arraysize] ;
			for(i=0; i<m_size; i++ ) {
				m_ptr[i] = alist.m_ptr[i] ;
			}
		}
		else {
			m_ptr=NULL ;
		}
	}

	~list() {
		if( m_arraysize>0 ) {
			delete [] m_ptr ;
		}
	}

	// compact list size to exactly item count
	void compact() {
		if( m_size<m_arraysize ) {
			int i ;
			T * p = NULL ;
			if( m_size>0 ) {
				p = new T [m_size] ;
				for( i=0; i<m_size; i++ ) {
					p[i] = m_ptr[i] ;
				}
			}
			delete [] m_ptr ;
			m_ptr = p ;
			m_arraysize=m_size ;
		}
	}

	// set list to a new size, item are undefined if list expands
	void setsize(int nsize) {
		expand( nsize );
		m_size=nsize ;
	}

	// append a new item
	T * append() {
		expand( m_size+1 ) ;
		return &(m_ptr[m_size++]) ;
	}

	// add a new item
	void add(T t) {
		expand( m_size+1 ) ;
		m_ptr[m_size++] = t ;
	}

	// get item pointer on given index, list automaticlly expand
	T * at( int index ) {
		if( index>=m_size ) {
			expand( index+1 ) ;
			m_size=index+1 ;
		}
		return &(m_ptr[index]) ;
	}

	T & operator [] (int index) {
		if( index>=m_size ) {
			expand( index+1 ) ;
			m_size=index+1 ;
		}
		return m_ptr[index] ;
	}

	// clean, delete all items
	void clean() {
		if( m_arraysize>0 ) {
			delete [] m_ptr ;
		}
		m_arraysize=0;
		m_size=0;
		m_ptr=NULL ;
	}

	// get total valid item number
	int size() {
		return m_size ;
	}

	// find the index of a pointer, return -1 for error
	int find( T * ptr )
	{
		int i;
		for(i=0;i<m_size;i++) {
			if( ptr==&m_ptr[i] ){
				return i ;
			}
		}
		return -1 ;
	}

	// insert one item, and return item pointer
	T * insert( int index ) {
		int i;
		for(i=m_size; i>index; i-- ) {
			*at(i) = * at(i-1);
		}
		return at(index) ;
	}

	// remove one item
	void remove( int index ) {
		int i;
		if( index>=m_size || m_size<=0 ) {
			return;
		}
		for(i=index; i<m_size-1; i++) {
			*at(i) = *at(i+1) ;
		}
		m_size-- ;
	}

	void sort() {
		if( m_size>1 ) {
			quicksort(0, m_size-1);
		}
	}

	// assignment
	list <T> & operator = ( list <T> & alist ) {
		if( this==&alist ) {		// same list
			return *this ;
		}
		if( m_arraysize>0 ) {
			delete [] m_ptr ;		// clean old array
		}
		m_size = alist.m_size ;
		m_arraysize = m_size ;
		m_ptr = new T [m_arraysize] ;
		for(i=0; i<m_size; i++ ) {
			m_ptr[i] = alist.m_ptr[i] ;
		}
		return *this ;
	}
} ;

// stream encrypt mode
#define ENC_MODE_UNKNOWN	(-1)
#define ENC_MODE_NONE		0
#define ENC_MODE_RC4FRAME	1
#define ENC_MODE_WIN		2

// DVR network functions

HPLAYER net_openpreview(int index ) ;
int net_finddevice();
int net_detectdevice( char * ipaddr );

// open a play back stream
//   return stream handle
//          -1 for error ;
int net_stream_open(SOCKET so, int channel);

// close playback stream
// return 1: success
//        0: error
int net_stream_close(SOCKET so, int handle);

// playback stream seek
// return 1: success
//        0: error
int net_stream_seek(SOCKET so, int handle, struct dvrtime * pseekto );

// get stream time
// return 1: success
// retur  0: failed
int net_stream_gettime(SOCKET so, int handle, struct dvrtime * t);

// int get DVR share root directory and password,
// parameter:
//		root: minimum size MAX_PATH bytes
int net_getshareinfo(SOCKET so, char * root );

// get stream day data availability information in a month
// return
//         bit mask of month info
//         0 for no data in this month
DWORD net_stream_getmonthinfo(SOCKET so, int handle, struct dvrtime * month);

// get data availability information of a day
// return 
//        sizeof tinfo item copied
//        if tinfosize is zero, return number of available items
int net_stream_getdayinfo( SOCKET so, int handle, struct dvrtime * day, struct cliptimeinfo * tinfo, int tinfosize);

// get locked file availability information of a day
// return 
//        sizeof tinfo item copied
//        if tinfosize is zero, return number of available items
int net_stream_getlockfileinfo(SOCKET so, int handle, struct dvrtime * day, struct cliptimeinfo * tinfo, int tinfosize);
int net_sendpwkeypad(SOCKET so, int keycode, int press );

struct dvr_device {
	int type ;						// 0: no device, 1: network dvr, 2: harddrive
	char *name ;				// DVR servername
} ;

extern struct dvr_device g_dvr[MAX_DVR_DEVICE] ;	// array of DVR devices have been found
extern int    g_dvr_number ;			// number of DVR devices

// key data (TVS/PWII) system support
struct key_data {
	char checksum[128] ;        // md5 check of key data (use 16 bytes only). calculated from field "size"
    int size ;                  // total file size exclude the checksum part
    int version ;                // 100
    char tag[4] ;                // "TVS" OR "PWII"
    char usbid[32] ;             // MFnnnn, INnnnn, PLnnnn
    char usbkeyserialid[512] ;   // Example: Ven_OCZ&Prod_RALLY2&Rev_1100\AA04012700017614
    char videokey[512] ;         // TVS video encryption key, only 256 bytes used.
    char manufacturerid[32] ;    // MFnnnn
    int counter ;                // Installer key counter
    int keyinfo_start ;          // offset of key text information
    int keyinfo_size ;           // size of key text information
   // followed by key text information
} ;

// DVR stream interface
class dvrstream {
public:
// constructors
	dvrstream(){}
	virtual ~dvrstream(){};

// interfaces

	// seek to specified time.
	virtual int seek( struct dvrtime * dvrt )		
	{
		return 0;
	}
	// seek to previous key frame position.
	virtual int prev_keyframe()						
	{ 
		return 0;
	}
	// seek to next key frame position.
	virtual int next_keyframe()		
	{
		return 0 ;
	}
	// pre-load video data, return 0: fifo full, no more pre-load, 1: pre-load need continue
	virtual int preloaddata() {		
		return 0;
	}
	// get dvr data frame
	virtual int getdata(char **framedata, int * framesize, int * frametype, int *headerlen)=0;	
	// get current stream time, (precise to miliseconds), return 1 if timer is in sync, 0 if timer is not sync
	virtual	int gettime( struct dvrtime * dvrt )=0;	
	// get server information
	virtual int getserverinfo(struct player_info *ppi)=0 ;
	// get stream channel information
	virtual int getchannelinfo(struct channel_info * pci)=0;
	// get stream day data availability information
	virtual int getdayinfo(dvrtime * daytime)				
	{ 
		return 0; 
	}
	// get data availability information
	virtual int gettimeinfo( struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize) 
	{
		return 0;
	}
	// get locked file availability information
	virtual int getlockfileinfo(struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize )					
	{
		return 0;
	}
	// provide and internal structure to sync other stream
	virtual void * getsynctimer(){								
		return NULL ;
	}
	// use synctimer to setup internal sync timer
	virtual void setsynctimer(void * synctimer) {} ;			
	// set stream passwords,
	//	passwdtype: password type, 0: no password, ENC_MODE_RC4FRAME: frame RC4, ENC_MODE_WIN: Windows RC4
	virtual void setpassword(int passwdtype, void * password, int passwdsize ) {} ;
	// Shut down stream, followng getdata() would fail or abort.
	virtual int shutdown()		
	{
		return 0;
	}
	// TVS/PWII USB key system support, set key data block
	virtual void setkeydata( struct key_data * keydata ) {} ;
	/* gps support */
	virtual int getlocation(struct dvrtime *, struct gpsinfo *) { return -1;} ;
} ;


#define PLAY_SYNC_START		(0)		// start synchronize. After call seek()
#define PLAY_SYNC_READY		(1)		// ready to running. (next frame data ready)
#define PLAY_SYNC_RUN		(2)		// running
#define PLAY_SYNC_JUMP		(3)		// big jump, need to show logo screen
#define PLAY_SYNC_NODATA	(4)		// no more stream data available

struct channel_t {
	struct		channel_info channelinfo ;
	dvrstream * stream ;
	decoder   * target ;								// target is a linked list of decoder
	int		    player_thread_state ;
	HANDLE		player_thread_handle ;
	int			syncstate ;								// player synchronization state
	HANDLE		hEvent;
	bool		bSeekDuringPause, bSeekReq, bBufferPending;
} ;

// a DVR player 
class dvrplayer {
protected:
//	char m_dvrname[MAX_PATH] ;							// servername or directory root or DVR file name.
	int  m_dvrtype ;									// DVR type, network server, smart server, file tree
	int  m_opentype ;									// preview, playback, smart server

	int  m_totalchannel ;


// player support
	int  m_play_stat ;									// current play state
	struct dvrtime m_playtime ;  						// current play time
	struct dvrtime m_synctime ;							// stream time when sync()
	DWORD	m_syncclock ;								// system clock when sync()


	// Windows type File Encryption Support
	HCRYPTPROV m_hCryptProv ;							// Enc provider
	HCRYPTKEY  m_hVideoKey	;							// Key for video file decryption

	// tvs/pwii key data block
	struct key_data * m_keydata ;
	int m_keydatasize ;
	bool m_bSeekDuringPause;
	dvrtime m_delayedSeek;

protected:
	SOCKET m_socket ;									// network socket
	char m_servername[MAX_PATH] ;						// DVR server name( for network connecting, or root path of DVR files

	CRITICAL_SECTION m_criticalsection ;					// Critical section used for threads synchronization
	void lock()   { /* locks m_channelinfo, m_playtime */
		EnterCriticalSection(&m_criticalsection); 
	}
	void unlock() {
		LeaveCriticalSection(&m_criticalsection); 
	}

protected:
	int initchannel();									// initialize channels, (initial m_channellist)
	int channelCanPlay(int ch, bool& hide_screen);
	int oneframeforward_nolock();
	int oneframeforward_nolock_single(int channel);

public:
	int  m_errorstate ;									// 0: no error, 1: password required, 2: paused while saving .DVR file
	// Frame RC4 password
	char    m_password[256] ;							// frame RC4 password
	list   <struct channel_t> m_channellist ;			// channel list, (member : struct channel_t)
	struct player_info m_playerinfo ;
#if 0
	void savedvrfilethread( struct dvrtime * begintime, 
							struct dvrtime * endtime, 
							char * duppath, 
							int flags, 
							struct dup_state * dupstate, 
							char * password,
							HANDLE hthreadstart );
#endif
public:
// constructor
	dvrplayer() ;
	~dvrplayer() ;

	void playerthread(int channel);						// player thread
	dvrstream * newstream(int channel);					// create a new data stream base on open type

// player open
	int openremote(char * netname, int opentype ) ;		// open DVR server over network, return 1:success, 0:fail.
	int openharddrive( char * path ) ;					// open hard driver (file tree), return 1:success, 0:fail.
	int openlocalserver( char * servername );			// open multiple disks with same server name, return 1:success, 0:fail
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
	int audioonwindow( HWND hwnd, int audioon);
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
	int pwkeypad(int keycode, int press);
	int getlocation(char *locationbuffer, int buffersize);
    int setblurarea(int channel, struct blur_area * ba, int banumber );
    int clearblurarea(int channel );
    void BlurDecFrame( int channel, char * pBuf, int nSize, int nWidth, int nHeight, int nType);
};

#endif	// __PLY614_H__