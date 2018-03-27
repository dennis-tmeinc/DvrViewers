
#include "stdafx.h"
#include <stdio.h>
#include <time.h>
#include <process.h>
#include "ply606.h"
#include "dvrnet.h"
#include "dvrpreviewstream.h"
#include "inputstream.h"
#include "mp4util.h"
#include "trace.h"


unsigned WINAPI LivePlaybackThreadFunc(PVOID pvoid) {
	dvrpreviewstream *p = (dvrpreviewstream *)pvoid;
	p->PlaybackThread();
	_endthreadex(0);
	return 0;
}

// DVR stream in interface
//     a single channel dvr data streaming in 

// open dvr server stream for preview
dvrpreviewstream::dvrpreviewstream(char * servername, int channel)
{
	m_channel=channel ;
	strncpy( m_server, servername, sizeof(m_server));
	m_socket=INVALID_SOCKET;
	m_shutdown=0 ;
	m_keydata = NULL ;
	m_voscSize = 0;
	m_gotVideoKeyFrame = false;
	m_bufsize = 0;
	m_bDie = m_bError = false;
	m_hThread = NULL;
	m_curTime = 0;
	//tme_thread_init(&m_ipc);
    tme_mutex_init(&g_ipc, &m_lock);
    tme_mutex_init(&g_ipc, &m_lockPts);
}

dvrpreviewstream::~dvrpreviewstream()
{
	shutdown();
	tme_mutex_destroy(&m_lockPts);
	tme_mutex_destroy(&m_lock);
	//tme_mutex_destroy(&(m_ipc.object_lock));
	//tme_cond_destroy(&(m_ipc.object_wait));
}

block_t *dvrpreviewstream::getdatablock()
{
	block_t *pBlock = NULL;
	int ret;

	//TRACE(_T("getdatablock(ch%d)\n"), m_channel);
	if( m_shutdown ) {
		return NULL;
	}

	if (m_hThread == NULL) {
		m_hThread = tme_thread_create( &g_ipc, "Live Playback Thread",
			LivePlaybackThreadFunc, this,				
			THREAD_PRIORITY_NORMAL, false );
		return NULL;
	}

#if 1
	/* Here we have
	 *  1. header + frame in p_block (size in i_buffer)
	 *  2. frametype in i_flags
	 */
	tme_mutex_lock(&m_lock);
	pBlock = getMpegFrameDataFromPacket(m_buf, &m_bufsize, m_videoObjectStartCode, m_voscSize, m_channel);
	tme_mutex_unlock(&m_lock);
	if (pBlock == NULL)
		return NULL;

	tme_mutex_lock(&m_lockPts);
	m_curTime = (pBlock->i_dts > 0) ? pBlock->i_dts : pBlock->i_pts;
	tme_mutex_unlock(&m_lockPts);

	if (!m_gotVideoKeyFrame) {
		if (pBlock->i_flags == BLOCK_FLAG_TYPE_I) {
			m_gotVideoKeyFrame = true;
		} else {
			// wait until 1st video key frame
			block_Release(pBlock);
			return NULL;
		}
	}
#else
	//m_bufsize = 0;
#endif
	return pBlock;
}

