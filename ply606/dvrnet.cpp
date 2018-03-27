
// dvrnet.cpp
// DVR 614 net protocol
#include "stdafx.h"
#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <lm.h>
#include "dvrnet.h"
#include "player.h"
#include "dvrpreviewstream.h"
#include "trace.h"

//#pragma comment(lib, "Netapi32.lib")
#pragma comment(lib, "Ws2_32.lib")

struct system_stru {
	//char IOMod[80]  ;
	char productid[8] ;
	char serial[24] ;
	char id1[24] ;
	char id2[24] ;
	char ServerName[80] ;
	int cameranum ;
	int alarmnum ;
	int sensornum ;
	int breakMode ;
	int breakTime ;
	int breakSize ;
	int minDiskSpace ;
	int shutdowndelay ;
	char autodisk[32] ;
	char sensorname[16][32];
	int sensorinverted[16];
	int eventmarker_enable ;
	int eventmarker ;
	int eventmarker_prelock ;
	int eventmarker_postlock ;
	char ptz_en ;
	char ptz_port ;			// COM1: 0
	char ptz_baudrate ;		// times of 1200
	char ptz_protocol ;		// 0: PELCO "D"   1: PELCO "P"
	int  videoencrpt_en ;	// enable video encryption
	char videopassword[32] ;
	char res[88];
};

USHORT g_dvr_port = PORT_TMEDVR;
int g_lan_detect ;
int g_net_watchdog ;
int g_remote_support ;

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

int read_nbytes(SOCKET sock, PBYTE buf, int bufsize, int rx_size, 
	struct timeval *timeout_tv, bool readExact)
{
	struct timeval tv;
	int total = 0, bytes, toread;

	if (bufsize < rx_size)
		return -1;

	while (1) {
		bytes = 0;
		tv = *timeout_tv;
		if (blockUntilReadable(sock, &tv) > 0) {
			toread = bufsize - total;
			if (readExact) {
				toread = rx_size - total;
			}

			bytes = recv(sock, (char *)buf + total, toread, 0);
			if (bytes > 0) {
				total += bytes;
			}
			if (total >= rx_size)
				break;
		}

		if (bytes <= 0) {
			break;
		}
	}

	return total;
}

// convert computer name (string) to sockad ( cp name example: 192.168.42.227 or 192.168.42.227:15111 )
// return 0 for success
int net_sockaddr(struct sockad * sad, char * cpname, int port)
{
	int result = -1 ;
	char ipaddr[256] ;
	char servname[128] ;
	struct addrinfo * res ;
	struct addrinfo hint ;
	char * portonname ;
	memset(&hint, 0, sizeof(hint));
	hint.ai_family = PF_INET ;
	hint.ai_socktype = SOCK_STREAM ;
	strncpy( ipaddr, cpname, 255 );
	ipaddr[255]=0 ;
	portonname = strchr(ipaddr, ':');
	if( portonname ) {
		strncpy( servname, portonname+1, 120 );
		*portonname=0 ;
	}
	else {
		sprintf(servname, "%d", port);
	}
	if( getaddrinfo( ipaddr, servname, &hint, &res )==0 ) {
		if( res->ai_socktype == SOCK_STREAM ) {
			sad->len = res->ai_addrlen ;
			memcpy(&(sad->addr), res->ai_addr, sad->len );
			result = 0 ;
		}
		freeaddrinfo(res);
	}
	return result ;
}

// convert sockad to computer name
// return 0 for success
int net_sockname(char * cpname, struct sockad * sad)
{
	return getnameinfo( &(sad->addr), sad->len, cpname, 128, NULL, 0, NI_NUMERICHOST);
}

// return 1 if socket ready to read, 0 for time out
int net_wait(SOCKET sock, int timeout)
{
	struct timeval tout ;
	if( timeout==0 ) {
		timeout=1;
	}
	fd_set set ;
	FD_ZERO(&set);
	FD_SET(sock, &set);
	tout.tv_sec=timeout/1000000 ;
	tout.tv_usec=(timeout%1000000)|1 ;
	return select( (int)sock+1, &set, NULL, NULL, &tout);
}

// clear received buffer
char dummybuf[100000] ;
void net_clear(SOCKET sock)
{
	int r;
	while( net_wait(sock, 1)==1 ) {
		r=recv(sock, dummybuf, 100000, 0);
		if( r<=0 ) {
			break;
		}
	}
}

// try to force server flush out data
void net_pingdata(SOCKET so)
{
/*
	char echobuf[100] ;
	struct Req_type req ;
	req.reqcode = REQECHO ;
	req.data = 0 ;
	req.reqsize = sizeof(echobuf);
	net_send( so, &req, sizeof(req) );
	net_send( so, echobuf, req.reqsize );
*/
}

// Shutdown socket
int net_shutdown( SOCKET s )
{
	return shutdown( s, SD_BOTH );
}

int serror ;
// send data with clear receiving buffer
void net_send( SOCKET sock, void * buf, int bufsize )
{
	int slen ;
	char * cbuf=(char *)buf ;
	while( bufsize>0 ) {
		slen=send(sock, cbuf, bufsize, 0);
		if( slen<0 ) return ;
		bufsize-=slen ;
		cbuf+=slen ;
	}
}

// recv network data until buffer filled. 
// return 1: success
//		  0: failed
int net_recv( SOCKET sock, void * buf, int bufsize, int wait )
{
	int retry=3 ;
	int ping=0;
	int recvlen ;
	int cbufsize=bufsize;
	char * cbuf = (char *) buf ;
	retry = wait/100000 ;
	while( cbufsize>0 ) {
		if( net_wait(sock, 100000)>0 ) {			// data available? Timeout 3 seconds
			recvlen=recv(sock, cbuf, cbufsize, 0 );
			if( recvlen>0 ) {
				cbufsize-=recvlen ;
				cbuf+=recvlen ;
			}
			else 
				return 0;								// recv 0 bytes, socket closed by the other size!
		}
		else {
			if( retry-->0 ) {
				net_pingdata(sock);
				ping++;
			}
			else {
				return 0;									// timeout or error
			}
		}
	}
	return 1;						// success
}

