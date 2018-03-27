// dvrplayer.cpp : DVR player object
//

#include "stdafx.h"
#include <process.h>
#include <crtdbg.h>
#include "ply614.h"
#include "dvrnet.h"
#include "dvrpreviewstream.h"
#include "dvrharddrivestream.h"
#include "dvrplaybackstream.h"
#include "dvrfilestream.h"
#include "h264filestream.h"
#include "EnterDVRPassword.h"
#include "mtime.h"
#include "utils.h"
#include "videoclip.h"

#define DELAYED_SEEK
#define PLAYER_TIME_SLICE	(5)
#define PLAYER_TIMEOUT_REFRESH (3000000LL)
#define NEW_CHANNEL
#define PLAY_TOLERANCE 500 /* in millisec (was 100) */

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
	return t2->millisecond - t1->millisecond + 1000 * (int)(tt2 - tt1) ;
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
		if( stream != NULL ) {
			stream->setkeydata( m_keydata ) ;
		}
	}
	else if( m_dvrtype == DVR_DEVICE_TYPE_HARDDRIVE && m_opentype == PLY_PLAYBACK  ) {	// playback hard drive.
		stream = new dvrharddrivestream( this->m_servername, channel ) ;				
		if( stream != NULL ) {
			stream->setkeydata( m_keydata ) ;
		}
	}
	else if( m_dvrtype == DVR_DEVICE_TYPE_LOCALSERVER && m_opentype == PLY_PLAYBACK  ) {	// playback hard drive.
		stream = new dvrharddrivestream( this->m_servername, channel, 1 ) ;				// open local server
		if( stream != NULL ) {
			stream->setkeydata( m_keydata ) ;
		}
	}
	else if( m_dvrtype == DVR_DEVICE_TYPE_DVR && m_opentype == PLY_PLAYBACK ) {	// playback DVR server.
		dvrplaybackstream * pbstream = new dvrplaybackstream(this->m_servername, channel, this->m_playerinfo.serverversion);
		if( pbstream != NULL ) {
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
		if( stream != NULL ) {
			stream->setkeydata( m_keydata ) ;
		}
	}
		
	return stream ;
}

int dvrplayer::channelCanPlay(int ch, bool& hide_screen)
{
	int i;
	struct channel_t * pch ;
	struct dvrtime t, dt, cht;
	int firstchannel = -1;
	bool thisChannelTimeFound = false;
	int startedChannels = 0;

	//TRACE(_T("ch %d,channelCanPlay\n"), ch);
	dvrtime_init(&dt, 2100);
	dvrtime_init(&cht, 1980);
	hide_screen = true;

	lock();
	if (ch >= m_totalchannel) {
		unlock();
		return 0;
	}

	pch = m_channellist.at(ch);

	/* find the 1st channel */
	for(i=0; i<m_totalchannel; i++) {
		pch = m_channellist.at(i);
		if( pch->stream!=NULL ) {
			if( pch->syncstate == PLAY_SYNC_RUN ) {
				pch->stream->gettime( &t );
				if( dvrtime_compare( &t, &dt )<0 ) {
					firstchannel = i ;
					dt = t ;
				}
				if (ch == i) {
					thisChannelTimeFound = true;
					cht = t;
				}
#if 1
			} else if (pch->syncstate == PLAY_SYNC_START) {
				startedChannels++; // other channels
#endif
			}
		}
	}
	unlock();

	if (firstchannel == ch) {
#if 1
		if (startedChannels) {
			TRACE(_T("ch %d,1st but startedChannels:%d\n"), ch,startedChannels);
			return 0;
		}
#endif
		//TRACE(_T("ch %d can go 1\n"), ch);
		hide_screen = false;
		return 1;
	}

#if 0
	int diff = 500; // for normal playback
	int ps = m_play_stat;

	if (ps > PLAY_STAT_FAST) {
		diff = (ps - PLAY_STAT_PLAY) * (30 * 1000);
	} else if (ps == PLAY_STAT_FAST) {
		diff = 15 * 1000;
	}
#endif
	if( thisChannelTimeFound ) {
		int diffms = dvrtime_diffms(&cht, &dt);
		if ((diffms > -PLAY_TOLERANCE) && (diffms < PLAY_TOLERANCE)) { 
			//TRACE(_T("ch %d can go 3\n"), ch);
			hide_screen = false;
			return 1;
		}

		if ((diffms > -(PLAY_TOLERANCE * 5)) && (diffms < (PLAY_TOLERANCE * 5))) {
			hide_screen = false;
		}
	}

	//TRACE(_T("ch %d cannot go, fc:%d,%d, %02d:%02d:%02d.%03d, %02d:%02d:%02d.%03d\n"),ch,firstchannel,diff,dt.hour,dt.min,dt.second,dt.millisecond,cht.hour,cht.min,cht.second,cht.millisecond);
	return 0 ;
}

DWORD timeGetTime2()
{
	return (DWORD)(mdate() / 1000);
}

static void showWindow(struct channel_t * pch, int nCmd, int channel)
{
	decoder * dec ;

	if (pch) {
		dec = pch->target ;
		while( dec ) {
			//TRACE(_T("ch%d refresh:%d\n"), channel, nCmd);
			dec->refresh(nCmd);
			dec=dec->m_next ;
		}
	}
}

