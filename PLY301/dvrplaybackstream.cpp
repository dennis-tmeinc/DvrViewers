
#include "stdafx.h"
#include <process.h>
#include "PLY301.h"
#include "player.h"
#include "dvrnet.h"
#include "dvrplaybackstream.h"

// DVR stream in interface
//     a single channel dvr data streaming in 

// open dvr server stream for playback
dvrplaybackstream::dvrplaybackstream(char * servername, int channel)
{
	m_channel=channel ;
	strncpy( m_server, servername, sizeof(m_server));
	m_socket=net_connect( m_server );
	if( m_socket>0 && dvrplayer::keydata ) {
		net_CheckKey(m_socket, (char *)dvrplayer::keydata );
	}
	m_streamhandle=0 ;
	m_curtime.year=1980 ;
	m_curtime.month=1 ;
	m_curtime.day=1 ;
	m_curtime.hour=0;
	m_curtime.min=0;
	m_curtime.second=0;
	m_curtime.millisecond=0;
	m_curtime.tz=0 ;
	memset( &m_month, 0, sizeof(m_month));

	// initialize frame fifo
	m_fifohead = NULL;
	m_fifotail = NULL ;
	m_fifosize = 0 ;
	m_threadstate=0;
	m_encryptmode = ENC_MODE_NONE ;

	m_clipinfolist = NULL ;
	m_clipinfolistsize = 0 ;
	m_clipinfolistday=0 ;

	m_lockinfolist = NULL ;
	m_lockinfolistsize=0 ;
	m_lockinfolistday=0 ;

}

dvrplaybackstream::~dvrplaybackstream()
{
    lock();
    if( m_socket>0 && m_streamhandle>0 ) {
        net_stream_close(m_socket, m_streamhandle);
    }

    if( m_threadstate==1 ) {
        m_threadstate=2 ;
        while(m_threadstate!=0) {
            Sleep(2);
        }
    }
    fifoclean();
    if( m_clipinfolist ) delete m_clipinfolist ;
    if( m_lockinfolist ) delete m_lockinfolist ;
    if( m_socket ) {
        net_close(m_socket);
    }
    unlock();
}

// playback stream buffering thread
static void dvrplaybackstream_thread(void *p)
{
	dvrplaybackstream * pstream = (dvrplaybackstream *)p ;
	pstream->playbackthread();
}

void dvrplaybackstream::playbackthread()
{
	int res ;
	struct hd_frame * pframe ;
	struct frame_t * framet ;
	
	m_threadstate=1 ;
	while( m_threadstate==1 ) {
		while( m_fifosize<500 ) {

			framet = new struct frame_t ;
			framet->next=NULL;
			framet->frametype = FRAME_TYPE_UNKNOWN ;
			framet->framedata = NULL;
			framet->framesize = 0 ;

			lock();
			res = net_stream_getdata(m_socket, m_streamhandle, &(framet->framedata), &(framet->framesize), &(framet->frametype) );
			if( res && framet->framesize>0 ) {
				pframe = (struct hd_frame *)(framet->framedata) ;
				if( pframe->flag==1 ) {
					if( pframe->frames == 1 ) {
						net_stream_gettime(m_socket, m_streamhandle, &m_keyframetime);
						framet->frametime = m_keyframetime ;
						m_keyframestamp = pframe->timestamp ;
					}
					else {
						framet->frametime = m_keyframetime ;
						dvrtime_addmilisecond(&(framet->frametime),  tstamp2ms(pframe->timestamp-m_keyframestamp));
					}
				}
				else {
					net_stream_gettime(m_socket, m_streamhandle, &(framet->frametime));
				}
				fifoputframe(framet);
			}
			else {
				delete framet ;
			}
            unlock();
			Sleep(1);
			if( m_threadstate!=1 ) {
				break;
			}
		}
		Sleep(10);
	}
	fifoclean();
	m_threadstate=0;		// thread end 
}

// open stream mode play back
int dvrplaybackstream::openstream()
{
	lock();
    m_seekreq=0 ;
	m_streamhandle = net_stream_open(m_socket, m_channel);
	fifoclean();
	unlock();
	return (m_streamhandle>0 );
}

// open file share
char * dvrplaybackstream::openshare()
{
	lock();
	if( net_getshareinfo(m_socket, m_server ) ) {
		unlock();
		return m_server ;
	}
	unlock();
	return NULL ;
}