static DWORD net_finddevice_tick ;
static SOCKET net_find_sock = INVALID_SOCKET ;

int net_finddevice()
{
    INTERFACE_INFO InterfaceList[20];
    unsigned long nBytesReturned;
	DWORD req=REQDVREX ;
	struct sockad mcaddr ;		// multicase address
	int i;

	net_finddevice_tick = GetTickCount();

	if( net_find_sock == INVALID_SOCKET ) {
		net_find_sock=socket(AF_INET, SOCK_DGRAM, 0);
	}

	// send multicast dvr search requests (to our dvr mc adress:228.229.230.231)
	net_sockaddr( &mcaddr, "228.229.230.231", g_dvr_port);
	DWORD ttl=30;
	setsockopt(net_find_sock, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&ttl, sizeof(ttl));
	sendto(net_find_sock, (char *)&req, sizeof(req),0, &mcaddr.addr, mcaddr.len );

    if (WSAIoctl(net_find_sock, SIO_GET_INTERFACE_LIST, 0, 0, &InterfaceList,
		sizeof(InterfaceList), &nBytesReturned, 0, 0) == SOCKET_ERROR) {
		return 0;
    }

	// sending broadcast dvr search requests
    int nNumInterfaces = nBytesReturned / sizeof(INTERFACE_INFO);
    for ( i = 0; i < nNumInterfaces; ++i) {
		if( (InterfaceList[i].iiFlags & IFF_BROADCAST) &&				// can bcast
			(InterfaceList[i].iiFlags & IFF_UP) &&						// interface up
			(InterfaceList[i].iiAddress.Address.sa_family==AF_INET) )
		{				
			InterfaceList[i].iiAddress.AddressIn.sin_addr.S_un.S_addr |=
					~(InterfaceList[i].iiNetmask.AddressIn.sin_addr.S_un.S_addr) ;
			InterfaceList[i].iiAddress.AddressIn.sin_port=htons(g_dvr_port);
			sendto(net_find_sock, (char *)&req, sizeof(req),0, &InterfaceList[i].iiAddress.Address, sizeof(InterfaceList[i].iiAddress.AddressIn));
		}
    }

    return 	0 ;

}

// detecting device through ip address
int net_detectdevice( char * ipaddr )
{
	DWORD req=REQDVREX ;
	struct sockad saddr ;

	DWORD ttl=30;

	if( net_find_sock != INVALID_SOCKET ) {
		if( net_sockaddr(&saddr, ipaddr, g_dvr_port)==0 ) {
			sendto(net_find_sock, (char *)&req, sizeof(req),0, &saddr.addr, saddr.len );
		}
		net_finddevice_tick = GetTickCount();
	}

	return 	0 ;
}

int net_polldevice()
{
	struct sockad recvaddr ;
	int recvlen ;
	char buf[16] ;
	int i;
	char name[256];

	// receiving DVR responses
	while( net_wait(net_find_sock, 0)>0 ) {
		recvaddr.len = sizeof(recvaddr)-sizeof(int);
		recvlen=recvfrom(net_find_sock,  buf, 16,  0,  &(recvaddr.addr), &(recvaddr.len) );
		if( recvlen>=sizeof(DWORD) ) {
			if( *(DWORD *)buf==DVRSVREX ) {
				net_finddevice_tick = GetTickCount();
				if( net_sockname(name, &recvaddr)==0 ) {
					for( i=0; i<g_dvr_number; i++ ) {
						if( strcmp( g_dvr[i].name, name )==0 ) {
							break ;
						}
					}
					if( i==g_dvr_number && g_dvr_number < MAX_DVR_DEVICE ) {
						if (g_dvr[g_dvr_number].name == NULL)
							g_dvr[g_dvr_number].name = new char[DVRNAME_LEN];
						strcpy(g_dvr[g_dvr_number].name, name);
						g_dvr[g_dvr_number].type = DVR_DEVICE_TYPE_DVR ;
						g_dvr_number++ ;
					}
				}
			}
		}
	}
	if( (int)(GetTickCount()-net_finddevice_tick) > 5000 ) {
		closesocket( net_find_sock ) ;
		net_find_sock = INVALID_SOCKET ;
		return 1 ;
	}
	else {
		return 0 ;
	}
}

// connect to a DVR server
SOCKET net_connect( char * dvrserver )
{
	SOCKET so ;
	struct sockad soad ;
	if( net_sockaddr( &soad, dvrserver, g_dvr_port )==0 ) {
		so=socket(AF_INET, SOCK_STREAM, 0);
		if( connect(so, &(soad.addr), soad.len )==0 ) {
			BOOL nodelay=1 ;
			setsockopt( so, IPPROTO_TCP, TCP_NODELAY, (char *)&nodelay, sizeof(nodelay) );
			return so ;
		}
		closesocket(so);
	}
	return 0 ;
}

// close a connection
void net_close(SOCKET so)
{
	closesocket(so);
}

