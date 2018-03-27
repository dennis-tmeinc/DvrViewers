
#ifndef __DVRPREVIEWSTREAM_H__
#define __DVRPREVIEWSTREAM_H__

#include "ply614.h"
#include "tme_thread.h"

#define NET_BUFSIZE (500 * 1024)

// DVR preivew stream
//     Single channel dvr data streaming for dvr preivew

class dvrpreviewstream : public dvrstream {
	int m_channel ;
	char m_server[256] ;
	SOCKET m_socket ;
	int  m_shutdown ;
	struct key_data * m_keydata ;
	BYTE m_buf[NET_BUFSIZE];
	int m_bufsize;
	bool m_bDie, m_bError;
	HANDLE m_hThread;
	//tme_object_t m_ipc;
    tme_mutex_t		m_lock;

public:
	dvrpreviewstream(char * servername, int channel );	// open dvr server stream for preview

	~dvrpreviewstream();
	int PlaybackThread();							// playback thread to cache frame data

	int getdata(char **framedata, int * framesize, int * frametype, int *headerlen);	// get dvr data frame
	int gettime( struct dvrtime * dvrt );	// get current time and timestamp
	int getserverinfo(struct player_info *ppi) ;	// get server information
	int getchannelinfo(struct channel_info * pci);	// get stream channel information
	int shutdown();

	// TVS/PWII USB key system support, set key data block
	virtual void setkeydata( struct key_data * keydata ) {
		m_keydata = keydata ;
	} ;
};

#endif	// __DVRPREVIEWSTREAM_H__