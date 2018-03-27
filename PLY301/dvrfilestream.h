
#ifndef __DVRFILESTREAM_H__
#define __DVRFILESTREAM_H__

#include "PLY301.h"
#include "crypt.h"

// DVR fie playback stream

extern char Head264[40];

// New .DVR file format. Compatible with old client software

// 'DVRF'
#define DVRFILEFLAG (0x44565246)

// .DVR file header
struct DVRFILEHEADER {
	DWORD	flag ;
	int		totalchannels ;
	DWORD	begintime ;
	DWORD   endtime ;
} ;

// .DVR channel data location
struct DVRFILECHANNEL {
	DWORD begin ;
	DWORD end ;
} ;

// 'DVEX'
#define DVRFILEFLAGEX	(0x44564558)
// 'DVE\64'
#define DVRFILEFLAGEX64	(0x44564564)

// Extra DVR file header
struct DVRFILEHEADEREX {
	int		size ;					// extra header size
	DWORD	flagex ;				// DVR file flag, 'DVEX'., "DVE\64" for 64bit offsets version use DVRFILECHANNELEX64
	DWORD   platform_type ;			// platform type, support (DVRFILE_DUEL_MIPS) only.
	int     version[4];				// Creator player version
	int     encryption_type ;		// encryption type, 0 for no encryption, 1 for frame RC4
	int		totalchannels ;			// total camera channels
	DWORD	date ;					// file start data in BCD, ex 2007-03-01 as 20070301
	DWORD   time ;					// file start time of the day, in seconds
	DWORD   length ;				// file length in seconds
	char    servername[128] ;		// DVR server name
} ;

// DVR file channel header
struct DVRFILECHANNELEX {
	int		size ;
    char	cameraname[120] ;		// camera name
    int     clipbegin ;				// begin of clip info
    int     clipend ;				// end of clip info
	DWORD	begin ;					// begin of clip data
	DWORD	end ;					// end of clip data
	DWORD	keybegin ;				// begin of key entry
	DWORD	keyend ;				// end of key entry
};

struct DVRFILEKEYENTRY {
	int 	time ;					// time from begin (seconds)
	DWORD	offset ;				// offset from channel data begin
};

struct DVRFILECLIPENTRY {
	int 	cliptime ;				// time from begin (seconds)
	int 	length ;				// clip length (seconds)
};

#define DVRFILECHANNELEX64_FLAG	(0x0ee06464)

struct DVRFILECHANNELEX64 {
	// same as DVRFILECHANNELEX
	int		size ;
    char	cameraname[120] ;		// camera name
    int     clipbegin ;				// begin of clip info
    int     clipend ;				// end of clip info
	DWORD	begin ;					// begin of clip data
	DWORD	end ;					// end of clip data
	DWORD	keybegin ;				// begin of key entry
	DWORD	keyend ;				// end of key entry
	// new fields for DVRFILECHANNELEX64
	OFF_T	begin64 ;
	OFF_T	end64 ;
	OFF_T	keybegin64 ;
	OFF_T	keyend64 ;
	OFF_T	clipbegin64 ;
	OFF_T	clipend64 ;
	int     ex64 ;					// DVRFILECHANNELEX64_FLAG
};

struct DVRFILEKEYENTRY64 {
	int 	time ;					// time from begin (seconds)
	OFF_T	offset ;				// offset from channel data begin
};

// .DVR file format
/*
	struct DVRFILEHEADER								// for old DVR compatible
	struct DVRFILECHANNEL [total channels]				// for old DVR compatible
	struct DVRFILEHEADEREX
	struct DVRFILECHANNELEX / DVRFILECHANNELEX64 [total channels]
	channel 0 data
		.264 FILE header	
		.264 data
		keyframe index
			struct DVRFILEKEYENTRY
			struct DVRFILEKEYENTRY
			...
        clip index
            DVRFILECLIPENTRY
            DVRFILECLIPENTRY
            ...
	channel 1 data
		.264 FILE header	
		.264 data
		keyframe index
			struct DVRFILEKEYENTRY
			struct DVRFILEKEYENTRY
			...
        clip index
            DVRFILECLIPENTRY
            DVRFILECLIPENTRY
            ...
	...

*/

class dvrfilestream : public dvrstream{ 
protected:

	int m_channel ;						// stream channel
	FILE * m_file ;						// file handle
	int		m_totalchannel;				// total channel

	struct dvrtime m_begintime ;
	struct dvrtime m_endtime ;
	OFF_T  m_beginpos ;
	OFF_T  m_endpos ;
	struct dvrtime m_curtime ;			// current playing time

	// extra information
	int		m_version[4] ;				// file version
	char	m_servername[128] ;			// Server name of the original data
	char	m_cameraname[128] ;			// stream camera name

	struct sync_timer {
		struct dvrtime m_synctime ;
		DWORD		   m_synctimestamp ;
		int			   m_insync ;			// if timer is in cync with all other channel
	} m_sync ;							// sync timer, use to synchronize other stream and calculate stream time in sync

	int m_maxframesize ;				// used for get preivew frame

	list <struct DVRFILEKEYENTRY64> m_keyindex ;	// list of key frame index
	list <struct DVRFILECLIPENTRY> m_clipindex ;	// list of clips info

public:
	dvrfilestream(char * filename, int channel);	// open dvr file stream for playback
	~dvrfilestream();

	int seek( struct dvrtime * dvrt ); 		// seek to specified time.
	OFF_T prev_keyframe();						// seek to previous key frame position.
	OFF_T next_keyframe();						// seek to next key frame position.
	int getdata(char **framedata, int * framesize, int * frametype);	// get dvr data frame
	int gettime( struct dvrtime * dvrt );	// get current stream time, (precise to miliseconds)
	int getserverinfo(struct player_info *ppi) ;						// get server information
	int getchannelinfo(struct channel_info * pci);						// get stream channel information
	int getdayinfo(dvrtime * daytime);		// get stream day data availability information
	int gettimeinfo( struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize); // get data availability information
	int getlockfileinfo(struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize );	// get locked file availability information

	void loadkey();		// load key index of all files

	// provide and internal structure to sync other stream
	void * getsynctimer();
	// use synctimer to setup internal sync timer
	void setsynctimer(void * synctimer) ;	

};

#endif	// __DVRFILESTREAM_H__