// get server(player) information. 
//        setup ppi->servername, ppi->total_channel, ppi->serverversion
// return 1: success
//        0: failed
int net_getserverinfo( SOCKET so, struct player_info * ppi )
{
	struct Req_type req ;
	struct Answer_type ans ;
	struct sockad sad ;
	struct system_stru sys ;

	int res=0 ;
	char * pbuf ;

	if( ppi->size<sizeof( struct player_info ) )
		return res ;

	net_clear(so);		// clear received buffer

	// Get DVR server IP name
	memset( &sad, 0, sizeof(sad));
	sad.len = sizeof(sad)-sizeof(sad.len) ;
	getpeername(so, &sad.addr, &sad.len);
	net_sockname(ppi->serverinfo, &sad);
	strncpy(ppi->servername, ppi->serverinfo, sizeof(ppi->servername));	// default server name

	// get server name
	req.reqcode = REQGETSYSTEMSETUP ;
	req.data = 0 ;
	req.reqsize = 0;
	net_send( so, &req, sizeof(req));
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANSSYSTEMSETUP && ans.anssize<MAX_ANSSIZE && ans.anssize>0 ) {
			pbuf = new char [ans.anssize] ;
			if( net_recv(so, pbuf, ans.anssize ) ) {
				if( ans.anssize>sizeof(struct system_stru) ) {
					ans.anssize = sizeof(struct system_stru) ;
				}
				memcpy( &sys, pbuf, ans.anssize ) ;
				memcpy(&(ppi->serverinfo[48]), sys.productid, 80 ) ;
				strncpy(ppi->servername, sys.ServerName, sizeof(ppi->servername));
				// check PTZ support
				if( sys.ptz_en ) {
					ppi->features |= PLYFEATURE_PTZ ;
				}
				ppi->total_channel = sys.cameranum ;
				res=1 ;
			}
			delete pbuf ;
		}
	}

	// get DVR version
	req.reqcode = REQGETVERSION ;
	req.data = 0 ;
	req.reqsize = 0;
	net_send( so, &req, sizeof(req) );
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANSGETVERSION && ans.anssize<MAX_ANSSIZE && ans.anssize>0 ) {
			pbuf = new char [ans.anssize] ;
			if( net_recv(so, pbuf, ans.anssize ) ) {
				if( ans.anssize >= 4*sizeof(int) ) {
					memcpy( ppi->serverversion, pbuf, 4*sizeof(int));
					res=1 ;
				}
			}
			delete pbuf ;
		}
	}

/*
	// get channel number
	ppi->total_channel = 0 ;

	req.reqcode = REQCHANNELINFO ;
	req.data = 0 ;
	req.reqsize = 0;
	net_send( so, &req, sizeof(req));

	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANSCHANNELDATA ) {
			ppi->total_channel = ans.data ;
			res=1 ;
		}
		if( ans.anssize<MAX_ANSSIZE && ans.anssize>0 ) {
			pbuf = new char [ans.anssize] ;
			net_recv(so, pbuf, ans.anssize );
			delete pbuf ;
		}
	}
*/

	return res;
}

// return number of DVR video channels
// return 0: error, 1: success
int net_channelinfo( SOCKET so, struct channel_info * pci, int totalchannel)
{
	int res=0;
	int i;

	net_clear(so);		// clear received buffer

	DvrChannel_attr	cam_attr ;
	for( i=0; i<totalchannel ; i++ ) {
		memset(&cam_attr, 0, sizeof( cam_attr ) );
		if( net_GetChannelSetup(so, &cam_attr, sizeof( cam_attr ), i ) ) {
			strncpy( pci[i].cameraname, cam_attr.CameraName, sizeof( pci[i].cameraname ) );
			if( cam_attr.Enable ) {
				pci[i].features |= 1 ;			// enabled bit
			}
			if( cam_attr.ptz_addr >= 0 ) {
				pci[i].features |= 2 ;
			}
			pci[i].status = 0 ;
			if( cam_attr.Resolution==0 ) {		// cif
				pci[i].xres = 352 ;
				pci[i].yres = 240 ;
			}
			else if( cam_attr.Resolution==1 ) {	// 2cif
				pci[i].xres = 704 ;
				pci[i].yres = 240 ;
			}
			else if( cam_attr.Resolution==2 ) {	// dcif
				pci[i].xres = 528 ;
				pci[i].yres = 320 ;
			}
			else if( cam_attr.Resolution==3 ) {	// 4cif
				pci[i].xres = 704 ;
				pci[i].yres = 480 ;
			}
			res=1 ;
		}
	}
	return res ;
}



// Get DVR server version
// parameter
//		so: socket
//		version: 4 int array 
// return 0: error, 1: success
int net_dvrversion(SOCKET so, int * version)
{
	int res = 0 ;
	struct Req_type req ;
	struct Answer_type ans ;
	net_clear(so);		// clear received buffer
	req.reqcode = REQGETVERSION ;
	req.data = 0 ;
	req.reqsize = 0;
	net_send( so, &req, sizeof(req) );
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANSGETVERSION && ans.anssize<MAX_ANSSIZE && ans.anssize>0 ) {
			char * pbuf = new char [ans.anssize] ;
			if( net_recv(so, pbuf, ans.anssize ) ) {
				if( ans.anssize >= 4*sizeof(int) ) {
					memcpy( version, pbuf, 4*sizeof(int));
					res=1 ;
				}
			}
			delete pbuf ;
		}
	}
	return res;
}

// Start play a preview channel
// return 1 : success
//        0 : failed
int net_preview_play( SOCKET so, int channel )
{
	int res = 0 ;
	struct Req_type req ;
	struct Answer_type ans ;
	net_clear(so);		// clear received buffer
	req.data=channel ;
	req.reqcode=REQREALTIME;
	req.reqsize = 0 ;
	net_send( so, &req, sizeof(req) );
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANSREALTIMEHEADER && ans.anssize<MAX_ANSSIZE && ans.anssize>0 ) {
			char * pbuf = new char [ans.anssize] ;
			if( net_recv(so, pbuf, ans.anssize ) ) {
#ifdef TEST_301
				if( pbuf[0]== '4' &&
					pbuf[1]== 'H' && 
					pbuf[2]== 'K' && 
					pbuf[3]== 'H' ){									// check hikvision H.264 header
#else
				if (*((int *)pbuf) == FOURCC_HKMI) {
#endif
						res=1 ;
				}
			}
			delete pbuf ;
		}
	}
	return res ;
}

// Get data from preview stream
// return 1 : success
//        0 : failed
int net_preview_getdata( SOCKET so, char ** framedata, int * framesize )
{
	if( net_wait(so, RECV_TIMEOUT)==1 ) {
		* framedata = (char *)malloc(102400) ;
		* framesize = recv(so, *framedata, 102400, 0) ;
		if( *framesize<=0 ) {
			free(*framedata);
			* framedata = NULL ;
			* framesize = 0 ;
			return 0 ;			// failed 
		}
		return 1 ;
	}
	return 0 ;
}

