// dvrplayer.cpp : DVR player object
//

#include "stdafx.h"
#include <process.h>
#include "ply301.h"
#include "dvrnet.h"
#include "dvrpreviewstream.h"
#include "dvrharddrivestream.h"
#include "dvrplaybackstream.h"
#include "dvrfilestream.h"
#include "h264filestream.h"
#include "EnterDVRPassword.h"

#define USE_VIDEOCLIP
#ifdef USE_VIDEOCLIP
#include "videoclip.h"
#endif

#define PLAYER_TIME_SLICE	(5)
#define PLAYER_TIMEOUT_REFRESH (5000/PLAYER_TIME_SLICE)

// add dvrtime (dvrt) in seconds
void dvrtime_add(dvrtime * dvrt, int seconds)
{
    struct tm t ;
	t.tm_year = dvrt->year-1900 ;   // years since 1900
	t.tm_mon =  dvrt->month-1 ;      // months since January - [0,11]
	t.tm_mday = dvrt->day ;         // day of the month - [1,31]
	t.tm_hour = dvrt->hour ;
	t.tm_min = dvrt->min ;
	t.tm_sec = dvrt->second + seconds ;
	t.tm_wday = 0 ;
	t.tm_yday = 0 ;
	t.tm_isdst = -1 ; // A value less than zero to have the C run-time library code compute whether standard time or daylight savings time is in effect. ;
	mktime( &t );     // mktime() would normalize t.
	dvrt->year =   t.tm_year+1900 ;
	dvrt->month =  t.tm_mon+1 ;
	dvrt->day =    t.tm_mday ;
	dvrt->hour =   t.tm_hour ;
	dvrt->min =    t.tm_min ;
	dvrt->second = t.tm_sec ;
}

// add miliseconds to dvrt
void dvrtime_addmilisecond(dvrtime * dvrt, int miliseconds)
{
    struct tm t ;
	t.tm_year = dvrt->year-1900 ;   // years since 1900
	t.tm_mon =  dvrt->month-1 ;      // months since January - [0,11]
	t.tm_mday = dvrt->day ;         // day of the month - [1,31]
	t.tm_hour = dvrt->hour ;
	t.tm_min = dvrt->min ;
	dvrt->millisecond+=miliseconds ;
	t.tm_sec = dvrt->second + dvrt->millisecond/1000 ;
	dvrt->millisecond %= 1000 ;
	while( dvrt->millisecond<0 ) {
		t.tm_sec-=1 ;
		dvrt->millisecond+=1000 ;
	}
	t.tm_wday = 0 ;
	t.tm_yday = 0 ;
	t.tm_isdst = -1 ; // A value less than zero to have the C run-time library code compute whether standard time or daylight savings time is in effect. ;
	mktime( &t );     // mktime() would normalize t.
	dvrt->year =   t.tm_year+1900 ;
	dvrt->month =  t.tm_mon+1 ;
	dvrt->day =    t.tm_mday ;
	dvrt->hour =   t.tm_hour ;
	dvrt->min =    t.tm_min ;
	dvrt->second = t.tm_sec ;
}

// diffence of dvrtime dvrtime in milliseconds
// return
//      (milliseconds)(t2 - t1)
//
int dvrtime_diffms(dvrtime * t1, dvrtime * t2)
{
	time_t tt1, tt2 ;
    struct tm tm1;
	tm1.tm_year = t1->year-1900 ;   // years since 1900
	tm1.tm_mon  = t1->month-1 ;     // months since January - [0,11]
	tm1.tm_mday = t1->day ;         // day of the month - [1,31]
	tm1.tm_hour = t1->hour ;
	tm1.tm_min  = t1->min ;
	tm1.tm_sec  = t1->second ;
	tm1.tm_wday = 0 ;
	tm1.tm_yday = 0 ;
	tm1.tm_isdst= -1 ; // A value less than zero to have the C run-time library code compute whether standard time or daylight savings time is in effect.
	tt1 = mktime( &tm1 );
	tm1.tm_year = t2->year-1900 ;   // years since 1900
	tm1.tm_mon  = t2->month-1 ;     // months since January - [0,11]
	tm1.tm_mday = t2->day ;         // day of the month - [1,31]
	tm1.tm_hour = t2->hour ;
	tm1.tm_min  = t2->min ;
	tm1.tm_sec  = t2->second ;
	tm1.tm_wday = 0 ;
	tm1.tm_yday = 0 ;
	tm1.tm_isdst= -1 ; // A value less than zero to have the C run-time library code compute whether standard time or daylight savings time is in effect.
	tt2 = mktime( &tm1 );
	return t2->millisecond-t1->millisecond+1000*(int)(tt2-tt1) ;
}

// diffence of dvrtime dvrtime in seconds
// return
//      (seconds)(t2 - t1)
//
int dvrtime_diff(dvrtime * t1, dvrtime * t2)
{
	time_t tt1, tt2 ;
    struct tm tm1, tm2 ;
	if( t1==NULL ) {
		tt1=0;
	}
	else {
		tm1.tm_year = t1->year-1900 ;   // years since 1900
		tm1.tm_mon  = t1->month-1 ;     // months since January - [0,11]
		tm1.tm_mday = t1->day ;         // day of the month - [1,31]
		tm1.tm_hour = t1->hour ;
		tm1.tm_min  = t1->min ;
		tm1.tm_sec  = t1->second ;
		tm1.tm_wday = 0 ;
		tm1.tm_yday = 0 ;
		tm1.tm_isdst= -1 ; // A value less than zero to have the C run-time library code compute whether standard time or daylight savings time is in effect.
		tt1 = mktime( &tm1 );
	}
	tm2.tm_year = t2->year-1900 ;   // years since 1900
	tm2.tm_mon  = t2->month-1 ;     // months since January - [0,11]
	tm2.tm_mday = t2->day ;         // day of the month - [1,31]
	tm2.tm_hour = t2->hour ;
	tm2.tm_min  = t2->min ;
	tm2.tm_sec  = t2->second ;
	tm2.tm_wday = 0 ;
	tm2.tm_yday = 0 ;
	tm2.tm_isdst= -1 ; // A value less than zero to have the C run-time library code compute whether standard time or daylight savings time is in effect.
	tt2 = mktime( &tm2 );
	return (int)(tt2-tt1);
}

// compare two dvrtime
// return >0: t1>t2
//        0 : t1==t2
//       <0 : t1<t2
int dvrtime_compare(dvrtime * t1, dvrtime * t2)
{
	if( t1->year!=t2->year) {
		return t1->year-t2->year ;
	}
	if( t1->month!=t2->month) {
		return t1->month-t2->month ;
	}
	if( t1->day!=t2->day) {
		return t1->day-t2->day ;
	}
	if( t1->hour!=t2->hour) {
		return t1->hour-t2->hour ;
	}
	if( t1->min!=t2->min) {
		return t1->min-t2->min ;
	}
	if( t1->second!=t2->second) {
		return t1->second-t2->second ;
	}
	if( t1->millisecond!=t2->millisecond) {
		return t1->millisecond-t2->millisecond ;
	}
	return 0 ;
}

void dvrtime_init(struct dvrtime * t, int year, int month, int day, int hour, int minute, int second, int milisecond)
{
	t->year = year ;
	t->month=month ;
	t->day=day ;
	t->hour=hour ;
	t->min=minute ;
	t->second=second ;
	t->millisecond=milisecond ;
	t->tz=0 ;
}

