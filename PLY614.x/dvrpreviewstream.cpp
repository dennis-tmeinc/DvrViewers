
#include "stdafx.h"
#include <process.h>
#include "ply614.h"
#include "dvrnet.h"
#include "dvrpreviewstream.h"

// DVR stream in interface
//     a single channel dvr data streaming in 

unsigned WINAPI LivePlaybackThreadFunc(PVOID pvoid) {
	dvrpreviewstream *p = (dvrpreviewstream *)pvoid;
	p->PlaybackThread();
	_endthreadex(0);
	return 0;
}

int blockUntilReadable(SOCKET socket, struct timeval* timeout) {
	int result = -1;
	do {
		fd_set rd_set;
		FD_ZERO(&rd_set);
		if (socket < 0) break;
		FD_SET((unsigned) socket, &rd_set);
		int numFds = (int)socket+1;

		result = select(numFds, &rd_set, NULL, NULL, timeout);
		if (timeout != NULL && result == 0) {
			break; // this is OK - timeout occurred
		} else if (result <= 0) {
			_RPT0(_CRT_WARN, "blockUntilReadable:select() error\n");
			break;
		}

		if (!FD_ISSET(socket, &rd_set)) {
			_RPT0(_CRT_WARN, "blockUntilReadable:select() error - !FD_ISSET\n");
			break;
		}
	} while (0);

	return result;
}

// open dvr server stream for preview
dvrpreviewstream::dvrpreviewstream(char * servername, int channel)
{
	m_channel=channel ;
	strncpy( m_server, servername, sizeof(m_server));
	m_socket=0;
//	m_socket=net_connect( m_server );
//	net_preview_play(m_socket, m_channel );
	m_shutdown=0 ;
	m_keydata = NULL;
	m_bufsize = 0;
	m_bDie = m_bError = false;
	m_hThread = NULL;
	//tme_thread_init(&m_ipc);
    tme_mutex_init(&g_ipc, &m_lock);
}

dvrpreviewstream::~dvrpreviewstream()
{
	shutdown();
	tme_mutex_destroy(&m_lock);
	//tme_mutex_destroy(&(m_ipc.object_lock));
	//tme_cond_destroy(&(m_ipc.object_wait));
}

// get dvr data frame
int dvrpreviewstream::getdata(char **framedata, int * framesize, int * frametype, int *headerlen)
{
	int ret = 0;

	*frametype = FRAME_TYPE_UNKNOWN ;
	*framedata = NULL ;
	*framesize = 0 ;

	if( m_shutdown ) {
		return 0 ;
	}

	if (m_hThread == NULL) {
		m_hThread = tme_thread_create( &g_ipc, "Live Playback Thread",
			LivePlaybackThreadFunc, this,				
			THREAD_PRIORITY_NORMAL, false );
		return 0;
	}

	tme_mutex_lock(&m_lock);
	if (m_bufsize) {
		*framedata = (char *)malloc(m_bufsize) ;
		if (*framedata) {
			memcpy(*framedata, m_buf, m_bufsize);
			*framesize = m_bufsize;
			m_bufsize = 0;
			ret = 1;
		}
	}
	tme_mutex_unlock(&m_lock);
	
	return ret ;
}

// get current time and timestamp
int dvrpreviewstream::gettime( struct dvrtime * dvrt )
{
	time_t t ;
	struct tm stm ;
	time(&t);								// get current computer time
	localtime_s( &stm, &t);
	dvrt->year = 1900+stm.tm_year ;
	dvrt->month = 1 + stm.tm_mon ;
	dvrt->day = stm.tm_mday ;
	dvrt->hour = stm.tm_hour ;
	dvrt->min = stm.tm_min ;
	dvrt->second = stm.tm_sec ;
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
	m_shutdown = 1;

	m_bDie = true;
	if (m_hThread != NULL) {
        _RPT0(_CRT_WARN, "Joining live playback thread\n" );
		tme_thread_join(m_hThread);
	}
	m_hThread = NULL;

	tme_mutex_lock(&m_lock);
	if( m_socket != 0 ) {
		net_close(m_socket);
	}
	tme_mutex_unlock(&m_lock);

	return 0;
}

int dvrpreviewstream::PlaybackThread()
{
	int ret, bytes;
	struct dvrtime curtime;
	int framesize;
	int error = 0;
	BYTE rxbuf[10 * 1024];
	int dummy;

	while (!m_bDie && !m_bError) {
		tme_mutex_lock(&m_lock);
		if( m_socket == 0 ) {
			m_socket=net_connect( m_server, false );
			if( m_socket > 0 ) {
				// TVS key block
				if( m_keydata ) {	
					net_CheckKey(m_socket, (char *)m_keydata, m_keydata->size + sizeof( m_keydata->checksum ) );
				}
				ret = net_preview_play(m_socket, m_channel );
				if( !ret ) {
					tme_mutex_unlock(&m_lock);
					net_close(m_socket);
					m_socket = 0 ;
					Sleep(1000);
					continue;
				}
			}
		}
		tme_mutex_unlock(&m_lock);

		if( m_socket == 0 ) {
			Sleep(1000);
			continue;
		}

		if (m_bDie) {
			break;
		}

		struct timeval tv = {5, 0};
		if (blockUntilReadable(m_socket, &tv) > 0) {
			bytes = recv(m_socket, (char *)rxbuf, sizeof(rxbuf), 0);
			if (bytes <= 0) {
				closesocket(m_socket);
				m_socket = 0;
				continue;
			}
		} else {
			TRACE(_T("ch %d timeout\n"), m_channel);
			closesocket(m_socket);
			m_socket = 0;
			continue;
		}
		tme_mutex_lock(&m_lock);
		if (bytes > 0) {
			//TRACE(_T("ch %d recved %d\n"), m_channel, bytes);
			if (m_bufsize + bytes > NET_BUFSIZE) {
				m_bufsize = 0;
			}
			memcpy(m_buf + m_bufsize, rxbuf, bytes);
			m_bufsize += bytes;
		}
		tme_mutex_unlock(&m_lock);
	}

	return 0;
}

