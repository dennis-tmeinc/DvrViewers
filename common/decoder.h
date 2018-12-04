

#ifndef	__DVR_DECODER__
#define __DVR_DECODER__

#include "../common/cstr.h"

// defines

typedef void * HPLAYER;                       // player handle

// all functions return one of these error codes if it failed.
#define DVR_ERROR                 (-1)        // general error
#define DVR_ERROR_AUTH            (-2)        // username/password required
#define DVR_ERROR_FILEPASSWORD    (-3)        // file password required
#define DVR_ERROR_NOALLOW         (-4)        // operation not allowed
#define DVR_ERROR_BUSY            (-5)        // operation not finish

#define DVR_ERROR_NOAPI		(-101)				// no api available for this deocoder

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
#define PLYFEATURE_DVRCONFIGURE  PLYFEATURE_CONFIGURE
#define PLYFEATURE_CAPTURE     (1<<5)     // support picture capture
#define PLYFEATURE_GPSINFO     (1<<6)     // gps infomation available
#define PLYFEATURE_LOGINFO     (1<<7)     // log infomation available
#define PLYFEATURE_PTZ         (1<<8)     // PTZ support
#define PLYFEATURE_SMARTSERVER (1<<9)     // support smart server feature
#define PLYFEATURE_PLAYERCONFIGURE (1<<10)     // support player configuration
#define PLYFEATURE_CLEAN       (1<<11)     // support clean all video files (or format disk)
#define PLYFEATURE_CLEANPARTIAL (1<<12)    // support clean partial video data
#define PLYFEATURE_MINITRACK    (1<<13)    // support minitrack feature
#define PLYFEATURE_DVRFUNC      (1<<14)    // require dvr_func interface

// protocol type for send/receive DVR data
#define PROTOCOL_LOGINFO       (1)        // receiving log inforamtion
#define PROTOCOL_TIME          (2)        // send time info to DVR
#define PROTOCOL_TIME_BEGIN    (3)        // send a begining time
#define PROTOCOL_TIME_END      (4)        // send a ending time
#define PROTOCOL_PTZ           (5)        // send PTZ command
#define PROTOCOL_POWERCONTROL  (6)        // set DVR power status
#define PROTOCOL_TVSKEYDATA	   (7)       // set TVS key data to dvr (TVS)
// #define PROTOCOL_TVSLOG		   (8)       // receive TVS log data
#define PROTOCOL_PWPLAYEREVENT	   (9)       // sent PW player event (make clip and make snapshot) to mini-track dll

// PWII get channel status
#define PROTOCOL_PW_GETSTATUS   	(1001)
#define PROTOCOL_PW_GETPOLICEIDLIST	(1002)
#define PROTOCOL_PW_SETPOLICEID		(1003)
#define PROTOCOL_PW_GETVRILISTSIZE	(1004)
#define PROTOCOL_PW_GETVRILIST		(1005)
#define PROTOCOL_PW_SETVRILIST		(1006)
#define PROTOCOL_PW_GETSYSTEMSTATUS	(1007)
#define PROTOCOL_PW_GETWARNINGMSG	(1008)
#define PROTOCOL_PW_SETCOVERTMODE	(1009)

// dvr open mode
#define PLY_PREVIEW				0
#define PLY_PLAYBACK			1
#define PLY_SMARTSERVER			2
#define PLY_PLAYFILE			3

// player type
#define PLAYER_TYPE_ERROR            (0)
#define PLAYER_TYPE_H264             (1)
#define PLAYER_TYPE_WISCHIP          (3)
#define PLAYER_TYPE_SMARTSERVER      (4)
#define PLAYER_TYPE_MINITRACK        (5)
#define PLAYER_TYPE_606              (6)
#define PLAYER_TYPE_614              (7)


// structures