// player thread (synchronizer)
void dvrplayer::playerthread(int channel)
{
	struct channel_t * pch ;
	char * framedata = NULL ;
	int framesize ;
	int frametype ;
	struct dvrtime frametime ;
	int dataok ;
	int timesync ;
	int i;
	bool bSleepDone;
	int state;
	__int64 start, end, dd, max=0;
	bool bShow = false;
	bool bStepPlay = false;
	bool bReplay = false;
	bool hideScreen;
	mtime_t hide_time = 0LL;

	// each channel has its own thread.
	lock();
	pch=m_channellist.at(channel);
	pch->player_thread_state=PLY_THREAD_RUN ;			// thread running
	unlock();

	while( pch->player_thread_state==PLY_THREAD_RUN ) {	// keep running if not required to quit
		bSleepDone = false;
					//TRACE(_T("ch%d --------\n"),channel);

		if( pch->stream && m_play_stat > PLAY_STAT_STOP) {			
			if( m_errorstate!=0) {
				if (m_errorstate!=2)
					_RPT2(_CRT_WARN, "ch %d error\n",channel,m_errorstate);
				Sleep(10);
				continue ;
			}
			if (!bReplay) {
				//_RPT1(_CRT_WARN, "ch:%d,getdata\n",channel);
				lock();
				dataok=pch->stream->getdata(&framedata, &framesize, &frametype, NULL);
				//TRACE(_T("ch:%d,getdata result:%d\n"),channel,dataok ? framesize : 0);

				if( dataok == 0 ) {
					_RPT1(_CRT_WARN, "ch:%d,PLAY_SYNC_NODATA\n",channel);
					framedata = NULL;
					pch->syncstate=PLAY_SYNC_NODATA ;
					unlock();
					//hide video (show screen during .DVR file creation)
					if (bShow && (m_errorstate != 2)) {
						if (hide_time == 0LL) {
							hide_time = mdate() + PLAYER_TIMEOUT_REFRESH;
						} else if (hide_time <= mdate()) {
							showWindow(pch, SW_HIDE, channel);
							TRACE(_T("ch:%d,screen hiding\n"),channel);
							bShow = false;
							hide_time = 0LL;
						}
					}
					Sleep(PLAYER_TIME_SLICE);
					continue;
				}
				pch->bBufferPending = true;
				unlock();

				pch->stream->gettime( &frametime );
				//TRACE(_T("ch:%d,gettime:%02d:%02d:%02d.%03d\n"),channel,frametime.hour,frametime.min,frametime.second,frametime.millisecond);
			}

			if( m_opentype == PLY_PLAYBACK ) {  // m_opentype != PLY_PREVIEW
				// do the sync
				pch->syncstate = PLAY_SYNC_RUN ;
				//refresh = 0;

				// the first channel can go
				while( pch->player_thread_state==PLY_THREAD_RUN &&
					   m_play_stat >= PLAY_STAT_PLAY &&
					   /* we need this not to get stuck in the loop */
					   pch->syncstate != PLAY_SYNC_START) {
					if (channelCanPlay(channel, hideScreen)) {
						//showWindow(pch, SW_SHOW, channel);
						break;
					} else {
						if (bShow && (m_errorstate != 2) && hideScreen) {
							if (hide_time == 0LL) {
								hide_time = mdate() + PLAYER_TIMEOUT_REFRESH;
							} else if (hide_time <= mdate()) {
								showWindow(pch, SW_HIDE, channel);
								TRACE(_T("ch:%d,screen hiding2:%d\n"),channel);
								bShow = false;
								hide_time = 0LL;
							}
						}
					}

					if (pch->bSeekReq) { // seek() called between getdata() and inputdata(), so don't inputdata()
						break;
					}

					bSleepDone = true;
					//TRACE(_T("ch%d waiting\n"),channel);
					Sleep( PLAYER_TIME_SLICE );

					if( m_play_stat>=PLAY_STAT_SLOWEST){
						// refresh screen
						if (bShow && (m_errorstate != 2)) {
							if (hide_time == 0LL) {
								hide_time = mdate() + PLAYER_TIMEOUT_REFRESH;
							} else if (hide_time <= mdate()) {
								showWindow(pch, SW_HIDE, channel);
								TRACE(_T("ch:%d,screen hiding3\n"),channel);
								bShow = false;
								hide_time = 0LL;
							}
						}
					}
				}

				/* this channel was just waiting for its turn, and then play button is clicked
				 * now check again if it can input data */
				if (pch->syncstate == PLAY_SYNC_START) {
					bReplay = true;
					continue;
				}
/*
				lock();
				if (pch->syncstate == PLAY_SYNC_RUN) {
					m_playtime=frametime ;
				}
				unlock();
				*/
			} else {
				//lock();
				//m_playtime=frametime ;
				//unlock();
				bSleepDone = true;
			}

			bReplay = false;

			// send data to decoder
			while( pch->player_thread_state==PLY_THREAD_RUN &&
					pch->target!=NULL ) {
				lock();
				if (pch->bSeekReq) { // seek() called between getdata() and inputdata(), so don't inputdata()
					free(framedata);
					framedata = NULL;
					unlock();
					break;
				}
				int ret = pch->target->inputdata( framedata, framesize, frametype );
				//TRACE(_T("ch:%d,inputdata:%d(%d)\n"),channel,ret,framesize);
				if ( ret>0 ) {
					//if ((frametype == FRAME_TYPE_KEYVIDEO) || (frametype == FRAME_TYPE_VIDEO)) {
						//videoInBuffer++;
					//}
					if (!bShow) {
						TRACE(_T("ch:%d,screen show\n"),channel);
						showWindow(pch, SW_SHOW, channel);
					}
					bShow = true;
					hide_time = 0LL;
					pch->bBufferPending = false;
					// release frame data
					free(framedata);
					framedata = NULL;
					m_playtime=frametime ;
					unlock();
					//TRACE(_T("ch %d,playtime:%02d:%02d:%02d.%03d\n"),channel,frametime.hour,frametime.min,frametime.second,frametime.millisecond);
					break;
				}
				unlock();
			    bSleepDone = true;
				Sleep(PLAYER_TIME_SLICE);
			}

			// if seek() called between getdata() and inputdata(), the data is garbaged already
			// so call getdata() again
			//TRACE(_T("ch:%d,bSeekReq:%d\n"),channel,pch->bSeekReq);
			lock();
			if (pch->bSeekReq) { 
				pch->bSeekReq = false;
				unlock();
			//TRACE(_T("ch:%d,bSeekReq xxx:%d\n"),channel,pch->bSeekReq);
				continue;
			}
#ifndef DELAYED_SEEK
			if (bStepPlay) {
				TRACE(_T("ch:%d,bStepPlay:%d\n"),channel,videoInBuffer);
				if (videoInBuffer >= 30) { 
				TRACE(_T("ch:%d,bStepPlay xxxxx\n"),channel);
					bStepPlay = false;
					videoInBuffer = 0;
					//oneframeforward_nolock_single(channel);
					pch->target->pause();
					pch->target->oneframeforward();
				}
			}
			// if seek was called during pause, call oneframeforward_nolock after getdata()/inputdata().
			if (pch->bSeekDuringPause) {
				TRACE(_T("ch:%d,bSeekDuringPause:%d\n"),channel,pch->bSeekDuringPause);
				pch->bSeekDuringPause = false;
				bStepPlay = true;
				videoInBuffer = 0;
			}
#endif
			unlock();
		}

		if (!bSleepDone) {
			//_RPT1(_CRT_WARN, "ch:%d sleeping\n",channel);
			int sleepTime = PLAYER_TIME_SLICE;
			if (m_play_stat > PLAY_STAT_PLAY) {
				sleepTime = 1;
			}
			Sleep(sleepTime);
		}
	}

	if (framedata) {
		free(framedata);
	}
	pch->player_thread_state=PLY_THREAD_NONE ;			// thread ended
	_RPT1(_CRT_WARN, "ch:%d stoping\n",channel);
}

