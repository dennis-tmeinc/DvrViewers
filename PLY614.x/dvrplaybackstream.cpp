
#include "stdafx.h"
#include <process.h>
#include "ply614.h"
#include "dvrnet.h"
#include "dvrplaybackstream.h"

unsigned WINAPI RemotePlaybackThreadFunc(PVOID pvoid) {
	dvrplaybackstream *p = (dvrplaybackstream *)pvoid;
	p->PlaybackThread();
	_endthreadex(0);
	return 0;
}

// DVR stream in interface
//     a single channel dvr data streaming in 

// open dvr server stream for playback
dvrplaybackstream::dvrplaybackstream(char * servername, int channel, int *dvrversion)
{
	m_channel=channel ;
	strncpy( m_server, servername, sizeof(m_server));
	m_socket = INVALID_SOCKET;
	m_socket=net_connect( m_server );

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
	memset( &m_dayinfoday, 0, sizeof(m_dayinfoday));
	memcpy(m_dvrversion, dvrversion, sizeof(m_dvrversion));

	dvrtime_init(&m_dvrTimeSave, 1980);
	m_dvrTimestampSave = 0LL;

	// initialize frame fifo
	m_fifohead = NULL;
	m_fifotail = NULL ;
	m_fifosize = 0 ;
	m_encryptmode = ENC_MODE_NONE ;
	m_keyhash = 0;

	m_bDie = m_bError = false;
	m_hThread = NULL;
	//tme_thread_init(&m_ipc);
    tme_mutex_init(&g_ipc, &m_lock);
    tme_cond_init(&g_ipc, &m_wait);
    tme_mutex_init(&g_ipc, &m_lockUI);
    tme_mutex_init(&g_ipc, &m_lockTime);
	m_pFifo = block_FifoNew(&g_ipc);
	m_bWaiting = m_bBufferPending = m_bSeekReq = false;
	m_keydata = NULL;

	m_ps.Init();
	m_versionChecked = false;
	m_oldversion = false;
}

dvrplaybackstream::~dvrplaybackstream()
{
	m_bDie = true;

	tme_mutex_lock(&m_lock);
	if (m_bWaiting)
	    tme_cond_signal(&m_wait);
	tme_mutex_unlock(&m_lock);

	if (m_hThread != NULL) {
        _RPT0(_CRT_WARN, "Joining remote playback thread\n" );
		tme_thread_join(m_hThread);
	}
	m_hThread = NULL;

	tme_mutex_lock(&m_lockUI);
	if( m_socket!=INVALID_SOCKET && m_streamhandle>0 ) {
		net_stream_close(m_socket, m_streamhandle);
	}

	net_close(m_socket);
	tme_mutex_unlock(&m_lockUI);

    block_FifoEmpty( m_pFifo );
    block_FifoRelease( m_pFifo );
	tme_mutex_destroy(&m_lockTime);
	tme_mutex_destroy(&m_lockUI);
	tme_mutex_destroy(&m_lock);
	tme_cond_destroy(&m_wait);
	//tme_mutex_destroy(&(m_ipc.object_lock));
	//tme_cond_destroy(&(m_ipc.object_wait));
}

void dvrplaybackstream::setpassword(int passwdtype, void * password, int passwdsize )
{
	if( passwdtype==ENC_MODE_RC4FRAME ) {			// Frame RC4
		unsigned char k256[256] ;
		m_encryptmode = ENC_MODE_RC4FRAME ;
		key_256( (char *)password, k256);
		RC4_crypt_table( m_RC4_table, sizeof(m_RC4_table), k256);
		m_keyhash = (unsigned int)m_RC4_table[0]
                 +  ((unsigned int)m_RC4_table[1] << 8 )
                 +  ((unsigned int)m_RC4_table[2] << 16 )
                 +  ((unsigned int)m_RC4_table[3] << 24 ) ;
		m_keyhash ^= FOURCC_HKMI ;
	}
}