// PTZ command buffer
struct Ptz_Cmd_Buffer {
	int channel ;
	DWORD cmd ;
	DWORD param ;
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

// blur area definition
struct blur_area {
    int x;
    int y;
    int w;
    int h;
    int radius;
    int shape;      // 0: rectangle, 1: ellipse
};

struct zoom_area {
	float x;		// start x 
	float y;		// start y
	float z;		// zoom in ratio	(0-1.0)
};

// structure used for background duplication
struct dup_state {
    int status;        //    <0: error, 0: not running, 1: success
    int cancel;        //    0: duplcation running, set to 1(non zero) to cancel duplicate  
    void (* dup_complete)();  // call back function when duplcate finished. may be null if not used.
    int percent ;        //  percentage of duplicate complete
    int totoalkbytes ;   //  total kbytes to be copied
    int copykbytes ;     //  copied kbytes
    char msg[128] ;      //  displayable message if a user interface is used
	int update ;		 //  need to update message 
    void * res1 ;         //  reserved communication data
} ;

// DVRViewer function interface
struct dvr_func {
    int (* openserver)( char * servername ) ;                      //  Open dvr media base on servername ;
    int (* openremote)( char * remoteserver );                     //  Open DVR server use IP address or network name;
    int (* openharddrive)( char * dirname );                       //  Open DVR media on local hard drive;
    int (* openfile)( char * dvrfilename );                        //  Open single .DVR or .264 file;
    int (* close)();                                               //  Close current player;
    int (* getplayerinfo)( struct player_info * pplayerinfo );     //  Get current player information;
    int (* getchannelinfo)( int channel, struct channel_info * pchannelinfo );     // Get channel information ;
    int (* play)();                                                //  Start play video;
    int (* pause)();                                               //  Pause;
    int (* stop)();                                                //  Stop;
    int (* fastforward)( int speed );                              //  Play fast forward;
    int (* slowforward)( int speed );                              //  Play slow forward;
    int (* oneframeforward)();                                     //  Play one frame forward;
    int (* backward)( int speed );                                 //  Play backward;
    int (* seek)( struct dvrtime * where );                        //  Seek to specified time;
    int (* getcurrenttime)( struct dvrtime * where );              //  Get current playing time;
    int (* getplayerstatus)();                                     //  Get playing status;
                                                                   //  ( 0: stop, 1: pause, 90-99: slow, 100 play 101-110: fast play)
	int (* getusername)( char * buffer, int buffersize) ;          //  Get current login user name;
    int (* senddvrdata)( int protocol, void * sendbuf, int sendsize);           // send dvr server data;  
    int (* recvdvrdata)( int protocol, void ** precvbuf, int * precvsize);      // receive dvr server data;
    int (* freedvrdata)( void * dvrbuf);                                        // free dvr server data;
    int (* createstream)( struct stream_interface * pinterface, void * pstream ) ;      //  Open DVR data stream for playback
    int (* getlocation)( char * locationbuffer, int buffersize) ; // Retrieve GPS location on current playing time
    int (* openlive)( char * remoteserver );                       //  Live view DVR server use IP address or network name;
} ;


#ifndef DLL_API
#define DLL_API __declspec(dllimport)
#endif

class decoder_library {
public:
	string  modulename ;
	HMODULE hmod ;
	int		busy ;
	int		filedate ;				// file date on BCD. (ex: 20080630)
	int     libtype ;
//	char * servernames[16] ;		// used by scanharddrive.

	decoder_library(LPCTSTR libname);
	~decoder_library();

	inline void * getapi(char * apiname) {
		if (hmod)
			return GetProcAddress(hmod, apiname);
		return NULL;
	}

	int (*player_init)() ;
	int (*player_deinit)();
	int (*finddevice)(int flags);
	int (*detectdevice)(char *ipname);
	int (*polldevice)();
	HPLAYER (*opendevice)(int index, int opentype ) ;
	HPLAYER (*openremote)(char * netname, int opentype ) ;
//	int (*scanharddrive)(char * drives, char **servernames, int maxcount);
	HPLAYER (*openharddrive)( char * path ) ;
	HPLAYER (*openfile)( char * dvrfilename );
	HPLAYER (*opendvr)(char * dvrname);

	int(*minitrack_start)(struct dvr_func * dvrfunc, HWND mainwindow);
	int(*minitrack_selectserver)(void);
	int(*minitrack_stop)(void);
	int(*minitrack_init_interface)(struct dvr_func * dvrfunc);	// r1.3, required by Tongrui

