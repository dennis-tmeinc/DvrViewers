
#include "stdafx.h"
#include "PLY301.h"
#include "dvrnet.h"
#include "player.h"
#include "dvrpreviewstream.h"

// DVR stream in interface
//     a single channel dvr data streaming in 

// open dvr server stream for preview
dvrpreviewstream::dvrpreviewstream(char * servername, int channel)
{
	m_channel=channel ;
	strncpy( m_server, servername, sizeof(m_server));
	m_socket=0;
//	m_socket=net_connect( m_server );
//	net_preview_play(m_socket, m_channel );
	m_shutdown=0 ;
	m_live2=0 ;
}

dvrpreviewstream::~dvrpreviewstream()
{
	if( m_socket>0 ) {
		net_close(m_socket);
	}
}

// get dvr data frame
int dvrpreviewstream::getdata(char **framedata, int * framesize, int * frametype)
{
	int res=0 ;
	if( m_shutdown ) {
		return res ;
	}
    lock();
	if( m_socket<=0 ) {
		m_socket=net_connect( m_server );
		if( m_socket>0 ) {
			// TVS key block
			if( dvrplayer::keydata ) {
				net_CheckKey(m_socket, (char *)dvrplayer::keydata );
			}
			m_live2 = net_live_open( m_socket, m_channel ) ;
			if( m_live2==0 ) {
				net_preview_play(m_socket, m_channel );
			}
		}
//		return res ;
	}

	if( m_live2 ) {
		* frametype = net_live_getframe(m_socket, framedata, framesize);
		if( *frametype>=0 ) {
			res = 1 ;
		}
		else {
			res = 0 ;
		}
	}
	else {
		* frametype = FRAME_TYPE_UNKNOWN ;
		res = net_preview_getdata(m_socket, framedata, framesize);
	}
    unlock();
	if( res<=0 ) {
		net_close(m_socket);
		m_socket = 0 ;
		return 0 ;
	}
	return res ;
}

// get current time and timestamp
int dvrpreviewstream::gettime( struct dvrtime * dvrt )
{
	time_t t ;
	struct tm * stm ;
	time(&t);								// get current computer time
	stm=localtime( &t);
	dvrt->year = 1900+stm->tm_year ;
	dvrt->month = 1 + stm->tm_mon ;
	dvrt->day = stm->tm_mday ;
	dvrt->hour = stm->tm_hour ;
	dvrt->min = stm->tm_min ;
	dvrt->second = stm->tm_sec ;
	dvrt->millisecond = 0 ;
	dvrt->tz = 0 ;
    return 1;
}

// get server information
int dvrpreviewstream::getserverinfo(struct player_info *ppi)
{
	return 0;
}

// get stream channel information
int dvrpreviewstream::getchannelinfo(struct channel_info * pci)
{
	return 0;
}

int dvrpreviewstream::shutdown()
{
	if( m_socket>0 ) {
		net_shutdown(m_socket);
		m_shutdown=1;
	}
	return 0;
}
