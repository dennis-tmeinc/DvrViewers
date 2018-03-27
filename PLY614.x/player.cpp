#include "stdafx.h"
#include <winsock2.h>
//#include <ws2tcpip.h>
#include <crtdbg.h>
#include <process.h>

#include "ply614.h"
#include "player.h"

#define PAUSE_TIME 100
//extern CRITICAL_SECTION m_cs ;	

#ifdef TEST_301
char Head264[40] = {
  0x34, 0x48, 0x4b, 0x48, 0xfe, 0xb3, 0xd0, 0xd6, 
  0x08, 0x03, 0x04, 0x20, 0x00, 0x00, 0x00, 0x00,
  0x03, 0x10, 0x02, 0x10, 0x01, 0x10, 0x10, 0x00,
  0x80, 0x3e, 0x00, 0x00, 0xc0, 0x02, 0xe0, 0x01,
  0x11, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 
} ;
#else
char Head264[40] = {
  0x49, 0x4d, 0x4b, 0x48, 0x01, 0x01, 0x01, 0x00, 
  0x02, 0x00, 0x01, 0x00, 0x21, 0x72, 0x01, 0x0e,
  0x80, 0x3e, 0x00, 0x00, 0x00, 0x7d, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 
} ;
#endif

unsigned WINAPI WindowEventThreadFunc(PVOID pvoid) {
	HikPlayer *p = (HikPlayer *)pvoid;
	p->WindowEventThread();
	TRACE(_T("Window Event Thread Ended\n"));
	_endthreadex(0);
	return 0;
}

unsigned WINAPI PlayerThreadFunc(PVOID pvoid) {
	HikPlayer *p = (HikPlayer *)pvoid;
	p->PlayerThread();
	_endthreadex(0);
	return 0;
}

unsigned char limittab[512] ;
int	ytab[256] ;
int	rvtab[256] ;
int	gutab[256] ;
int	gvtab[256] ;
int	butab[256] ;
static void YV12_RGB( void * bufRGB, int pitch, void * bufYV12, int pitchYV12, int width, int height ) 
{
	static int yvinit=0 ;
	unsigned char * rgb ;
	unsigned char * py, * pu, * pv ;
	unsigned char * ly, *lu, *lv ;
	unsigned char * prgb ;

	int i, j;

	if( yvinit==0 ) {
		for( i=0;i<128;i++) 
			limittab[i]=0;
		for( i=128;i<384;i++)
			limittab[i]=i-128;
		for( i=384;i<512;i++)
			limittab[i]=255;
		for( i=0; i<256; i++) {
			ytab[i] = (int)(1.164 * (i-16)    + 128);
			rvtab[i] = (int)(1.596 *(i-128)) ;
			gutab[i] = (int)(- 0.391 * (i-128 )) ;
			gvtab[i] = (int)(- 0.813 * (i-128 )) ;
			butab[i] = (int)(2.018 * (i-128 ));
		}
		yvinit=1 ;
	}

	rgb = (unsigned char *)bufRGB ;
	py = (unsigned char * ) bufYV12 ;
	pv = py + pitchYV12*height ;
	pu = pv + pitchYV12*height/4 ;

	for(  i=0 ;i<height ; i++ ) {
		prgb = rgb + i*pitch ;
		ly = py + pitchYV12 * i ;
		lv = pv + (pitchYV12/2) * (i/2) ;
		lu = pu + (pitchYV12/2) * (i/2) ;
		for( j = 0; j<(width/2) ; j++ ) {
			int y, u, v ;
			int dr, dg, db ;

			u = *lu++ ;
			v = *lv++ ;

			dr = rvtab[v] ;
			dg = gutab[u] + gvtab[v] ;
			db = butab[u] ;

			y = ytab[*ly++] ;
			*prgb++=limittab[y+db];
			*prgb++=limittab[y+dg];
			*prgb++=limittab[y+dr];
			prgb++ ;

			y = ytab[*ly++] ;
			*prgb++=limittab[y+db];
			*prgb++=limittab[y+dg];
			*prgb++=limittab[y+dr];
			prgb++ ;
		}
	}
}

char   CaptureImage[128];

HikPlayer *	g_phikplyr[PLAYM4_MAX_SUPPORTS] ;

void CALLBACK DisplayCBFunc(long nPort,char * pBuf,long nSize,long nWidth,long nHeight,long nStamp,long nType,long nReserved)
{
	if( nType != T_YV12 ) return ;
	if( g_phikplyr[nPort] == NULL ) 
		return ;
	g_phikplyr[nPort]->displaycallback( pBuf, nSize, nWidth, nHeight, nStamp, nType, nReserved);
} 

void CALLBACK AudioCBFunc(long nPort,char *pBuf,long nSize,long nStamp,long nType,long nUser)
{
} 

#ifdef AUDIO_TEST
void CALLBACK DecCBFunc(long nPort,char *pBuf,long nSize,FRAME_INFO *pFrameInfo,long nRes,long nRes2)
{
	TRACE(_T("type:%d, size:%d\n"),pFrameInfo->nType,nSize);
	if (pFrameInfo->nType == T_AUDIO16) {				
		fwrite(pBuf, 1, nSize, g_phikplyr[nPort]->m_fp);				
	}
} 
#endif
/*
void CALLBACK SourceBufCallBack(long nPort,DWORD nBufSize,DWORD dwUser,void*pResvered)
{
	HikPlayer *p = (HikPlayer *)dwUser;
	p->SourceBufCB(nBufSize);
}
void HikPlayer::SourceBufCB(DWORD dwSize)
{
	m_bufSize = (int)dwSize;
}
*/