	int (*initsmartserver)(HPLAYER handle);
	int (*close)(HPLAYER handle);
	int (*getplayerinfo)(HPLAYER handle, struct player_info * pplayerinfo );
	int (*setuserpassword)( HPLAYER handle, char * username, void * password, int passwordsize );
	int (*setvideokey)( HPLAYER handle, void * key, int keysize );
	int (*getchannelinfo)(HPLAYER handle, int channel, struct channel_info * pchannelinfo );
	int (*attach)(HPLAYER handle, int channel, HWND hwnd );
	int (*detach)(HPLAYER handle);
	int (*detachwindow)(HPLAYER handle, HWND hwnd );
	int (*play)(HPLAYER handle);
	int (*audioon)(HPLAYER handle, int channel, int audioon);
	int (*audioonwindow)(HPLAYER handle, HWND hwnd, int audioon);
	int (*pause)(HPLAYER handle);
	int (*stop)(HPLAYER handle);
    int (*refresh)(HPLAYER handle, int channel) ;
	int (*fastforward)( HPLAYER handle, int speed );
	int (*slowforward)( HPLAYER handle, int speed );
	int (*oneframeforward)( HPLAYER handle );
	int (*getdecframes)( HPLAYER handle );
	int (*backward)( HPLAYER handle, int speed );
	int (*capture)( HPLAYER handle, int channel, char * capturefilename );
	int (*capturewindow)( HPLAYER handle, HWND hwnd, char * capturefilename );
	int (*seek)( HPLAYER handle, struct dvrtime * where );
	int (*getcurrenttime)( HPLAYER handle, struct dvrtime * where );
    int (*getdaylist)(HPLAYER handle, int * daylist, int sizeoflist );			// retrieve video available day list, return the number of days, -1 for error
	int (*getcliptimeinfo)( HPLAYER handle, int channel, struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize);
	int (*getclipdayinfo)( HPLAYER handle, int channel, dvrtime * daytime ) ;
	int (*getlockfiletimeinfo)( HPLAYER handle, int channel, struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize);
    int (*geteventtimelist)( HPLAYER handle, struct dvrtime * date, int eventnum, int * elist, int elistsize);
	int (*savedvrfile)(HPLAYER handle, struct dvrtime * begintime, struct dvrtime * endtime, char * duppath, int flags, struct dup_state * dupstate);
	int (*dupvideo)(HPLAYER handle, struct dvrtime * begintime, struct dvrtime * endtime, char * duppath, int flags, struct dup_state * dupstate);
	int (*configureDVR)(HPLAYER handle);
	int (*senddvrdata)(HPLAYER handle, int protocol, void * sendbuf, int sendsize);
	int (*recvdvrdata)(HPLAYER handle, int protocol, void ** precvbuf, int * precvsize);
	int (*freedvrdata)(HPLAYER handle, void * dvrbuf);
	int (*configurePlayer)(HPLAYER handler);
	int (*lockdata)(HPLAYER handle, struct dvrtime * begintime, struct dvrtime * endtime );
	int (*unlockdata)(HPLAYER handle, struct dvrtime * begintime, struct dvrtime * endtime );
    int (*pwkeypad)(HPLAYER handle, int keycode, int pressdown);
	int (*cleandata)(HPLAYER handle, int mode, struct dvrtime * begintime, struct dvrtime * endtime );
	int (*getlocation)(HPLAYER handle, char * locationbuffer, int buffersize);
	int (*setblurarea)(HPLAYER handle, int channel, struct blur_area * ba, int blur_area_number );
	int (*clearblurarea)(HPLAYER handle, int channel);
	int (*showzoomin)(HPLAYER handle, int channel, struct zoom_area * za);
	int (*drawrectangle)(HPLAYER handle, int channel, float x, float y, float w, float h);
	int(*selectfocus)(HPLAYER handle, HWND hwnd);
	
};

#define MAXLIBRARY	(10)
#define MAXDECODER  (16)

#define DEV_DIR     (-555)

struct device_list_item 
{
    int lib ;               // >=0 : index of library, -555 
    int number ;
    string dir ;
    device_list_item * next ;
};

// dynamic device list
class device_list {
protected:
    device_list_item * m_head ;
public:
    device_list()
    {
        m_head = NULL ;
    }
    ~device_list()
    {
        clean();
    }

    void clean(){
        device_list_item * it ;
        while( m_head ) {
            it = m_head->next ;
            delete m_head ;
            m_head = it ;
        }
    }

    int totaldev()
    {
        int devs=0 ;
        device_list_item * it=m_head ;
        while( it ) {
            devs+=it->number ;
            it=it->next ;
        }
        return devs ;
    }

	void update(int lib, int devicenum)
	{
        if( m_head == NULL ) {
             m_head = new device_list_item ;
             m_head->lib = lib ;
             m_head->number = devicenum ;
             m_head->next = NULL ;
        }
        else {
            device_list_item * it=m_head ;
            while( it && devicenum>0 ) {
                if( it->lib == lib ) {
                    devicenum -= it->number ;
                }               
                if( it->next == NULL && devicenum>0 ) {        // last item 
                    if( it->lib == lib ) {
                        it->number += devicenum ;
                    }
                    else {
                        it->next = new device_list_item ;
                        it->next->lib = lib ;
                        it->next->number = devicenum ;
                        it->next->next = NULL ;
                   }
                   break;
                }
                it=it->next ;
             }
        }
    }