// open a live stream
// return 0 : failed
//        1 : success
int net_live_open( SOCKET so, int channel, char *vosc, int *voscSize )
{
	struct Req_type req ;
	struct Answer_type ans ;

	*voscSize = 0;

	net_clear(so);		// clear received buffer
	req.data=channel ;
	req.reqcode=REQOPENLIVE;
	req.reqsize = 0 ;
	net_send( so, &req, sizeof(req) );
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANSSTREAMOPEN ) {
			if( ans.anssize<MAX_ANSSIZE && ans.anssize>0 ) {
				// will never happen
				char * pbuf = new char [ans.anssize] ;
				net_recv(so, pbuf,ans.anssize);
				delete pbuf ;
			}
			// get video object start code
			if( net_recv( so, &ans, sizeof(ans))) {
				if( ans.anscode == ANSSTREAMDATA ) {
					if( ans.anssize<MAX_ANSSIZE && ans.anssize>0 ) {
						// consume the data
						char * pbuf = new char [ans.anssize] ;
						net_recv(so, pbuf,ans.anssize);

						if (ans.data == 10) {
							if (ans.anssize > VOSC_MAX_SIZE) {
								ans.anssize = VOSC_MAX_SIZE;
							}
							memcpy(vosc, pbuf, ans.anssize);
							*voscSize = ans.anssize;
						}
						delete pbuf ;

						return *voscSize ? 1 : 0 ;
					}
				}
			}
		}
	}
	return 0 ;
}

// Get as much data as possible from live stream
int net_live_getdata( SOCKET so, PBYTE buf, int *buflen, int ch )
{
	int recvlen;
#if 0
	struct Answer_type *pAns;
	if (*buflen > sizeof(struct Answer_type)) {
		pAns = (struct Answer_type *)buf;
		if (pAns->anscode != ANSSTREAMDATA) {
			*buflen = 0;
			TRACE(_T("**********************\n"));
		}
	}
#endif
	if( net_wait(so, RECV_TIMEOUT)==1 ) {
		TRACE(_T("ch %d recving:%d,%d\n"), ch,NET_BUFSIZE - *buflen, *buflen);
		recvlen=recv(so, (char *)(buf + *buflen), NET_BUFSIZE - *buflen, 0);
		if (recvlen > 0) {
			TRACE(_T("ch %d recvd:%d\n"), ch,recvlen);
			*buflen += recvlen;
			return recvlen;
		}
	}
	TRACE(_T("Timeout.@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"));
	return 0 ;
}
#if 0
// int get DVR share root directory and password,
// parameter:
//		root: minimum size MAX_PATH bytes
int net_getshareinfo(SOCKET so, char * root )
{
	struct Req_type req ;
	struct Answer_type ans ;
	int res=0 ;
	char serveripname[128] ;		// DVR server ip name
	char password[128] ;
	struct sockad sad ;

	net_clear(so);		// clear received buffer

	memset( &sad, 0, sizeof(sad));
	sad.len = sizeof(sad)-sizeof(sad.len) ;

	// get DVR server ip name and share root
	getpeername(so, &sad.addr, &sad.len);
	net_sockname(serveripname, &sad);
	sprintf(root, "\\\\%s\\share$", serveripname );

	// get DVR server share password
	strcpy(password, "dvruser");	// default password
	req.reqcode = REQSHAREPASSWD ;
	req.data = 0 ;
	req.reqsize = 0 ;
	net_send( so, &req, sizeof(req) );
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANSSHAREPASSWD && ans.anssize<MAX_ANSSIZE && ans.anssize>0 ) {
			char * pbuf = new char [ans.anssize] ;
			if( net_recv(so, pbuf, ans.anssize ) ) {
				if( ans.anssize>128 )  {
					ans.anssize=128 ;
				}
				strncpy( password, pbuf, 128 );
				res = 1 ;
			}
			delete pbuf ;
		}
	}


	USE_INFO_2 useinfo ;
	DWORD error ;
	wchar_t wpasswd[128] ;
	wchar_t wremote[MAX_PATH] ;
	memset( &useinfo, 0, sizeof(useinfo));

	MultiByteToWideChar(CP_ACP, 0, root, (int)strlen(root)+1, wremote, MAX_PATH);
	MultiByteToWideChar(CP_ACP, 0, password, (int)strlen(password)+1, wpasswd, 128 );

	useinfo.ui2_remote = (LMSTR)wremote ;
	useinfo.ui2_username = (LMSTR)(L"dvruser");
	useinfo.ui2_password = wpasswd;
	error=0;
	NetUseAdd(NULL, 2, (LPBYTE)&useinfo, &error);

	return res ;
}
#endif
// open a play back stream
//   return stream handle
//          -1 for error ;
int net_stream_open(SOCKET so, int channel)
{
	int res = -1 ;
	struct Req_type req ;
	struct Answer_type ans ;

	net_clear(so);		// clear received buffer

	req.reqcode = REQSTREAMOPEN ;
	req.data = channel ;
	req.reqsize = 0 ;
	net_send( so, &req, sizeof(req) );
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANSSTREAMOPEN ) {
			res=ans.data ;
		}
	}
	return res ;
}

// close playback stream
// return 1: success
//        0: error
int net_stream_close(SOCKET so, int handle)
{
	int res = 0 ;
	struct Req_type req ;
	struct Answer_type ans ;

	net_clear(so);		// clear received buffer

	req.reqcode = REQSTREAMCLOSE ;
	req.data = handle ;
	req.reqsize = 0 ;
	net_send( so, &req, sizeof(req) );
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANSOK ) {
			res=1 ;
		}
	}
	return res ;
}

// playback stream seek
// return : 
//		  0: no encrypted stream
//		 -1: error 
//		other: RC4 key hash value
int net_stream_seek(SOCKET so, int handle, struct dvrtime * pseekto )
{
	int res = -1 ;
	struct Req_type req ;
	struct Answer_type ans ;

	net_clear(so);		// clear received buffer

	req.reqcode = REQSTREAMSEEK ;
	req.data = handle ;
	req.reqsize = sizeof(*pseekto) ;
	net_send( so, &req, sizeof(req) );
	net_send( so, pseekto, req.reqsize );
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANSOK ) {
			return (int)ans.data ;
		}
	}
	return res ;
}