/*
-------init---------
PlayM4_GetPort
PlayM4_SetDDrawDevice
PlayM4_SetVolume
PlayM4_SetEncChangeMsg
PlayM4_SetEncTypeChangeCallBack
-------open--------
PlayM4_SetStreamOpenMode(1)
PlayM4_GetFileHeadLength
PlayM4_OpenStream
PlayM4_SyncToAudio(False)
------play-----------
PlayM4_SetCheckWatermarkCallBack
PlayM4_ThrowBFrameNum
PlayM4_SetImageSharpen(0)
PlayM4_SetPlayMode(0)
PlayM4_SetDecodeFrameType
PlayM4_ResetSourceBuffer
PlayM4_Play(hwnd)
PlayM4_PlaySound
------play end-------
PlayM4_SetPicQuality(1)
PlayM4_SetDeflash(1)
PlayM4_GetPictureSize
PlayM4_SetVolume
----open end--------
PlayM4_InputData
-------status-------
PlayM4_GetPlayedTime
PlayM4_GetCurrentFrameNum
-------close----
PlayM4_Stop
PlayM4_CloseFile
PlayM4_RealeseDDraw
PlayM4_ReleaseDDrawDevice
PlayM4_FreePort
*/
// constructor
HikPlayer::HikPlayer( int channel, HWND hplaywindow, int streammode, bool audio_only ) 
: decoder(hplaywindow), 
  m_playstat(PLAY_STAT_STOP),
  m_maxframesize(0),
  m_bAudioOnly(audio_only)
{
    // init blur variable
    m_ba_number=0 ;   // no blur
    m_blur_save_CapBuf=NULL ;

	DWORD ret;
	////InitializeCriticalSection(&m_cs);
	m_ch = channel;
	m_show = -1;
	m_bEventDie = m_bDie = false;
	m_hparent = hplaywindow;
	m_hwnd = m_hvideownd = NULL;
	m_iWindowX = 0;
	m_iWindowY = 0;
	m_iWindowWidth = m_iWindowHeight = 0;
	SetRectEmpty( &m_rectParent );
  _sntprintf( m_class_main, sizeof(m_class_main)/sizeof(*m_class_main),
               _T("TME MSW %p"), this );
  _sntprintf( m_class_video, sizeof(m_class_video)/sizeof(*m_class_video),
               _T("TME MSW video %p"), this );
	if (m_hparent != NULL) {
		RECT rc;
		GetClientRect(m_hparent, &rc);
		m_iWindowWidth = rc.right;
		m_iWindowHeight = rc.bottom;
	}
  tme_mutex_init(&g_ipc, &m_lock);
	m_hEventThread = NULL;
	m_bPlay = m_bReady = false;
	m_hThread = tme_thread_create( &g_ipc, "Player Thread",
			PlayerThreadFunc, this,				
			THREAD_PRIORITY_NORMAL, false );


	//m_bufSize = 0;
	//m_bufSizeEstimate = 0;
	////EnterCriticalSection(&m_cs); 
	PlayM4_GetPort(&m_port);
#ifdef USE_REALMODE
	m_hikstreammode = STREAME_REALTIME;
#else
	m_hikstreammode = streammode;
#endif

#ifdef AUDIO_TEST
	m_fp = fopen("D:\\audiotest614.pcm", "wb");
#endif

	HIK_MEDIAINFO hmi;
	SecureZeroMemory(&hmi, sizeof(hmi));
	hmi.media_fourcc = FOURCC_HKMI;
	hmi.media_version = 0x0101;
	hmi.device_id = 1;
	hmi.system_format = SYSTEM_MPEG2_PS;
	hmi.video_format = m_bAudioOnly ? VIDEO_NULL : VIDEO_H264;
	hmi.audio_format = AUDIO_G722_1;
	hmi.audio_channels = 1;
	hmi.audio_bits_per_sample = 14;
	hmi.audio_samplesrate = 16000;
	hmi.audio_bitrate = 16000;

	PlayM4_SetStreamOpenMode(m_port,m_hikstreammode);	
	ret = PlayM4_OpenStream(m_port, (PBYTE)&hmi, sizeof(hmi), /*SOURCE_BUF_MIN*/SOURCEBUFSIZE );
	if( ret != 0 ) {
		g_phikplyr[m_port]=this ;

		PlayM4_SyncToAudio(m_port, FALSE);
		PlayM4_SetPlayMode(m_port, FALSE);
		PlayM4_SetDeflash(m_port, TRUE);

		PlayM4_SetDisplayType( m_port, DISPLAY_NORMAL );
		//PlayM4_SetPicQuality( m_port, TRUE );
		ret = PlayM4_SetDisplayCallBack( m_port, DisplayCBFunc );
		/*
		int error;
		if (!PlayM4_SetAudioCallBack( m_port, AudioCBFunc, (long)this )) {
			error = PlayM4_GetLastError(m_port);
		}*/
#ifdef AUDIO_TEST
		int error;
		if (!PlayM4_SetDecCallBack( m_port, DecCBFunc)) {
			error = PlayM4_GetLastError(m_port);
		}
#endif
		//PlayM4_SetSourceBufCallBack(m_port, SOURCEBUFSIZE, SourceBufCallBack, (DWORD)this, NULL);
		PlayM4_ResetSourceBuffer(m_port);
		////LeaveCriticalSection(&m_cs); 
		return ;
	} else {
		ret = PlayM4_GetLastError(m_port);
	}
	m_port = -1 ;												// failed to open decoder
	////LeaveCriticalSection(&m_cs); 
}

HikPlayer::~HikPlayer() 
{
	m_bDie = true;
	if( m_port>=0 && m_port<PLAYM4_MAX_SUPPORTS ){
		stop();
		g_phikplyr[m_port]=NULL ;
		////EnterCriticalSection(&m_cs); 
		PlayM4_SetDisplayCallBack( m_port , NULL );
		PlayM4_CloseStream(m_port);
		PlayM4_FreePort(m_port);
		////LeaveCriticalSection(&m_cs); 
	}
	CloseVideo();
  tme_thread_join( m_hThread );
	tme_mutex_destroy(&m_lock);
	////DeleteCriticalSection(&m_cs);
#ifdef AUDIO_TEST
	fclose(m_fp);
#endif

    // clear blur buffer
    if( m_blur_save_CapBuf ) {
        delete m_blur_save_CapBuf ;
        m_blur_save_CapBuf=NULL ;
    }

}