    int isdir(int index)
    {
        // locate idx 
        int devs=0 ;
        device_list_item * it=m_head ;
        while( it ) {
            devs+=it->number ;
            if( devs>index ) {      // found it
                if( it->lib ==  DEV_DIR  ) {
                    return TRUE ;
                }
                break ;
            }
            it=it->next ;
        }
        return FALSE ;
    }
    
    char * getdir(int index)
    {
        int devs=0 ;
        device_list_item * it=m_head ;
        while( it ) {
            devs+=it->number ;
            if( devs>index ) {      // found it
                if( it->lib ==  DEV_DIR  ) {
                    return (char *)(it->dir) ;
                }
                break ;
            }
            it=it->next ;
        }
       return NULL ;
    }

    void adddir(char * dir) {
        if( m_head == NULL ) {
            m_head = new device_list_item ;
            m_head->lib = DEV_DIR ;
            m_head->number = 1 ;
            m_head->dir = dir ;
            m_head->next = NULL ;
        }
        else {
            device_list_item * it=m_head ;
            while( it ) {
                if( it->next == NULL ) {        // last list 
                    it->next = new device_list_item ;
                    it->next->lib = DEV_DIR ;
                    it->next->number = 1 ;
                    it->next->dir = dir ;
                    it->next->next = NULL ;
                    break;
                }
                it=it->next ;
            }
        }
    }

    // return lib number and index of that lib, return -1 for error
	int findlib( int &index )
	{
        int lib=-1 ;
        int devs=0 ;
        device_list_item * it=m_head ;
        while( it ) {
            devs+=it->number ;
            if( devs>index ) {      // found it
                lib = it->lib ;
                int lidx = index - (devs - it->number ) ;
                device_list_item * lit = m_head ;
                while( lit && lit!=it ) {
                    if( lit->lib == lib ) {
                        lidx += lit->number ;
                    }
                    lit=lit->next ;
                }
                index = lidx ;
                return lib ;
            }
            it=it->next ;
        }
        return -1 ;
    }
} ;

#define DECODER_PASSWORD_NUMBER (5)

class decoder {
private:
	static decoder_library * library[MAXLIBRARY] ;
	static int num_library ;
    static device_list * devlist ;

public:
	HPLAYER m_hplayer ;
	int		m_library ;
	int		m_attachnum ;
	struct  player_info m_playerinfo ;
	string  m_displayname ;
    string  m_password[DECODER_PASSWORD_NUMBER] ;
    static  string  remotehost ;

	decoder() {
		m_library=0 ;
		m_hplayer=NULL ;
		m_attachnum=0 ;
	}

    ~decoder() {
		close();
	}

	int isopen() {
		return (m_hplayer!=NULL) ;
	}

	int libtype() {
		if( m_hplayer )
			return library[m_library]->libtype ;
		else
			return 0 ;
	}

	void * getapi(char * apiname) {
		if (m_hplayer != NULL && library[m_library]) {
			return library[m_library]->getapi(apiname);
		}
		else
			return NULL;
	}

	int support_api(char * apiname) {
		return getapi(apiname) != NULL;
	}

	int getchannel()
	{
		if( m_hplayer )
			return m_playerinfo.total_channel ;
		else
			return 0 ;
	}

	int getattachnum()
	{
		if( m_hplayer )
			return m_attachnum ;
		else
			return 0 ;
	}

	int getfeatures()
	{
		if( m_hplayer )
			return m_playerinfo.features ;
		else
			return 0 ;
	}

	char * getservername()
	{
		if( m_hplayer )
			return m_playerinfo.servername ;
		else
			return "" ;
	}

	char * getserverinfo()
	{
		if( m_hplayer ) {
			if( m_playerinfo.serverinfo[0] ) {
				return m_playerinfo.serverinfo ;
			}
			else {
				return m_displayname ;
			}
		}
		else
			return "" ;
	}

	int open( int index, int opentype );		// call finddevice first
	int openremote(char * netname, int opentype ) ;
	int openharddrive( char * path ) ;
	int openfile( char * dvrfilename ) ;
	int opendvr( char * dvrname);				// open dvr by name (for playback)