// get playback stream data
// return 1: success
//        0: no data
int net_stream_getdata(SOCKET so, int handle, char ** framedata, int * framesize, int * frametype )
{
	struct Req_type req ;
	struct Answer_type ans ;

	net_clear(so);		// clear received buffer

	req.reqcode = REQSTREAMGETDATA ;
	req.data = handle ;
	req.reqsize = 0 ;
	net_send( so, &req, sizeof(req) );
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANSSTREAMDATA && ans.anssize<MAX_ANSSIZE && ans.anssize>0 ) {
			char * pbuf = (char *)malloc(ans.anssize) ;
			if( net_recv(so, pbuf, ans.anssize) ) {
				*framedata = pbuf ;
				*framesize = ans.anssize ;
				return 1 ;
			}
			free(pbuf) ;
		}
	}
	return 0 ;
}

int net_stream_getdata2(SOCKET so, int handle, PBYTE pbuf, int bufsize, int *framesize, struct dvrtime *dt)
{
	struct Req_type req ;
	struct Answer_type ans ;

	*framesize = 0;

	net_clear(so);		// clear received buffer

	req.reqcode = REQ2STREAMGETDATAEX ;
	req.data = handle ;
	req.reqsize = 0 ;
	net_send( so, &req, sizeof(req) );
	if( net_recv( so, &ans, sizeof(ans))) {
		if (ans.anscode == ANSERROR) {
			return -1;
		}
		if( ans.anscode == ANS2STREAMDATAEX && ans.anssize<MAX_ANSSIZE && ans.anssize>sizeof(struct dvrtime) ) {
			if (net_recv(so, dt, sizeof(struct dvrtime))) {
				//TRACE(_T("time:%02d:%02d:%02d.%03d\n"), dt->hour,dt->min,dt->second,dt->millisecond);
				int dataSize = ans.anssize - sizeof(struct dvrtime);
				if ((dataSize > 0) && (bufsize >= dataSize)) {
					if( net_recv(so, pbuf, dataSize) ) {
					//TRACE(_T("framesize:%d\n"), dataSize);
						*framesize = dataSize ;
						return 1 ;
					}
				}
			}
		}
	}
	return 0 ;
}

// get stream time
// return 1: success
// retur  0: failed
int net_stream_gettime(SOCKET so, int handle, struct dvrtime * t)
{
	struct dvrtime * pdtime;
	struct Req_type req ;
	struct Answer_type ans ;
	int res=0 ;

	net_clear(so);		// clear received buffer

	req.reqcode = REQSTREAMTIME ;
	req.data = handle ;
	req.reqsize = 0 ;
	net_send( so, &req, sizeof(req) );
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANSSTREAMTIME && ans.anssize<MAX_ANSSIZE && ans.anssize>0 ) {
			char * pbuf = new char [ans.anssize] ;
			if( net_recv(so, pbuf, ans.anssize ) ) {
				pdtime = (struct dvrtime *) pbuf ;
				t->year = pdtime->year ;
				t->month = pdtime->month ;
				t->day = pdtime->day ;
				t->hour = pdtime->hour ;
				t->min = pdtime->min ;
				t->second = pdtime->second ;
				t->millisecond=pdtime->millisecond ;
				t->tz = 0;
				res = 1 ;
			}
			delete pbuf ;
		}
	}
	return res ;
}

// get stream day data availability information in a month
// return
//         bit mask of month info
//         0 for no data in this month
DWORD net_stream_getmonthinfo(SOCKET so, int handle, struct dvrtime * month)
{
	struct Req_type req ;
	struct Answer_type ans ;

	net_clear(so);		// clear received buffer

	req.reqcode = REQSTREAMMONTHINFO ;
	req.data = handle ;
	req.reqsize = sizeof(* month) ;
	net_send( so, &req, sizeof(req) );
	net_send( so, month, req.reqsize );
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANSSTREAMMONTHINFO ) {
			return (DWORD) ans.data ;
		}
	}
	return 0 ;
}

// get data availability information of a day
// return 
//        sizeof tinfo item copied
//        if tinfosize is zero, return number of available items
int net_stream_getdayinfo( SOCKET so, int handle, struct dvrtime * day, struct cliptimeinfo * tinfo, int tinfosize)
{
	struct Req_type req ;
	struct Answer_type ans ;
	int res=0 ;

	net_clear(so);		// clear received buffer

	req.reqcode = REQSTREAMDAYINFO ;
	req.data = handle ;
	req.reqsize = sizeof(* day) ;
	net_send( so, &req, sizeof(req) );
	net_send( so, day, req.reqsize );
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANSSTREAMDAYINFO && ans.anssize<MAX_ANSSIZE && ans.anssize>0 ) {
			char * pbuf = new char [ans.anssize] ;
			if( net_recv(so, pbuf, ans.anssize ) ) {
				res = ans.anssize / sizeof( *tinfo ) ;
				if( tinfosize!=0 && tinfo!=NULL ) {				// 
					if( res>tinfosize ) {
						res = tinfosize ;
					}
					memcpy( tinfo, pbuf, res*sizeof(*tinfo));
				}
			}
			delete pbuf ;
		}
	}
	return res;
}

// get locked file availability information of a day
// return 
//        sizeof tinfo item copied
//        if tinfosize is zero, return number of available items
int net_stream_getlockfileinfo(SOCKET so, int handle, struct dvrtime * day, struct cliptimeinfo * tinfo, int tinfosize)
{
	struct Req_type req ;
	struct Answer_type ans ;
	int res=0 ;

	net_clear(so);		// clear received buffer

	req.reqcode = REQLOCKINFO ;
	req.data = handle ;
	req.reqsize = sizeof(* day) ;
	net_send( so, &req, sizeof(req) );
	net_send( so, day, req.reqsize );
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANSSTREAMDAYINFO && ans.anssize<MAX_ANSSIZE && ans.anssize>0 ) {
			char * pbuf = new char [ans.anssize] ;
			if( net_recv(so, pbuf, ans.anssize ) ) {
				res = ans.anssize / sizeof( *tinfo ) ;
				if( tinfosize!=0 && tinfo!=NULL ) {				// 
					if( res>tinfosize ) {
						res = tinfosize ;
					}
					memcpy( tinfo, pbuf, res*sizeof(*tinfo));
				}
			}
			delete pbuf ;
		}
	}
	return res;
}