// set key block
void dvrplaybackstream::setkeydata( struct key_data * keydata )
{
	unsigned char * k256 ;
	m_keydata = keydata ;
	if( m_keydata && m_socket ) {
		net_CheckKey(m_socket, (char *)m_keydata, m_keydata->size + sizeof( m_keydata->checksum ) );
		m_encryptmode = ENC_MODE_RC4FRAME ;
		k256 = (unsigned char *)m_keydata->videokey ;
		RC4_crypt_table( m_RC4_table, sizeof(m_RC4_table), k256 );
		m_keyhash = (unsigned int)m_RC4_table[0]
                 +  ((unsigned int)m_RC4_table[1] << 8 )
                 +  ((unsigned int)m_RC4_table[2] << 16 )
                 +  ((unsigned int)m_RC4_table[3] << 24 ) ;
		m_keyhash ^= FOURCC_HKMI ;
	}
}

int dvrplaybackstream::PlaybackThread()
{
	int ret;
	BYTE rxbuf[500*1024];
	struct dvrtime curtime, adjtime;
	int rxsize;

	while (!m_bDie && !m_bError) {
		//_RPT1(_CRT_WARN, "PlaybackThread lock:(%d)\n", m_channel);
		if (m_pFifo->i_depth > 60) {
			//_RPT2(_CRT_WARN, "wait:%d(%d)\n",m_pFifo->i_depth, m_channel);
			tme_mutex_lock(&m_lock);
			m_bWaiting = true;
			tme_cond_wait(&m_wait, &m_lock);
			m_bWaiting = false;
			tme_mutex_unlock(&m_lock);
			//_RPT2(_CRT_WARN, "wait ok:%d(%d)\n",m_pFifo->i_depth, m_channel);
		}
		if (m_bDie) {
			break;
		}

		bool gotData = false;
		int fType, hLen;
		__int64 fTimestamp;
		char *pData;
		int size, remainingSize, total = 0;


		tme_mutex_lock(&m_lockUI);
		if (!m_versionChecked) {
			m_versionChecked = true;
			ret = net_stream_getdata2(m_socket, m_streamhandle, rxbuf, sizeof(rxbuf), &rxsize, &curtime);
			if (ret == -1) {
				m_oldversion = true;
				ret = net_stream_getdata1(m_socket, m_streamhandle, rxbuf, sizeof(rxbuf), &rxsize);
			}
		} else {		
			if (m_oldversion)
				ret = net_stream_getdata1(m_socket, m_streamhandle, rxbuf, sizeof(rxbuf), &rxsize);
			else
				ret = net_stream_getdata2(m_socket, m_streamhandle, rxbuf, sizeof(rxbuf), &rxsize, &curtime);
		}

		if (ret > 0) {
			gotData = true;

			if (m_oldversion)
				ret = net_stream_gettime(m_socket, m_streamhandle, &curtime);
			
			if (ret) {
				//TRACE(_T("(%d)%d/%d/%d,%d:%d:%d.%d\n"),m_channel,curtime.year,curtime.month,curtime.day,curtime.hour,curtime.min,curtime.second,curtime.millisecond);
				m_bBufferPending = true;
				remainingSize = rxsize;

				pData = (char *)rxbuf;
				size = remainingSize;
				// sometimes multiframe packet comes in...
				while (/*!m_bSeekReq &&*/ m_ps.GetFrameInfo(pData, &size, &fType, &fTimestamp, &hLen)) {	
					block_t *p_block;
					
					adjtime = curtime; // looping can be more than once
					// 614 updates time only on key frame~~~
					ret = dvrtime_compare(&curtime, &m_dvrTimeSave);
					if (ret == 0) {
						__int64 diff = fTimestamp - m_dvrTimestampSave;
						__int64 diff_in_milsec = diff / 1000;
						if (diff_in_milsec > 0) {
							dvrtime_addmilisecond(&adjtime, (int)diff_in_milsec);
						}
					} else {
						if (ret < 0) {
							TRACE(_T("  -----------------------\n"));
						} else {
							m_dvrTimeSave = curtime;
							m_dvrTimestampSave = fTimestamp;
						}
					}

					// sanity check
					total += size;
					if (total > rxsize) {
						break;
					}

					p_block = block_New( size );
					if (p_block) {
						memcpy(p_block->p_buffer, pData, size);
						p_block->i_buffer = size;
						// temporary storage
						p_block->i_flags = fType;
						p_block->i_pts = fTimestamp;
						p_block->i_length = hLen;
						// use i_dts for frametime
						struct dvr_t *dv = (struct dvr_t *)&p_block->i_dts;
						dv->year = adjtime.year - 2000;
						dv->mon = adjtime.month;
						dv->day = adjtime.day;
						dv->hour = adjtime.hour;
						dv->min = adjtime.min;
						dv->sec = adjtime.second;
						dv->millisec = adjtime.millisecond;
						block_FifoPut( m_pFifo, p_block );
						//TRACE(_T("(%d)%d/%d/%d,%d:%d:%d.%d,%I64d\n"),m_channel,dv->year,dv->mon,dv->day,dv->hour,dv->min,dv->sec,dv->millisec,fTimestamp);
					}
					pData += size;
					remainingSize -= size;

					if (remainingSize <= 0)
						break;

					size = remainingSize;
				}
				m_bBufferPending = false;
				//m_bSeekReq = false;
			}
		}
		tme_mutex_unlock(&m_lockUI);

		// DVR not responding?
		if (!gotData) {
			Sleep(30);
		}

		//_RPT1(_CRT_WARN, "PlaybackThread unlock:(%d)\n", m_channel);
	}

	return 0;
}