	int close();
	int initsmartserver();
	int getplayerinfo(struct player_info * pplayerinfo );
	int tvskeycheck();
	int setuserpassword( char * username, void * password, int passwordsize );
	int setvideokey( void * key, int keysize );
	int getchannelinfo( int channel, struct channel_info * pchannelinfo );
	int attach(int channel, HWND hwnd );
	int detach();
	int detachwindow(HWND hwnd );
	int selectfocus(HWND hwnd);
	int play();
	int audioon(int channel, int audioon);
	int audioonwindow(HWND hwnd, int audioon);
	int pause();
	int stop();
    int refresh(int channel) ;
	int fastforward(int speed );
	int slowforward(int speed );
	int oneframeforward();
	int getdecframes();
	int backward(int speed );
	int capture(int channel, char * capturefilename );
	int capturewindow(HWND hwnd, char * capturefilename );
	int seek(struct dvrtime * where );
	int getcurrenttime(struct dvrtime * where );
    int getdaylist(int * daylist, int sizeoflist );		
	int getcliptimeinfo(int channel, struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize);
	int getclipdayinfo(int channel, dvrtime * daytime ) ;
	int getlockfiletimeinfo(int channel, struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize);
	int savedvrfile(struct dvrtime * begintime, struct dvrtime * endtime, char * duppath, int flags, struct dup_state * dupstate);
	int savedvrfile2(struct dvrtime * begintime, struct dvrtime * endtime, char * duppath, int flags, struct dup_state * dupstate, char * channels, char * password);
	int dupvideo(struct dvrtime * begintime, struct dvrtime * endtime, char * duppath, int flags, struct dup_state * dupstate);
	int configureDVR();
	int senddvrdata(int protocol, void * sendbuf, int sendsize);
	int recvdvrdata(int protocol, void ** precvbuf, int * precvsize);
	int freedvrdata(void * dvrbuf);
	int configurePlayer();
	int lockdata( struct dvrtime * begintime, struct dvrtime * endtime );
	int unlockdata( struct dvrtime * begintime, struct dvrtime * endtime );
	int cleandata( int mode, struct dvrtime * begintime, struct dvrtime * endtime );
    int pwkeypad( int keycode, int press );
	int checkpassword();
   	int getlocation( char * locationbuffer, int buffersize);
    int geteventtimelist( struct dvrtime * date, int eventnum, int * elist, int elistsize);
    int setblurarea( int channel, struct blur_area * ba, int blur_area_number );
	int clearblurarea(int channel);
	// pwz5 get officer id list
	int getofficeridlist( string * idlist[], int maxlist );
	int setofficerid( char * oid );

	int getvrilistsize( int * rowsize );
	int getvrilist( char * vri, int vsize );
	int setvrilist( char * vrilist, int vrisize );

    int blur_supported() {
        return ( library[m_library]->setblurarea != NULL )  ;
    }

	// draw functions , to support zoom-in / ROC / AOI
	int drawrectangle(int channel, float x, float y, float w, float h);
	int drawline(int channel, float x1, float y1, float x2, float y2);
	int clearlines(int channel);
	int setdrawcolor(int channel, COLORREF color);

	// zoom in support interfaces
	int showzoomin(int channel, struct zoom_area * za);
	int supportzoomin();
	
	// rotate support interfaces
	int setrotation(int channel, int degree);
	int supportrotate();

    // get rec state (PWII)
    int getrecstate();
	int getsystemstate(string & st);

	// set covert mode (turn off some LEDS)
    int covert( int onoff );

//  global function
	static int initlibrary();
	static void releaselibrary();
	static int finddevice(int flags);
    static void stopfinddevice();
	static int detectdevice(char * ipaddr);
	static int polldevice();
	static int settvskey(struct tvskey_data * tvskey);

//  global player operation
	static int g_close();
	static int g_play();
	static int g_pause();
	static int g_stop();
	static int g_fastforward(int speed );
	static int g_slowforward(int speed );
	static int g_oneframeforward();
	static int g_getdecframes();
	static int g_backward(int speed );
	static int g_seek(struct dvrtime * t );
	static int g_getcurrenttime(struct dvrtime * t );
	static int g_getlivetime(struct dvrtime * t );

	static int g_getlibnum() {
        return decoder::num_library ;
    }
	static int g_getlibname(int index, LPTSTR libname);	// max 128 charater
    static LPCSTR g_getlibname(int index) ;
	static int g_getlibversion(int index, int * version );	// 4 integer
	static int g_checkpassword();						// check username /password 

	static int g_minitrack_start(decoder * pdec) ;		// start minitrack service
	static int g_minitrack_selectserver(decoder * pdec);// DVRView selected a new DVR server
	static int g_minitrack_stop();						// stop minitrack service
	static int g_minitrack_init_interface() ;			// r1.3, required by Tongrui
	static int g_minitrack_sendpwevent(void *buf, int bufsize);	 // send patrol witness user event to minitrack DLL
};


extern decoder * g_decoder ;
extern int g_minitracksupport ;
extern int g_minitrack_up ;
extern int g_minitrack_used ;

#endif