// get DVR system time
// return 
//        1 success, DVR time in *t
//        0 failed
int net_getdvrtime(SOCKET so, struct dvrtime * t )
{
	struct Req_type req ;
	struct Answer_type ans ;
	int res=0 ;
	SYSTEMTIME * psystm ;
	struct dvrtime * pdvrt ;

	net_clear(so);		// clear received buffer

	req.reqcode = REQ2GETLOCALTIME ;
	req.data = 0 ;
	req.reqsize = 0;
	net_send( so, &req, sizeof(req) );
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANS2TIME && ans.anssize<MAX_ANSSIZE && ans.anssize>0 ) {
			char * pbuf = new char [ans.anssize] ;
			if( net_recv(so, pbuf, ans.anssize ) ) {
				pdvrt = (struct dvrtime *)pbuf ;
				t->year=pdvrt->year ;
				t->month = pdvrt->month ;
				t->day = pdvrt->day ;
				t->hour = pdvrt->hour ;
				t->min = pdvrt->min ;
				t->second = pdvrt->second ;
				t->millisecond = pdvrt->millisecond;
				res=1 ;
			}
			delete pbuf ;
		}
	}
	if( res ) {
		return res ;
	}

	// Try Use REQGETLOCALTIME
	req.reqcode = REQGETLOCALTIME ;
	req.data = 0 ;
	req.reqsize = 0;
	net_send( so, &req, sizeof(req) );
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANSGETTIME && ans.anssize<MAX_ANSSIZE && ans.anssize>0 ) {
			char * pbuf = new char [ans.anssize] ;
			if( net_recv(so, pbuf, ans.anssize ) ) {
				psystm = (SYSTEMTIME *)pbuf ;
				t->year=psystm->wYear ;
				t->month = psystm->wMonth ;
				t->day = psystm->wDay ;
				t->hour = psystm->wHour ;
				t->min = psystm->wMinute ;
				t->second = psystm->wSecond ;
				t->millisecond = psystm->wMilliseconds ;
				res=1 ;
			}
			delete pbuf ;
		}
	}
	return res ;
}

// get DVR system setup
int net_GetSystemSetup(SOCKET so, void * buf, int bufsize)
{
	int res = 0 ;

	struct Req_type req ;
	struct Answer_type ans ;

	net_clear(so);		// clear received buffer

	req.reqcode = REQGETSYSTEMSETUP ;
	req.data = 0 ;
	req.reqsize = 0;
	net_send( so, &req, sizeof(req) );
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANSSYSTEMSETUP && ans.anssize<MAX_ANSSIZE && ans.anssize>0 ) {
			char * pbuf = new char [ans.anssize] ;
			if( net_recv(so, pbuf, ans.anssize ) ) {
				if( ans.anssize>bufsize ) {
					ans.anssize = bufsize ;
				}
				memcpy( buf, pbuf, ans.anssize ) ;
				res = 1 ;
			}
			delete pbuf ;
		}
	}
	return res ;
}

int net_SetSystemSetup(SOCKET so, void * buf, int bufsize)
{
	int res=0;
	struct Req_type req ;
	struct Answer_type ans ;

	net_clear(so);		// clear received buffer

	req.reqcode = REQSETSYSTEMSETUP ;
	req.data = 0 ;
	req.reqsize = bufsize;
	net_send( so, &req, sizeof(req) );
	net_send( so, buf, bufsize );
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANSOK ) {
			res = 1 ;
		}
	}
	return res ;
}

int net_GetChannelSetup(SOCKET so, void * buf, int bufsize, int channel)
{
	int res = 0 ;
	struct Req_type req ;
	struct Answer_type ans ;
	req.data = channel;
	req.reqcode = REQGETCHANNELSETUP ;
	req.reqsize = 0 ;

	net_clear(so);		// clear received buffer

	net_send(so, &req, sizeof(req));
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANSCHANNELSETUP && ans.anssize<MAX_ANSSIZE && ans.anssize>0 ) {
			char * pbuf = new char [ans.anssize] ;
			if( net_recv(so, pbuf, ans.anssize ) ) {
				if( ans.anssize>bufsize ) {
					ans.anssize = bufsize ;
				}
				memcpy( buf, pbuf, ans.anssize ) ;
				res = 1 ;
			}
			delete pbuf ;
		}
	}
	return res ;
}

int net_SetChannelSetup(SOCKET so, void * buf, int bufsize, int channel)
{
	int res = 0 ;
	struct Req_type req ;
	struct Answer_type ans ;

	net_clear(so);		// clear received buffer

	req.data=channel;
	req.reqcode = REQSETCHANNELSETUP ;
	req.reqsize = bufsize ;
	net_send( so, &req, sizeof(req) );
	net_send( so, buf, bufsize );
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANSOK ) {
			res = 1 ;
		}
	}
	return res ;
}

int net_DVRGetChannelState(SOCKET so, void * buf, int bufsize )
{
	int res = 0 ;
	struct Req_type req ;
	struct Answer_type ans ;

	net_clear(so);		// clear received buffer

	req.reqcode = REQGETCHANNELSTATE ;
	req.data = 16 ;
	req.reqsize = 0 ;
	net_send(so, &req, sizeof(req));
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANSGETCHANNELSTATE && ans.anssize<MAX_ANSSIZE && ans.anssize>0 ) {
			char * pbuf = new char [ans.anssize] ;
			if( net_recv(so, pbuf, ans.anssize ) ) {
				if( ans.anssize>bufsize ) {
					ans.anssize = bufsize ;
				}
				memcpy( buf, pbuf, ans.anssize ) ;
				res = ans.anssize ;
			}
			delete pbuf ;
		}
	}
	return res ;
}

