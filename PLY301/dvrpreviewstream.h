
#ifndef __DVRPREVIEWSTREAM_H__
#define __DVRPREVIEWSTREAM_H__

#include "PLY301.h"

// DVR preivew stream
//     Single channel dvr data streaming for dvr preivew

class dvrpreviewstream : public dvrstream {
	int m_channel ;
	char m_server[256] ;
	SOCKET m_socket ;
	int  m_live2 ;		// (2nd version live stream)
	int  m_shutdown ;

public:
	dvrpreviewstream(char * servername, int channel );	// open dvr server stream for preview

	~dvrpreviewstream();

//	int seek( struct dvrtime * dvrt ); 		// seek to specified time.
//	int prev_keyframe();						// seek to previous key frame position.
//	int next_keyframe();						// seek to next key frame position.
	int getdata(char **framedata, int * framesize, int * frametype);	// get dvr data frame
	int gettime( struct dvrtime * dvrt );	// get current time and timestamp
	int getserverinfo(struct player_info *ppi) ;	// get server information
	int getchannelinfo(struct channel_info * pci);	// get stream channel information
//	int getdayinfo(dvrtime * daytime);							// get stream day data availability information
//	int gettimeinfo( struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize); // get data availability information
//	int getlockfileinfo(struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize );					// get locked file availability information
	int shutdown();
};

#endif	// __DVRPREVIEWSTREAM_H__