dvrplayer::dvrplayer()
{
	// initialize critical section
	InitializeCriticalSection(&m_criticalsection);

	m_servername[0]=0;
	m_dvrtype=0;
	m_opentype=0;
	m_totalchannel=0;
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

	m_errorstate=0;
	m_syncclock = 0;
	dvrtime_init(&m_synctime, 1980);
	m_keydata = NULL ;
	m_keydatasize = 0 ;
	m_bSeekDuringPause = false;
}

dvrplayer::~dvrplayer()
{
	detach();						// detach all target decoders, streams and stop all threads

	if( m_dvrtype == DVR_DEVICE_TYPE_DVR && m_socket ){	// if connect to a DVR server
		net_close(m_socket);							// close socket
	}

	// delete critical section
	DeleteCriticalSection(&m_criticalsection);

	if( m_hVideoKey ) {
		CryptDestroyKey(m_hVideoKey);
	}
	CryptReleaseContext(m_hCryptProv,0);

	if( m_keydata!=NULL ) {
		delete (char *) m_keydata ;
	}
}

int dvrplayer::initchannel()
{
	int i ;
	struct channel_t * pch ;
	for(i=0;i<m_totalchannel;i++) {
		pch=m_channellist.at(i);
		pch->stream=NULL ;
		pch->target=NULL ;
		pch->player_thread_state=PLY_THREAD_NONE ;		// thread not start
		pch->hEvent = NULL;
		memset(&pch->channelinfo, 0, sizeof(struct channel_info));
		pch->bSeekDuringPause = pch->bSeekReq = pch->bBufferPending = false;
	}
	m_channellist.compact();							// compact channel list, save some memory
	return 1 ;
}