time_t dvrtime_mktime( struct dvrtime * dvrt )
{
	time_t tt ;
    struct tm tm ;
	tm.tm_year = dvrt->year-1900 ;   // years since 1900
	tm.tm_mon  = dvrt->month-1 ;     // months since January - [0,11]
	tm.tm_mday = dvrt->day ;         // day of the month - [1,31]
	tm.tm_hour = dvrt->hour ;
	tm.tm_min  = dvrt->min ;
	tm.tm_sec  = dvrt->second ;
	tm.tm_wday = 0 ;
	tm.tm_yday = 0 ;
	tm.tm_isdst= -1 ; // A value less than zero to have the C run-time library code compute whether standard time or daylight savings time is in effect.
	tt = mktime( &tm );
	return tt ;
}

void dvrtime_localtime( struct dvrtime * dvrt, time_t tt ) 
{
	struct tm ttm ;
	localtime_s(&ttm, &tt);
	dvrt->year =   ttm.tm_year+1900 ;
	dvrt->month =  ttm.tm_mon+1 ;
	dvrt->day =    ttm.tm_mday ;
	dvrt->hour =   ttm.tm_hour ;
	dvrt->min =    ttm.tm_min ;
	dvrt->second = ttm.tm_sec ;
	dvrt->millisecond = 0 ;
	dvrt->tz = 0 ;
}

// create a new data stream base on open type
dvrstream * dvrplayer::newstream(int channel)
{
	dvrstream * stream = NULL ;
	if( channel<0 || channel>=m_totalchannel )
		return NULL ;

	if( m_dvrtype == DVR_DEVICE_TYPE_DVR && m_opentype == PLY_PREVIEW ) {	// preview a DVR server
		stream = new dvrpreviewstream( this->m_servername, channel ) ;		
	}
	else if( m_dvrtype == DVR_DEVICE_TYPE_HARDDRIVE && m_opentype == PLY_PLAYBACK  ) {	// playback hard drive.
		stream = new dvrharddrivestream( this->m_servername, channel ) ;				
	}
	else if( m_dvrtype == DVR_DEVICE_TYPE_LOCALSERVER && m_opentype == PLY_PLAYBACK  ) {	// playback hard drive.
		stream = new dvrharddrivestream( this->m_servername, channel, 1 ) ;				// open local server
	}
	else if( m_dvrtype == DVR_DEVICE_TYPE_DVR && m_opentype == PLY_PLAYBACK ) {	// playback DVR server.
		dvrplaybackstream * pbstream = new dvrplaybackstream( this->m_servername, channel );
		// set tvs/pwii key data.
		if( pbstream != NULL && m_keydata!=NULL ) {
			pbstream->setkeydata( m_keydata ) ;
		}
		if( pbstream->openstream() ) {
			stream=pbstream ;
		}
		else {
			if( m_servername[0]!='\\' ) {			// share fold not connected?
				char * shareroot = pbstream->openshare();
				if( shareroot ) {
					sprintf( m_servername, "%s\\_%s_\\", shareroot, this->m_playerinfo.servername );
				}
			}
			delete pbstream ;
			stream = new dvrharddrivestream( m_servername, channel );
		}
	}
	else if( m_dvrtype == DVR_DEVICE_TYPE_DVRFILE && m_opentype == PLY_PLAYBACK ) {
		stream = new dvrfilestream( this->m_servername, channel );
	}
	else if( m_dvrtype == DVR_DEVICE_TYPE_264FILE && m_opentype == PLY_PLAYBACK ) {
		stream = new h264filestream( this->m_servername, channel );
	}

	// set tvs/pwii key data.
	if( stream != NULL && m_keydata != NULL ) {
		stream->setkeydata( m_keydata ) ;
	}

	return stream ;
}

// find first channel to send frame data. (sync help function)
int dvrplayer::findfirstch()
{
	int i;
	struct channel_t * pch ;
	int firstchannel = -1 ;
	struct dvrtime t, firstt ;

	for(i=0; i<m_totalchannel; i++) {
		pch = &m_channellist[i];
		if( pch->stream!=NULL ) {
			if( pch->syncstate == PLAY_SYNC_START ) {
				return -1 ;
			}
			else if( pch->syncstate != PLAY_SYNC_NODATA ) {
				pch->stream->gettime( &t );
				if( firstchannel < 0 ) {
					firstchannel = i ;
					firstt = t ;
				}
				else {
					if( dvrtime_compare( &t, &firstt )<0 ) {
						firstchannel = i ;
						firstt = t ;
					}
				}
			}
		}
	}
	return firstchannel ;
}

// player thread (synchronizer)
void dvrplayer::playerthread(int channel)
{
	struct channel_t * pch ;
	char * framedata ;
	int framesize ;
	int frametype ;
	struct dvrtime frametime ;
	int refresh=0 ;
	int dataok ;
	int timesync ;
	int i;
	decoder * dec ;

	pch=&m_channellist[channel];
	while( pch->player_thread_state>0 ) {	            // keep running if not required to quit
        if( pch->player_thread_state ==  PLY_THREAD_SUSPEND ) {
            pch->player_thread_state =  PLY_THREAD_SUSPENDED ;
            continue ;
        }

        ResetEvent( pch->player_event ) ;
        WaitForSingleObject( pch->player_event, PLAYER_TIME_SLICE ) ;

        if( pch->player_thread_state ==  PLY_THREAD_SUSPENDED ) {
            continue ;
        }
		if( refresh++>PLAYER_TIMEOUT_REFRESH ){
			// refresh screen ;
			refresh = 0 ;
			dec = pch->target ;
			while( dec ) {
				dec->refresh();
				dec=dec->m_next ;
			}
		}

		if( pch->stream ) {
			if( m_errorstate!=0 ) {
				continue ;
			}

			dataok=pch->stream->getdata(&framedata, &framesize, &frametype);

			if( dataok>0 ) {

				// send data to decoder
				while( pch->player_thread_state==PLY_THREAD_RUN &&
					   pch->target!=NULL &&
					   pch->target->inputdata( framedata, framesize, frametype )<=0 ) {
                    WaitForSingleObject( pch->player_event, PLAYER_TIME_SLICE ) ;
				}
				if( pch->target ) {
					dec = pch->target->m_next ;
					while( dec ) {
						dec->inputdata( framedata, framesize, frametype );
						dec=dec->m_next ;
					}
				}
				// release frame data
				delete framedata ;

				timesync=pch->stream->gettime( &frametime );
				refresh = 0 ;

				if( m_opentype == PLY_PLAYBACK ) {  // m_opentype != PLY_PREVIEW
					// do the sync
					pch->syncstate = PLAY_SYNC_RUN ;
					// the first channel can go
					while( pch->player_thread_state==PLY_THREAD_RUN &&
							pch->syncstate == PLAY_SYNC_RUN &&
							findfirstch() != channel ) {
						if( pch->stream->preloaddata()==0 ) {
                            WaitForSingleObject( pch->player_event, PLAYER_TIME_SLICE ) ;
						}
						if( m_play_stat>=PLAY_STAT_SLOWEST && refresh++>PLAYER_TIMEOUT_REFRESH ){
							// refresh screen ;
							refresh = 0 ;
							dec = pch->target ;
							while( dec ) {
								dec->refresh();
								dec=dec->m_next ;
							}
						}
					}

					int diffms=dvrtime_diffms( &m_synctime, &frametime ) ;
					int diffclock=(int)(timeGetTime()-m_syncclock) ;
					if( (diffclock-diffms)>1250 ) {
						m_syncclock=timeGetTime();
						m_synctime=frametime ;
						diffms=0 ;
					}
					else if( (diffms-diffclock)>1250 ) {
						m_syncclock=timeGetTime()+300;
						m_synctime=frametime ;
						diffms=0 ;
					}

					// wait for sync
					while( pch->player_thread_state==PLY_THREAD_RUN &&
							pch->syncstate == PLAY_SYNC_RUN &&
							m_play_stat==PLAY_STAT_PLAY &&
							diffms > ((int)timeGetTime()-(int)m_syncclock)  
						) {
						if( pch->stream->preloaddata()==0 ) {
                            WaitForSingleObject( pch->player_event, PLAYER_TIME_SLICE ) ;
						}
					}

					// sync timer on all channels
					if( timesync==0 &&
						pch->syncstate == PLAY_SYNC_RUN )
					{										// timer not in sync
						void * synctimer = NULL ;
						for(i=0; i<m_totalchannel; i++) {
							if( i==channel ) {
								continue ;
							}
							struct channel_t * pcht = &m_channellist[i];
							if( pcht->stream ) {
								// get sync time ?
								synctimer=pcht->stream->getsynctimer();
								if( synctimer ) {
									break;
								}
							}
						}
						pch->stream->setsynctimer( synctimer );
					}
				}
				 m_playtime=frametime ;
			}
			else {
				pch->syncstate=PLAY_SYNC_NODATA ;
			}
		}
	}
	pch->player_thread_state=PLY_THREAD_NONE ;			// thread ended
}