// input data into player (decoder)
// return 1: success
//		  0: should retry, or failed
int HikPlayer::inputdata( char * framebuf, int framesize, int frametype )	
{
//		if( frametype==FRAME_TYPE_AUDIO && m_audioon==FALSE )		// don't decode audio frame
//			return 0 ;
	int remain ;

	if( framesize>m_maxframesize ){
		m_maxframesize=framesize ;
	}
#if 0
	if (m_bufSize == -1) {
		// buf size unknown or remaining data big
		_RPT1(_CRT_WARN, "bufsize unkown:%d\n",framesize);
		remain = m_bufSizeEstimate;
	} else {
		remain = m_bufSize;
		m_bufSizeEstimate = m_bufSize;
		m_bufSize = -1;
		EnterCriticalSection(&m_cs); 
		PlayM4_ResetSourceBufFlag(m_port);
		LeaveCriticalSection(&m_cs); 
	}
	//_RPT2(_CRT_WARN, "id:%lu, %lu\n",remain, framesize);
	if( m_playstat < PLAY_STAT_PLAY ) {
		if(remain>m_maxframesize) {
			//_RPT1(_CRT_WARN, "id2:%d\n",m_maxframesize);
			return 0 ;								// should retry
		}
	}
	if( SOURCEBUFSIZE-remain < framesize ) {
			_RPT1(_CRT_WARN, "id3:%d\n",framesize);
			return 0 ;
	}
	EnterCriticalSection(&m_cs); 
	if (PlayM4_InputData( m_port, (PBYTE)framebuf, (DWORD)framesize)) {
		m_bufSizeEstimate += framesize;
	}
	LeaveCriticalSection(&m_cs); 
#else
	/*
	while(!m_bEnd && !PlayM4_InputData( m_port, (PBYTE)framebuf, (DWORD)framesize)) {
		if ( PlayM4_GetLastError(m_port) == PLAYM4_BUF_OVER ) {
			_RPT0(_CRT_WARN, "Inputdata error=========\n");
			Sleep(10);
			continue;
		}
		break;
	}
	*/
	////EnterCriticalSection(&m_cs); 
	remain = PlayM4_GetSourceBufferRemain(m_port);
	//TRACE(_T("remain:%d,%d,%d\n"), remain, SOURCEBUFSIZE, m_maxframesize);
#if 0
	if( m_playstat < PLAY_STAT_PLAY ) {
		if(remain>m_maxframesize) {
			return 0 ;								// should retry
		}
	}
#endif
	//_RPT2(_CRT_WARN, "--%d,%d--\n",remain,framesize);
	//if( SOURCEBUFSIZE-remain < framesize ) {
		//_RPT4(_CRT_WARN, "%d,%d,%d,%d+++++++++++++++++++++++++++++++++++++\n",SOURCEBUFSIZE,remain,SOURCEBUFSIZE-remain,framesize);
		//return 0;
	//}
	if(!PlayM4_InputData( m_port, (PBYTE)framebuf, (DWORD)framesize)) {
		int err;
		err = PlayM4_GetLastError(m_port);
		if ( err == PLAYM4_BUF_OVER ) {
			////LeaveCriticalSection(&m_cs); 
			//_RPT0(_CRT_WARN, "Buffer full=========\n");
			return 0;
		} else {
			TRACE(_T("PlayM4_InputData error:%d\n"), err);
		}
	}
	//remain = PlayM4_GetSourceBufferRemain(m_port);
	////LeaveCriticalSection(&m_cs); 
	//_RPT4(_CRT_WARN, "%d,%d,%d,%d\n",SOURCEBUFSIZE,remain,SOURCEBUFSIZE-remain,framesize);
#endif

	return 1;
}

// stop playing and show background screen (logo)
int HikPlayer::stop()
{
	if( m_playstat!=PLAY_STAT_STOP ) {						// already playing?
		////EnterCriticalSection(&m_cs); 
		PlayM4_Stop( m_port );
		////LeaveCriticalSection(&m_cs); 
		m_playstat=PLAY_STAT_STOP ;
		// refresh screen
		//InvalidateRect(m_playerwindow, NULL, FALSE);
	}
	return 0;
}

// play mode
int	HikPlayer::play() 
{
//	if( m_playstat!=PLAY_STAT_PLAY ) {						// already playing?
		////EnterCriticalSection(&m_cs); 
		if( m_playstat==PLAY_STAT_PAUSE) {
			PlayM4_Pause(m_port, FALSE);
		}
#ifdef USE_REALMODE
		if( m_hikstreammode != STREAME_REALTIME ) {				// switch to realtime mode
			PlayM4_Stop( m_port );
			m_hikstreammode = STREAME_REALTIME ;
			PlayM4_SetStreamOpenMode(m_port,m_hikstreammode);
		}
#endif
		PlayM4_ThrowBFrameNum(m_port, 0);

		if (m_bReady) {
			PlayM4_Play( m_port, m_hvideownd );
		} else {
			m_bPlay = true;
			TRACE(_T("PlayM4_Play1(ch:%d)\n"), m_ch );
		}

		m_playstat=PLAY_STAT_PLAY ;
		if( m_audioon ) {
			PlayM4_PlaySoundShare(m_port);
		} else {
			PlayM4_StopSoundShare(m_port);
		}
		////LeaveCriticalSection(&m_cs); 
//	}
	return 0;
}
													
