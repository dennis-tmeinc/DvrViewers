// dvrplayer.cpp : DVR player object
//

#include "stdafx.h"
#include <shellapi.h>
#include <process.h>
#include <time.h>
#include <stdio.h>
#include <shlwapi.h>
#include <vector>
#include "ply606.h"
#include "dvrnet.h"
#include "dvrpreviewstream.h"
#include "dvrharddrivestream.h"
#include "dvrplaybackstream.h"
#include "dvrfilestream.h"
#include "mp4filestream.h"
#include "player.h"
#include "mtime.h"
#include "trace.h"
#include "utils.h"
#include "videoclip.h"
#include "block.h"

//#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "shlwapi.lib")

#define DELAYED_SEEK
#define PLAYER_TIME_SLICE	(5)
#define PLAYER_TIMEOUT_REFRESH (5000000LL)
#define NEW_CHANNEL
#define PLAY_TOLERANCE 500 /* in millisec (was 100) */

struct sync_timer {
	struct dvrtime synctime ;
	int			   insync ;
	__int64		   synctimestamp ;
};

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
	if (dvrt->millisecond >= 0) {
		t.tm_sec = dvrt->second + dvrt->millisecond/1000 ;
	} else {
		t.tm_sec = dvrt->second;
		while( dvrt->millisecond<0 ) {
			t.tm_sec-=1 ;
			dvrt->millisecond+=1000 ;
		}
	}
	dvrt->millisecond %= 1000 ;

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
__int64 dvrtime_diffms(dvrtime * t1, dvrtime * t2)
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
	return t2->millisecond - t1->millisecond + 1000 * (tt2 - tt1) ;
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

__int64 dvrtime_mktime_ms( struct dvrtime * dvrt )
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
	return tt * 1000LL + dvrt->millisecond ;
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
		dvrplaybackstream * pbstream = new dvrplaybackstream( this->m_servername, channel );
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
		stream = new mp4filestream( this->m_servername, channel );
		if( stream != NULL ) {
			stream->setkeydata( m_keydata ) ;
		}
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

	//TRACE(_T("findfirstch\n"));
	lock();
	for(i=0; i<m_totalchannel; i++) {
		pch = m_channellist.at(i);
		if( pch->stream!=NULL ) {
			//TRACE(_T("i:%d,s:%d\n"), i, pch->syncstate);
			if( pch->syncstate == PLAY_SYNC_RUN ) {
				pch->stream->gettime( &t );
				if( firstchannel == -1 ) {
					firstchannel = i ;
					firstt = t ;
					//TRACE(_T("i:%d,fc:%d"), i, i); printDvrtime(&t);
				}
				else {
					if( dvrtime_compare( &t, &firstt )<0 ) {
						firstchannel = i ;
						firstt = t ;
						//TRACE(_T("i:%d,fc2:%d"), i, i); printDvrtime(&t);
					}
				}
			}
			else if( pch->syncstate == PLAY_SYNC_START ) {
				firstchannel=i;
						//TRACE(_T("i:%d,fc3:%d\n"), i, i);
				break;
			}
		}
	}
	unlock();
	return firstchannel ;
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
/*
	if (firstchannel == -1) {
		TRACE(_T("ch %d can go 2\n"), ch);
		return 1;
	}
*/
	if( thisChannelTimeFound ) {
		__int64 diffms = dvrtime_diffms(&cht, &dt);
		if ((diffms > -PLAY_TOLERANCE) && (diffms < PLAY_TOLERANCE)) { /* was 100 */
			//TRACE(_T("ch %d can go 3\n"), ch);
			hide_screen = false;
			return 1;
		}

		if ((diffms > -(PLAY_TOLERANCE * 5)) && (diffms < (PLAY_TOLERANCE * 5))) {
			hide_screen = false;
		}
	}

	//TRACE(_T("ch %d cannot go, fc:%d, %02d:%02d:%02d.%03d, %02d:%02d:%02d.%03d\n"),ch,firstchannel,dt.hour,dt.min,dt.second,dt.millisecond,cht.hour,cht.min,cht.second,cht.millisecond);
	return 0 ;
}