int net_DVRGetTimezone( SOCKET so, TIME_ZONE_INFORMATION * tzi )
{
	int res = 0 ;
	struct Req_type req ;
	struct Answer_type ans ;

	net_clear(so);		// clear received buffer

	req.reqcode = REQGETTIMEZONE ;
	req.data = 0 ;
	req.reqsize = 0 ;

	net_send( so, &req, sizeof(req) );
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANSTIMEZONEINFO && ans.anssize<MAX_ANSSIZE && ans.anssize>0 ) {
			char * pbuf = new char [ans.anssize] ;
			if( net_recv(so, pbuf, ans.anssize ) ) {
				if( ans.anssize>sizeof(*tzi) ) {
					ans.anssize = sizeof(*tzi) ;
				}
				memcpy( tzi, pbuf, ans.anssize ) ;
				res = 1 ;
			}
			delete pbuf ;
		}
	}
	return res ;
}

int net_DVRSetTimezone( SOCKET so, TIME_ZONE_INFORMATION * tzi )
{
	int res = 0 ;
	struct Req_type req ;
	struct Answer_type ans ;

	net_clear(so);		// clear received buffer

	req.reqcode = REQSETTIMEZONE ;
	req.data = 0 ;
	req.reqsize = sizeof( TIME_ZONE_INFORMATION ) ;

	net_send( so, &req, sizeof(req) );
	net_send( so, tzi, req.reqsize );
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANSOK ) {
			res = 1 ;
		}
	}
	return res ;
}

int net_DVRGetLocalTime(SOCKET so, SYSTEMTIME * st )
{
	int res ;
	struct Req_type req ;
	struct Answer_type ans ;

	net_clear(so);		// clear received buffer

	req.reqcode = REQGETLOCALTIME ;
	req.data = 0 ;
	req.reqsize = 0 ;
	net_send( so, &req, sizeof(req) );
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANSGETTIME && ans.anssize<MAX_ANSSIZE && ans.anssize>0 ) {
			char * pbuf = new char [ans.anssize] ;
			if( net_recv(so, pbuf, ans.anssize ) ) {
				if( ans.anssize>sizeof(*st) ) {
					ans.anssize = sizeof(*st) ;
				}
				memcpy( st, pbuf, ans.anssize ) ;
				res = 1 ;
			}
			delete pbuf ;
		}
	}
	return res ;
}

int net_DVRSetLocalTime(SOCKET so, SYSTEMTIME * st )
{
	int res = 0 ;
	struct Req_type req ;
	struct Answer_type ans ;

	net_clear(so);		// clear received buffer

	req.reqcode = REQSETLOCALTIME ;
	req.data = 0 ;
	req.reqsize = sizeof( SYSTEMTIME ) ;
	net_send( so, &req, sizeof(req) );
	net_send( so, st, req.reqsize );
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANSOK ) {
			res = 1 ;
		}
	}
	return res ;
}

int net_DVRGetSystemTime(SOCKET so, SYSTEMTIME * st )
{
	int res ;
	struct Req_type req ;
	struct Answer_type ans ;

	net_clear(so);		// clear received buffer

	req.reqcode = REQGETSYSTEMTIME ;
	req.data = 0 ;
	req.reqsize = 0 ;
	net_send( so, &req, sizeof(req) );
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANSGETTIME && ans.anssize<MAX_ANSSIZE && ans.anssize>0 ) {
			char * pbuf = new char [ans.anssize] ;
			if( net_recv(so, pbuf, ans.anssize ) ) {
				if( ans.anssize>sizeof(*st) ) {
					ans.anssize = sizeof(*st) ;
				}
				memcpy( st, pbuf, ans.anssize ) ;
				res = 1 ;
			}
			delete pbuf ;
		}
	}
	return res ;
}

int net_DVRSetSystemTime(SOCKET so, SYSTEMTIME * st )
{
	int res = 0 ;
	struct Req_type req ;
	struct Answer_type ans ;

	net_clear(so);		// clear received buffer

	req.reqcode = REQSETSYSTEMTIME ;
	req.data = 0 ;
	req.reqsize = sizeof( SYSTEMTIME ) ;
	net_send( so, &req, sizeof(req) );
	net_send( so, st, req.reqsize );
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANSOK ) {
			res = 1 ;
		}
	}
	return res ;
}

// Get time zone info
// return buffer size returned, return 0 for error
int net_DVR2ZoneInfo( SOCKET so, char ** zoneinfo)
{
	int res =0;
	struct Req_type req ;
	struct Answer_type ans ;

	req.reqcode = REQ2GETZONEINFO ;
	req.data = 0 ;
	req.reqsize = 0 ;
	net_send( so, &req, sizeof(req) );
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANS2ZONEINFO && ans.anssize<MAX_ANSSIZE && ans.anssize>0 ) {
			char * pbuf = new char [ans.anssize] ;
			if( net_recv(so, pbuf, ans.anssize ) ) {
				*zoneinfo = pbuf ;
				res = ans.anssize ;
			}
			else {
				delete pbuf ;
			}
		}
	}
	return res ;
}

// Set DVR time zone
//   timezone: timezone name to set
// return 1:success, 0:fail
int net_DVR2SetTimeZone(SOCKET so, char * timezone)
{
	int res = 0 ;
	struct Req_type req ;
	struct Answer_type ans ;

	net_clear(so);		// clear received buffer

	req.reqcode = REQ2SETTIMEZONE ;
	req.data = 0 ;
	req.reqsize = (int)strlen( timezone )+1 ;
	net_send( so, &req, sizeof(req) );
	net_send( so, timezone, req.reqsize );
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANSOK ) {
			res = 1 ;
		}
	}
	return res ;
}

// Get DVR time zone
// return buffer size returned, return 0 for error
//    timezone: DVR timezone name 
int net_DVR2GetTimeZone( SOCKET so, char ** timezone )
{
	int res = 0 ;
	struct Req_type req ;
	struct Answer_type ans ;

	net_clear(so);		// clear received buffer

	req.reqcode = REQ2GETTIMEZONE ;
	req.data = 0 ;
	req.reqsize = 0 ;
	net_send( so, &req, sizeof(req) );
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANS2TIMEZONE && ans.anssize<MAX_ANSSIZE && ans.anssize>0 ) {
			char * pbuf = new char [ans.anssize] ;
			if( net_recv(so, pbuf, ans.anssize ) ) {
				*timezone = pbuf ;
				res = ans.anssize ;
			}
			else {
				delete pbuf ;
			}
		}
	}
	return res ;
}