// pause mode
int HikPlayer::pause()
{
	if( m_playstat!=PLAY_STAT_PAUSE ) {					// already paused?
		////EnterCriticalSection(&m_cs); 
#ifdef USE_REALMODE
		if( m_hikstreammode != STREAME_FILE ) {
			PlayM4_Stop( m_port );
			m_hikstreammode = STREAME_FILE ;
			PlayM4_SetStreamOpenMode(m_port,m_hikstreammode);
			PlayM4_ThrowBFrameNum(m_port, 0);
			PlayM4_Play( m_port, m_playerwindow );
			PlayM4_Pause(m_port, TRUE);
			//PlayM4_OneByOne( m_port);
		}
		else 
#endif
			PlayM4_StopSoundShare(m_port);
			PlayM4_Pause(m_port, TRUE);
		m_playstat = PLAY_STAT_PAUSE ;
		////LeaveCriticalSection(&m_cs); 
		//Sleep(PAUSE_TIME);
	}
	return 0;
}

// slow play mode
int HikPlayer::slow(int speed)
{
	int ret;
	////EnterCriticalSection(&m_cs); 
	if( m_playstat != PLAY_STAT_SLOW ) {
		m_playstat=PLAY_STAT_SLOW;
#ifdef USE_REALMODE
		if( m_hikstreammode != STREAME_FILE ) {
			PlayM4_Stop( m_port );
			m_hikstreammode = STREAME_FILE ;
			PlayM4_SetStreamOpenMode(m_port,m_hikstreammode);
			PlayM4_Play( m_port, m_playerwindow );
			PlayM4_ThrowBFrameNum(m_port, 0);
		}
#endif
	}
	ret = PlayM4_Slow( m_port);
	////LeaveCriticalSection(&m_cs); 
	return ret;
}

// fast play mode
int HikPlayer::fast(int speed)  
{
	int ret;
	////EnterCriticalSection(&m_cs); 
	if( m_playstat != PLAY_STAT_FAST ) {
		m_playstat=PLAY_STAT_FAST;
#ifdef USE_REALMODE
		if( m_hikstreammode != STREAME_FILE ) {
			PlayM4_Stop( m_port );
			m_hikstreammode = STREAME_FILE ;
			PlayM4_SetStreamOpenMode(m_port,m_hikstreammode);
			PlayM4_Play( m_port, m_playerwindow );
		}
#endif
	}

	if( speed >=3 )
		PlayM4_ThrowBFrameNum(m_port, 3);
	else if( speed >=2 ) 
		PlayM4_ThrowBFrameNum(m_port, 2);
	else
		PlayM4_ThrowBFrameNum(m_port, 1);

	ret = PlayM4_Fast( m_port);
	////LeaveCriticalSection(&m_cs); 
	return ret;
}

int HikPlayer::oneframeforward()                                          // play one frame forward
{
	int ret;
	TRACE(_T("oneframeforward\n"));

	if( m_playstat!=PLAY_STAT_PAUSE )  {
		TRACE(_T("oneframeforward xxx\n"));
		return -1 ;												// one frame forward on paused state only
	}

	////EnterCriticalSection(&m_cs); 
#ifdef USE_REALMODE
	if( m_hikstreammode != STREAME_FILE ) {
		PlayM4_Stop( m_port );
		m_hikstreammode = STREAME_FILE ;
		PlayM4_SetStreamOpenMode(m_port,m_hikstreammode);
		PlayM4_ThrowBFrameNum(m_port, 0);
	    PlayM4_Play( m_port, m_playerwindow );
		PlayM4_Pause( m_port, TRUE );
		Sleep(PAUSE_TIME);
	}
#endif
	ret = PlayM4_OneByOne( m_port);
	if (ret == 0) {
		int err;
		err = PlayM4_GetLastError(m_port);
		_RPT1(_CRT_WARN, "PlayM4_OneByOne error:%d\n",err);
		////LeaveCriticalSection(&m_cs); 
#if 0
		Sleep(PAUSE_TIME);
		////EnterCriticalSection(&m_cs); 
	TRACE(_T("xxx1\n"));
	    PlayM4_Play( m_port, m_playerwindow );
		////LeaveCriticalSection(&m_cs); 
	TRACE(_T("xxx2\n"));
		Sleep(PAUSE_TIME);
		////EnterCriticalSection(&m_cs); 
	TRACE(_T("xxx3\n"));
		ret = PlayM4_OneByOne( m_port);
		if (ret == 0) {
			_RPT0(_CRT_WARN, "PlayM4_OneByOne error2\n");
		}
#endif
	}
	////LeaveCriticalSection(&m_cs); 
	return ret;
}

// suspend diaplay and show background screen (logo)
int HikPlayer::suspend()							
{
	if( m_playstat!=PLAY_STAT_SUSPEND ) {
		m_playstat=PLAY_STAT_SUSPEND ;
		// refresh screen.
		//InvalidateRect(m_playerwindow, NULL, FALSE);
	}
	return 0;
}

void HikPlayer::refresh(int nCmdShow)
{
	if (m_bReady) {
		TRACE(_T("show window:%d(ch:%d)\n"), nCmdShow, m_ch);
		ShowWindow(m_hwnd, nCmdShow);
	} else {
		TRACE(_T("cmd window:%d(ch:%d)\n"), nCmdShow,m_ch);
		m_show = nCmdShow;
	}
#if 0
	if (nCmdShow == SW_HIDE)
		InvalidateRect(m_playerwindow, NULL, FALSE);
#endif
}

// set audio on/off state
int HikPlayer::audioon( int audioon ) 
{
	int oldaudio=m_audioon ;
	m_audioon=audioon ;
	if( oldaudio!=audioon ) {
		////EnterCriticalSection(&m_cs); 
		if( audioon ) {
			PlayM4_PlaySoundShare(m_port);
		}
		else {
			PlayM4_StopSoundShare(m_port);
		}
		////LeaveCriticalSection(&m_cs); 
	}
	return oldaudio ;
}

void HikPlayer::displaycallback(char * pBuf,long nSize,long nWidth,long nHeight,long nStamp,long nType,long nReserved)
{
//	ENTER_CRITICAL ;
	m_CapBuf = pBuf ;
	m_CapSize = nSize ;
	m_CapWidth = nWidth ;
	m_CapHeight = nHeight ;
	m_CapStamp = nStamp;
	m_CapType = nType;

    // blur
    if(nType == T_YV12 && m_ba_number>0 ) {
        blurYV12Buf();
    }
//	LEAVE_CRITICAL ;
}