struct player_thread_st {
    dvrplayer * player ;
    int channel ;
} ;

DWORD WINAPI PlyThreadProc(LPVOID lpParam) 
{
    struct player_thread_st pts = *(struct player_thread_st *)lpParam ;
    delete  (struct player_thread_st *)lpParam ;
    pts.player->playerthread( pts.channel );
    return 0 ;
}

void dvrplayer::startplayerthread(int channel)
{
    struct player_thread_st *pts = new struct player_thread_st ;
    pts->player=this ;
    pts->channel=channel ;
    m_channellist[channel].player_thread_state = PLY_THREAD_SUSPENDED ;
    m_channellist[channel].player_event = CreateEvent( NULL, TRUE, FALSE, NULL );
    m_channellist[channel].player_thread_handle = CreateThread( NULL, 0, PlyThreadProc, (LPVOID)pts, 0, NULL );
}


void dvrplayer::killplayerthread(int channel)
{
    if( m_channellist[channel].player_thread_handle && m_channellist[channel].player_thread_state > 0 ) {
        m_channellist[channel].player_thread_state = PLY_THREAD_QUIT ;
        while( m_channellist[channel].player_thread_state != PLY_THREAD_NONE ) {
            if( m_channellist[channel].player_event ) {
                SetEvent(m_channellist[channel].player_event) ;
            }
            WaitForSingleObject(m_channellist[channel].player_thread_handle, 1000);
        }
        CloseHandle(m_channellist[channel].player_thread_handle);
        m_channellist[channel].player_thread_handle=NULL ;
        if( m_channellist[channel].player_event ) {
            CloseHandle(m_channellist[channel].player_event) ;
            m_channellist[channel].player_event=NULL ;
        }
    }
}

void dvrplayer::suspendplayerthread(int channel)
{
    if( m_channellist[channel].player_thread_handle && m_channellist[channel].player_thread_state > 0 ) {
        m_channellist[channel].player_thread_state=PLY_THREAD_SUSPEND ;
        if( m_channellist[channel].player_event ) {
            SetEvent(m_channellist[channel].player_event) ;
        }
        while( m_channellist[channel].player_thread_state != PLY_THREAD_SUSPENDED ) {
            if( m_channellist[channel].player_event ) {
                SetEvent(m_channellist[channel].player_event) ;
            }
            Sleep(5);
        }
    }
}

void dvrplayer::resumeplayerthread(int channel)
{
    if( m_channellist[channel].player_thread_handle && m_channellist[channel].player_thread_state == PLY_THREAD_SUSPENDED ) {
        m_channellist[channel].player_thread_state=PLY_THREAD_RUN ;
        if( m_channellist[channel].player_event ) {
            SetEvent(m_channellist[channel].player_event) ;
        }
    }
}