// get dvr data frame
int dvrplaybackstream::getdata(char **framedata, int * framesize, int * frametype)
{
    struct hd_frame * pframe ;
    struct frame_t * framet ;
    int res = 0 ;

//	if( m_threadstate==0 ) {
//		_beginthread( dvrplaybackstream_thread, 0, (void *)this );
//	}

    if( m_seekreq ) {
        lock();
        m_seekreq=0 ;
        m_curtime=m_seektime ;
        fifoclean();
        net_stream_seek(m_socket, m_streamhandle, &m_curtime);
        m_keyframestamp=0;
        unlock();
    }
    framet = fifogetframe();
    if( framet ) {
        * framedata = framet->framedata ;
        * framesize = framet->framesize ;
        * frametype = framet->frametype ;
        m_curtime = framet->frametime ;
        delete framet ;
        res=1;
    }
    else {
        if( m_socket<=0 ) {
            return res ;
        }
        * frametype = FRAME_TYPE_UNKNOWN ;
        lock();
        res = net_stream_getdata(m_socket, m_streamhandle, framedata, framesize, frametype);
		if( res ) {
			pframe = (struct hd_frame *)*framedata ;
			if( pframe->flag==1 ) {
				if( m_encryptmode==ENC_MODE_RC4FRAME ) {		// decrypt RC4 frame
					// Decrypt frame data
					int dec_size = pframe->framesize ;
					if( dec_size>1024 ) {
						dec_size=1024 ;
					}
					RC4_crypt( ((unsigned char *)pframe)+sizeof(struct hd_frame), dec_size ); 
				}
				if(  m_keyframestamp == 0 || 
					 pframe->timestamp<m_keyframestamp || 
					 (pframe->timestamp-m_keyframestamp)>10000 ) 
				{
					if( net_stream_gettime(m_socket, m_streamhandle, &m_keyframetime) ){
					    m_curtime = m_keyframetime ;
					    m_keyframestamp = pframe->timestamp ;
					}
				}
				else {
					m_curtime = m_keyframetime ;
					dvrtime_addmilisecond(&m_curtime, tstamp2ms((int)(pframe->timestamp)-(int)m_keyframestamp));
				}
			}
			else {
				net_stream_gettime(m_socket, m_streamhandle, &m_curtime);
			}
		}
//		else {
//			net_close( m_socket ) ;
//			m_socket=0 ;
//		}
        unlock();
	}
	return res ;
}

// get current time and timestamp
int dvrplaybackstream::gettime( struct dvrtime * dvrt )
{
	*dvrt = m_curtime ;
	return  1 ;
}

// get server information
int dvrplaybackstream::getserverinfo(struct player_info *ppi)
{
	return 0;
}

// get stream channel information
int dvrplaybackstream::getchannelinfo(struct channel_info * pci)
{
	return 0;
}

// seek to specified time.
// return 1: success
//        0: error
//        DVR_ERROR_FILEPASSWORD: need password/password error
int dvrplaybackstream::seek( struct dvrtime * dvrt )
{
	int res ;
	lock();
	m_seektime=*dvrt ;
	m_seekreq=0 ;
	m_curtime=m_seektime ;
	fifoclean();
	res = net_stream_seek(m_socket, m_streamhandle, &m_curtime);
	if( res == 0x484b4834 ) {
		m_encryptmode = ENC_MODE_NONE ;
		res = 1 ;
	}
	else if( res == 0 ) {
		res = 1 ;
	}
	else if( res==-1 ) {
		res = 0 ;
	}
	else {
		RC4_crypt( (unsigned char *)&res, 4 ) ;
		if( res == 0x484b4834 ) {
			m_encryptmode = ENC_MODE_RC4FRAME ;
			res = 1 ;
		}
		else {
			m_encryptmode = ENC_MODE_NONE ;
			res = DVR_ERROR_FILEPASSWORD ;
		}
	}

	m_keyframestamp=0;
	unlock();
	return res ;
}

// get stream day data availability information
int dvrplaybackstream::getdayinfo(dvrtime * day)
{
    int di ;
	lock();
	if( day->month!=m_month.month ||
		day->year!=m_month.year ) {			// day info cache available?
			m_month=*day ;
			m_monthinfo = net_stream_getmonthinfo( m_socket, m_streamhandle, &m_month);
	}
    di = ( m_monthinfo & (1<<(day->day-1)) );
	unlock();
	return di ;
}