// capture current screen into a bmp file
int HikPlayer::capturesnapshot(char * bmpfilename)
{
	BOOL result=FALSE ;
	if( m_port<0 || m_port>=PLAYM4_MAX_SUPPORTS || m_playstat==PLAY_STAT_STOP ) return FALSE ;

	char * tBuf ;
	long   tSize ;
	long   tWidth ;
	long   tHeight ;
	long   tType ;

	if( !IsBadReadPtr( m_CapBuf, m_CapSize ) ) {
//		ENTER_CRITICAL ;
		tSize = m_CapSize ;
		tWidth = m_CapWidth ;
		tHeight = m_CapHeight ;
		tType = m_CapType ;
		tBuf = new char [tSize] ;
		memcpy( tBuf, m_CapBuf, tSize );
//		LEAVE_CRITICAL ;
		////EnterCriticalSection(&m_cs); 
		result=PlayM4_ConvertToBmpFile( tBuf, tSize, tWidth, tHeight, tType, bmpfilename );
		////LeaveCriticalSection(&m_cs); 

		delete tBuf ;	
	}
	return result;
}

int HikPlayer::getremaindatasize(){ // get remaining data(internal buffer) haven't been decoded
	int ret;
	////EnterCriticalSection(&m_cs); 
	ret = PlayM4_GetSourceBufferRemain(m_port);
	////LeaveCriticalSection(&m_cs); 
	return ret;
}
int HikPlayer::resetbuffer(){
	int ret;
	////EnterCriticalSection(&m_cs); 
	ret = PlayM4_ResetSourceBuffer(m_port);
	////LeaveCriticalSection(&m_cs); 
	return ret;
}

int HikPlayer::WindowEventThread()
{
    MSG msg;
	HMODULE hkernel32;

	/* Create a window for the video */
    /* Creating a window under Windows also initializes the thread's event
     * message queue */
    if( player_CreateWindow() )
    {
        TRACE(_T("out of memory or wrong window handle\n"));
    }

	tme_thread_ready(&g_ipc);

    /* Set power management stuff */
    if( (hkernel32 = GetModuleHandle( _T("KERNEL32") ) ) )
    {
        ULONG (WINAPI* OurSetThreadExecutionState)( ULONG );

        OurSetThreadExecutionState = (ULONG (WINAPI*)( ULONG ))
            GetProcAddress( hkernel32, "SetThreadExecutionState" );

        if( OurSetThreadExecutionState )
            /* Prevent monitor from powering off */
            OurSetThreadExecutionState( ES_DISPLAY_REQUIRED | ES_CONTINUOUS );
        else
            TRACE(_T("no support for SetThreadExecutionState()\n") );
    }

    /* Main loop */
    /* GetMessage will sleep if there's no message in the queue */
    while( !m_bEventDie)
    {
		 int ret = GetMessage( &msg, 0, 0, 0 );
		 if (!ret) // 0: received WM_QUIT, -1: error
			 break;

        /* Check if we are asked to exit */
        if( m_bEventDie )
            break;


		switch( msg.message )
        {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
			PostMessage(m_hparent, msg.message, msg.wParam, msg.lParam);
            break;

        default:
            /* Messages we don't handle directly are dispatched to the
             * window procedure */
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            //break;

        } /* End Switch */
    } /* End Main loop */

    /* Check for WM_QUIT if we created the window */
    if( !m_hparent && msg.message == WM_QUIT )
    {
        _RPT0(_CRT_WARN, "WM_QUIT... should not happen!!\n" );
        m_hwnd = NULL; /* Window already destroyed */
    }

    _RPT0(_CRT_WARN, "DirectXEventThread terminating\n" );

    //Direct3DCloseWindow();

	return 0;
}

struct win_data {
	HWND hWnd;
	int nIndex;
	LONG dwNewLong;
};

static DWORD WINAPI SetWindowLongPtrTimeoutThread(LPVOID lpParam)
{
	win_data *data = (win_data *)lpParam;
	HWND hWnd = data->hWnd;
	int nIndex = data->nIndex;
	LONG dwNewLong = data->dwNewLong;
	delete data;

	TRACE(_T("SetWindowLongPtr:%p, %d, %x\n"), hWnd, nIndex, dwNewLong);
	SetLastError(0);
	if (0 == SetWindowLongPtr(hWnd, nIndex, dwNewLong))
	{
		TRACE(_T("SetWindowLongPtr error:%d\n", GetLastError()));
	}

	return 0;
}

bool SetWindowLongPtrTimeout(HWND hWnd, int nIndex, LONG dwNewLong, int timeout)
{
	win_data *data = new win_data;
	data->hWnd = hWnd;
	data->nIndex = nIndex;
	data->dwNewLong = dwNewLong;

	HANDLE threadHandle;
	threadHandle = CreateThread(0, 0, SetWindowLongPtrTimeoutThread, data, 0, NULL);
	if (WaitForSingleObject(threadHandle, timeout) == WAIT_TIMEOUT) {
		CloseHandle(threadHandle);
		TRACE(_T("SetWindowLongPtrTimeout\n"));
		return false;
	}
	CloseHandle(threadHandle);
	return true;
}