// get current time and timestamp
int dvrpreviewstream::gettime( struct dvrtime * dvrt )
{
	time_t t ;
	struct tm stm ;

	tme_mutex_lock(&m_lockPts);
	t = (time_t)(m_curTime / 1000000LL);
	tme_mutex_unlock(&m_lockPts);

	localtime_s( &stm, &t);
	dvrt->year = 1900+stm.tm_year ;
	dvrt->month = 1 + stm.tm_mon ;
	dvrt->day = stm.tm_mday ;
	dvrt->hour = stm.tm_hour ;
	dvrt->min = stm.tm_min ;
	dvrt->second = stm.tm_sec ;
	dvrt->millisecond = (int)(m_curTime % 1000000LL) / 1000 ;
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
	if( m_socket != INVALID_SOCKET ) {
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
		if( m_socket == INVALID_SOCKET ) {
			connectDvr( m_server );
			if( m_socket != INVALID_SOCKET ) {
				// TVS key block
				if( m_keydata ) {
					net_CheckKey(m_socket, (char *)m_keydata,
						m_keydata->size + sizeof( m_keydata->checksum ) );
				}
				ret = openStream();
				if( ret ) {
					tme_mutex_unlock(&m_lock);
					net_close(m_socket);
					m_socket = INVALID_SOCKET ;
					Sleep(1000);
					continue;
				}
			}
		}
		tme_mutex_unlock(&m_lock);
		if( m_socket == INVALID_SOCKET ) {
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
				m_socket = INVALID_SOCKET;
				continue;
			}
		} else {
			closesocket(m_socket);
			m_socket = INVALID_SOCKET;
			continue;
		}
		tme_mutex_lock(&m_lock);
		if (bytes > 0) {
			//TRACE(_T("recved %d\n"), bytes);
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

int dvrpreviewstream::connectDvr(char *server)
{
	LPHOSTENT hostEntry;
	int nret;

	hostEntry = gethostbyname(server);	// Specifying the server by its name;

	if (!hostEntry)
	{
		nret = WSAGetLastError();
		return 1;
	}


	m_socket = socket(AF_INET,			// Go over TCP/IP
			   SOCK_STREAM,			// This is a stream-oriented socket
			   IPPROTO_TCP);		// Use TCP rather than UDP

	if (m_socket == INVALID_SOCKET)
	{
		nret = WSAGetLastError();
		return 1;
	}


	// Fill a SOCKADDR_IN struct with address information
	SOCKADDR_IN serverInfo;

	serverInfo.sin_family = AF_INET;

	// At this point, we've successfully retrieved vital information about the server,
	// including its hostname, aliases, and IP addresses.  Wait; how could a single
	// computer have multiple addresses, and exactly what is the following line doing?
	// See the explanation below.

	serverInfo.sin_addr = *((LPIN_ADDR)*hostEntry->h_addr_list);

	serverInfo.sin_port = htons(PORT_TMEDVR);		// Change to network-byte order and
							// insert into port field

	// Connect to the server
	nret = connect(m_socket,
		       (LPSOCKADDR)&serverInfo,
		       sizeof(struct sockaddr));

	if (nret == SOCKET_ERROR)
	{
		nret = WSAGetLastError();
		closesocket(m_socket);
		m_socket = INVALID_SOCKET;
		return 1;
	}

	return 0;
}

int dvrpreviewstream::openStream()
{
	struct Req_type req ;
	struct Answer_type ans ;
	int ret;

	req.data=m_channel ;
	req.reqcode=REQOPENLIVE;
	req.reqsize = 0 ;

	ret = send(m_socket, (char *)&req, sizeof(req), 0);
	if (ret == SOCKET_ERROR) {
		closesocket(m_socket);
		m_socket = INVALID_SOCKET;
		return 1;
	}

	struct timeval tv = {5, 0};
	ret = read_nbytes(m_socket, (PBYTE)&ans, sizeof(ans), sizeof(ans), &tv, true);
	if ((ret < sizeof(ans)) || (ans.anscode != ANSSTREAMOPEN)) {
		closesocket(m_socket);
		m_socket = INVALID_SOCKET;
		return 1;
	}

	ret = read_nbytes(m_socket, (PBYTE)&ans, sizeof(ans), sizeof(ans), &tv, true);
	if ((ret < sizeof(ans)) || (ans.anscode != ANSSTREAMDATA)) {
		closesocket(m_socket);
		m_socket = INVALID_SOCKET;
		return 1;
	}

	if (ans.data == 10) {
		if (ans.anssize > VOSC_MAX_SIZE) {			
			ans.anssize = VOSC_MAX_SIZE;
		}

		ret = read_nbytes(m_socket, (PBYTE)m_videoObjectStartCode, VOSC_MAX_SIZE, ans.anssize, &tv, true);
		if (ret < ans.anssize) {
			closesocket(m_socket);	
			m_socket = INVALID_SOCKET;
			return 1;
		}						
		m_voscSize = ans.anssize;
	}

	return 0;
}