// get data availability information
int dvrplaybackstream::gettimeinfo( struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize)
{
	int i;
	int getsize = 0 ;
	int daybegintime, dayendtime ;		// convert begintime, endtime to seconds of the day

	// convert begintime, endtime to seconds of the day
	int day ;
	struct dvrtime daytime ;

	memset( &daytime, 0, sizeof(daytime));
	daytime.year = begintime->year ;
	daytime.month = begintime->month ;
	daytime.day = begintime->day ;
	day = daytime.year * 10000 + daytime.month * 100 + daytime.day ;

	daybegintime = begintime->hour * 3600 +
				   begintime->min * 60 +
				   begintime->second ;
	dayendtime =   endtime->hour * 3600 +
				   endtime->min * 60 +
				   endtime->second ;

	if( dayendtime<=daybegintime ) {				// parameter error!
		return 0 ;
	}

	lock();
	if( m_clipinfolistday!=day ) {				// cache missed
			if(m_clipinfolist) delete m_clipinfolist ;
			m_clipinfolist = NULL ;
			m_clipinfolistsize = net_stream_getdayinfo(m_socket, m_streamhandle, &daytime, &m_clipinfolist );
			m_clipinfolistday = day ;
	}

	for( i=0; i<m_clipinfolistsize; i++) {
		// check if time overlapped with begintime - endtime
		if( m_clipinfolist[i].on_time >= dayendtime ||
   			m_clipinfolist[i].off_time <= daybegintime ) {
				continue ;				// clip time not included
		}

		if( tinfosize>0 && tinfo!=NULL ) {
			if( getsize>=tinfosize ) {
				break;
			}
			// set clip on time
			if( m_clipinfolist[i].on_time > daybegintime ) {
				tinfo[getsize].on_time = m_clipinfolist[i].on_time - daybegintime ;
			}
			else {
				tinfo[getsize].on_time = 0 ;
			}
			// set clip off time
			tinfo[getsize].off_time = m_clipinfolist[i].off_time - daybegintime ;
		}
		getsize++ ;
	}
    unlock();
	return getsize ;
}

// get locked file availability information
int dvrplaybackstream::getlockfileinfo(struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize )
{
	int i;
	int getsize = 0 ;
	int daybegintime, dayendtime ;		// convert begintime, endtime to seconds of the day

	// convert begintime, endtime to seconds of the day
	int day ;
	struct dvrtime daytime ;

	memset( &daytime, 0, sizeof(daytime));
	daytime.year = begintime->year ;
	daytime.month = begintime->month ;
	daytime.day = begintime->day ;
	day = daytime.year * 10000 + daytime.month * 100 + daytime.day ;

	daybegintime = begintime->hour * 3600 +
				   begintime->min * 60 +
				   begintime->second ;
	dayendtime =   endtime->hour * 3600 +
				   endtime->min * 60 +
				   endtime->second ;

	if( dayendtime<=daybegintime ) {				// parameter error!
		return 0 ;
	}

    lock();
	if( m_lockinfolistday!=day ) {				// cache missed
			if(m_lockinfolist) delete m_lockinfolist ;
			m_lockinfolist = NULL ;
			m_lockinfolistsize = net_stream_getlockfileinfo(m_socket, m_streamhandle, &daytime, &m_lockinfolist );
			m_lockinfolistday = day ;
	}

	for( i=0; i<m_lockinfolistsize; i++) {
		// check if time overlapped with begintime - endtime
		if( m_lockinfolist[i].on_time >= dayendtime ||
   			m_lockinfolist[i].off_time <= daybegintime ) {
				continue ;				// clip time not included
		}

		if( tinfosize>0 && tinfo!=NULL ) {
			if( getsize>=tinfosize ) {
				break;
			}
			// set clip on time
			if( m_lockinfolist[i].on_time > daybegintime ) {
				tinfo[getsize].on_time = m_lockinfolist[i].on_time - daybegintime ;
			}
			else {
				tinfo[getsize].on_time = 0 ;
			}
			// set clip off time
			tinfo[getsize].off_time = m_lockinfolist[i].off_time - daybegintime ;
		}
		getsize++ ;
	}
    unlock();

	return getsize ;
}