static long FAR PASCAL WindowEventProc( HWND hwnd, UINT message,
																			 WPARAM wParam, LPARAM lParam )
{
	HikPlayer *p_vout;

	if( message == WM_CREATE )
	{
		/* Store p_vout for future use */
		p_vout = (HikPlayer *)((CREATESTRUCT *)lParam)->lpCreateParams;
		SetWindowLongPtr( hwnd, GWLP_USERDATA, (LONG_PTR)p_vout );
		return TRUE;
	}
	else
	{
		p_vout = (HikPlayer *)GetWindowLongPtr( hwnd, GWLP_USERDATA );
		if( !p_vout )
		{
			/* Hmmm mozilla does manage somehow to save the pointer to our
			* windowproc and still calls it after the vout has been closed. */
			return (long)DefWindowProc(hwnd, message, wParam, lParam);
		}
	}
		//TRACE(_T("Msg:%x(%x,%x)%x\n"), hwnd, p_vout->m_hwnd,p_vout->m_hvideownd,message);

	/* Catch the screensaver and the monitor turn-off */
	if( message == WM_SYSCOMMAND &&
		( (wParam & 0xFFF0) == SC_SCREENSAVE || (wParam & 0xFFF0) == SC_MONITORPOWER ) )
	{
		return 0; /* this stops them from happening */
	}

	if( hwnd == p_vout->m_hvideownd ) {
		return (long)DefWindowProc(hwnd, message, wParam, lParam);
	}

	switch( message )
	{
	case WM_NULL: /* Relay message received from CloseWindow to this thread's message queue */
		if (InSendMessage()) 
			ReplyMessage(TRUE); 
		return PostMessage(hwnd, WM_NULL, 0,0);

	case WM_WINDOWPOSCHANGED:
		{
		//LPWINDOWPOS p = (LPWINDOWPOS)lParam;
//TRACE(_T("WindowEventProc:%d,%d,%d,%d\n"), p->x, p->y, p->cx, p->cy);
		p_vout->WindowUpdateRects( true );
		return 0;
		}
		/* the user wants to close the window */
	case WM_CLOSE:
		{
			return 0;
		}

		/* the window has been closed so shut down everything now */
	case WM_DESTROY:
		/* just destroy the window */
		PostQuitMessage( 0 );
		return 0;

	case WM_PAINT:
	case WM_NCPAINT:
	case WM_ERASEBKGND:
		/* We do not want to relay these messages to the parent window
		* because we rely on the background color for the overlay. */
		return (long)DefWindowProc(hwnd, message, wParam, lParam);

	default:
		break;
	}

	/* Let windows handle the message */
	return (long)DefWindowProc(hwnd, message, wParam, lParam);
}

void HikPlayer::WindowUpdateRects( bool b_force )
{
	RECT  rect;
	POINT point;

	/* Retrieve the window size */
	GetClientRect( m_hwnd, &rect );
	//TRACE(_T("WindowUpdateRects:%dx%d\n"), rect.right - rect.left, rect.bottom - rect.top );

	/* Retrieve the window position */
	point.x = point.y = 0;
	ClientToScreen( m_hwnd, &point );

	/* If nothing changed, we can return */
	if( !b_force
		&& m_iWindowWidth == rect.right
		&& m_iWindowHeight == rect.bottom
		&& m_iWindowX == point.x
		&& m_iWindowY == point.y )
	{
		return;
	}

	/* Update the window position and size */
	m_iWindowX = 0;//point.x;
	m_iWindowY = 0;//point.y;
	m_iWindowWidth = rect.right;
	m_iWindowHeight = rect.bottom;

	if( m_hvideownd )
		SetWindowPos( m_hvideownd, HWND_TOP,
		m_iWindowX, m_iWindowY, m_iWindowWidth, m_iWindowHeight, 0);

}

