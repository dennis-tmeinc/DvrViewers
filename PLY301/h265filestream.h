
#ifndef __H265FILESTREAM_H__
#define __H265FILESTREAM_H__

#include "PLY301.h"
#include "crypt.h"

#ifdef H264FILEFLAG
#undef H264FILEFLAG
#endif
#define H264FILEFLAG  (0x484b4d49)

class h265filestream : public dvrstream{ 
protected:

	int m_channel ;						// stream channel
	FILE * m_file ;						// file handle
	int		m_totalchannel;				// total channel

	int m_filesize ;					// file size in bytes
	int m_maxframesize ;				// used for get preivew frame

	struct dvrtime m_begintime ;
	int            m_filelen ;
	DWORD		   m_begintimestamp ;

	struct dvrtime m_curtime ;			// current playing time

	// extra information
	int		m_version[4] ;				// file version
	char	m_servername[12] ;			// Server name of the original data
	char	m_cameraname[256] ;			// stream camera name

	OFF_T find_frame();
	OFF_T next_frame();
public:
	h265filestream(char * filename, int channel);	// open dvr file stream for playback
	~h265filestream();

	int seek( struct dvrtime * dvrt ); 		// seek to specified time.
	OFF_T prev_keyframe();						// seek to previous key frame position.
	OFF_T next_keyframe();						// seek to next key frame position.
	int getdata(char **framedata, int * framesize, int * frametype);	// get dvr data frame
	int gettime( struct dvrtime * dvrt );	// get current stream time, (precise to miliseconds)
	int getserverinfo(struct player_info *ppi) ;						// get server information
	int getchannelinfo(struct channel_info * pci);						// get stream channel information
	int getdayinfo(dvrtime * daytime);		// get stream day data availability information
	int gettimeinfo( struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize); // get data availability information

};

#endif	// __H265FILESTREAM_H__