void *dvrplayer::find_first_sync()
{
	int i;
	struct channel_t * pch ;
	struct dvrtime dt;
	void *ret = NULL;

	dvrtime_init(&dt, 2100);

	for(i=0; i<m_totalchannel; i++) {
		pch = m_channellist.at(i);
		if( pch->stream ) {
			struct sync_timer *st = (struct sync_timer *)pch->stream->getsynctimer();
			//struct sync_timer *st = (struct sync_timer *)synctimer;
			if (st && st->insync) {
				//TRACE(_T("ch:%d synctime:,"),i);printDvrtime(&st->synctime);
				if (dvrtime_compare(&dt, &st->synctime) > 0) {
					dt = st->synctime;
					ret = st;
				}
			} else {
				TRACE(_T("ch:%d out of sync\n"),i);
			}
		}
	}

	return ret;
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
	struct dvrtime frametime ;
	int i;
	bool bSleepDone;
	bool bStepPlay = false;
	block_t *pBlock = NULL;
	bool bShow = false;
	bool bReplay = false; 
	bool hideScreen;
	mtime_t hide_time = 0LL;

	lock();
	pch=m_channellist.at(channel);
	pch->player_thread_state=PLY_THREAD_RUN ;			// thread running
	unlock();

	while( pch->player_thread_state==PLY_THREAD_RUN ) {	// keep running if not required to quit
		bSleepDone = false;

		if( pch->stream && m_play_stat > PLAY_STAT_STOP) {
			
			if( m_errorstate!=0 ) {
				if (m_errorstate!=2)
					_RPT2(_CRT_WARN, "ch %d error\n",channel,m_errorstate);
				Sleep(10);
				continue ;
			}

			if (!bReplay) {
				lock();
				pBlock=pch->stream->getdatablock();

				if(pBlock == NULL) {
					//TRACE(_T("ch:%d, thread:PLAY_SYNC_NODATA\n"),channel);
					//printDvrtime(&frametime);
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
				//TRACE(_T("ch:%d,getdata:%d,%I64d,%d\n"),channel, pBlock->i_buffer, pBlock->i_dts,m_play_stat);

				pch->stream->gettime( &frametime );
			}

			//TRACE(_T("ch:%d,gettime:"),channel);
			//printDvrtime(&frametime);
			if( m_opentype == PLY_PLAYBACK ) {  // m_opentype != PLY_PREVIEW
				// do the sync
				//TRACE(_T("ch:%d, PLAY_SYNC_RUN\n"),channel);
				pch->syncstate = PLAY_SYNC_RUN ;

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

					if (pch->bSeekReq) { // seek() called [between getdata() and inputdata()], so don't inputdata()
						break;
					}
					//if( pch->stream->preloaddata()==0 ) {
					bSleepDone = true;
					//TRACE(_T("ch%d waiting\n"),channel);
					Sleep( PLAYER_TIME_SLICE );
					//}
					if( m_play_stat>=PLAY_STAT_SLOWEST ){
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
					
					if (pch && pch->target) {
						pch->target->checkPendingShowCmd();
					}
				} /* while */

				/* this channel was just waiting for its turn, and then play button is clicked
				 * now check again if it can input data */
				if (pch->syncstate == PLAY_SYNC_START) {
					bReplay = true;
					continue;
				}
			} else {
				bSleepDone = true; // Don't sleep in preview
			}

			bReplay = false;

			// send data to decoder
			while( pch->player_thread_state==PLY_THREAD_RUN &&
				pch->target!=NULL ) {
				lock();
				if (pch->bSeekReq) { // seek() called [between getdata() and inputdata()], so don't inputdata()
					TRACE(_T("ch:%d,bSeekReq--------------------\n"),channel);
					block_Release(pBlock);
					pBlock = NULL;
					unlock();
					break;
				}

				int ret = pch->target->inputdata( pBlock );
				//TRACE(_T("ch:%d,inputdata:%d(%d,%I64d)\n"),channel,ret,pBlock->i_buffer,pBlock->i_dts);
				if ( ret>0 ) {
					if (!bShow) {
						TRACE(_T("ch:%d,screen showing\n"),channel);
						showWindow(pch, SW_SHOW, channel);
					}
					bShow = true;
					hide_time = 0LL;
					pch->bBufferPending = false; // data sent, so clear the flag
					pBlock = NULL;
					m_playtime=frametime ;
					unlock();
					break;
				}
				unlock();
			    bSleepDone = true;
				//TRACE(_T("channel %d input waiting\n"), channel);
				Sleep(PLAYER_TIME_SLICE);

				if (pch && pch->target) {
					pch->target->checkPendingShowCmd();
				}
			}

			// if seek() called [between getdata() and inputdata()], the data is garbaged already
			// so call getdata() again
			lock();
			if (pch->bSeekReq) { 
				pch->bSeekReq = false;
				unlock();
				continue;
			}
			
#ifndef DELAYED_SEEK
			if (bStepPlay) {
				bStepPlay = false;
				oneframeforward_nolock_single(channel);
			}
			// if seek was called during pause, call oneframeforward_nolock after getdata()/inputdata().
			if (pch->bSeekDuringPause) {
				pch->bSeekDuringPause = false;
				bStepPlay = true;
			}
#endif
			unlock();
		} /* if (stream) */

		if (!bSleepDone) {
			//_RPT1(_CRT_WARN, "ch:%d sleeping\n",channel);
			int sleepTime = PLAYER_TIME_SLICE;
			if (m_play_stat > PLAY_STAT_PLAY) {
				sleepTime = 1;
			}
			Sleep(sleepTime);
		}

		if (pch && pch->target) {
			pch->target->checkPendingShowCmd();
		}
	} /* while */

	if (pBlock)
		block_Release(pBlock);

	pch->player_thread_state=PLY_THREAD_NONE ;			// thread ended
	_RPT1(_CRT_WARN, "ch:%d stoping\n",channel);
}

dvrplayer::dvrplayer()
{
	// initialize critical section
	m_lock_count=0 ;
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

	m_errorstate=0;
	m_syncclock = 0;
	dvrtime_init(&m_synctime, 1980);
	m_keydata = NULL ;
	m_keydatasize = 0 ;
	m_bSeekDuringPause = false;
	m_bOperator = false;
}

dvrplayer::~dvrplayer()
{
	detach();						// detach all target decoders, streams and stop all threads

	if( m_dvrtype == DVR_DEVICE_TYPE_DVR && m_socket ){	// if connect to a DVR server
		net_close(m_socket);							// close socket
	}

	// delete critical section
	DeleteCriticalSection(&m_criticalsection);

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
		/* operator key support */
		pch->bEnable = true;
	}
	m_channellist.compact();							// compact channel list, save some memory
	return 1 ;
}