int HikPlayer::player_CreateWindow()
{
    HINSTANCE  hInstance;
    RECT       rect_window;
    WNDCLASS   wc;                            /* window class components */
    HICON      tme_icon = NULL;
    TCHAR       tme_path[MAX_PATH+1];
    int        i_style, i_stylex;

    /* Get this module's instance */
    hInstance = GetModuleHandle(NULL);

		if (m_hparent) {
			// specify client area coordinate here
			m_iWindowX = 0;
			m_iWindowY = 0;
		}

    /* Get the Icon from the main app */
    if( GetModuleFileName( NULL, tme_path, MAX_PATH ) )
    {
		tme_icon = ::ExtractIcon( hInstance, tme_path, 0 );
    }

    /* Fill in the window class structure */
    wc.style         = CS_OWNDC | CS_DBLCLKS;          /* style: dbl click */
    wc.lpfnWndProc   = (WNDPROC)WindowEventProc;       /* event handler */
    wc.cbClsExtra    = 0;                         /* no extra class data */
    wc.cbWndExtra    = 0;                        /* no extra window data */
    wc.hInstance     = hInstance;                            /* instance */
    wc.hIcon         = tme_icon;                /* load the vlc big icon */
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);    /* default cursor */
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);  /* background color */
    wc.lpszMenuName  = NULL;                                  /* no menu */
    wc.lpszClassName = m_class_main;         /* use a special class */

    /* Register the window class */
    if( !RegisterClass(&wc) )
    {
        WNDCLASS wndclass;

        if( tme_icon ) DestroyIcon( tme_icon );

        /* Check why it failed. If it's because one already exists
         * then fine, otherwise return with an error. */
        if( !GetClassInfo( hInstance, m_class_main, &wndclass ) ) {
            TRACE(_T("Window RegisterClass FAILED (err=%lu)\n"), GetLastError() );
            return 1;
        }
    }

    /* Register the video sub-window class */
    wc.lpszClassName = m_class_video;
		wc.hIcon = 0;
    if( !RegisterClass(&wc) )
    {
        WNDCLASS wndclass;

        /* Check why it failed. If it's because one already exists
         * then fine, otherwise return with an error. */
        if( !GetClassInfo( hInstance, m_class_video, &wndclass ) ) {
            TRACE(_T("Video Window RegisterClass FAILED (err=%lu)\n"), GetLastError() );
            return 1;
        }
    }

    /* When you create a window you give the dimensions you wish it to
     * have. Unfortunatly these dimensions will include the borders and
     * titlebar. We use the following function to find out the size of
     * the window corresponding to the useable surface we want */
    rect_window.top    = 10;
    rect_window.left   = 10;
    rect_window.right  = rect_window.left + m_iWindowWidth;
    rect_window.bottom = rect_window.top + m_iWindowHeight;

        /* No window decoration */
        AdjustWindowRect( &rect_window, WS_POPUP, 0 );
        i_style = WS_POPUP|WS_VISIBLE|WS_CLIPCHILDREN;
        i_stylex = 0; 

    if( m_hparent )
    {
        //i_style = WS_VISIBLE|WS_CLIPCHILDREN|WS_CHILD;
			i_style = WS_CLIPCHILDREN|WS_CHILD;
        i_stylex = 0;
    }

    //TRACE(_T("Creating Main window\n"));
    /* Create the window */
    m_hwnd =
        CreateWindowEx( WS_EX_NOPARENTNOTIFY | i_stylex,
                    m_class_main,               /* name of window class */
                    _T("(PLY614 Output)"),  /* window title */
                    i_style,                                 /* window style */
                    (m_iWindowX < 0) ? CW_USEDEFAULT :
                        (UINT)m_iWindowX,   /* default X coordinate */
                    (m_iWindowY < 0) ? CW_USEDEFAULT :
                        (UINT)m_iWindowY,   /* default Y coordinate */
                    rect_window.right - rect_window.left,    /* window width */
                    rect_window.bottom - rect_window.top,   /* window height */
                    m_hparent,                 /* parent window */
                    NULL,                          /* no menu in this window */
                    hInstance,            /* handle of this program instance */
                    (LPVOID)this );            /* send p_vout to WM_CREATE */

    if( !m_hwnd )
    {
        TRACE(_T("Video Window create window FAILED (err=%lu)\n"), GetLastError() );
        return 1;
    }

    if( m_hparent )
    {
        LONG i_style;
		HWND hParentOwner = GetParent(m_hparent);

		if (hParentOwner != NULL) {
	        i_style = GetWindowLong( hParentOwner, GWL_STYLE );

			if( !(i_style & WS_CLIPCHILDREN) ) {
				SetWindowLong( hParentOwner, GWL_STYLE, i_style | WS_CLIPCHILDREN );
				//SetWindowLongPtrTimeout( hParentOwner, GWL_STYLE, i_style | WS_CLIPCHILDREN, 200 );
			}
		}

       /* We don't want the window owner to overwrite our client area */
        i_style = GetWindowLong( m_hparent, GWL_STYLE );

				if( !(i_style & WS_CLIPCHILDREN) ) {
            /* Hmmm, apparently this is a blocking call... */
            //SetWindowLong( m_hparent, GWL_STYLE, i_style | WS_CLIPCHILDREN );
					SetWindowLongPtrTimeout(m_hparent, GWL_STYLE, i_style | WS_CLIPCHILDREN, 200);
				}
	}

    /* Create video sub-window. This sub window will always exactly match
     * the size of the video, which allows us to use crazy overlay colorkeys
     * without having them shown outside of the video area. */
    m_hvideownd =
    CreateWindow( m_class_video, _T(""),   /* window class */
        WS_CHILD | WS_VISIBLE,                   /* window style */
        0, 0,
        m_iWindowWidth,         /* default width */
        m_iWindowHeight,        /* default height */
        m_hwnd,                    /* parent window */
        NULL, hInstance,
        (LPVOID)this );            /* send p_vout to WM_CREATE */

		if( !m_hvideownd ) {
			TRACE(_T("can't create video sub-window(ch:%d)\n"), m_ch );
		} else {
			TRACE(_T("created video sub-window(ch:%d)\n"), m_ch );
		}

    /* Now display the window */
    //ShowWindow( m_hwnd, SW_HIDE );
		m_bReady = true;

    return 0;
}

void HikPlayer::CloseVideo()
{
    if( m_hEventThread )
    {
        /* Kill WindowEventThread */
        m_bEventDie = true;

        /* we need to be sure WindowEventThread won't stay stuck in
         * GetMessage, so we send a fake message */
        if( m_hwnd )
        {
			/* Messages sent with PostMessage gets lost sometimes,
			 * so, do SendMessage to the windows procedure
			 * and the windows procedure will relay the message 
			 * with PostMessage call
			 */
			SendMessage(m_hwnd, WM_NULL, 0, 0);
        }

        tme_thread_join( m_hEventThread );
    }
}

int HikPlayer::PlayerThread()
{
	if (m_hEventThread == NULL) {
		m_hEventThread = tme_thread_create(&g_ipc, "WindowEventThread",
			WindowEventThreadFunc, this, 0, true);
	}

	while( !m_bDie) {
		if (m_bPlay && m_bReady) {
			m_bPlay = false;
			TRACE(_T("PlayM4_Play(ch:%d,%p)\n"), m_ch, m_hvideownd);
			PlayM4_Play( m_port, m_hvideownd );
		}
		if ((m_show >= 0) && m_bReady) {
			TRACE(_T("show window1:%d(ch:%d)\n"), m_show, m_ch);
	    ShowWindow( m_hwnd, m_show );
			m_show = -1;
		}

		Manage();
			
		Sleep(20);
	}

	return 0;
}

int HikPlayer::Manage() {
    /* If we do not control our window, we check for geometry changes
     * ourselves because the parent might not send us its events. */
    //tme_mutex_lock( &m_lock );
    if( m_hparent)
    {
        RECT rect_parent;
        POINT point;

        //tme_mutex_unlock( &m_lock );

        GetClientRect( m_hparent, &rect_parent );
        point.x = point.y = 0;
        ClientToScreen( m_hparent, &point );
        OffsetRect( &rect_parent, point.x, point.y );

        if( !EqualRect( &rect_parent, &m_rectParent ) )
        {
            m_rectParent = rect_parent;

            SetWindowPos( m_hwnd, 0, 0, 0,
                          rect_parent.right - rect_parent.left,
                          rect_parent.bottom - rect_parent.top, SWP_NOZORDER );
        }
    }
    else
    {
        //tme_mutex_unlock( &m_lock );
    }

    return 0;
}



