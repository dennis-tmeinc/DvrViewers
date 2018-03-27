
#ifndef __DVRPLAYBACKSTREAM_H__
#define __DVRPLAYBACKSTREAM_H__

#include <stdlib.h>
#include <malloc.h>
#include "ply606.h"
#include "crypt.h"
#include "mpeg4.h"
#include "block.h"
#include "tme_thread.h"

// DVR preivew stream
//     Single channel dvr data streaming for dvr preivew

class dvrplaybackstream : public dvrstream {
protected:
	int m_channel ;
	SOCKET m_socket ;
	int m_streamhandle ;			// stream mode handle
	char m_server[MAX_PATH] ;
 
	DWORD m_monthinfo ;
	struct dvrtime m_month ;

	xlist   <struct cliptimeinfo> m_clipinfolist ;	// list of day clip info.
	struct dvrtime m_dayinfoday ;
	xlist   <struct cliptimeinfo> m_lockinfolist ;	// list of locked file clip info.
	struct dvrtime m_lockinfoday ;


	struct dvrtime m_curtime ;
	struct dvrtime m_keyframetime ;
	__int64		   m_keyframestamp ;
	CMpegPs m_ps;

	// file encryption support, RC4 frame data
	int m_encryptmode;					// 0: no encryption, 1: windows mode, 2: Frame data first 1k bytes RC4
	unsigned char m_RC4_table[1024] ;
	// (tvs/pwii usb key) support
	struct key_data * m_keydata ;
	DWORD m_keyhash;

	bool m_bDie, m_bError;
	HANDLE m_hThread;
    block_fifo_t *m_pFifo;
	//tme_object_t m_ipc;
    tme_mutex_t		m_lock;
    tme_cond_t      m_wait;
    tme_mutex_t		m_lockUI, m_lockTime;
	bool m_bWaiting, m_bBufferPending, m_bSeekReq;

	int m_threadstate;
	int seek2(struct dvrtime *);

public:
	dvrplaybackstream(char * servername, int channel);	// open dvr server stream for preview
	~dvrplaybackstream();

	int PlaybackThread();							// playback thread to cache frame data

	int openstream();						// open stream mode play back
	char * openshare();						// open file share play back, return shared root directory

	int seek( struct dvrtime * dvrt ); 		// seek to specified time.
	int gettime( struct dvrtime * dvrt );	// get current time and timestamp
	int getserverinfo(struct player_info *ppi) ;	// get server information
	int getchannelinfo(struct channel_info * pci);	// get stream channel information
	int getdayinfo(dvrtime * daytime);							// get stream day data availability information
	int gettimeinfo( struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize); // get data availability information
	int getlockfileinfo(struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize );					// get locked file availability information
	void setpassword(int passwdtype, void * password, int passwdsize );
	virtual block_t *getdatablock();	
	// TVS/PWII USB key system support, set key data block
	virtual void setkeydata( struct key_data * keydata );
};

#endif	// __DVRPLAYBACKSTREAM_H__