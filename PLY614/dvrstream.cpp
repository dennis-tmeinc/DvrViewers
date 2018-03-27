
#include "stdafx.h"
#include "PLY301.h"
#include "dvrnet.h"

// DVR stream in interface
//     a single channel dvr data streaming in 

// open dvr server stream for preview
dvrpreviewstream::dvrpreviewstream(char * servername, int channel)
{
	m_channel=channel ;
	strncpy( m_server, servername, sizeof(m_server));
	m_socket=net_preview_connect( servername, channel );
}

dvrpreviewstream::~dvrpreviewstream()
{
	net_close(m_socket);
}

// seek to specified time.
int dvrpreviewstream::seek( struct dvrtime * dvrt ) 		
{
	return 0;	// not supported
}

// seek to previous key frame position.
int dvrpreviewstream::prev_keyframe()						
{
	return 0;	// not supported
}

// seek to next key frame position.
int dvrpreviewstream::next_keyframe()						
{
	return 0;	// not supported
}

// get dvr data frame
int dvrpreviewstream::getdata(char **framedata, int * framesize, int * frametype)
{
	* frametype = FRAME_TYPE_UNKNOWN ;
	return net_preview_getdata(m_socket, framedata, framesize);
}

// get current time and timestamp
int dvrpreviewstream::gettime( struct dvrtime * dvrt, unsigned long * timestamp )
{
    return 0;
}

// get stream information
int dvrpreviewstream::getinfo()
{
	return 0;
}

// get stream day data availability information
int dvrpreviewstream::getdayinfo()			
{
	return 0;	// not used
}

// get data availability information
int dvrpreviewstream::gettimeinfo()	
{
	return 0;	// not used
}

// get locked file availability information
int dvrpreviewstream::getlockfileinfo()	
{
	return 0;	// not used
}