// open DVR server over network, return 1:success, 0:fail.
int dvrplayer::openremote(char * netname, int opentype )
{
	int i, ret = 0;

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
#ifdef NEW_CHANNEL
				for (i = 0; i < m_totalchannel; i++) {
					struct channel_info ci;
					ci.size = sizeof(struct channel_info);
					getchannelinfo(i, &ci);
				}
#endif
			}
			ret = 1 ;
		}
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
			ret = 1 ;
		}
	}

	return ret;
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

int isMp5File(char *filename)
{
	char *ext;
	int len = strlen(filename);
	int mp5len = strlen(FILE_EXT);
	if (len > mp5len) {
		ext = &filename[len - mp5len];
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
	DWORD fileflag[4] ;
	dvrfile=fopen( dvrfilename, "rb");
	if( dvrfile==NULL ) {
		return 0 ;
	}
	fread( fileflag, 1, sizeof(fileflag), dvrfile );
	fclose( dvrfile );

	if( fileflag[0] == DVRFILEFLAG ) {
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
	else if( fileflag[3] == 0x5453494c && isMp5File(dvrfilename)) {					// .264 file
		m_dvrtype = DVR_DEVICE_TYPE_264FILE ;
		m_opentype = PLY_PLAYBACK ;
		strncpy( m_servername, dvrfilename, sizeof(m_servername) );
		
		m_totalchannel=0 ;

		mp4filestream * filestream = new mp4filestream(m_servername, 0) ;

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

	int i;
	struct channel_t * pch ;
	lock();
	for(i=0; i<m_totalchannel; i++) {
		pch = m_channellist.at(i);
		if( pch->stream ) {
			pch->stream->setpassword(ENC_MODE_RC4FRAME, m_password, sizeof(m_password) );			// set frameRC4 password
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
		}

		pch = m_channellist.at(channel);
		if( pch->channelinfo.size == sizeof(struct channel_info) ) {		
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
	TRACE(_T("======================================attach ch:%d/%d\n"), channel,m_totalchannel);

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
	/* operator key support */
	if (!pch->bEnable) {
		unlock();
		return 0 ;
	}

	target = new MpegPlayer(hwnd, channel);
	// attach decoder to target list
	target->m_next = pch->target ;

	pch->target = target ;
	int stat = m_play_stat;
	// play new target base on current play mode
	if( m_play_stat == PLAY_STAT_PLAY ) {
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

	TRACE(_T("======================================detachwindow win:%x\n"), hwnd);
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
	TRACE(_T("======================================detachwindow End win:%x\n"), hwnd);

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

	TRACE(_T("PLAY==================\n"));
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

	lock();

	for(i=0; i<m_totalchannel; i++) {
		pch=m_channellist.at(i);
		dec = pch->target ;
		while( dec!=NULL ) {
			
			if (m_play_stat == PLAY_STAT_SLOW) {
				dec->play();
			}
			
			dec->fast(speed);
			dec=dec->m_next ;
		}
		res=0;
	}
	m_play_stat=PLAY_STAT_FAST ;

	unlock();

	return res ;
}

int dvrplayer::slowforward( int speed )
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
			
			if (m_play_stat >= PLAY_STAT_FAST) {
				dec->play();
			}
			
			dec->slow(speed);
			dec=dec->m_next ;
		}
		res=0;
	}
	m_play_stat=PLAY_STAT_SLOW ;

	unlock();

	return res ;
}
#if 0
int dvrplayer::oneframeforward_nolock_delayed()
{
	int i;
	int res= DVR_ERROR ;
	decoder * dec ;
	struct channel_t * pch ;

	for(i=0; i<m_totalchannel; i++) {
		pch=m_channellist.at(i);
		dec = pch->target ;
		while( dec!=NULL ) {
			dec->oneframeforward_delayed();
			dec=dec->m_next ;
		}
		res=0;
	}
	//m_play_stat=PLAY_STAT_ONEFRAME ;
	m_play_stat=PLAY_STAT_PAUSE ;

	return res ;
}
#endif
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
	TRACE(_T("seek----------------\n"));
	printDvrtime(seekto);
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
		TRACE(_T("seek(%d)----------------:%d\n"),i,pch->bBufferPending);
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
			}
#endif
		}
	}
	m_playtime=*seekto ;
	m_syncclock=timeGetTime2();
	m_synctime=m_playtime ;
	TRACE(_T("SEEK:(%lu)\n"), m_syncclock);
	//printDvrtime(&m_playtime);
	unlock();