// get dvr (version2) time
//    st: return dvr time
//    system: 0-get localtime, 1-get system time
// return: 0: fail, 1: success
int net_DVR2GetTime(SOCKET so, SYSTEMTIME * st, int system)
{
	int res = 0 ;
	struct Req_type req ;
	struct Answer_type ans ;
	struct dvrtime * pdvrt ;

	net_clear(so);		// clear received buffer

	if( system )
		req.reqcode = REQ2GETSYSTEMTIME ;
	else 
		req.reqcode = REQ2GETLOCALTIME ;
	req.data = 0 ;
	req.reqsize = 0 ;
	net_send( so, &req, sizeof(req) );
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANS2TIME && ans.anssize<MAX_ANSSIZE && ans.anssize>0 ) {
			char * pbuf = new char [ans.anssize] ;
			if( net_recv(so, pbuf, ans.anssize ) ) {
				pdvrt = (struct dvrtime *)pbuf ;
				st->wYear = pdvrt->year ;
				st->wMonth = pdvrt->month ;
				st->wDay = pdvrt->day ;
				st->wDayOfWeek=0;
				st->wHour = pdvrt->hour ;
				st->wMinute = pdvrt->min ;
				st->wSecond = pdvrt->second ;
				st->wMilliseconds = pdvrt->millisecond ;
				res = 1 ;
			}
			delete pbuf ;
		}
	}
	return res ;
}

// set dvr (version2) time
//    st: return dvr time
//    system: 0-set localtime, 1-set system time
// return: 0: fail, 1: success
int net_DVR2SetTime(SOCKET so, SYSTEMTIME * st, int system)
{
	int res = 0 ;
	struct Req_type req ;
	struct Answer_type ans ;
	struct dvrtime dvrt ;

	net_clear(so);		// clear received buffer

	if( system )
		req.reqcode = REQ2SETSYSTEMTIME ;
	else 
		req.reqcode = REQ2SETLOCALTIME ;
	req.data = 0 ;
	req.reqsize = sizeof( dvrt ) ;
	net_send(so, &req, sizeof(req));
	dvrt.year=st->wYear;
	dvrt.month=st->wMonth;
	dvrt.day=st->wDay;
	dvrt.hour=st->wHour;
	dvrt.min=st->wMinute;
	dvrt.second=st->wSecond;
	dvrt.millisecond=0 ;
	net_send(so, &dvrt, sizeof(dvrt));
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANSOK ) {
			res = 1 ;
		}
	}
	return res ;
}

// get dvr timezone information entry
// return: 0: fail, 1: success
int net_GetDVRTZIEntry(SOCKET so, void * buf, int bufsize )
{
	int res = 0 ;
	struct Req_type req ;
	struct Answer_type ans ;

	net_clear(so);		// clear received buffer

	req.reqcode = REQGETTZIENTRY ;
	req.data = 0 ;
	req.reqsize = 0 ;
	net_send( so, &req, sizeof(req) );
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANSTZENTRY && ans.anssize<MAX_ANSSIZE && ans.anssize>0 ) {
			char * pbuf = new char [ans.anssize] ;
			if( net_recv(so, pbuf, ans.anssize ) ) {
				if( ans.anssize > bufsize ) {
					ans.anssize = bufsize ;
				}
				memcpy( buf, pbuf, ans.anssize );
				res = 1 ;
			}
			delete pbuf ;
		}
	}
	return res ;
}

// Kill DVR application, if Reboot != 0 also reset DVR
int net_DVRKill(SOCKET so, int Reboot )
{
	int res = 0 ;
	struct Req_type req ;
	struct Answer_type ans ;

	net_clear(so);		// clear received buffer

	req.reqcode = REQKILL ;
	if( Reboot ) {
		req.data = -1 ;
	}
	else {
		req.data = 0 ;
	}
	req.reqsize = 0 ;
	net_send(so, &req, sizeof(req));
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANSOK ) {
			res = 1 ;
		}
	}
	return res ;
}

int net_GetSetupPage(SOCKET so, char * pagebuf, int bufsize)
{
	int res = 0 ;
	struct Req_type req ;
	struct Answer_type ans ;

	net_clear(so);		// clear received buffer

#ifdef TVSVIEWER
	char tvs[]="tvs2" ;
	req.reqcode = REQ2GETSETUPPAGE ;
	req.data = 0 ;
	req.reqsize = 4 ;
	net_send( so, &req, sizeof(req) );
	net_send( so, &tvs, req.reqsize );
#else
	req.reqcode = REQ2GETSETUPPAGE ;
	req.data = 0 ;
	req.reqsize = 0 ;
	net_send( so, &req, sizeof(req) );
#endif
	if( net_recv( so, &ans, sizeof(ans))) {
		if( ans.anscode == ANS2SETUPPAGE && ans.anssize<MAX_ANSSIZE && ans.anssize>0 ) {
			char * pbuf = new char [ans.anssize] ;
			if( net_recv(so, pbuf, ans.anssize ) ) {
				if( ans.anssize > bufsize ) {
					ans.anssize = bufsize ;
				}
				memcpy( pagebuf, pbuf, ans.anssize );
				pagebuf[bufsize-1]=0 ;
				res = 1 ;
			}
			delete pbuf ;
		}
	}
	return res ;
}

int net_CheckKey(SOCKET so, char * keydata, int datalen)
{
	struct Req_type req ;
	struct Answer_type ans ;
	int ret;

	req.reqcode = REQCHECKTVSKEY ;
	req.data = 0 ;
	req.reqsize = datalen ;
	ret = send(so, (char *)&req, sizeof(req), 0);
	if (ret == SOCKET_ERROR) {
		return 1;
	}
	ret = send( so, keydata, req.reqsize, 0 );
	if (ret == SOCKET_ERROR) {
		return 1;
	}

	struct timeval tv = {5, 0};
	ret = read_nbytes(so, (PBYTE)&ans, sizeof(ans), sizeof(ans), &tv, true);
	if ((ret < sizeof(ans)) || (ans.anscode != ANSOK)) {
		return 0;
	}

	return 1 ;
}