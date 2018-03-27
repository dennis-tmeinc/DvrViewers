
#ifndef __DVRPREVIEWSTREAM_H__
#define __DVRPREVIEWSTREAM_H__

#include "ply606.h"
#include "tme_thread.h"
#include "mtime.h"

#define VOSC_MAX_SIZE 64
#define NET_BUFSIZE (500 * 1024)

// DVR preivew stream
//     Single channel dvr data streaming for dvr preivew

class dvrpreviewstream : public dvrstream {
private:
	int m_channel ;
	char m_server[256] ;
	SOCKET m_socket ;
	int  m_shutdown ;
	struct key_data * m_keydata ;
	char m_videoObjectStartCode[VOSC_MAX_SIZE];
	int m_voscSize;
	bool m_gotVideoKeyFrame;
	BYTE m_buf[NET_BUFSIZE];
	int m_bufsize;
	bool m_bDie, m_bError;
	HANDLE m_hThread;
	//tme_object_t m_ipc;
    tme_mutex_t		m_lock, m_lockPts;
	mtime_t m_curTime;

public:
	dvrpreviewstream(char * servername, int channel );	// open dvr server stream for preview

	~dvrpreviewstream();
	int PlaybackThread();							// playback thread to cache frame data

//	int seek( struct dvrtime * dvrt ); 		// seek to specified time.
//	int prev_keyframe();						// seek to previous key frame position.
//	int next_keyframe();						// seek to next key frame position.
	int gettime( struct dvrtime * dvrt );	// get current time and timestamp
	int getserverinfo(struct player_info *ppi) ;	// get server information
	int getchannelinfo(struct channel_info * pci);	// get stream channel information
//	int getdayinfo(dvrtime * daytime);							// get stream day data availability information
//	int gettimeinfo( struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize); // get data availability information
//	int getlockfileinfo(struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize );					// get locked file availability information
	int shutdown();
	virtual block_t *getdatablock();	
	// TVS/PWII USB key system support, set key data block
	virtual void setkeydata( struct key_data * keydata ) {
		m_keydata = keydata ;
	} ;
	int connectDvr(char *server);
	int openStream();
};

#endif	// __DVRPREVIEWSTREAM_H__