
#ifndef __DVRFILESTREAM_H__
#define __DVRFILESTREAM_H__

#include "ply614.h"
#include "crypt.h"
#include "mpegps.h"

// DVR fie playback stream

// New .DVR file format. Compatible with old client software

#define DVRFILEFLAG ('DVRN')

// .DVR file header
struct DVRFILEHEADER {
	DWORD	flag ;
	int		totalchannels ;
	DWORD	begintime ;
	DWORD   endtime ;
} ;

// .DVR channel data location
struct DVRFILECHANNEL {
	__int64 begin ;
	__int64 end ;
} ;

#define DVRFILEFLAGEX	('DVEX')
#define DVRFILE_DUAL_MIPS	1001	// TME duel-mips 
#define DVRFILE_614	1002	// 614

// Extra DVR file header
struct DVRFILEHEADEREX {
	int		size ;					// extra header size
	DWORD	flagex ;				// DVR file flag, 'DVEX'.
	DWORD   platform_type ;			// platform type, support (DVRFILE_DUEL_MIPS, DVRFILE_614) only.
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
	int     reserved;
    char	cameraname[128] ;		// camera name
	__int64	begin ;
	__int64	end ;
	__int64	keybegin ;
	__int64	keyend ;
};

struct DVRFILEKEYENTRY {
	DWORD	time ;					// time from begin
	DWORD	offset ;				// offset from channel data begin
};

// new .DVR file format
/*
	struct DVRFILEHEADER								// for old DVR compatible
	struct DVRFILECHANNEL [total channels]				// for old DVR compatible
	struct DVRFILEHEADEREX
	struct DVRFILECHANNELEX [total channels]
	channel 0 data
		.264 FILE header	
		.264 data
		keyframe index
			struct DVRFILEKEYENTRY
			struct DVRFILEKEYENTRY
			...
	channel 1 data
		.264 FILE header	
		.264 data
		keyframe index
			struct DVRFILEKEYENTRY
			struct DVRFILEKEYENTRY
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
	__int64	   m_beginpos ;
	__int64    m_endpos ;
	struct dvrtime m_curtime ;			// current playing time

	// extra information
	int		m_version[4] ;				// file version
	char	m_servername[128] ;			// Server name of the original data
	char	m_cameraname[128] ;			// stream camera name

	struct sync_timer {
		struct dvrtime m_synctime ;
		int			   m_insync ;			// if timer is in cync with all other channel
		__int64		   m_synctimestamp ;
	} m_sync ;							// sync timer, use to synchronize other stream and calculate stream time in sync

	int m_maxframesize ;				// used for get preivew frame

	list <struct DVRFILEKEYENTRY> m_keyindex ;		// list of key frame index

	// file encryption support
	int m_encryptmode;					// 0: no encryption, 1: windows mode, 2: Frame data first 1k bytes RC4
	CMpegPs m_ps;
	//int m_countK; // debugging

	// file encryption support, RC4 frame data
	unsigned char m_crypt_table[4096] ;
	void RC4_crypt( unsigned char * text, int textsize ) {
		RC4_block_crypt( text, textsize, 0, m_crypt_table, sizeof(m_crypt_table));
	}
	// generate cryption table
	void RC4_gentable(char * password) {
		unsigned char k256[256] ;
		key_256(password, k256);
		RC4_crypt_table( m_crypt_table, sizeof(m_crypt_table), k256);
	}

public:
	dvrfilestream(char * filename, int channel);	// open dvr file stream for playback
	~dvrfilestream();

	int seek( struct dvrtime * dvrt ); 		// seek to specified time.
	int getdata(char **framedata, int * framesize, int * frametype, int *headerlen);	// get dvr data frame
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
	void setpassword(int passwdtype, void * password, int passwdsize )
	{
		if( passwdtype==ENC_MODE_RC4FRAME ) {			// Frame RC4
			RC4_gentable((char *)password);
		}
	}
};

#endif	// __DVRFILESTREAM_H__