dvrplayer::dvrplayer()
{
	m_servername[0]=0;
	m_dvrtype=0;
	m_opentype=0;
	m_totalchannel=0;
    m_channellist=NULL;
	memset( &m_playerinfo, 0, sizeof(m_playerinfo));
	m_playerinfo.size = sizeof(m_playerinfo);
	m_play_stat=PLAY_STAT_STOP ;
	m_playtime.year=1980;
	m_playtime.month=1;
	m_playtime.day=1;
	m_playtime.hour=0;
	m_playtime.min=0;
	m_playtime.second=0;
	m_playtime.millisecond=0;
	m_playtime.tz=0;

	m_hCryptProv=NULL ;	
	CryptAcquireContext( &m_hCryptProv, NULL, MS_ENHANCED_PROV, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
	m_hVideoKey=NULL	;	

	m_socket=0;
	m_errorstate=0;
	m_keydata = NULL ;
	m_keydatasize = 0 ;
}

dvrplayer::~dvrplayer()
{
    int i ;
	detach();						// detach all target decoders, streams and stop all threads

	if( m_dvrtype == DVR_DEVICE_TYPE_DVR && m_socket ){	// if connect to a DVR server
		net_close(m_socket);							// close socket
	}

	if( m_hVideoKey ) {
		CryptDestroyKey(m_hVideoKey);
	}
	CryptReleaseContext(m_hCryptProv,0);

	if( m_keydata!=NULL ) {
		delete (char *) m_keydata ;
	}
    for( i=0;i<m_totalchannel;i++) {
        killplayerthread(i);
    }
  	if( m_channellist!=NULL) delete [] m_channellist ;

    // delete critical section
//	DeleteCriticalSection(&m_criticalsection);

}

int dvrplayer::initchannel()
{
	int i ;
	struct channel_t * pch ;
    if( m_channellist!=NULL ) {
        delete [] m_channellist ;
        m_channellist=NULL;
    }
    if( m_totalchannel>0 ) {
        m_channellist = new channel_t [m_totalchannel] ;
        for(i=0;i<m_totalchannel;i++) {
            memset( &m_channellist[i], 0, sizeof( channel_t ) );
            startplayerthread( i );
        }
    }
	return 1 ;
}

// open DVR server over network, return 1:success, 0:fail.
int dvrplayer::openremote(char * netname, int opentype )
{
	m_dvrtype = DVR_DEVICE_TYPE_DVR ;
	strncpy( m_servername, netname, sizeof(m_servername) );

	m_totalchannel=0 ;

	if( opentype == PLY_PREVIEW ){
		m_opentype = opentype ;
		m_socket=net_connect( m_servername );
		if( m_socket ) {
			m_playerinfo.size = sizeof(m_playerinfo) ;
			net_getserverinfo( m_socket, &m_playerinfo );		// get server information
			m_playerinfo.features = PLYFEATURE_REALTIME | PLYFEATURE_CONFIGURE | PLYFEATURE_CAPTURE ;
			m_totalchannel = m_playerinfo.total_channel ;
			if( m_totalchannel>0 ) {
				initchannel();
			}
			return 1 ;
		}
		return 0 ;
	}
	else if( opentype == PLY_PLAYBACK ) {
		m_opentype = opentype ;
		m_socket=net_connect( m_servername );
		if( m_socket ) {
			m_playerinfo.size = sizeof(m_playerinfo) ;
			net_getserverinfo( m_socket, &m_playerinfo );		// get server information
			m_playerinfo.features = PLYFEATURE_PLAYBACK | PLYFEATURE_REMOTE | PLYFEATURE_CAPTURE  ;
			m_totalchannel = m_playerinfo.total_channel ;
			if( m_totalchannel>0 ) {
				initchannel();
			}
			return 1 ;
		}
		return 0 ;
	}
	return 0;
}

// open hard driver (file tree), return 1:success, 0:fail.
int dvrplayer::openharddrive( char * path )
{
	int res=0 ;
	int i ;
	dvrharddrivestream * hdstream ; 
	m_dvrtype = DVR_DEVICE_TYPE_HARDDRIVE ;
	strncpy( m_servername, path, sizeof(m_servername) );

	m_totalchannel=0 ;

	i=(int)strlen(m_servername) ;
	if( i>0 && m_servername[i-1]!='\\' ) {			// add trail '\\'
		m_servername[i]='\\' ;
		m_servername[i+1]='\0' ;
	}
	m_opentype =  PLY_PLAYBACK ;
	hdstream = new dvrharddrivestream(path, 0) ;
	m_playerinfo.size = sizeof(m_playerinfo) ;
	if( hdstream->getserverinfo( &m_playerinfo ) ) {
		m_playerinfo.features = PLYFEATURE_PLAYBACK | PLYFEATURE_HARDDRIVE | PLYFEATURE_CAPTURE  ;
		m_totalchannel = m_playerinfo.total_channel ;
	}
	if( m_totalchannel>0 ) {
		initchannel();
		res=1 ;
	}
	delete hdstream ;
	return res ;
}

// open multiple disks with same server name, return 1:success, 0:fail.
int dvrplayer::openlocalserver( char * servername )
{
	int res=0 ;
	dvrharddrivestream * hdstream ; 
	m_dvrtype = DVR_DEVICE_TYPE_LOCALSERVER ;
	strncpy( m_servername, servername, sizeof(m_servername) );

	m_totalchannel=0 ;
	m_opentype =  PLY_PLAYBACK ;
	hdstream = new dvrharddrivestream(servername, 0, 1) ;

	m_playerinfo.size = sizeof(m_playerinfo) ;
	if( hdstream->getserverinfo( &m_playerinfo ) ) {
		m_playerinfo.features = PLYFEATURE_PLAYBACK | PLYFEATURE_HARDDRIVE | PLYFEATURE_CAPTURE  ;
		m_totalchannel = m_playerinfo.total_channel ;
	}
	if( m_totalchannel>0 ) {
		initchannel();
		res=1 ;
	}
	delete hdstream ;
	return res ;
}

// open .DVR file, return 1:success, 0:fail.
int dvrplayer::openfile( char * dvrfilename )
{
	int res=0 ;
	FILE * dvrfile ;
	DWORD fileflag ;
	dvrfile=fopen( dvrfilename, "rb");
	if( dvrfile==NULL ) {
		return 0 ;
	}
	fread( &fileflag, 1, sizeof(fileflag), dvrfile );
	fclose( dvrfile );

	if( fileflag == DVRFILEFLAG ) {
		m_dvrtype = DVR_DEVICE_TYPE_DVRFILE ;
		m_opentype = PLY_PLAYBACK ;
		strncpy( m_servername, dvrfilename, sizeof(m_servername) );
		
		m_totalchannel=0 ;

		dvrfilestream * filestream = new dvrfilestream(m_servername, 0) ;

		m_playerinfo.size = sizeof(m_playerinfo) ;
		if( filestream->getserverinfo( &m_playerinfo ) ) {
			m_playerinfo.features = PLYFEATURE_PLAYBACK | PLYFEATURE_HARDDRIVE | PLYFEATURE_CAPTURE  ;
			m_totalchannel = m_playerinfo.total_channel ;
		}
		if( m_totalchannel>0 ) {
			initchannel();
			res=1 ;
		}
		delete filestream ;
	}
	else if( fileflag == 0x484b4834 ) {					// .264 file
		m_dvrtype = DVR_DEVICE_TYPE_264FILE ;
		m_opentype = PLY_PLAYBACK ;
		strncpy( m_servername, dvrfilename, sizeof(m_servername) );
		
		m_totalchannel=0 ;

		h264filestream * filestream = new h264filestream(m_servername, 0) ;

		m_playerinfo.size = sizeof(m_playerinfo) ;
		if( filestream->getserverinfo( &m_playerinfo ) ) {
			m_playerinfo.features = PLYFEATURE_PLAYBACK | PLYFEATURE_HARDDRIVE | PLYFEATURE_CAPTURE  ;
			m_totalchannel = m_playerinfo.total_channel ;
		}
		if( m_totalchannel>0 ) {
			initchannel();

			res=1 ;
		}
		delete filestream ;
	}
	return res ;
}

// follow functions are according to DVRClient DLL interface

int dvrplayer::getplayerinfo( struct player_info * pplayerinfo )
{
	if( pplayerinfo->size>=sizeof(struct player_info) ) {
		*pplayerinfo = m_playerinfo ;
		return 0;
	}
	return DVR_ERROR ;
}

int dvrplayer::setuserpassword( char * username, void * password, int passwordsize )
{
	return DVR_ERROR ;
}

int dvrplayer::setvideokey( void * key, int keysize )
{
	int res = DVR_ERROR ;

	memset( m_password, 0, sizeof( m_password ));
	if( keysize>256 ) {
		keysize=256 ;
	}
	memcpy( m_password, key, keysize);

	HCRYPTHASH hHash ;
	if( m_hCryptProv == NULL ) {
		return DVR_ERROR ;
	}
	CryptCreateHash(
		m_hCryptProv, 
		CALG_SHA, 
		0, 
		0, 
		&hHash) ;
	CryptHashData(
		hHash, 
		(PBYTE)m_password,
		32,
		0);
	if( CryptDeriveKey(
		m_hCryptProv,
		CALG_RC4,
		hHash,
		0,
		&m_hVideoKey)==0 ) {		// failed
		m_hVideoKey=NULL ;
	}
	CryptDestroyHash(hHash);

	int i;
	struct channel_t * pch ;
	for(i=0; i<m_totalchannel; i++) {
		pch = &m_channellist[i];
		if( pch->stream ) {
			pch->stream->setpassword(ENC_MODE_RC4FRAME, m_password, sizeof(m_password) );			// set frameRC4 password
			pch->stream->setpassword(ENC_MODE_WIN, &m_hVideoKey, sizeof(m_hVideoKey) ) ;		// set winRC4 password
		}
	}

	return 0 ;
}

int dvrplayer::getchannelinfo( int channel, struct channel_info * pchannelinfo )
{
	int res = DVR_ERROR ;
	int i;
//	lock();
	if( channel < m_totalchannel && pchannelinfo->size>=sizeof(struct channel_info) ) {
		struct channel_t * pch ;
		pch = &m_channellist[channel] ;
		if( pch->channelinfo.size != sizeof(struct channel_info) ) {		// ok, we read all channel info now
			if( m_dvrtype == DVR_DEVICE_TYPE_DVR ) {						// connect to a DVR server?
				struct channel_info * pci = new channel_info [m_totalchannel] ;
				memset( pci,0,m_totalchannel* sizeof(channel_info));
				for( i=0; i<m_totalchannel; i++ ){
					pci[i].size = sizeof(struct channel_info ) ;
				}
				net_channelinfo( m_socket, pci, m_totalchannel );
				for( i=0; i<m_totalchannel; i++) {
					if( (m_playerinfo.features & PLYFEATURE_PTZ)==0 ) {
						pci[i].features &= ~2 ;
					}
					m_channellist[i].channelinfo=pci[i];
				}
				delete [] pci ;
			}
			else if( m_dvrtype == DVR_DEVICE_TYPE_HARDDRIVE ) {				// connect to hard drive
				for( i=0; i<m_totalchannel; i++) {
					pch=&m_channellist[i];
					memset( &(pch->channelinfo), 0, sizeof( pch->channelinfo ) );
					pch->channelinfo.size = sizeof( pch->channelinfo ) ;
					pch->channelinfo.features = 1 ;
					sprintf(pch->channelinfo.cameraname, "camera%d", i+1 );
				}
			}
			else if( m_dvrtype == DVR_DEVICE_TYPE_DVRFILE || 
					 m_dvrtype == DVR_DEVICE_TYPE_264FILE ) {				// connect to hard drive
				for( i=0; i<m_totalchannel; i++) {
					pch=&m_channellist[i];
					memset( &(pch->channelinfo), 0, sizeof( pch->channelinfo ) );
					pch->channelinfo.size = sizeof( pch->channelinfo ) ;
					pch->channelinfo.features = 1 ;
					dvrstream * stream = newstream(i);
					if( stream ) {
						stream->getchannelinfo(&pch->channelinfo);
						delete stream ;
					}
				}
			}
		}
        else {
   			if( m_dvrtype == DVR_DEVICE_TYPE_DVR ) {						// connect to a DVR server?
                int recvsize;
                struct channelstatus * cs = new channelstatus  [m_totalchannel]  ;
                memset(cs, 0, m_totalchannel*sizeof(struct channelstatus) );
                recvsize=net_DVRGetChannelState(m_socket, cs, m_totalchannel*sizeof(struct channelstatus) );
                if( recvsize>0 ) {
				    for( i=0; i<m_totalchannel; i++) {
					    pch=&m_channellist[i];
                        pch->channelinfo.status = 0 ;
                        if( cs[i].mot ) {
                            pch->channelinfo.status|= 2 ;
                        }
                        if( cs[i].rec ) {
                            pch->channelinfo.status|= 4 ;
                        }
                        if( cs[i].sig ) {                   // signal lost
                            pch->channelinfo.status|= 1 ;
                        }
				    }
                }
				delete [] cs ;
			}
        }

		pch = &m_channellist[channel];
		if( pch->channelinfo.size == sizeof(struct channel_info) ) {		
			*pchannelinfo = pch->channelinfo ;
			res = 0 ;
		}
	}
//	unlock();
	return res ;
}

int dvrplayer::attach( int channel, HWND hwnd )
{
	decoder * target ;

	if( channel<0 || channel>=m_totalchannel )
		return DVR_ERROR ;

	struct channel_t * pch ;
	pch = &m_channellist[channel];

    suspendplayerthread(channel);

	// creat a new decoder
	target = new HikPlayer( hwnd, 1 );

	// attach decoder to target list
	target->m_next = pch->target ;

	pch->target = target ;
	// play new target base on current play mode
	if( m_play_stat == PLAY_STAT_PLAY ) {
		target->play();
	}
	else if( m_play_stat >= PLAY_STAT_FAST && m_play_stat <= PLAY_STAT_FASTEST ) {
		target->play();
		target->fast(m_play_stat-PLAY_STAT_FAST+1);
	}
	else if( m_play_stat <= PLAY_STAT_SLOW && m_play_stat >= PLAY_STAT_SLOWEST ) {
		target->play();
		target->slow(PLAY_STAT_SLOW-m_play_stat+1);
	}

	if( pch->stream == NULL ) {				// input stream not been setup
		// create a new stream for this channel
		pch->stream = newstream( channel ) ;

		if( pch->stream ) {
			if( pch->stream->seek( &m_playtime )==DVR_ERROR_FILEPASSWORD ) {
				// set stream password
				pch->stream->setpassword(ENC_MODE_RC4FRAME, m_password, sizeof(m_password) );			// set frameRC4 password
				pch->stream->setpassword(ENC_MODE_WIN, &m_hVideoKey, sizeof(m_hVideoKey) ) ;		// set winRC4 password
				if( pch->stream->seek( &m_playtime )==DVR_ERROR_FILEPASSWORD ) {
					m_errorstate=1 ;
				}
			}
			pch->syncstate=PLAY_SYNC_START ;
		}
	}

    resumeplayerthread(channel);

    return 0 ;												// return OK
}

// detach all channels, suspend all player threads
int dvrplayer::detach( )
{
	decoder * dec ;		// temperary decoder pointer
	int i;
	struct channel_t * pch ;

	if( m_totalchannel>0 ) {

        for(i=0; i<m_totalchannel;i++) {
            pch = &m_channellist[i];
            if( pch->stream ) {
                // shutdown network if possible
                pch->stream->shutdown();
            }
            suspendplayerthread(i);

            // remove all decoder
			while( pch->target!=NULL ) {
				dec = pch->target ;
				pch->target = dec->m_next ;
				delete dec ;
			}
			// delete stream
			if( pch->stream ) {
				delete pch->stream ;
				pch->stream = NULL ;
			}
		}
	}
	return 0 ;
}

int dvrplayer::detachwindow( HWND hwnd )
{
	decoder * dec1, * dec2 ;		// temperary decoder pointer
	int i;
	struct channel_t * pch ;

    int channel=-1 ;
	if( m_totalchannel>0 ) {

        // lock up for what channel attached this window
		for(i=0; i<m_totalchannel&&channel==-1;i++) {
			pch=&m_channellist[i];
            dec1=pch->target ;
            while( dec1 ) {
                if( dec1->window() == hwnd ) {      // found this window
                    channel=i ;
                    break ;
                }
                dec1=dec1->m_next ;
            }
        }
        if( channel<0 ) {
            return DVR_ERROR ;                      // could not find this window attached
        }

        // suspend player thread
        suspendplayerthread(channel);

        // now it is safe to do the works

        pch=&m_channellist[channel];
        // remove this decoder target
        if( pch->target->window() == hwnd ) {
            dec1=pch->target ;
            pch->target = dec1->m_next ;
            delete dec1 ;
        }
        else {
            for( dec1=pch->target; dec1->m_next; dec1=dec1->m_next ) {
                if( dec1->m_next->window()==hwnd ) {
                    dec2=dec1->m_next ;
                    dec1->m_next = dec2->m_next ;
                    delete dec2 ;
                    break;
                }
            }
        }

        // if all target removed, stop player thread
        if( pch->target == NULL ) {
            if( pch->stream ) {
                delete pch->stream ;
                pch->stream=NULL ;
            }
        }
        else {
            resumeplayerthread(channel);
        }
		return 0;									// return OK
	}
	return DVR_ERROR ;
}


int dvrplayer::play()
{
	int i;
	int res= DVR_ERROR ;
	decoder * dec ;
	struct channel_t * pch ;

	if( m_errorstate==1 ) {
		struct dvrtime t = m_playtime ;
		if( seek( &t )==DVR_ERROR_FILEPASSWORD ) {
			return DVR_ERROR_FILEPASSWORD ;
		}
	}
	m_errorstate=0;					// clear password error

	if( m_play_stat!=PLAY_STAT_PLAY ) {
		for(i=0; i<m_totalchannel; i++) {
			pch=&m_channellist[i];
			dec = pch->target ;
			while( dec!=NULL ) {
				dec->play();
				dec=dec->m_next ;
			}
			pch->syncstate = PLAY_SYNC_START ;
			res=0;
		}
		m_play_stat=PLAY_STAT_PLAY ;
	}
	return res ;
}

int dvrplayer::audioon( int channel, int audioon)
{
	decoder * dec ;
	if( channel<0 || channel>=m_totalchannel ) 
		return DVR_ERROR ;
	dec = m_channellist[channel].target ;
	while( dec != NULL ) {
		dec->audioon(audioon);
		dec=dec->m_next ;
	}
	return 0;				// return OK
}

int dvrplayer::audioonwindow( HWND hwnd, int audioon)
{
	int channel ;
	decoder * dec ;
	for( channel=0; channel<m_totalchannel; channel++ ) {
		dec = m_channellist[channel].target ;
		while( dec != NULL ) {
			if( dec->window() == hwnd ) {
				dec->audioon(audioon);
			}
			dec=dec->m_next ;
		}
	}
	return 0;				// return OK
}
	
int dvrplayer::pause()
{
	m_play_stat=PLAY_STAT_PAUSE ;
	int i;
	int res= DVR_ERROR ;
	decoder * dec ;

	for(i=0; i<m_totalchannel; i++) {
        dec = m_channellist[i].target ;
		while( dec!=NULL ) {
			dec->pause();
			dec=dec->m_next ;
		}
		res=0;
	}
	m_play_stat=PLAY_STAT_PAUSE ;

	return res ;
}

int dvrplayer::stop()
{
	int i;
	int res= DVR_ERROR ;
	decoder * dec ;

	for(i=0; i<m_totalchannel; i++) {
        dec = m_channellist[i].target ;
		while( dec!=NULL ) {
			dec->stop();
			dec=dec->m_next ;
		}
		res=0;
	}
	m_play_stat=PLAY_STAT_STOP ;

	return res ;
}

int dvrplayer::fastforward( int speed )
{
	int i;
	int res= DVR_ERROR ;
	decoder * dec ;

	for(i=0; i<m_totalchannel; i++) {
        dec = m_channellist[i].target ;
		while( dec!=NULL ) {
			dec->fast(speed);
			dec=dec->m_next ;
		}
		res=0;
	}
	m_play_stat=PLAY_STAT_FAST ;

	return res ;
}

int dvrplayer::slowforward( int speed )
{
	int i;
	int res= DVR_ERROR ;
	decoder * dec ;
	struct channel_t * pch ;

	for(i=0; i<m_totalchannel; i++) {
        dec = m_channellist[i].target ;
		while( dec!=NULL ) {
			dec->slow(speed);
			dec=dec->m_next ;
		}
		res=0;
	}
	m_play_stat=PLAY_STAT_SLOW ;

	return res ;
}

int dvrplayer::getdecframes()
{
	decoder * dec ;
	int i ;
	int decframes=0 ;

	for(i=0; i<m_totalchannel; i++) {
        dec = m_channellist[i].target ;
		while( dec!=NULL ) {
			decframes+=dec->getdecframes();
			dec=dec->m_next ;
		}
	}

	return decframes ;
}

int dvrplayer::oneframeforward()
{
	int i;
	int res= DVR_ERROR ;
	decoder * dec ;
	int decframes, decmaxretry ;
	decframes=getdecframes() ;

    for(i=0; i<m_totalchannel; i++) {
        dec = m_channellist[i].target ;
		while( dec!=NULL ) {
			dec->oneframeforward();
			dec=dec->m_next ;
		}
		res=0;
	}
	m_play_stat=PLAY_STAT_PAUSE ;

	// wait until at lease on frame been decoded. To support TVS JPEG exports
	decmaxretry = 100 ;
	while( --decmaxretry>0 ) {
		if( getdecframes()!=decframes ) {
			return 1 ;
		}
		Sleep(10);
	}
	return res ;
}

int dvrplayer::backward( int speed )
{
	return DVR_ERROR ;
}

int dvrplayer::capture( int channel, char * capturefilename )
{
	int res=DVR_ERROR ;
	struct channel_t * pch ;
	pch=&m_channellist[channel];
	if( pch->target ) {
		if( pch->target->capturesnapshot(capturefilename)) {
			res=0;
		}
	}
	return res ;
}

int dvrplayer::seek( struct dvrtime * seekto )
{
	int res = 0 ;
	int i;
	decoder * dec ;
	struct channel_t * pch ;
	for(i=0; i<m_totalchannel; i++) {
        suspendplayerthread(i);
		pch = &m_channellist[i];
		if( pch->stream ) {
			pch->syncstate=PLAY_SYNC_START ;		// start sync
		}
		dec = pch->target ;
		while(dec) {
			dec->resetbuffer();
			dec=dec->m_next ;
		}
	}
	for(i=0; i<m_totalchannel; i++) {
		pch = &m_channellist[i];
		if( pch->stream ) {
			res = pch->stream->seek( seekto ) ;			// ready for next frame
			if( res == DVR_ERROR_FILEPASSWORD ) {
				m_errorstate=1 ;					// password error
				break;
			}
		}
	}
	m_playtime=*seekto ;
	m_syncclock=timeGetTime();
	m_synctime=m_playtime ;
	for(i=0; i<m_totalchannel; i++) {
        resumeplayerthread(i);
    }
    if( m_play_stat == PLAY_STAT_PAUSE ) {
		Sleep(10);
		oneframeforward();
	}
	return res ;
}

int dvrplayer::getcurrenttime( struct dvrtime * t )
{
	if( m_dvrtype == DVR_DEVICE_TYPE_DVR && m_opentype == PLY_PREVIEW ) {	// preview a DVR server
		if( net_getdvrtime(m_socket, t)==0 ) {
			* t = m_playtime ;
		}
	}
	else {
		* t = m_playtime ;
	}
	return 0 ;
}

int dvrplayer::getcliptimeinfo( int channel, struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize)
{
	struct channel_t * pch ;
	int res = DVR_ERROR ;
	if( channel<m_totalchannel ) {
		pch = &m_channellist[channel];
		if( pch->stream ) {
			res= pch->stream->gettimeinfo(begintime, endtime, tinfo, tinfosize);
		}
	}
	return res ;
}

int dvrplayer::getclipdayinfo( int channel, dvrtime * daytime )
{
	int res=0 ;
	struct channel_t * pch ;
	if( channel<m_totalchannel ) {
		pch = &m_channellist[channel];
		if( pch->stream ) {
			res = pch->stream->getdayinfo( daytime )>0 ;
		}
	}
	return res ;
}

int dvrplayer::getlockfiletimeinfo( int channel, struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize)
{
	struct channel_t * pch ;
	int res = DVR_ERROR ;
	if( channel<m_totalchannel ) {
		pch = &m_channellist[channel];
		if( pch->stream ) {
			res= pch->stream->getlockfileinfo(begintime, endtime, tinfo, tinfosize);
		}
	}
	return res ;
}
#ifndef USE_VIDEOCLIP
// this struct is used to transfer parameter to dvr saving thread
struct dvrfile_save_info {
	dvrplayer * player ;
	struct dvrtime * begintime ;
	struct dvrtime * endtime ;
	char * duppath ;
	int    flags ;
	struct dup_state * dupstate ;
	char * password ;
	HANDLE hRunEvent ;
};

// dvr file saving thread
static void _savedvrfilethread(void *p)
{
	struct dvrfile_save_info * dsi=(struct dvrfile_save_info *)p ;
	dsi->player->savedvrfilethread(dsi->begintime, 
									dsi->endtime, 
									dsi->duppath, 
									dsi->flags, 
									dsi->dupstate, 
									dsi->password, 
									dsi->hRunEvent );
 }
#endif
 // back ground thread to save a .DVR file
void dvrplayer::savedvrfilethread( struct dvrtime * begintime, struct dvrtime * endtime, char * duppath, int flags, struct dup_state * dupstate, char * password, HANDLE hthreadstart )
{
	struct DVRFILEHEADER fileheader ;
	struct DVRFILECHANNEL * pchannel ;
	struct DVRFILEHEADEREX fileheaderex ;
	struct DVRFILECHANNELEX * pchannelex ;
	struct DVRFILEKEYENTRY * pkeyentry ;

	int ch ;
	struct dvrtime begint, endt ;
	FILE * dvrfile ;
	int progress_chmax, progress_chmin ;		// maximum/minimum progress on a channel
	int tdiff;									// total time lenght 
	int chdiff ;								
	char * framedata ;							// frame data
	int    framesize, frametype ;
	struct dvrtime frametime ;
	struct hd_frame * pframe ;
	int    fsize ;								// frame size

	int enc ;
	unsigned char k256[256];
	unsigned char crypt_table[4096] ;

	if( password ) {
		enc=1 ;
		key_256( password, k256);
		RC4_crypt_table( crypt_table, 4096, k256 );
	}
	else {
		enc=0;
	}

	list <struct DVRFILEKEYENTRY> keyindex ;	// key frame index

	begint=*begintime ;
	endt  =*endtime ;

	if( m_playerinfo.total_channel<=0 ) {		// total channel is 0, so we do nothing
		SetEvent(hthreadstart);					// let calling function go	if( dvrfile==NULL ) {
		return ;
	}

	dvrfile = fopen(duppath, "wb");
	SetEvent(hthreadstart);						// let calling function go	if( dvrfile==NULL ) {
	if( dvrfile==NULL ) {
		dupstate->status=-1 ;		// error on file creation
		return ;
	}

	// pause player
	if( m_dvrtype == DVR_DEVICE_TYPE_DVR ){
		m_errorstate = 2 ;							// pause player
	}

	// set file header
	fileheader.flag = DVRFILEFLAG ;
	fileheader.totalchannels = m_playerinfo.total_channel ;
	
	fileheader.begintime = (DWORD) dvrtime_mktime( &begint);
	fileheader.endtime = (DWORD) dvrtime_mktime( &endt ) ;

	// set file extra header
	fileheaderex.size = sizeof( fileheaderex );
	fileheaderex.flagex = DVRFILEFLAGEX ;
	fileheaderex.platform_type = 0 ;
	fileheaderex.version[0] = m_playerinfo.serverversion[0] ;
	fileheaderex.version[1] = m_playerinfo.serverversion[1] ;
	fileheaderex.version[2] = m_playerinfo.serverversion[2] ;
	fileheaderex.version[3] = m_playerinfo.serverversion[3] ;
	if( enc ) {
		fileheaderex.encryption_type = ENC_MODE_RC4FRAME ;
	}
	else {
		fileheaderex.encryption_type = 0 ;
	}
	fileheaderex.totalchannels = m_playerinfo.total_channel ;
	fileheaderex.date = begintime->year * 10000 + begintime->month*100 + begintime->day ;
	fileheaderex.time = begintime->hour * 3600 + begintime->min * 60 + begintime->second ;
	fileheaderex.length = dvrtime_diff( begintime, endtime );
	strncpy( fileheaderex.servername, m_playerinfo.servername, sizeof(fileheaderex.servername) );

	pchannel = new struct DVRFILECHANNEL [fileheader.totalchannels] ;
	pchannelex = new struct DVRFILECHANNELEX [fileheaderex.totalchannels] ;

	memset(pchannel, 0, fileheader.totalchannels * sizeof(struct DVRFILECHANNEL) );
	memset(pchannelex, 0, fileheaderex.totalchannels * sizeof(struct DVRFILECHANNELEX) );

	dupstate->percent=1 ;										// preset to 1%

	tdiff=dvrtime_diff(&begint, &endt);							// total time length

	// goto first channel data position
	fseek(dvrfile, 
		  sizeof(fileheader) + 
			fileheader.totalchannels * sizeof(struct DVRFILECHANNEL) + 
			sizeof(fileheaderex) + 
			fileheaderex.totalchannels * sizeof(struct DVRFILECHANNELEX),
		  SEEK_SET );

	for( ch=0; ch<fileheader.totalchannels; ch++) {

		// initial channel header
		strncpy( pchannelex[ch].cameraname, this->m_channellist[ch].channelinfo.cameraname, 128 );
		pchannel[ch].end = pchannel[ch].begin = 
			pchannelex[ch].begin = pchannelex[ch].end = ftell(dvrfile) ;
		pchannelex[ch].size = sizeof( pchannelex[ch] ) ;
		pchannelex[ch].keybegin=pchannelex[ch].keyend = 0 ;

		// clean key index list
		keyindex.clean();

		// setup progress informaion
		progress_chmin= 100*ch/fileheader.totalchannels+1 ;
		progress_chmax= 100*(ch+1)/fileheader.totalchannels ;

		dupstate->percent = progress_chmin ;

		// create a file copy stream
		dvrstream * stream = newstream(ch);

		if( stream ) {
			// write stream header
			memcpy( k256, Head264, 40);
			if( enc ) {			
				// encrypt 264 header
				RC4_block_crypt( k256, 40, 0, crypt_table, 4096);
			}
			fwrite( k256, 1, 40, dvrfile);

			// seek to begin position
			if( stream->seek( &begint )==DVR_ERROR_FILEPASSWORD ) {
				// set stream password
				stream->setpassword(ENC_MODE_RC4FRAME, m_password, sizeof(m_password) );	// set frameRC4 password
				stream->setpassword(ENC_MODE_WIN, &m_hVideoKey, sizeof(m_hVideoKey) ) ;		// set winRC4 password
				stream->seek( &begint );													// seek again
			}

			framedata=NULL ;
			while( stream->getdata( &framedata, &framesize, &frametype ) ) {
				stream->gettime( &frametime ) ;
				chdiff = dvrtime_diff(&begint, &frametime) ;

				if(chdiff>=tdiff) {				// up to totol length in time
					delete framedata ;
					break;
				}

				if( enc ) {
					// encrypt frame data
					pframe =(struct hd_frame *)framedata ;
					fsize = pframe->framesize ;
					if( fsize>1024 ) {
						fsize=1024 ;
					}
					RC4_block_crypt( (unsigned char *)framedata+sizeof(struct hd_frame), 
									 fsize,
									 0, crypt_table, 4096);
				}
				// write frame data to file
				fwrite( framedata, 1, framesize, dvrfile);
				// release frame data
				delete framedata ;
				framedata=NULL ;

				// check key frame
				if( frametype==FRAME_TYPE_KEYVIDEO ) {
					pkeyentry=keyindex.append();
					pkeyentry->offset = ftell(dvrfile)-pchannelex[ch].begin;
					pkeyentry->time = chdiff ;
				}

				if( dupstate->update ) {
					dupstate->update=0;
					dupstate->percent = progress_chmin + chdiff*100/tdiff/fileheader.totalchannels ;
					sprintf(dupstate->msg, "Channel %d - %02d:%02d:%02d", 
						ch, 
						frametime.hour, 
						frametime.min, 
						frametime.second );
				}

				if( dupstate->cancel || dupstate->status<0 ) {		// cancelled or error?
						break;
				}
			}
			// delete file copy stream ;
			delete stream ;
		}
		
		// channel end position
		pchannel[ch].end = pchannelex[ch].end = ftell(dvrfile) ;

		// write key index
		if( keyindex.size()>0 ) {
			pchannelex[ch].keybegin = ftell(dvrfile) ;
			fwrite( keyindex.at(0), sizeof( struct DVRFILEKEYENTRY ), keyindex.size(), dvrfile );
			pchannelex[ch].keyend = ftell(dvrfile);
		}

		if( dupstate->cancel || dupstate->status<0 ) {		// cancelled or error?
			break;
		}
	}


	// write completed file header
	fseek( dvrfile, 0, SEEK_SET );
	fwrite( &fileheader, sizeof(fileheader), 1, dvrfile);			// write file header
	fwrite( pchannel, sizeof( struct DVRFILECHANNEL ), fileheader.totalchannels, dvrfile );		// old channel header
	fwrite( &fileheaderex, sizeof(fileheaderex), 1, dvrfile);		// extra file header
	fwrite( pchannelex, sizeof( struct DVRFILECHANNELEX), fileheader.totalchannels, dvrfile );	// extra channel header
	fclose(dvrfile);

	// release memory for channel header
	delete pchannel ;
	delete pchannelex ;

	dupstate->msg[0]='\0';								// empty progress message

	if( dupstate->cancel || dupstate->status<0 ) {		// cancelled or error?
		remove(duppath);								// remove the file
		if( dupstate->status==0 )
			dupstate->status=-1 ;						// Error!
	}
	else {
		dupstate->percent=100 ;		// 100% finish
		dupstate->status=1 ;		// success
	}

	// resume player
	if( m_dvrtype == DVR_DEVICE_TYPE_DVR ){
		m_errorstate = 0 ;						
	}

}

// save a .DVR file
int dvrplayer::savedvrfile( struct dvrtime * begintime, struct dvrtime * endtime, char * duppath, int flags, struct dup_state * dupstate)
{
	struct dvrfile_save_info sdfinfo;
	if( !(flags & DUP_ALLFILE) )		// support all channels record for now
		return DVR_ERROR ;

	char password[256] ;
	sdfinfo.password = NULL ;			// No password by default

	// ask for password encryption?

	CEnterDVRPassword EnterPassword ;
	while( EnterPassword.DoModal()==IDOK ) {
		if( EnterPassword.m_password1 != EnterPassword.m_password2 ) {
			AfxMessageBox(_T("Passwords not match, please try again!"));
			continue;
		}
		else {
			LPCTSTR pass = (LPCTSTR)EnterPassword.m_password1;
			int i;
			for( i=0; i<255; i++ ) {
				if( pass[i]==0 ) {
					break;
				}
				password[i] = (char)pass[i] ;
			}
			password[i]=0;
			if( password[0] ) {
				sdfinfo.password = password ;
			}
			break;
		}
	}

	// start file save thread ;
	sdfinfo.player=this ;
	sdfinfo.begintime = begintime;
	sdfinfo.endtime = endtime ;
	sdfinfo.duppath = duppath ;
	sdfinfo.flags = flags ;
	sdfinfo.dupstate = dupstate ;
	sdfinfo.hRunEvent = CreateEvent( NULL, TRUE, FALSE, NULL);
#ifdef USE_VIDEOCLIP
	HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, dvrfile_save_thread, &sdfinfo, 0, NULL);
	if (hThread == 0) {
			CloseHandle(sdfinfo.hRunEvent);
			return DVR_ERROR ;
	}
#else
	if( _beginthread( _savedvrfilethread, 0, &sdfinfo )
		== -1 ) {
		CloseHandle(sdfinfo.hRunEvent);
		return DVR_ERROR ;
	}
#endif
	WaitForSingleObject( sdfinfo.hRunEvent, 10000 );			// wait until dvrfile thread running
	CloseHandle(sdfinfo.hRunEvent);

	if( !(flags&DUP_BACKGROUND) ) {		// wait until saving thread finish?
		while( dupstate->status == 0 ) {
			Sleep(1000);
		}
		if( dupstate->status<0 )
			return dupstate->status ;
		else
			return 0 ;
	}
	else
		return 0;						// let background thread running
}