void  HikPlayer::refreshdecoder()
{
    if( m_playstat == PLAY_STAT_PAUSE &&
        m_CapBuf && m_CapSize>0 &&
        m_CapType == T_YV12 && 
        m_ba_number>0 ) 
    {
        blurYV12Buf();
    }
    PlayM4_RefreshPlay(m_port);
}

int  HikPlayer::setblurarea(struct blur_area * ba, int banumber )
{
    m_ba_number=0 ;
    if( banumber<=0 || ba==NULL ) {
        return 0;
    }
    if( banumber>MAX_BLUR_AREA ) {
        banumber=MAX_BLUR_AREA ;
    }
    memcpy( m_blur_area, ba, banumber*sizeof(struct blur_area));

    if( m_playstat == PLAY_STAT_PAUSE &&
        m_CapBuf && m_CapSize>0 &&
        m_CapType == T_YV12 ) 
    {
         // save decoder buffer ;
        if( m_blur_save_CapBuf == NULL ) {
            m_blur_save_CapBuf = new char [m_CapSize] ;
            memcpy( m_blur_save_CapBuf, m_CapBuf, m_CapSize );
        }
        else {
            // restore to original pic
            memcpy( m_CapBuf, m_blur_save_CapBuf, m_CapSize );
        }
    }
    m_ba_number=banumber ;

    refreshdecoder();
    return 0;
}

int  HikPlayer::clearblurarea( )
{
    m_ba_number = 0 ;
    if( m_blur_save_CapBuf ) {
        // restore pause buffer
        memcpy( m_CapBuf, m_blur_save_CapBuf, m_CapSize );
        delete m_blur_save_CapBuf ;
        m_blur_save_CapBuf=NULL ;
    }
    refreshdecoder();
    return 0 ;
}

// quick blur
void blur( unsigned char *pix, int pitch, int bx, int by, int bw, int bh, int radius, int shape ) 
{
    if (radius<1) return;
    int wm=bw-1;
    int hm=bh-1;
    int csum,x,y,i,p,yp,yi;
    int div=radius+radius+1;
    int vmin, vmax ;
    unsigned char * c = new unsigned char [bw*bh] ;

    int a, b, aa, bb ;
    a = bw/2 ;
    b = bh/2 ;
    aa = a*a ;
    bb = b*b ;

    yi=0;

    unsigned char * pixline = pix+by*pitch+bx ;
    for (y=0;y<bh;y++){
        csum=0;
        for(i=-radius;i<=radius;i++){
            p = min(wm, max(i,0));
            csum += pixline[p];
        }
        for (x=0;x<bw;x++){
            c[yi]=csum/div ;
            vmin = min(x+radius+1,wm) ;
            vmax = max(x-radius, 0 );
            csum += pixline[vmin] - pixline[vmax];
            yi++;
        }
        pixline += pitch ;
    }

    pixline = pix+by*pitch+bx ;

    for (x=0;x<bw;x++){
        csum=0;
        yp=-radius*bw;
        for(i=-radius;i<=radius;i++){
            yi=max(0,yp)+x;
            csum+=c[yi];
            yp+=bw ;
        }

        yi = 0 ;
        for (y=0;y<bh;y++){

            if( shape == 0 ){
                pixline[ yi ] = csum/div ;
            }
            else {
                int dx = x-a ;
                int dy = y-b ;
                if( 512*dx*dx/aa + 512*dy*dy/bb < 512 ) {
                    pixline[ yi ] = csum/div ;
                }
            }
            vmin = min(y+radius+1,hm)*bw;
            vmax = max(y-radius,0)*bw;
            csum+=c[x+vmin]-c[x+vmax];
            yi+=pitch;
        }
        pixline += 1 ;
    }

    delete c ;
}

void HikPlayer::blurYV12Buf()
{
    int i;
    for( i=0;i<m_ba_number;i++) {
        int bx, by, bw, bh, radius ;
        bx = m_blur_area[i].x*m_CapWidth / 256 ;
        by = m_blur_area[i].y*m_CapHeight / 256 ;
        bw = m_blur_area[i].w*m_CapWidth / 256 ;
        bh = m_blur_area[i].h*m_CapHeight / 256 ;
        radius = m_blur_area[i].radius*(m_CapWidth+m_CapHeight)/512 ;

        if(bx<0) bx=0 ;
        if(by<0) by=0 ;
        if(bw+bx>=m_CapWidth) {
            bw = m_CapWidth - bx ;
        }
        if(bh+by>=m_CapHeight) {
            bh = m_CapHeight - by ;
        }
        if( bw>3 && bh>3 && radius>3 && radius<bw && radius<bh ) {
            blur( (unsigned char *)m_CapBuf, m_CapWidth, bx, by, bw, bh, radius/2, m_blur_area[i].shape );
            blur( (unsigned char *)m_CapBuf, m_CapWidth, bx, by, bw, bh, radius/2, m_blur_area[i].shape );
            blur( (unsigned char *)m_CapBuf+m_CapWidth*m_CapHeight, m_CapWidth/2, bx/2, by/2, bw/2, bh/2, radius/2, m_blur_area[i].shape );
            blur( (unsigned char *)m_CapBuf+m_CapWidth*m_CapHeight+m_CapWidth*m_CapHeight/4, m_CapWidth/2, bx/2, by/2, bw/2, bh/2, radius/2, m_blur_area[i].shape );
        }
    }
}

// blur process requested from .AVI encoder
void HikPlayer::BlurDecFrame( char * pBuf, int nSize, int nWidth, int nHeight, int nType)
{
    if(nType == T_YV12 && m_ba_number>0 ) {
        m_CapBuf = pBuf ;
        m_CapSize = nSize ;
        m_CapWidth = nWidth ;
        m_CapHeight = nHeight ;
        m_CapType = nType;
        blurYV12Buf();
    }
}
