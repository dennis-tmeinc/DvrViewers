
#include "stdafx.h"
#include <process.h>
#include "ply606.h"
#include "dvrnet.h"
#include "dvrplaybackstream.h"
#include "player.h"
#include "mp4util.h"
#include "trace.h"
#include "utils.h"

#define USE_PBTHREAD
#define USE_MUTEX

struct dvr_t {
	unsigned char year, mon, day, hour;
	unsigned char min, sec;
	unsigned short millisec;
};

unsigned WINAPI RemotePlaybackThreadFunc(PVOID pvoid) {
	dvrplaybackstream *p = (dvrplaybackstream *)pvoid;
	p->PlaybackThread();
	_endthreadex(0);
	return 0;
}

// DVR stream in interface
//     a single channel dvr data streaming in 

// open dvr server stream for playback
dvrplaybackstream::dvrplaybackstream(char * servername, int channel)
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
}

dvrplaybackstream::~dvrplaybackstream()
{
	m_bDie = true;
#ifdef USE_MUTEX
    tme_mutex_lock(&m_lock);
	if (m_bWaiting)
	    tme_cond_signal(&m_wait);
	tme_mutex_unlock(&m_lock);
#endif
	if (m_hThread != NULL) {
        _RPT0(_CRT_WARN, "Joining remote playback thread\n" );
		tme_thread_join(m_hThread);
	}
	m_hThread = NULL;

    tme_mutex_lock(&m_lockUI);
	if( m_socket!=INVALID_SOCKET && m_streamhandle>0 ) {
		//TRACE(_T("+++++++++++++++++++++++++++++closestream:%d\n"), m_channel);
		net_stream_close(m_socket, m_streamhandle);
	}

	net_close(m_socket);
    tme_mutex_unlock(&m_lockUI);
	// delete critical section

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
	struct dvrtime curtime;
	int framesize;

	while (!m_bDie && !m_bError) {
		//_RPT1(_CRT_WARN, "PlaybackThread lock:(%d)\n", m_channel);
		if (m_pFifo->i_depth > 60) {
			//_RPT2(_CRT_WARN, "wait:%d(%d)\n",m_pFifo->i_depth, m_channel);
#ifdef USE_MUTEX
			tme_mutex_lock(&m_lock);
			m_bWaiting = true;
			tme_cond_wait(&m_wait, &m_lock);
			m_bWaiting = false;
			tme_mutex_unlock(&m_lock);
#else
			Sleep(10);
			continue;
#endif
			//_RPT2(_CRT_WARN, "wait ok:%d(%d)\n",m_pFifo->i_depth, m_channel);
		}
		if (m_bDie) {
			break;
		}
#if 0
		enter_critical();
		if (m_seekreq) {
			m_seekreq = 0;
			m_curtime=m_seektime ;
			seek2();
			leave_critical();		
		    block_FifoEmpty( m_pFifo );
			enter_critical();
		}
		leave_critical();		
#endif
		bool gotData = false;
		tme_mutex_lock(&m_lockUI);
		ret = net_stream_getdata2(m_socket, m_streamhandle, rxbuf, sizeof(rxbuf), &framesize, &curtime);
		if (ret > 0) {
			gotData = true;
			//_RPT2(_CRT_WARN, "getdata2:%d(%d)\n",framesize, m_channel);
			//if (net_stream_gettime(m_socket, m_streamhandle, &curtime)) {
			m_bBufferPending = true;
			block_t *p_block;
			p_block = getMpegFrameDataFromPacket2(rxbuf, framesize);
			if (p_block) {
				//TRACE(_T("getdata2 ch[%d](%I64d:%d)\n"),m_channel,p_block->i_dts,p_block->i_buffer);
				// use i_length for frametime
				struct dvr_t *dv = (struct dvr_t *)&p_block->i_length;
				dv->year = curtime.year - 2000;
				dv->mon = curtime.month;
				dv->day = curtime.day;
				dv->hour = curtime.hour;
				dv->min = curtime.min;
				dv->sec = curtime.second;
				dv->millisec = curtime.millisecond;
				//printDvrtime(&curtime);
				block_FifoPut( m_pFifo, p_block );
			}
			m_bBufferPending = false;
			//}
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
	TRACE(_T("+++++++++++++++++++++++++++++openstream:%d\n"), m_channel);
	return (m_streamhandle>0 );
}

// open file share
char * dvrplaybackstream::openshare()
{
	return NULL ;
}

block_t *dvrplaybackstream::getdatablock()
{
	block_t *p_block;
	struct frame_t * framet ;
	int res = 0 ;
	mtime_t ts;

	if( m_socket == INVALID_SOCKET ) {
		return NULL;
	}

	if (m_hThread == NULL) {
		m_hThread = tme_thread_create( &g_ipc, "Remote Playback Thread",
			RemotePlaybackThreadFunc, this,				
			THREAD_PRIORITY_NORMAL, false );
		return NULL;
	}

	m_bSeekReq = false;

	if (m_pFifo->i_depth < 1) {
		return NULL;
	}

	//_RPT1(_CRT_WARN, "fifoget(%d)\n", m_channel);
	if( ( p_block = block_FifoGet( m_pFifo ) ) == NULL )
	{
		_RPT1(_CRT_WARN, "fifoget error=================(%d)\n", m_channel);
		m_bError = true;
		return NULL;		
	}

	// get the current time info from i_length(from playback thread) and set it back to 0
	ts = p_block->i_length;
	p_block->i_length = 0;

	// let the thread know fifo is no longer full.
    tme_mutex_lock(&m_lock);
	if (m_bWaiting) {
		tme_cond_signal(&m_wait);
	}
	tme_mutex_unlock(&m_lock);

	__int64 fTimestamp = 0;
	PBYTE pData;
	int size;

	pData = p_block->p_buffer;
	size = p_block->i_buffer;
	fTimestamp = max(p_block->i_pts, p_block->i_dts);

	if( m_encryptmode==ENC_MODE_RC4FRAME &&
		p_block->i_flags != BLOCK_FLAG_TYPE_AUDIO ) {		// decrypt RC4 frame
		// Decrypt frame data
		int dec_size = size;
		if( dec_size>1024 ) {
			dec_size=1024 ;
		}
		// TODO: decrypt only video
		RC4_block_crypt((unsigned char *)(pData), dec_size, 0, m_RC4_table, sizeof(m_RC4_table) ); 
	}

	tme_mutex_lock(&m_lockTime);
	if(  m_keyframestamp == 0 || 
		 fTimestamp<m_keyframestamp || 
		 (fTimestamp-m_keyframestamp)>1000000LL ) {
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
	} else {
		m_curtime = m_keyframetime ;
		__int64 diff = fTimestamp-m_keyframestamp;
		__int64 diff_in_milsec = diff / 1000;
		dvrtime_addmilisecond(&m_curtime, diff_in_milsec);
	}
	tme_mutex_unlock(&m_lockTime);

	//TRACE(_T("GetData ch[%d](%I64d:%d)\n"),m_channel,p_block->i_dts,p_block->i_buffer);
	return p_block;
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

	TRACE(_T("+++++++++++++++++++++++++++++seekstream:%d\n"), m_channel);
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
	TRACE(_T("---seek(%d):%d-%d-%d,%d:%d:%d\n"),m_channel,dvrt->year,  dvrt->month,dvrt->day,dvrt->hour,dvrt->min,  dvrt->second);

	int ret;
	struct dvrtime curtime;
	tme_mutex_lock(&m_lockUI);
	m_bSeekReq = true;
    block_FifoEmpty( m_pFifo );
	tme_mutex_lock(&m_lockTime);
	curtime = m_curtime =*dvrt ;
	m_keyframestamp=0;
	tme_mutex_unlock(&m_lockTime);
	ret = seek2(&curtime);
	// let the thread know fifo is no longer full.
	tme_mutex_lock(&m_lock);
	if (m_bWaiting) {
		//TRACE(_T("signal-->:%d(%d)\n"),m_pFifo->i_depth, m_channel);
		tme_cond_signal(&m_wait);
	}
	tme_mutex_unlock(&m_lock);
		//TRACE(_T("signal--> ok:%d(%d)\n"),m_pFifo->i_depth, m_channel);
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