int dvrplayer::dupvideo( struct dvrtime * begintime, struct dvrtime * endtime, char * duppath, int flags, struct dup_state * dupstate)
{
	return DVR_ERROR ;
}

// return
//       0: success
//       1: success and need to close connection
//       DVR_ERROR: error
int dvrplayer::configureDVR( )
{
	char url[200] ;
	TCHAR TURL[200] ;
	SOCKET cfgsocket ;
	char pagebuf[200] ;

	cfgsocket = net_connect( m_servername );
	if( cfgsocket==0 ) {
		return DVR_ERROR ;
	}

	if( m_keydata ) {
		if( net_CheckKey(cfgsocket, (char *)m_keydata, m_keydatasize)==0 ){
			net_close(cfgsocket);
			return DVR_ERROR ;
		}
	}

	if( net_GetSetupPage(cfgsocket, pagebuf, sizeof( pagebuf ) ) ) {
		sprintf(url, "http://%s%s", m_servername, pagebuf);
		MultiByteToWideChar(CP_UTF8, 0, url, -1, TURL, 200);
		ShellExecute(NULL, _T("open"), TURL, NULL, NULL, SW_SHOWNORMAL );
		net_close(cfgsocket);
		return 1 ;
	}

	net_close(cfgsocket);

	// fall back to old method
	extern int DvrConfigure( char * servername ) ;
	return DvrConfigure(m_servername);
}