// open DVR server over network, return 1:success, 0:fail.
int dvrplayer::openremote(char * netname, int opentype )
{
	int i;

	m_dvrtype = DVR_DEVICE_TYPE_DVR ;
	strncpy( m_servername, netname, sizeof(m_servername) );

	m_totalchannel=0 ;

	/* TODO: add channel bit map for the case of disabled channels in the middle */
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
#ifdef NEW_CHANNEL
				for (i = 0; i < m_totalchannel; i++) {
					struct channel_info ci;
					ci.size = sizeof(struct channel_info) + 1; // cheap trick
					getchannelinfo(i, &ci);
				}
#endif
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

int is265File(char *filename)
{
	char *ext;
	int len = strlen(filename);
	int extlen = strlen(FILE_EXT);
	if (len > extlen) {
		ext = &filename[len - extlen];
		if (!strcmp(ext, FILE_EXT))
			return 1;
	}

	return 0;
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
		return res ;
	}
	else if( is265File(dvrfilename) ) {					// .264 file
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
		return res ;
	}
	else {
		return 0 ;
	}
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
	lock();
	for(i=0; i<m_totalchannel; i++) {
		pch = m_channellist.at(i);
		if( pch->stream ) {
			pch->stream->setpassword(ENC_MODE_RC4FRAME, m_password, sizeof(m_password) );			// set frameRC4 password
			pch->stream->setpassword(ENC_MODE_WIN, &m_hVideoKey, sizeof(m_hVideoKey) ) ;		// set winRC4 password
		}
	}
	unlock();

	return 0 ;
}