// open stream mode play back
int dvrplaybackstream::openstream()
{
	tme_mutex_lock(&m_lockUI);
    block_FifoEmpty( m_pFifo );
	m_streamhandle = net_stream_open(m_socket, m_channel);
	tme_mutex_unlock(&m_lockUI);
	return (m_streamhandle>0 );
}

// open file share
char * dvrplaybackstream::openshare()
{
	return NULL ;
}

// get dvr data frame
int dvrplaybackstream::getdata(char **framedata, int * framesize, int * frametype, int *headerlen)
{
	block_t *p_block;
	struct frame_t * framet ;
	int res = 0 ;
	mtime_t ts, fTimestamp;
	int fType, hLen;

	*frametype = FRAME_TYPE_UNKNOWN ;

	//_RPT2(_CRT_WARN, "getdata:%p(%d)\n", m_socket,m_channel);

	if( m_socket == INVALID_SOCKET ) {
		return 0;
	}

	if (m_hThread == NULL) {
		m_hThread = tme_thread_create( &g_ipc, "Remote Playback Thread",
			RemotePlaybackThreadFunc, this,				
			THREAD_PRIORITY_NORMAL, false );
		return 0;
	}
/*
	if (m_bSeekReq && (m_pFifo->i_depth < 30)) 
		return 0;
*/
	m_bSeekReq = false;

	if (m_pFifo->i_depth < 1) {
		return 0;
	}

	//TRACE(_T("fifoget:%d(%d)\n"), m_pFifo->i_depth, m_channel);
	if( ( p_block = block_FifoGet( m_pFifo ) ) == NULL )
	{
		_RPT1(_CRT_WARN, "fifoget error=================(%d)\n", m_channel);
		m_bError = true;
		return 0;		
	}

	*framedata = (char *)malloc(p_block->i_buffer);
	if (*framedata == NULL) {
		return 0;
	}

	memcpy(*framedata, p_block->p_buffer, p_block->i_buffer);
	ts = p_block->i_dts;
	fTimestamp = p_block->i_pts;
	hLen = p_block->i_length;
	fType = p_block->i_flags;
	*framesize = p_block->i_buffer;
	*frametype = fType;
	if (headerlen) *headerlen = hLen;
    block_Release( p_block );

	// let the thread know fifo is no longer full.
    tme_mutex_lock(&m_lock);
	if (m_bWaiting) {
		tme_cond_signal(&m_wait);
	}
	tme_mutex_unlock(&m_lock);

	//_RPT2(_CRT_WARN, "Got data:%d(%d)\n", p_block->i_buffer, m_channel );
#ifdef TEST_301
	enter_critical();
	struct hd_frame * pframe ;
	pframe = (struct hd_frame *)*framedata ;
	if( pframe->flag==1 ) {
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
	leave_critical();
#else
	int size = *framesize;
	char *pData = *framedata;
	if( m_encryptmode==ENC_MODE_RC4FRAME ) {		// decrypt RC4 frame
		// Decrypt frame data
		int dec_size = size - hLen ;
		if( dec_size>1024 ) {
			dec_size=1024 ;
		}
		RC4_block_crypt((unsigned char *)(pData + hLen), dec_size, 0, m_RC4_table, sizeof(m_RC4_table) ); 
	}

	tme_mutex_lock(&m_lockTime);
	if(  m_keyframestamp == 0 || 
		 fTimestamp<m_keyframestamp || 
		 (fTimestamp-m_keyframestamp)>10000 ) 
	{
		struct dvr_t *dv = (struct dvr_t *)&ts;
		m_keyframetime.year = dv->year + 2000;
		m_keyframetime.month = dv->mon;
		m_keyframetime.day = dv->day;
		m_keyframetime.hour = dv->hour;
		m_keyframetime.min = dv->min;
		m_keyframetime.second = dv->sec;
		m_keyframetime.millisecond = dv->millisec;
	    m_curtime = m_keyframetime ;
	    m_keyframestamp = fTimestamp ;
	}
	else {
		m_curtime = m_keyframetime ;
		__int64 diff = fTimestamp-m_keyframestamp;
		__int64 diff_in_milsec = diff / 1000;

		dvrtime_addmilisecond(&m_curtime, diff_in_milsec);
	}
	tme_mutex_unlock(&m_lockTime);
#endif

	return 1;
}

// get current time and timestamp
int dvrplaybackstream::gettime( struct dvrtime * dvrt )
{
	tme_mutex_lock(&m_lockTime);
	*dvrt = m_curtime ;
	tme_mutex_unlock(&m_lockTime);
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

/* should be called with mutex locked */
int dvrplaybackstream::seek2(struct dvrtime *curtime)
{
	int res = 0 ;

	res = net_stream_seek(m_socket, m_streamhandle, curtime);
	if( res == FOURCC_HKMI ) {
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
		if( m_keyhash == (DWORD)res ) {
			m_encryptmode = ENC_MODE_RC4FRAME ;
			res = 1 ;
		}
		else {
			m_encryptmode = ENC_MODE_NONE ;
			res = DVR_ERROR_FILEPASSWORD ;
		}
	}
	
	return res ;
}

// seek to specified time.
// return 1: success
//        0: error
//        DVR_ERROR_FILEPASSWORD: need password/password error
int dvrplaybackstream::seek( struct dvrtime * dvrt )
{
	//_RPT4(_CRT_WARN, "---seek:%d-%d-%d,%d:",dvrt->year,  dvrt->month,dvrt->day,dvrt->hour);
	//_RPT4(_CRT_WARN, "%d:%d, %d(channel:%d)\n",dvrt->min,  dvrt->second, m_seekreq,m_channel);

	int ret;
	struct dvrtime curtime;
	tme_mutex_lock(&m_lockUI);
	//if (m_bBufferPending) {
		m_bSeekReq = true;
	//}
    block_FifoEmpty( m_pFifo );
	tme_mutex_lock(&m_lockTime);
	curtime = m_curtime = *dvrt ;
	m_keyframestamp=0;
	tme_mutex_unlock(&m_lockTime);
	ret = seek2(&curtime);

	// let the thread know fifo is no longer full.
	tme_mutex_lock(&m_lock);
	if (m_bWaiting) {
		//_RPT2(_CRT_WARN, "signal-->:%d(%d)\n",m_pFifo->i_depth, m_channel);
		tme_cond_signal(&m_wait);
	}
	tme_mutex_unlock(&m_lock);
		//_RPT2(_CRT_WARN, "signal--> ok:%d(%d)\n",m_pFifo->i_depth, m_channel);

	tme_mutex_unlock(&m_lockUI);
	return ret;
}

// get stream day data availability information
int dvrplaybackstream::getdayinfo(dvrtime * day)
{
	tme_mutex_lock(&m_lockUI);
	if( day->month!=m_month.month ||
		day->year!=m_month.year ) {			// day info cache available?
			m_month=*day ;
			m_monthinfo = net_stream_getmonthinfo( m_socket, m_streamhandle, &m_month);
	}
	tme_mutex_unlock(&m_lockUI);
	return ( m_monthinfo & (1<<(day->day-1)) );

}

// get data availability information
int dvrplaybackstream::gettimeinfo( struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize)
{
	int i;
	int getsize = 0 ;
	struct cliptimeinfo * pcti ;
	int daybegintime, dayendtime ;		// convert begintime, endtime to seconds of the day

	// convert begintime, endtime to seconds of the day
	daybegintime = begintime->hour * 3600 +
				   begintime->min * 60 +
				   begintime->second ;
	dayendtime =   endtime->hour * 3600 +
				   endtime->min * 60 +
				   endtime->second ;

	if( dayendtime<=daybegintime ) {				// parameter error!
		return 0 ;
	}

	if( m_dayinfoday.day != begintime->day ||
		m_dayinfoday.month != begintime->month ||
		m_dayinfoday.year != begintime->year ) {		// day info cache available ?
			int clipcount ;
			tme_mutex_lock(&m_lockUI);
			m_dayinfoday = * begintime ;
			m_clipinfolist.setsize(2000) ;
			clipcount=net_stream_getdayinfo(m_socket, m_streamhandle, &m_dayinfoday, m_clipinfolist.at(0), 2000 ) ;
			m_clipinfolist.setsize(clipcount);
			m_clipinfolist.compact();
			tme_mutex_unlock(&m_lockUI);
	}

	for( i=0; i<m_clipinfolist.size(); i++) {
		pcti = m_clipinfolist.at(i) ;

		// check if time overlapped with begintime - endtime
		if( pcti->on_time >= dayendtime ||
   			pcti->off_time <= daybegintime ) {
				continue ;				// clip time not included
		}

		if( tinfosize>0 && tinfo!=NULL ) {
			if( getsize>=tinfosize ) {
				break;
			}
			// set clip on time
			if( pcti->on_time > daybegintime ) {
				tinfo[getsize].on_time = pcti->on_time - daybegintime ;
			}
			else {
				tinfo[getsize].on_time = 0 ;
			}
			// set clip off time
			tinfo[getsize].off_time = pcti->off_time - daybegintime ;
		}
		getsize++ ;
	}

	return getsize ;
}

// get locked file availability information
int dvrplaybackstream::getlockfileinfo(struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize )
{
	int i;
	int getsize = 0 ;
	struct cliptimeinfo * pcti ;
	int daybegintime, dayendtime ;		// convert begintime, endtime to seconds of the day

	// convert begintime, endtime to seconds of the day
	daybegintime = begintime->hour * 3600 +
				   begintime->min * 60 +
				   begintime->second ;
	dayendtime =   endtime->hour * 3600 +
				   endtime->min * 60 +
				   endtime->second ;

	if( dayendtime<=daybegintime ) {				// parameter error!
		return 0 ;
	}

	if( m_lockinfoday.day != begintime->day ||
		m_lockinfoday.month != begintime->month ||
		m_lockinfoday.year != begintime->year ) {		// day info cache available ?
			int clipcount ;
			tme_mutex_lock(&m_lockUI);
			m_lockinfoday = * begintime ;
			m_lockinfolist.setsize(2000) ;
			clipcount=net_stream_getlockfileinfo(m_socket, m_streamhandle, &m_lockinfoday, m_lockinfolist.at(0), 2000 ) ;
			m_lockinfolist.setsize(clipcount);
			m_lockinfolist.compact();
			tme_mutex_unlock(&m_lockUI);
	}

	for( i=0; i<m_lockinfolist.size(); i++) {
		pcti = m_lockinfolist.at(i) ;

		// check if time overlapped with begintime - endtime
		if( pcti->on_time >= dayendtime ||
			pcti->off_time <= daybegintime ) {
				continue ;				// clip time not included
		}

		if( tinfosize>0 && tinfo!=NULL ) {
			if( getsize>=tinfosize ) {
				break;
			}
			// set clip on time
			if( pcti->on_time > daybegintime ) {
				tinfo[getsize].on_time = pcti->on_time - daybegintime ;
			}
			else {
				tinfo[getsize].on_time = 0 ;
			}
			// set clip off time
			tinfo[getsize].off_time = pcti->off_time - daybegintime ;
		}
		getsize++ ;
	}
	return getsize ;
}