int dvrplayer::senddvrdata( int protocol, void * sendbuf, int sendsize)
{
	if( protocol==PROTOCOL_KEYDATA ) {
		if( m_keydata!=NULL ) {
			delete (char *) m_keydata ;
			m_keydata=NULL ;
			m_keydatasize=0 ;
		}
		if( sendsize > sizeof(struct key_data) && 
			sendsize < 8000 ) {
			m_keydatasize = sendsize ;
			m_keydata = (struct key_data *)new char [m_keydatasize] ;
			memcpy( m_keydata, sendbuf, sendsize ) ;
			if( m_socket ) {
				net_CheckKey( m_socket, (char *) m_keydata, sendsize );
			};
			return 0 ;
		}
	}
	return DVR_ERROR ;
}

int dvrplayer::recvdvrdata( int protocol, void ** precvbuf, int * precvsize)
{
/*
	int loglen ;
	if( protocol==PROTOCOL_KEYLOG ) {
		if( m_socket ) {
			char * keylog=NULL;
			loglen=0 ;
			if( net_GetKeyLog(m_socket, &keylog, &loglen )) {
				*precvbuf = (void *)keylog ;
				*precvsize = loglen ;
				return 0 ;
			}
		}
	}
*/
	return DVR_ERROR ;
}

int dvrplayer::freedvrdata(void * dvrbuf)
{
	if( dvrbuf!=NULL ) {
		free( dvrbuf );
	}
	return 0 ;
}

int dvrplayer::pwkeypad(int keycode, int press)
{
	if( m_dvrtype == DVR_DEVICE_TYPE_DVR && m_socket ){	// if connect to a DVR server
		return net_sendpwkeypad( m_socket, keycode, press );
	}
	return 0 ;
}