int dvrplayer::getchannelinfo( int channel, struct channel_info * pchannelinfo )
{
	int res = DVR_ERROR ;
	int i;
	lock();
	if( channel < m_totalchannel && pchannelinfo->size>=sizeof(struct channel_info) ) {
		struct channel_t * pch ;
		pch = m_channellist.at(channel) ;
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
					pch=m_channellist.at(i);
					pch->channelinfo=pci[i]; 
				}
				delete [] pci ;
			}
			else if( m_dvrtype == DVR_DEVICE_TYPE_HARDDRIVE ) {				// connect to hard drive
				for( i=0; i<m_totalchannel; i++) {
					pch=m_channellist.at(i);
					memset( &(pch->channelinfo), 0, sizeof( pch->channelinfo ) );
					pch->channelinfo.size = sizeof( pch->channelinfo ) ;
					pch->channelinfo.features = 1 ;
					sprintf(pch->channelinfo.cameraname, "camera%d", i+1 );
				}
			}
			else if( m_dvrtype == DVR_DEVICE_TYPE_DVRFILE || 
					 m_dvrtype == DVR_DEVICE_TYPE_264FILE ) {				// connect to hard drive
				for( i=0; i<m_totalchannel; i++) {
					pch=m_channellist.at(i);
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
		} else {
			if( m_dvrtype == DVR_DEVICE_TYPE_DVR ) {	// connect to a DVR server?
				int recvsize=0;
				struct channelstatus * cs = new channelstatus [m_totalchannel]  ;
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

		pch = m_channellist.at(channel);
		if( pch->channelinfo.size == sizeof(struct channel_info) ) {	

			// Add native res
			if( pch->target ) {
				pch->target->getres( &pch->channelinfo.xres, &pch->channelinfo.yres );
			}

			*pchannelinfo = pch->channelinfo ;
			res = 0 ;
		}
	}
	unlock();
	return res ;
}

struct dvrplayer_thread_argument {
	int			channel ;
	HANDLE		hrunevent ;
	dvrplayer * player ;
} ;

// player thread
static void dvrplayer_thread(void *p)
{
	dvrplayer * pply = ((struct dvrplayer_thread_argument *)p)->player ;
	int channel = ((struct dvrplayer_thread_argument *)p)->channel ;
	SetEvent( ((struct dvrplayer_thread_argument *)p)->hrunevent );			// let calling thread go
	pply->playerthread( channel );
}

int dvrplayer::attach( int channel, HWND hwnd )
{
	decoder * target ;

	if( channel<0 || channel>=m_totalchannel )
		return DVR_ERROR ;

	lock();

	struct channel_t * pch ;
	pch = m_channellist.at(channel);

#ifdef NEW_CHANNEL
	if ((m_opentype == PLY_PREVIEW) && ((pch->channelinfo.features & 1) == 0)) {
		unlock();
		return 0 ;
	}
#endif

#ifdef USE_REALMODE
	// creat a new decoder
	target = new HikPlayer( hwnd, 1 );
#else
	target = new HikPlayer(channel, hwnd, (m_opentype == PLY_PREVIEW) ?
									STREAME_REALTIME : STREAME_FILE);
#endif

	// attach decoder to target list
	target->m_next = pch->target ;

	pch->target = target ;
	int stat = m_play_stat;
	// play new target base on current play mode
	if( stat == PLAY_STAT_PLAY ) {
		target->play();
	}
	else if( stat >= PLAY_STAT_FAST && stat <= PLAY_STAT_FASTEST ) {
		target->play();
		target->fast(stat-PLAY_STAT_FAST+1);
	}
	else if( stat <= PLAY_STAT_SLOW && stat >= PLAY_STAT_SLOWEST ) {
		target->play();
		target->slow(PLAY_STAT_SLOW-stat+1);
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

	if( pch->player_thread_state == PLY_THREAD_NONE ) {		// no player thread running
		// create a new player thread for this channel
		struct dvrplayer_thread_argument arg ;
		arg.player = this ;
		arg.hrunevent=CreateEvent( NULL, TRUE, FALSE, NULL);
		arg.channel=channel ;
        _RPT1(_CRT_WARN, "dvrplayer_thread %d\n",  channel);
		pch->player_thread_handle=(HANDLE)_beginthread( dvrplayer_thread, 0, (void *)&arg );
        if( !SetThreadPriority(pch->player_thread_handle, THREAD_PRIORITY_ABOVE_NORMAL) )
        {
            _RPT0(_CRT_WARN, "couldn't set a faster priority\n" );
        }
		WaitForSingleObject( arg.hrunevent, 10000 );			// wait player thread running
		CloseHandle(arg.hrunevent);
	}

	unlock();
	return 0 ;												// return OK
}

// detach all channels, kill all player threads
int dvrplayer::detach( )
{
	decoder * dec ;		// temperary decoder pointer
	int i;
	struct channel_t * pch ;

	if( m_totalchannel>0 ) {
		
		// to protect m_target
		lock();

		for(i=0; i<m_totalchannel;i++) {
			pch = m_channellist.at(i);

			// signal player thread to quit
			if( pch->player_thread_state == PLY_THREAD_RUN ){
				pch->player_thread_state = PLY_THREAD_QUIT ;

				// shutdown network if possible
				if( pch->stream ) {
					pch->stream->shutdown();
				}
				unlock();

				// wait player thread to quit
				while( pch->player_thread_state != PLY_THREAD_NONE ){
					Sleep(20);
				}	

				lock();
			}


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

		unlock();
	}
	return 0 ;
}

int dvrplayer::detachwindow( HWND hwnd )
{
	decoder * dec1, * dec2 ;		// temperary decoder pointer
	int i;
	struct channel_t * pch ;

	if( m_totalchannel>0 ) {

		lock();

		for(i=0; i<m_totalchannel;i++) {
		
			pch=m_channellist.at(i);

			// check deteach head of list
			while( pch->target && pch->target->window() == hwnd ) {	// detach this player
				// signal player thread to quit
				if( pch->player_thread_state == PLY_THREAD_RUN ){
					pch->player_thread_state = PLY_THREAD_QUIT ;

					/* clear this flag, otherwise 1st frame is lost when attached again */
					pch->bBufferPending = pch->bSeekReq = false;

					// shutdown network if possible
					if( pch->stream ) {
						pch->stream->shutdown();
					}
					unlock();

					// wait player thread to quit
					while( pch->player_thread_state != PLY_THREAD_NONE ){
						Sleep(20);
					}	

					lock();
				}

				dec1 = pch->target ;
				pch->target = dec1->m_next ;
				delete dec1 ; 
			}

			// check list member
			dec1 = pch->target ;
			while( dec1!=NULL ) {
				while( dec1->m_next!=NULL && dec1->m_next->window() == hwnd ) {
					dec2 = dec1->m_next ;
					dec1->m_next = dec2->m_next ;
					delete dec2 ;
				}
				dec1=dec1->m_next ;
			}

			if( pch->target==NULL ) {		// no target decoder on this channel?
											// we delete the stream of this channel
				// signal player thread to quit
				if( pch->player_thread_state == PLY_THREAD_RUN ){
					pch->player_thread_state = PLY_THREAD_QUIT ;

					// shutdown network if possible
					if( pch->stream ) {
						pch->stream->shutdown();
					}
					unlock();

					// wait player thread to quit
					while( pch->player_thread_state != PLY_THREAD_NONE ){
						Sleep(20);
					}

					lock();
				}
				if( pch->stream ) {
					delete pch->stream ;
					pch->stream=NULL ;
				}
			}
		}
		
		unlock();

		return 0;									// return OK
	}
	return DVR_ERROR ;
}
#if 0
int dvrplayer::detachwindow( HWND hwnd )
{
	decoder * dec1, * dec2 ;		// temperary decoder pointer
	int i;
	struct channel_t * pch ;

	if( m_totalchannel>0 ) {

		lock();

		for(i=0; i<m_totalchannel;i++) {
		
			pch=m_channellist.at(i);

			// check deteach head of list
			while( pch->target && pch->target->window() == hwnd ) {	// detach this player
				dec1 = pch->target ;
				pch->target = dec1->m_next ;
				delete dec1 ; 
			}

			// check list member
			dec1 = pch->target ;
			while( dec1!=NULL ) {
				while( dec1->m_next!=NULL && dec1->m_next->window() == hwnd ) {
					dec2 = dec1->m_next ;
					dec1->m_next = dec2->m_next ;
					delete dec2 ;
				}
				dec1=dec1->m_next ;
			}

			if( pch->target==NULL ) {		// no target decoder on this channel?
											// we delete the stream of this channel
				// signal player thread to quit
				if( pch->player_thread_state == PLY_THREAD_RUN ){
					pch->player_thread_state = PLY_THREAD_QUIT ;

					// shutdown network if possible
					if( pch->stream ) {
						pch->stream->shutdown();
					}
					unlock();

					// wait player thread to quit
					while( pch->player_thread_state != PLY_THREAD_NONE ){
						Sleep(20);
					}

					lock();
				}
				if( pch->stream ) {
					delete pch->stream ;
					pch->stream=NULL ;
				}
			}
		}
		
		unlock();

		return 0;									// return OK
	}
	return DVR_ERROR ;
}
#endif

int dvrplayer::play()
{
	int i;
	int res= DVR_ERROR ;
	decoder * dec ;
	struct channel_t * pch ;

	lock();
	if( m_errorstate==1 ) {
		struct dvrtime t = m_playtime ;
		unlock();
		if( seek( &t )==DVR_ERROR_FILEPASSWORD ) {
			return DVR_ERROR_FILEPASSWORD ;
		}
		lock();
	}
	m_errorstate=0;					// clear password error

	if( m_play_stat!=PLAY_STAT_PLAY ) {
		for(i=0; i<m_totalchannel; i++) {
			pch=m_channellist.at(i);
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
	unlock();
#ifdef DELAYED_SEEK
	if (m_bSeekDuringPause) {
		m_bSeekDuringPause = false;
		seek(&m_delayedSeek);
	}
#endif
	return res ;
}

int dvrplayer::audioon( int channel, int audioon)
{
	decoder * dec ;
	if( channel<0 || channel>=m_totalchannel ) 
		return DVR_ERROR ;
	lock();
	dec = m_channellist.at(channel)->target ;
	while( dec != NULL ) {
		dec->audioon(audioon);
		dec=dec->m_next ;
	}
	unlock();
	return 0;				// return OK
}

int dvrplayer::audioonwindow( HWND hwnd, int audioon)
{
	int channel ;
	decoder * dec ;
	lock();
	for( channel=0; channel<m_channellist.size(); channel++ ) {
		dec = m_channellist.at(channel)->target ;
		while( dec != NULL ) {
			if( dec->window() == hwnd ) {
				dec->audioon(audioon);
			}
			dec=dec->m_next ;
		}
	}
	unlock();
	return 0;				// return OK
}
	
int dvrplayer::pause()
{
	int i;
	int res= DVR_ERROR ;
	decoder * dec ;
	struct channel_t * pch ;

	TRACE(_T("------------PAUSE\n"));
	lock();

	for(i=0; i<m_totalchannel; i++) {
		pch=m_channellist.at(i);
		dec = pch->target ;
		while( dec!=NULL ) {
			dec->pause();
			dec=dec->m_next ;
		}
		res=0;
	}
	m_play_stat=PLAY_STAT_PAUSE ;

	unlock();
	return res ;
}

int dvrplayer::stop()
{
	int i;
	int res= DVR_ERROR ;
	decoder * dec ;
	struct channel_t * pch ;

	lock();

	for(i=0; i<m_totalchannel; i++) {
		pch=m_channellist.at(i);
		dec = pch->target ;
		while( dec!=NULL ) {
			dec->stop();
			dec=dec->m_next ;
		}
		res=0;
	}
	m_play_stat=PLAY_STAT_STOP ;

	unlock();

	return res ;
}

int dvrplayer::fastforward( int speed )
{
	int i;
	int res= DVR_ERROR ;
	decoder * dec ;
	struct channel_t * pch ;

	int curstat = m_play_stat;
	if (curstat == PLAY_STAT_FASTEST)
		return 0;

	lock();

	for(i=0; i<m_totalchannel; i++) {
		pch=m_channellist.at(i);
		dec = pch->target ;
		while( dec!=NULL ) {
			
			if (m_play_stat <= PLAY_STAT_SLOW) {
				dec->play();
			}
			
			dec->fast(speed);
			dec=dec->m_next ;
		}
		res=0;
	}
	curstat = m_play_stat;
	if ((curstat >= PLAY_STAT_FAST) && (curstat < PLAY_STAT_FASTEST))
		m_play_stat = curstat + 1 ;
	else 
		m_play_stat = PLAY_STAT_FAST;

	unlock();

	return res ;
}

int dvrplayer::slowforward( int speed )
{
	int i;
	int res= DVR_ERROR ;
	decoder * dec ;
	struct channel_t * pch ;

	int curstat = m_play_stat;
	if (curstat == PLAY_STAT_SLOWEST)
		return 0;

	lock();

	for(i=0; i<m_totalchannel; i++) {
		pch=m_channellist.at(i);
		dec = pch->target ;
		while( dec!=NULL ) {
			
			if (m_play_stat >= PLAY_STAT_FAST) {
				dec->play();
			}
			
			dec->slow(speed);
			dec=dec->m_next ;
		}
		res=0;
	}
	curstat = m_play_stat;
	if ((curstat > PLAY_STAT_SLOWEST) && (curstat <= PLAY_STAT_SLOW))
		m_play_stat = curstat - 1 ;
	else 
		m_play_stat = PLAY_STAT_SLOW;

	unlock();

	return res ;
}

int dvrplayer::oneframeforward_nolock_single(int channel)
{
	int res= DVR_ERROR ;
	decoder * dec ;
	struct channel_t * pch ;

	if ((channel < 0) || (channel >= m_totalchannel))
		return DVR_ERROR;

	pch=m_channellist.at(channel);
	dec = pch->target ;
	while( dec!=NULL ) {
		dec->oneframeforward();
		dec=dec->m_next ;
	}
	res=0;

	//m_play_stat=PLAY_STAT_PAUSE ;

	return res ;
}

int dvrplayer::oneframeforward_nolock()
{
	int i;
	int res= DVR_ERROR ;
	decoder * dec ;
	struct channel_t * pch ;

	for(i=0; i<m_totalchannel; i++) {
		pch=m_channellist.at(i);
		dec = pch->target ;
		while( dec!=NULL ) {
			dec->oneframeforward();
			dec=dec->m_next ;
		}
		res=0;
	}
	//m_play_stat=PLAY_STAT_ONEFRAME ;
	m_play_stat=PLAY_STAT_PAUSE ;

	return res ;
}

int dvrplayer::oneframeforward()
{
	int res= DVR_ERROR ;

	lock();
	res = oneframeforward_nolock();
	unlock();

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
	lock();
	pch=m_channellist.at(channel);
	if( pch->target ) {
		if( pch->target->capturesnapshot(capturefilename)) {
			res=0;
		}
	}
	unlock();
	return res ;
}

int dvrplayer::seek( struct dvrtime * seekto )
{
	int res = 0 ;
	int i;
	decoder * dec ;
	struct channel_t * pch ;
	TRACE(_T("dvrplayer::seek %d:%d:%d\n"), seekto->hour, seekto->min, seekto->second);
	lock();
#ifdef DELAYED_SEEK
	// don't support seek during pause
	if( m_play_stat == PLAY_STAT_PAUSE ) {	
		m_bSeekDuringPause = true;
		m_delayedSeek = *seekto;
		m_playtime=*seekto ;
		unlock();
		return 1;
	}
	m_bSeekDuringPause = false;
#endif
	for(i=0; i<m_totalchannel; i++) {
		pch = m_channellist.at(i);
		if( pch->stream ) {
			pch->syncstate=PLAY_SYNC_START ;		// start sync
		}
		//if (pch->bBufferPending) {
			pch->bSeekReq = true;
		//}
		dec = pch->target ;
		while(dec) {
			dec->resetbuffer();
			dec=dec->m_next ;
		}
	}
	for(i=0; i<m_totalchannel; i++) {
		pch = m_channellist.at(i);
		if( pch->stream ) {
			pch->syncstate=PLAY_SYNC_START ;		// start sync
			res = pch->stream->seek( seekto ) ;			// ready for next frame
			if( res == DVR_ERROR_FILEPASSWORD ) {
				m_errorstate=1 ;					// password error
				break;
			}
#ifndef DELAYED_SEEK
			if( m_play_stat == PLAY_STAT_PAUSE ) {
				pch->bSeekDuringPause = true;
				pch->target->play();
			}
#endif
		}
	}
	m_playtime=*seekto ;
	m_syncclock=timeGetTime2();
	m_synctime=m_playtime ;
	unlock();
	TRACE(_T("SEEK:%lu\n"), m_syncclock);
#if 0
	if( m_play_stat == PLAY_STAT_PAUSE ) {
		Sleep(10);
		oneframeforward();
	}
#endif
	return res ;
}

int dvrplayer::getcurrenttime( struct dvrtime * t )
{
	lock();
	if( m_dvrtype == DVR_DEVICE_TYPE_DVR && m_opentype == PLY_PREVIEW ) {	// preview a DVR server
		if( net_getdvrtime(m_socket, t)==0 ) {
			* t = m_playtime ;
		}
	}
	else {
		* t = m_playtime ;
	}
	unlock();
	//printDvrtime(t);
	return 0 ;
}

int dvrplayer::getcliptimeinfo( int channel, struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize)
{
	struct channel_t * pch ;
	int res = DVR_ERROR ;
	lock();
	if( channel<m_totalchannel ) {
		pch = m_channellist.at(channel);
		if( pch->stream ) {
			res= pch->stream->gettimeinfo(begintime, endtime, tinfo, tinfosize);
		}
	}
	unlock();

	return res ;
}

int dvrplayer::getclipdayinfo( int channel, dvrtime * daytime )
{
	int res=0 ;
	struct channel_t * pch ;
	lock();
	if( channel<m_totalchannel ) {
		pch = m_channellist.at(channel);
		if( pch->stream ) {
			res = pch->stream->getdayinfo( daytime )>0 ;
		}
	}
	unlock();
	return res ;
}

int dvrplayer::getlockfiletimeinfo( int channel, struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize)
{
	struct channel_t * pch ;
	int res = DVR_ERROR ;
	lock();
	if( channel<m_totalchannel ) {
		pch = m_channellist.at(channel);
		if( pch->stream ) {
			res= pch->stream->getlockfileinfo(begintime, endtime, tinfo, tinfosize);
		}
	}
	unlock();
	return res ;
}

static int get_filetype(char *filepath)
{
	char *ptr;

	if (filepath == NULL)
		return FILETYPE_UNKNOWN;

	ptr = strrchr(filepath, '.'); 
	if (ptr == NULL)
		return FILETYPE_UNKNOWN;

	ptr++;

	if (0 == _stricmp(ptr, "dvr"))
		return FILETYPE_DVR;

	if (0 == _stricmp(ptr, "avi"))
		return FILETYPE_AVI;

	return FILETYPE_UNKNOWN;
}

// save a .DVR file
int dvrplayer::savedvrfile( struct dvrtime * begintime, struct dvrtime * endtime, char * duppath, int flags, struct dup_state * dupstate)
{
	HANDLE hThread;
	char password[256];
	struct dvrfile_save_info sdfinfo;
	if( !(flags & DUP_ALLFILE) )		// support all channels record for now
		return DVR_ERROR ;

	if (m_playerinfo.total_channel <= 0)
		return DVR_ERROR;

	int filetype = get_filetype(duppath);
	if (filetype == FILETYPE_UNKNOWN)
		return DVR_ERROR ;

	sdfinfo.enc_password = NULL ;			// No password by default

	// ask for password encryption?
	if (filetype == FILETYPE_DVR) {
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
					sdfinfo.enc_password = password ;
				}
				break;
			}
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
	sdfinfo.filetype = filetype;
	hThread = (HANDLE)_beginthreadex(NULL, 0, dvrfile_save_thread, &sdfinfo, 0, NULL);
	if (hThread == 0) {
			CloseHandle(sdfinfo.hRunEvent);
			return DVR_ERROR ;
	}
	WaitForSingleObject( sdfinfo.hRunEvent, 10000 );			// wait until dvrfile thread running
	CloseHandle(sdfinfo.hRunEvent);

	if( !(flags&DUP_BACKGROUND) ) {		// wait until saving thread finish?
		while( dupstate->status == 0 ) {
			Sleep(1000);
		}
		if( dupstate->status<0 ) {
			return dupstate->status ;
		}
		return 0 ;
	}
	
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
	return DVR_ERROR ;
}

int dvrplayer::senddvrdata( int protocol, void * sendbuf, int sendsize)
{
	if( protocol==PROTOCOL_KEYDATA ) {
		if( m_keydata!=NULL ) {
			delete (char *) m_keydata ;
			m_keydata=NULL ;
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
	return DVR_ERROR ;
}

int dvrplayer::freedvrdata(void * dvrbuf)
{
	if( dvrbuf!=NULL ) {
		free( dvrbuf );
	}

	return DVR_ERROR ;
}

int dvrplayer::pwkeypad(int keycode, int press)
{
	int res = 0 ;
	if( m_dvrtype == DVR_DEVICE_TYPE_DVR && m_socket ){ // if connect to a DVR server
		lock(); 
		res = net_sendpwkeypad( m_socket, keycode, press );
		unlock();
	}
	return res ;
}

int dvrplayer::getlocation(char *locationbuffer, int buffersize)
{
	struct channel_t * pch;
	int i;
	struct dvrtime dt;
	struct gpsinfo gi;
	bool found = false;


	/* support only local harddisk playback */
	if (m_dvrtype == DVR_DEVICE_TYPE_LOCALSERVER) {
		getcurrenttime(&dt);
		/* get location from one channel(playing current day) only */
		lock();
		for(i=0; i<m_totalchannel; i++) {
			pch = m_channellist.at(i);
			if( pch->stream ) {
				if (pch->stream->getlocation(&dt, &gi) == 0) {
					found = true;
					break;
				}
			}
		}
		unlock();

		if (found) {
			_snprintf(locationbuffer, buffersize,
				"01,%02d:%02d:%02d,%09.6f%c%010.6f%c%.1fD%06.2f",
				gi.hour, gi.min, gi.second,
				gi.latitude < 0 ? -gi.latitude : gi.latitude,
				gi.latitude < 0 ? 'S' : 'N',
				gi.longitude < 0 ? -gi.longitude : gi.longitude,
				gi.longitude < 0 ? 'W' : 'E',
				gi.speed,
				gi.angle);

			return strlen(locationbuffer);
		}
	}
	return DVR_ERROR ;
}


// Blur feature support

int dvrplayer::setblurarea(int channel, struct blur_area * ba, int banumber )
{
    if( channel<0 || channel>=m_totalchannel ) {
        return DVR_ERROR ;
    }
    decoder * dec = m_channellist[channel].target ; 
    while( dec ) {
        dec->setblurarea( ba, banumber ) ;
        dec = dec->m_next ;
    }
    return DVR_ERROR_NONE ;
}

int dvrplayer::clearblurarea(int channel )
{
    if( channel<0 || channel>=m_totalchannel ) {
        return DVR_ERROR ;
    }
    decoder * dec = m_channellist[channel].target ; 
    while( dec ) {
        dec->clearblurarea() ;
        dec = dec->m_next ;
    }
    return DVR_ERROR_NONE ;
}

// blur frame buffer, (sent from .AVI encoder)
void dvrplayer::BlurDecFrame( int channel, char * pBuf, int nSize, int nWidth, int nHeight, int nType)
{
    if( channel<0 || channel>=m_totalchannel ) {
        return ;
    }
    decoder * dec = m_channellist[channel].target ; 
    if ( dec ) {
        dec->BlurDecFrame( pBuf, nSize, nWidth, nHeight, nType);
    }
    return ;
}

// get event list
int dvrplayer::geteventtimelist( struct dvrtime * date, int eventnum, int * elist, int elistsize)
{
    int ch ;
    for(ch=0; ch<m_totalchannel ; ch++) {
        if( m_channellist[ch].stream!=NULL ) {
            return  m_channellist[ch].stream->geteventtimelist(date, eventnum, elist, elistsize );
        }
    }
    return DVR_ERROR ;
}