#if 0
	if( m_play_stat == PLAY_STAT_PAUSE ) {
		Sleep(50);
		oneframeforward();
	}
#endif
	return res ;
}

int dvrplayer::getcurrenttime( struct dvrtime * t )
{
	int i;

	lock();
#if 0
	if( m_dvrtype == DVR_DEVICE_TYPE_DVR && m_opentype == PLY_PREVIEW ) {	// preview a DVR server
		if( net_getdvrtime(m_socket, t)==0 ) {
			* t = m_playtime ;
		}
	}
	else {
#endif
		int max_delay = 0;
		for(i=0; i<m_totalchannel; i++) {
			struct channel_t *pch;
			decoder *dec ;
			pch=m_channellist.at(i);
			dec = pch->target ;
			while( dec!=NULL ) {
				int delay = dec->getBufferDelay();
				if (delay > max_delay) {
					max_delay = delay;
				}
				dec=dec->m_next ;
			}
		}
		*t = m_playtime ;
		printDvrtime(&m_playtime);
		TRACE(_T("delay:%d\n"), max_delay);
		dvrtime_addmilisecond(t, -max_delay);
	//}
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

TCHAR szVideoKey1[80], szVideoKey2[80];
BOOL CALLBACK encryptDlgProc(HWND hDlg, UINT message,
				  			    WPARAM wParam, LPARAM lParam)
{
	switch (message) {
		case WM_INITDIALOG:
			return TRUE;

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDOK:
					if (!GetDlgItemText(hDlg, IDC_EDIT_PASSWORD1, szVideoKey1, sizeof(szVideoKey1)))
						szVideoKey1[0] = 0;
					if (!GetDlgItemText(hDlg, IDC_EDIT_PASSWORD2, szVideoKey2, sizeof(szVideoKey2)))
						szVideoKey2[0] = 0;
				case IDCANCEL:
					EndDialog(hDlg, LOWORD(wParam));
					return TRUE;
			}
			break;
	}

	return FALSE;
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
	int ret;
	struct dvrfile_save_info sdfinfo;
	if( !(flags & DUP_ALLFILE) )		// support all channels record for now
		return DVR_ERROR ;
	
	if( m_playerinfo.total_channel<=0 )		// total channel is 0, so we do nothing
		return DVR_ERROR ;

	int filetype = get_filetype(duppath);
	if (filetype == FILETYPE_UNKNOWN)
		return DVR_ERROR ;


	sdfinfo.password[0] = 0 ;			// No password by default

	if (filetype == FILETYPE_DVR) {
	// ask for password encryption?
	while (1) {
			ret = DialogBox(GetCurrentModule(), MAKEINTRESOURCE(IDD_DIALOG_FILEPASSWORD),
						GetForegroundWindow(), (DLGPROC)encryptDlgProc);

			if (ret == IDOK) {
				if (StrCmp(szVideoKey1, szVideoKey2)) {
					MessageBox(GetForegroundWindow(), 
						_T("Passwords don't match, please try again!"),
						_T("Password Error"), 0);
					continue;
				} else {
	#ifdef UNICODE
					WideCharToMultiByte(CP_ACP, 0, szVideoKey1, -1, sdfinfo.password, sizeof(sdfinfo.password), NULL, NULL);
	#else
					strcpy(sdfinfo.password, szVideoKey1);
	#endif
					break;
				}
			} else {
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

// open html setup dialog
int openhtmldlg( char * servername, char * page )
{
	char url[200] ;
	TCHAR TURL[200] ;
	sprintf(url, "http://%s%s", servername, page);

	MultiByteToWideChar(CP_UTF8, 0, url, -1, TURL, 200);

	ShellExecute(NULL, _T("open"), TURL, NULL, NULL, SW_SHOWNORMAL );
	return 1;
}

// return
//       0: success
//       1: success and need to close connection
//       DVR_ERROR: error
int dvrplayer::configureDVR( )
{
	int res=0 ;
	SOCKET cfgsocket = net_connect( m_servername );
	if( cfgsocket==0 ) {
		return DVR_ERROR ;
	}

	if( m_keydata ) {
		if( net_CheckKey(cfgsocket, (char *)m_keydata, m_keydatasize)==0 ){
			net_close(cfgsocket);
			return DVR_ERROR ;
		}
	}

	char pagebuf[200] ;
	if( net_GetSetupPage(cfgsocket, pagebuf, sizeof( pagebuf ) ) ) {
		openhtmldlg( m_servername, pagebuf );
		net_close(cfgsocket);
		return 1 ;
	}

	net_close(cfgsocket);
	return 0;
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

			/* operator key support */
			if ((m_keydata->usbid[0] == 'O') && (m_keydata->usbid[1] == 'P')) {
				if (!m_bOperator) {
					 /* do this only once */
					m_bOperator = true;
					do_operator_key();
				}
			}

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

#if 0
/* operator key support */
void dvrplayer::do_operator_key()
{
	int i;

	/* do this only for operator key */
	if (!m_bOperator)
		return;
	
	int x = m_channellist.size();

	/* report 0 channels if only ch1 or ch2 are enabled */
	if (m_totalchannel <= 2) {
		m_totalchannel = 0;
		while (m_channellist.size() > 0) {
			m_channellist.remove(m_channellist.size() - 1);
		}
	}

	/* remap ch0 --> ch3 */
	m_channellist[0] = m_channellist[2];
	memcpy(m_channellist[0].channelinfo.cameraname,
		m_channellist[2].channelinfo.cameraname,
		sizeof(m_channellist[0].channelinfo.cameraname));

	/* remap ch1 --> ch4 */
	if (m_totalchannel > 3) {
		m_channellist[1] = m_channellist[3];
		memcpy(m_channellist[1].channelinfo.cameraname,
			m_channellist[3].channelinfo.cameraname,
			sizeof(m_channellist[1].channelinfo.cameraname));
	}

	/* limit to 2 channels */
	while (m_channellist.size() > 2) {
		m_channellist.remove(m_channellist.size() - 1);
	}
	m_totalchannel = 2;
	m_playerinfo.total_channel = 2;
}
#endif
void dvrplayer::do_operator_key()
{
	int i;

	/* do this only for operator key */
	if (!m_bOperator)
		return;

	/* enable only ch 3, ch 4 */
	for (i = 0; i < m_totalchannel; i++) {
		if ((i != 2) && (i != 3)) {
			m_channellist[i].bEnable = false;
		}
	}
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
void dvrplayer::BlurDecFrame( int channel, picture_t * pic )
{
    if( channel<0 || channel>=m_totalchannel ) {
        return ;
    }
    decoder * dec = m_channellist[channel].target ; 
    if ( dec ) {
        dec->BlurDecFrame(pic);
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
