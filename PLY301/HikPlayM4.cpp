// HikPlayM4.cpp: implementation of the HikPlayM4 class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
//#include "DVRCLIENT.h"
//#include "desfile.h"
//#include "dvrfile.h"
//#include "videopass.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

#include "HikPlayM4.h"
#include "dvrclient.h"
#include "Client.h"

extern long g_CurrentCopied;
extern int g_CancelCopy;

char firstdrive = 'I' ;
char lastdrive = 'C' ;

char Head264[40] = {
  0x34, 0x48, 0x4b, 0x48, 0xfe, 0xb3, 0xd0, 0xd6, 
  0x08, 0x03, 0x04, 0x20, 0x00, 0x00, 0x00, 0x00,
  0x03, 0x10, 0x02, 0x10, 0x01, 0x10, 0x10, 0x00,
  0x80, 0x3e, 0x00, 0x00, 0xc0, 0x02, 0xe0, 0x01,
  0x11, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 
} ;

Synchronizer syn ;
int focus ;

HikPlayM4	* HikPlayer[MAXHIKPLAYER] ;
CPlayer * Player[MAXHIKPLAYER] ;

void CALLBACK SourceBufCallBack(long nPort, DWORD nBufSize, DWORD dwUser, void * pReserved);
void CALLBACK DisplayCBFunc(long nPort,char * pBuf,long nSize,long nWidth,long nHeight,long nStamp,long nType,long nReserved);


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CString ServerName = "DVR";

HikPlayM4::HikPlayM4( HWND msgWnd, HWND hplyWnd, int Channel )
{
	nChannel = Channel ;
	nPort=-1 ;
	playMode = PLAYFILE ;
	hMsgWnd = msgWnd ;
	hPlayWnd =hplyWnd ;
	client=NULL ;
	SoundOn=FALSE ;
	playstat = CLOSED ;
	syncState = IDLE ;
	syncStamp = 0 ;
	st_stream = 0 ;
	keyframes.SetSize(0);
	filestartTime = CTime::GetCurrentTime();
	hPlaylist = NULL;
	CapBuf = NULL ;
	streamHandle = NULL ;

	// day infoc cache
	dayinfoday=CTime( 1999, 1, 1, 0, 0, 0 );
	dayinfo = NULL ;
	dayinfosize=0;
	monthinfoday = CTime( 1999, 1, 2, 0, 0, 0 );
}

HikPlayM4::~HikPlayM4()
{
	ClearPlayList();
	if( nPort>=0 ) {
		Close();
	}
	if( hPlaylist ) {
		ENTER_CRITICAL ;
//		_close(hPlaylist);
		fileClose( hPlaylist );
		LEAVE_CRITICAL ;
	}
	if( client!=NULL ) delete client ;
}

void HikPlayM4::OpenFile( LPCSTR pathname )
{
	if( nPort>=0 ) {
		Close ();
	}
	for( nPort=1; nPort<PLAYM4_MAX_SUPPORTS; nPort++) {
		if( PlayM4_OpenFile(nPort, (char *)pathname)==0 ) continue ;
		playMode = PLAYFILE ;
		PlayM4_GetPictureSize( nPort,&Width, &Height);
		PlayM4_SetFileEndMsg(nPort, hMsgWnd, WM_FILEEND);
		return ;
	}
	nPort = -1 ;
}

void HikPlayM4::OpenList()
{
	int readed ;
	struct hd_file hd ;
	video_file_item * pfile ;
	pfile = (video_file_item *)PlayList[currentPlay] ;
	playMode = PLAYLIST ;
	playstat = SUSPEND ;
	if( nPort<= 0 ) {
//		int hfile = _open( (LPCSTR)(pfile->filepath), _O_BINARY|_O_RDONLY );
//		readed=_read( hfile, &hd, 40);
//		_close( hfile );

		// check file header and encryptions
		while( nPort<=0 ) {
			DVRFILE hfile ;
			hfile = fileOpen(  (LPCSTR)(pfile->filepath), GENERIC_READ );
			if( hfile == NULL ) return ;
			fileSetKey( hfile, g_hVideoKey );
			readed = fileRead( hfile, &hd, 40 );
			fileClose( hfile );
			if( readed == 40 ) {
				if( hd.flag == 0x484b4834 ){
					OpenStream( (BYTE *)&hd, 40, 0) ;
				}
				else {
					// file probably encrypted ;
					if( g_hVideoKey==NULL ) {		// ask for new password
						CVideoPass videopass ;
						if( videopass.DoModal() == IDOK ) {
							TCHAR   videopassword[32] ;
							memset( videopassword, 0, sizeof( videopassword ));
							strncpy( videopassword, videopass.m_password, sizeof( videopassword ));
							HCRYPTHASH hHash ;
							CryptCreateHash(
								g_hCryptProv, 
								CALG_SHA, 
								0, 
								0, 
								&hHash) ;
							CryptHashData(
								hHash, 
								(PBYTE)videopassword,
								sizeof(videopassword),
								0);
							CryptDeriveKey(
								g_hCryptProv,
								CALG_RC4,
								hHash,
								0,
								&g_hVideoKey);
							CryptDestroyHash(hHash);

						}
						else {
							break ;
						}
					}
					else {
						CryptDestroyKey( g_hVideoKey );	// try no key read
						g_hVideoKey = NULL ;
					}
				}
			}
			else {
				break;			// error
			}
		}
	}
	playMode = PLAYLIST ;
}

void HikPlayM4::OpenStream( PBYTE FileHeader, DWORD szFileHeader, int Mode)
{
	if( nPort>=0 ) {
		Close();
	}

	for( nPort=1; nPort<PLAYM4_MAX_SUPPORTS; nPort++) {
		if( PlayM4_OpenStream(nPort, FileHeader, szFileHeader, SOURCEBUFSIZE )==0 ) 
			continue ;
		StreamMode(Mode);
//		PlayM4_SetSourceBufCallBack(nPort, SOURCEBUFTHRESHOLD,  SourceBufCallBack, (DWORD)this, NULL);
		PlayM4_GetPictureSize( nPort,&Width, &Height);
		return ;
	}
	nPort=-1;
}

void HikPlayM4::StreamMode(int mode)
{
	if( mode==0 ) {
		PlayM4_SetStreamOpenMode(nPort,STREAME_REALTIME);
	}
	else {
		PlayM4_SetStreamOpenMode(nPort,STREAME_FILE);
	}
}

// return TRUE for STREAM_FILE, FALSE for STREAM_REALTIME
int HikPlayM4::GetStreamMode()
{
	return (PlayM4_GetStreamOpenMode(nPort) == STREAME_FILE );
}

ULONG HikPlayM4::GetFileTime( LPCSTR pathname )
{
	ULONG filetime=0 ;
	HikPlayM4 player(NULL, NULL, 0) ;
	player.OpenFile(pathname);
	if( player.nPort>=0 ) {
		filetime=player.GetFileTime();
		player.Close();
	}
	return filetime ; 
}

void HikPlayM4::Close()
{
	if( nPort>=0 ) {
		PlayM4_SetDisplayCallBack( nPort , NULL );
		Stop();
		if( playMode==PLAYFILE ) {
			PlayM4_CloseFile(nPort);
			nPort = -1 ;
		}
		else {
			PlayM4_CloseStream(nPort);
			nPort = -1 ;
			if( client!=NULL ) {
				delete client ;
				client=NULL ;
			}
		}
	}
	playstat = CLOSED ;
	syncState = IDLE ;
	Refresh();

	// clear day info cache
	if( dayinfosize>0 ) 
		delete dayinfo ;
	dayinfosize=0;
	dayinfo=NULL ;
	dayinfoday=CTime( 1999, 1, 1, 0, 0, 0 );
	monthinfoday = CTime( 1999, 1, 2, 0, 0, 0 );
}

void  HikPlayM4::Reset()
{
	if( nChannel<0 || nChannel>=MAXHIKPLAYER) return ;
	if( playMode == PLAYSTREAM ) {
		ConnectChannel();
	}
	else if( playMode == PLAYLIST ) {
	}
	else if( playMode == PLAYMULTI ) {
	}
}

BOOL HikPlayM4::Play()
{
	BOOL v ;
	if( nPort<0 ) return FALSE ;
	if( GetStreamMode() ) {
		PlayM4_Stop( nPort );
		StreamMode(0);
	}
    v = PlayM4_Play( nPort, hPlayWnd );
	PlayM4_SetDisplayCallBack( nPort , DisplayCBFunc );
	playstat=PLAY ;
	PlayM4_ThrowBFrameNum(nPort, 0);
	if( SoundOn ) PlaySound();
	return v ;
}

BOOL  HikPlayM4::Stop()
{
	playstat=STOP;  
	CapBuf = NULL ;
	StopStreamData();
	return PlayM4_Stop( nPort );
}

BOOL  HikPlayM4::Refresh()
{
	RedrawWindow( hPlayWnd, NULL, NULL, RDW_INVALIDATE|RDW_UPDATENOW );
	return TRUE ;
}


unsigned char limittab[512] ;
int	ytab[256] ;
int	rvtab[256] ;
int	gutab[256] ;
int	gvtab[256] ;
int	butab[256] ;
void YV12_RGB( void * bufRGB, int pitch, void * bufYV12, int pitchYV12, int width, int height ) 
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
			ytab[i] = 1.164 * (i-16)    + 128;
			rvtab[i] = 1.596 *(i-128) ;
			gutab[i] = - 0.391 * (i-128 ) ;
			gvtab[i] = - 0.813 * (i-128 ) ;
			butab[i] = 2.018 * (i-128 );
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

BOOL drawFrame( HDC hDc, RECT * rect, char * framebuf, int width, int height)
{
	Rect gRect(0, 0, width, height) ;
	BitmapData lockedBitmapData;
	Graphics graphics(hDc);
	Bitmap yv12( width, height, PixelFormat32bppRGB);
	yv12.LockBits(&gRect, ImageLockModeWrite, PixelFormat32bppRGB, &lockedBitmapData );
	YV12_RGB( lockedBitmapData.Scan0, lockedBitmapData.Stride , framebuf, width, width, height);
	yv12.UnlockBits(&lockedBitmapData);
	graphics.DrawImage( &yv12, 0 , 0 , rect->right , rect->bottom );
	return TRUE ;
}

BOOL HikPlayM4::RefreshPlay( HDC hDc, RECT * rect)
{
	BOOL result = FALSE ;
	if( nPort>0 && playstat == PAUSE ) {
		if( !IsBadReadPtr( CapBuf, CapSize ) ) {
			ENTER_CRITICAL ;
			result = drawFrame( hDc, rect, CapBuf, CapWidth, CapHeight );
			LEAVE_CRITICAL ;
		}
	}
	if( nPort>0 && playstat == ONEBYONE ) {
		result = PlayM4_RefreshPlay(nPort) ;
	}
	return result ;
}

DWORD HikPlayM4::GetKeyFramePosByTime(DWORD milisecs) 
{
	FRAME_POS FramePos ;
	FramePos.nFilePos = 0 ;
	PlayM4_GetKeyFramePos(nPort,milisecs, BY_FRAMETIME, &FramePos );
	return FramePos.nFilePos ;
}

DWORD HikPlayM4::GetKeyFramePosByFrame(DWORD nFrame ) 
{
	FRAME_POS FramePos ;
	FramePos.nFilePos = 0 ;
	PlayM4_GetKeyFramePos(nPort,nFrame, BY_FRAMENUM, &FramePos );
	return FramePos.nFilePos ;
}

DWORD HikPlayM4::GetNextKeyFramePosByTime(DWORD milisecs) 
{
	FRAME_POS FramePos ;
	FramePos.nFilePos = 0 ;
	PlayM4_GetNextKeyFramePos(nPort,milisecs, BY_FRAMETIME, &FramePos );
	return FramePos.nFilePos ;
}

DWORD HikPlayM4::GetNextKeyFramePosByFrame(DWORD nFrame ) 
{
	FRAME_POS FramePos ;
	FramePos.nFilePos = 0 ;
	PlayM4_GetNextKeyFramePos(nPort,nFrame, BY_FRAMENUM, &FramePos );
	return FramePos.nFilePos ;
}

// Possibly at the end of file
void HikPlayM4::OnFileEnd()
{
//	Stop();
	if( currentPlay<(PlayList.GetSize()-1) ) {
		currentPlay++ ;
		PlayCurrent();
	}
	else {
		Close();
	}
}

int	 HikPlayM4::ConnectChannel()
{
	Close();
	if( client!=NULL ) 
		delete client ;
	playMode = PLAYSTREAM ;
	if( nChannel<0 || nChannel>=MAXHIKPLAYER) 
		return 0;
	client = new CClient(this, nChannel) ;
	return client->ConnectRealTime();
}

int findkeyframe( int hfile, DWORD timestamp )
{
	return 0;
}

int findmainframe( int hfile )
{
	int i;
	int pos ;
	int readed ;
	pos = _lseek( hfile, 0, SEEK_CUR );
	pos&=~3 ;	// align to dword
	_lseek( hfile, pos, SEEK_SET );

	int * pbuf = new int [1024] ;

	for( i=0; i<50; i++ ) {
		readed = _read( hfile, pbuf, 4096 );
		if( readed != 4096 ) {
			delete [] pbuf ;
			return -1 ;
		}
		for( int j=0; j<1024; j++ ) {
			if( pbuf[j]==1 ) {		// found
				delete [] pbuf ;
				pos += i*4096+j*4 ;
				_lseek( hfile, pos, SEEK_SET );
				return pos ;
			}
		}
	}
	delete [] pbuf;
	return -1 ;
}

int findmainframe( DVRFILE hfile )
{
	int i;
	int pos ;
	int readed ;
	pos = fileSeek( hfile, 0, FILE_CURRENT );
	pos&=~3 ;	// align to dword
	fileSeek( hfile, pos, FILE_BEGIN );

	int * pbuf = new int [1024] ;

	for( i=0; i<50; i++ ) {
		readed = fileRead( hfile, pbuf, 4096 );
		if( readed != 4096 ) {
			delete [] pbuf ;
			return -1 ;
		}
		for( int j=0; j<1024; j++ ) {
			if( pbuf[j]==1 ) {		// found
				delete [] pbuf ;
				pos += i*4096+j*4 ;
				fileSeek( hfile, pos, FILE_BEGIN );
				return pos ;
			}
		}
	}
	delete [] pbuf;
	return -1 ;
}

int nextmainframe( int hfile )
{
	struct hd_frame mainframe ;
	struct hd_subframe subframe ;
	DWORD readbytes ;
	int pos ;
	readbytes=_read( hfile, &mainframe, sizeof(mainframe));
	if( readbytes == sizeof(mainframe) && mainframe.flag == 1 && mainframe.frames<10 && mainframe.framesize<1000000 ) {
		int subframes = mainframe.frames ;
		DWORD framesize = mainframe.framesize ;
		pos = _lseek( hfile, mainframe.framesize, SEEK_CUR );
		while( --subframes>0 ) {
			_read( hfile, &subframe, sizeof(subframe));
			if( subframe.flag != 0x1001 && subframe.flag != 0x1005 ) {
				return 0;
			}
			pos = _lseek( hfile, subframe.framesize , SEEK_CUR );
		}
		return pos ;
	}
	return 0 ;
}

int prevmainframe( int hfile )
{
	int i;
	int pos = _lseek( hfile, 0, SEEK_CUR );
	pos = (pos&(~3)) - 4040 ;
	if( pos<40 ) pos = 40 ;
	int * pbuf ;
	pbuf = new int [1024] ;

	for( i=0; i<50; i++, pos-=4000) {
		_lseek( hfile, pos, SEEK_SET);
		_read( hfile, pbuf, 4096);
		for( int j=999; j>=0; j-- ){
			if( pbuf[j]==1 ) {		// mainframe found
				struct hd_frame * pmainframe ;
				pmainframe = (struct hd_frame *) &pbuf[j] ;
				if( pmainframe->frames < 10 && pmainframe->framesize < 1000000 ) {
					delete pbuf ;
					pos += j*4 ;
					_lseek( hfile, pos, SEEK_SET );
					return pos ;
				}
			}
		}
	}
	delete pbuf ;
	return 0 ;
}

struct hd_frame * getmainframe(int hfile)
{
	DWORD readed ;
	struct hd_frame frame ;
	frame.flag = 0 ;
	int pos = _lseek( hfile, 0, SEEK_CUR );
	readed = _read( hfile, &frame, sizeof(frame));
	if( readed != sizeof(frame) ){
		return NULL ;
	}
    if( frame.flag != 1 ) {
		int keypos ;
		_lseek( hfile, pos, SEEK_SET );
		keypos = findmainframe( hfile );
		if( keypos < 0 ) {
			return NULL ;
		}
		else {
			_lseek( hfile, keypos, SEEK_SET);
			readed = _read( hfile, &frame, sizeof(frame));
			if( readed != sizeof(frame) || frame.flag != 1 ) {
				return NULL ;
			}
		}
	}
	frame.timestamp &= 0x7fffffff ;
	if( frame.framesize > 1000000 )
		return NULL ;

	PBYTE framebuf = new BYTE [sizeof( struct hd_frame ) + frame.framesize ] ;
	if( framebuf==NULL ) return NULL ;
	memcpy( framebuf, &frame, sizeof( struct hd_frame ) );
	readed = _read( hfile, &framebuf[sizeof( struct hd_frame )], frame.framesize );
	if( readed!=frame.framesize ) {
		delete framebuf ;
		return NULL ;
	}
	return (hd_frame *)framebuf ;
}

struct hd_frame * getmainframe(DVRFILE hfile)
{
	DWORD readed ;
	struct hd_frame frame ;
	frame.flag = 0 ;
	int pos = fileSeek( hfile, 0, FILE_CURRENT );
	readed = fileRead( hfile, &frame, sizeof(frame));
	if( readed != sizeof(frame) ){
		return NULL ;
	}
    if( frame.flag != 1 ) {
		int keypos ;
		fileSeek( hfile, pos, FILE_BEGIN );
		keypos = findmainframe( hfile );
		if( keypos < 0 ) {
			return NULL ;
		}
		else {
			fileSeek( hfile, keypos, FILE_BEGIN);
			readed = fileRead( hfile, &frame, sizeof(frame));
			if( readed != sizeof(frame) || frame.flag != 1 ) {
				return NULL ;
			}
		}
	}
	frame.timestamp &= 0x7fffffff ;
	if( frame.framesize > 1000000 )
		return NULL ;

	PBYTE framebuf = new BYTE [sizeof( struct hd_frame ) + frame.framesize ] ;
	if( framebuf==NULL ) return NULL ;
	memcpy( framebuf, &frame, sizeof( struct hd_frame ) );
	readed = fileRead( hfile, &framebuf[sizeof( struct hd_frame )], frame.framesize );
	if( readed!=frame.framesize ) {
		delete framebuf ;
		return NULL ;
	}
	return (hd_frame *)framebuf ;
}

struct hd_subframe * getsubframe( int hfile )
{
	DWORD readed ;
	struct hd_subframe subframe ;
	subframe.flag = 0 ;
	int pos = _lseek( hfile, 0, SEEK_CUR );
	readed = _read( hfile, &subframe, sizeof(subframe) );
	if( readed != sizeof(subframe) ){
		return NULL ;
	}
    if( subframe.flag == 0x1001 || subframe.flag == 0x1005 ) {
		if( subframe.framesize > 0 && subframe.framesize <1000000 ) {
			PBYTE pbuf = new BYTE [sizeof(subframe)+subframe.framesize] ;
			if( pbuf==NULL ) return NULL ;
			memcpy( pbuf, &subframe, sizeof(subframe));
			readed = _read(hfile, &pbuf[sizeof(subframe)], subframe.framesize );
			if( readed!=subframe.framesize ) {
				delete pbuf;
				return NULL ;
			}
			return (struct hd_subframe *)pbuf ;
		}
	}
	return NULL ;
}

struct hd_subframe * getsubframe( DVRFILE hfile )
{
	DWORD readed ;
	struct hd_subframe subframe ;
	subframe.flag = 0 ;
	int pos = fileSeek( hfile, 0, FILE_CURRENT );
	readed = fileRead( hfile, &subframe, sizeof(subframe) );
	if( readed != sizeof(subframe) ){
		return NULL ;
	}
    if( subframe.flag == 0x1001 || subframe.flag == 0x1005 ) {
		if( subframe.framesize > 0 && subframe.framesize <1000000 ) {
			PBYTE pbuf = new BYTE [sizeof(subframe)+subframe.framesize] ;
			if( pbuf==NULL ) return NULL ;
			memcpy( pbuf, &subframe, sizeof(subframe));
			readed = fileRead(hfile, &pbuf[sizeof(subframe)], subframe.framesize );
			if( readed!=subframe.framesize ) {
				delete pbuf;
				return NULL ;
			}
			return (struct hd_subframe *)pbuf ;
		}
	}
	return NULL ;
}

// the stream is a one big file contain 4 channels
/*
void HikPlayM4::PlayStream( int handle, DWORD start, DWORD end, CTime stTime, int length)
{
	BYTE buf [40] ;
	playMode = PLAYMULTI ;
	streamHandle = 0;
	if( start<=0 || start>=end ) {
		Close();
		streamHandle = 0;
		return ;
	}
	streamHandle = handle ;
	streamPointer = 0 ;
	streamStartTime=stTime ;
	streamLength = length ;

	ENTER_CRITICAL ;
	DesSeek(streamHandle, start, SEEK_SET);
	DesRead( streamHandle, buf, 40 );
	streamStart=DesSeek( streamHandle, 0, SEEK_CUR );

	struct hd_frame * frame = getmainframe(streamHandle) ;
	if( frame ) {
		startStamp = frame->timestamp ;
		delete (PBYTE) frame ;
	}
	else {
		startStamp = 0 ;
	}

	DesSeek( streamHandle, streamStart, SEEK_SET );
	LEAVE_CRITICAL ;

	streamSize = end-streamStart ;
	if( streamSize<=0 ) streamSize=0;
	CapStamp=0 ;
	OpenStream( buf, 40, 0 );

	playMode = PLAYMULTI ;
}
*/

void HikPlayM4::PlayStream( LPCTSTR playfilename, DWORD start, DWORD end, CTime stTime, int length)
{
	BYTE buf [40] ;
	playMode = PLAYMULTI ;
	if( streamHandle ){
		fileClose( streamHandle );
		streamHandle = NULL ;
	}

	if( start<=0 || start>=end ) {
		Close();
		return ;
	}

	streamHandle = fileOpen( playfilename, GENERIC_READ );
	streamPointer = 0 ;
	streamStartTime=stTime ;
	streamLength = length ;

	fileSeek(streamHandle, start, FILE_BEGIN);
	fileRead(streamHandle, buf, 40 );
	streamStart=fileSeek( streamHandle, 0, FILE_CURRENT);

	struct hd_frame * frame = getmainframe(streamHandle) ;
	if( frame ) {
		startStamp = frame->timestamp ;
		delete (PBYTE) frame ;
	}
	else {
		startStamp = 0 ;
	}

	fileSeek( streamHandle, streamStart, FILE_BEGIN );
//	LEAVE_CRITICAL ;

	streamSize = end-streamStart ;
	if( streamSize<=0 ) streamSize=0;
	CapStamp=0 ;
	OpenStream( buf, 40, 0 );

	playMode = PLAYMULTI ;
}

UINT streamDataThread(LPVOID param)
{
	return ((HikPlayM4 *)(param))->StreamData();
}

void HikPlayM4::StartStreamData()
{
	if( st_stream !=1 ) {
		AfxBeginThread(streamDataThread, this, THREAD_PRIORITY_LOWEST);
	}
}

void HikPlayM4::StopStreamData()
{
	if( st_stream==1 ) {
		st_stream=2 ;
		for( int i=0; i<1000; i++ ) {
			Sleep(2);
			if( st_stream==0 ) break ;
		}
	}
	syncState = IDLE ;
	st_stream=0 ;
}


UINT HikPlayM4::StreamData()
{
	struct hd_frame * frame = NULL ;
	int synPause = 0 ;
	int refreshcounter ;
	int kFrameSize = 100 ;
	syncState = RUN ;

	Refresh();
	if( playMode == PLAYMULTI ) {
		syncStamp = 0 ;
		st_stream = 1 ;
		while( st_stream == 1 ) {
			if( playstat == SUSPEND ) {
				Sleep(2);
				continue;
			}
			if( playstat == CLOSED || playstat==STOP ) 
			{
				goto quitStream ;
			}
			if( streamHandle==NULL ) {
				goto quitStream ;
			}

			if( streamPointer>=streamSize ) {
				frame = NULL ;
			}
			else {
//				ENTER_CRITICAL ;
//				_lseek( streamHandle, streamStart+streamPointer, SEEK_SET );
				fileSeek( streamHandle, streamStart+streamPointer, FILE_BEGIN );
				frame = getmainframe(streamHandle);
//				streamPointer = _lseek(streamHandle, 0, SEEK_CUR) - streamStart ; 
				streamPointer = fileSeek(streamHandle, 0, FILE_CURRENT) - streamStart ; 
//				LEAVE_CRITICAL ;
			}

			if( frame==NULL ) {
				goto quitStream ;
			}

			int thredhold ;
			if( playstat == ONEBYONE || playstat == SLOW ) 
				thredhold = 1 ;
			else 
				thredhold = 2 * kFrameSize ;
			while( PlayM4_GetSourceBufferRemain(nPort) > thredhold ){
				if( st_stream!=1 ) {
					goto quitStream ;
				}
				Sleep(2);
			}

			int frames = frame->frames ;
			if( frames == 1 ) {
				Height = frame->height ;
				Width = frame->width ;
			}

			DWORD oldStamp = syncStamp ;
			syncStamp = frame->timestamp ;

			if( (syncStamp<oldStamp ) && (oldStamp-syncStamp)>300 ) {
				syncState = ROLLOVER ;
			}
			else {
				syncState = RUN ;
			}
			refreshcounter=0 ;
			while( syncState != RUN ) {
				if( st_stream != 1 ) {
					goto quitStream ;
				}
				Sleep(2);
				if( ++refreshcounter==300 ) 
					Refresh();
				ENTER_CRITICAL ;
				syn.CheckState();
				LEAVE_CRITICAL ;
			}

			DWORD curStamp  ;
			refreshcounter=0 ;
			while( (curStamp = syn.GetStamp()) < syncStamp ) {
				if( st_stream!=1 ) {
					goto quitStream ;
				}
				Sleep(2);
				if( (syncStamp-curStamp)>200 ) {
					syncState = BIGJUMP ;
					ENTER_CRITICAL ;
					syn.CheckState();
					LEAVE_CRITICAL ;
					if( ++refreshcounter==300 ) 
						Refresh();
				}
				if( playstat == ONEBYONE || playstat == SLOW || playstat==FAST ) {
					syn.AdjustStamp();
				}
			}
			syncState = RUN ;

			// submit frames
			if( frames == 1 ) {
				kFrameSize = frame->framesize+sizeof( struct hd_frame) ;
				if( (curStamp-syncStamp)<300 ) {
					PushStreamData( (PBYTE) frame, frame->framesize+sizeof( struct hd_frame) );
				}
				delete (PBYTE) frame ;
				frame = NULL ;
			}
			else {
				if( (curStamp-syncStamp)<100 ) {
					PushStreamData( (PBYTE) frame, frame->framesize+sizeof( struct hd_frame) );
				}
				delete (PBYTE) frame ;
				frame = NULL ;
				while( frames > 1 ) {
					struct hd_subframe * subframe  ;
					if( streamPointer < streamSize ) {
//						ENTER_CRITICAL ;
//						_lseek( streamHandle, streamStart+streamPointer, SEEK_SET );
						fileSeek(streamHandle, streamStart+streamPointer, FILE_BEGIN );
						subframe = getsubframe( streamHandle );
//						streamPointer = _lseek(streamHandle, 0, SEEK_CUR) - streamStart ; 
						streamPointer = fileSeek(streamHandle, 0, FILE_CURRENT ) - streamStart ; 
//						LEAVE_CRITICAL ;
					}
					if( subframe == NULL ) {
						break;
					}
					if( (curStamp-syncStamp)<100 ) {
						PushStreamData( (PBYTE)subframe, subframe->framesize+sizeof( struct hd_subframe) );
					}
					delete [] (PBYTE)subframe ;
					frames-- ;
				}
			}
		}
	}
	else if( playMode == PLAYLIST  ) {
		syncStamp = 0 ;
		st_stream = 1 ;
		while( st_stream == 1 ) {
			if( playstat == SUSPEND ) {
				Sleep(2);
				continue ;
			}
			if( playstat == CLOSED || playstat==STOP ) 
			{
				Refresh();
				goto quitStream ;
			}
			while( hPlaylist==NULL ) {
				currentPlay++ ;
				if( currentPlay>=0 && currentPlay<PlayList.GetSize() ){
					ENTER_CRITICAL ;
//					hPlaylist = _open( (LPCSTR)((video_file_item *)PlayList[currentPlay])->filepath, _O_BINARY|_O_RDONLY );
					hPlaylist = fileOpen( (LPCSTR)((video_file_item *)PlayList[currentPlay])->filepath, GENERIC_READ );
					if( hPlaylist ) 
						fileSetKey( hPlaylist, g_hVideoKey );
					LEAVE_CRITICAL ;
					if( hPlaylist == NULL ) continue ;
//					_lseek( hPlaylist, 40, SEEK_SET );
					fileSeek( hPlaylist, 40, FILE_BEGIN );
					frame = getmainframe(hPlaylist);
					if(frame==NULL ) {
						ENTER_CRITICAL ;
//						_close( hPlaylist );
//						hPlaylist=-1;
						fileClose( hPlaylist );
						hPlaylist=NULL;
						LEAVE_CRITICAL ;
					}
					else {
						startStamp = frame->timestamp ;
						startTime = ((video_file_item *)PlayList[currentPlay])->filetime ;
					}
				}	
				else {
					playstat = SUSPEND ;
					syncState = IDLE ;
					
					while( st_stream == 1 ) {
						Sleep(2);
						if( PlayM4_GetSourceBufferRemain(nPort)<1000 ) 
							break;
					}

					// delay 1 seconds, wait all frames are displayed
					int loop=200 ;
					while( st_stream == 1 && --loop>0) {
						Sleep(2);
					}

					Refresh();
					goto quitStream ;
				}
			}

			if( frame==NULL ) 
				frame = getmainframe(hPlaylist);
			if( frame==NULL ) {
				ENTER_CRITICAL ;
//				_close( hPlaylist );
//				hPlaylist = -1 ;
				fileClose( hPlaylist );
				hPlaylist = NULL ;
				LEAVE_CRITICAL ;
				continue ;
			}

			int thredhold ;
			if( playstat == ONEBYONE || playstat == SLOW ) 
				thredhold = 100 ;
			else 
				thredhold = 2 * kFrameSize ;
			while( PlayM4_GetSourceBufferRemain(nPort) > thredhold ){
				if( st_stream!=1 ) {
					goto quitStream ;
				}
				Sleep(2);
			}
			
			int frames = frame->frames ;
			if( frames == 1 ) {
				Height = frame->height ;
				Width = frame->width ;
			}

			DWORD oldStamp = syncStamp ;
			syncStamp = frame->timestamp ;

			if( (syncStamp<oldStamp ) && (oldStamp-syncStamp)>300 ) {
				syncState = ROLLOVER ;
			}
			else {
				syncState = RUN ;
			}
			refreshcounter=0 ;
			while( syncState != RUN ) {
				if( st_stream != 1 ) {
					goto quitStream ;
				}
				Sleep(2);
				if( ++refreshcounter==300 ) 
					Refresh();
				ENTER_CRITICAL ;
				syn.CheckState();
				LEAVE_CRITICAL ;
			}

			DWORD curStamp  ;
			refreshcounter=0 ;
			while( (curStamp = syn.GetStamp()) < syncStamp ) {
				if( st_stream!=1 ) {
					goto quitStream ;
				}
				Sleep(2);
				if( (syncStamp-curStamp)>200 ) {
					syncState = BIGJUMP ;
					ENTER_CRITICAL ;
					syn.CheckState();
					LEAVE_CRITICAL ;
					if( ++refreshcounter==300 ) 
						Refresh();
				}

				if( playstat == ONEBYONE || playstat == SLOW || playstat==FAST ) {
					syn.AdjustStamp();
				}
			}
			syncState = RUN ;

			// submit mulitple subframes
			if( frames == 1 ) {
				kFrameSize = frame->framesize+sizeof( struct hd_frame) ;
				if( (curStamp-syncStamp)<300 ) {
					PushStreamData( (PBYTE) frame, frame->framesize+sizeof( struct hd_frame) );
				}
				delete (PBYTE) frame ;
				frame = NULL ;
			}
			else {
				if( (curStamp-syncStamp)<100 ) {
					PushStreamData( (PBYTE) frame, frame->framesize+sizeof( struct hd_frame) );
				}
				delete (PBYTE) frame ;
				frame = NULL ;
				while( frames > 1 ) {
					struct hd_subframe * subframe = getsubframe( hPlaylist );
					if( subframe == NULL ) {
						ENTER_CRITICAL ;
//						_close( hPlaylist );
//						hPlaylist = -1 ;
						fileClose( hPlaylist );
						hPlaylist = NULL ;
						LEAVE_CRITICAL ;
						break;
					}
					if( (curStamp-syncStamp)<100 ) {
						PushStreamData( (PBYTE)subframe, subframe->framesize+sizeof( struct hd_subframe) );
					}
					delete [] (PBYTE)subframe ;
					frames-- ;
				}
			}
		}
	}
	else if( playMode == PLAYSERVER ) {
		syncStamp = 0 ;
		st_stream = 1 ;
		while( st_stream == 1 ) {
			if( playstat == SUSPEND ) {
				Sleep(2);
				continue;
			}
			if( playstat == CLOSED || playstat==STOP ) 
			{
				goto quitStream ;
			}
			if( dvrstream<0 ) {
				goto quitStream ;
			}

			// get dvr stream data
			int fsize ;
			fsize=0;
			if( DVRStreamGetData( dvrstream, (void **)&frame, &fsize )<=0 ) {	// error or end of stream
				goto quitStream ;
			}

			int thredhold ;
			if( playstat == ONEBYONE || playstat == SLOW ) 
				thredhold = 1 ;
			else 
				thredhold = kFrameSize ;
			while( PlayM4_GetSourceBufferRemain(nPort) > thredhold ){
				if( st_stream!=1 ) {
					goto quitStream ;
				}
				Sleep(2);
			}

			int frames = frame->frames ;
			if( frames == 1 ) {
				Height = frame->height ;
				Width = frame->width ;
				kFrameSize = fsize  ;
			}

			DWORD oldStamp = syncStamp ;
			syncStamp = frame->timestamp ;

			if( (syncStamp<oldStamp ) && (oldStamp-syncStamp)>300 ) {
				syncState = ROLLOVER ;
			}
			else {
				syncState = RUN ;
			}
			refreshcounter=0 ;
			while( syncState != RUN ) {
				if( st_stream != 1 ) {
					goto quitStream ;
				}
				Sleep(2);
				if( ++refreshcounter==300 ) 
					Refresh();
				ENTER_CRITICAL ;
				syn.CheckState();
				LEAVE_CRITICAL ;
			}

			DWORD curStamp = syn.GetStamp() ;
			if( curStamp>syncStamp && (curStamp-syncStamp)>200 )
				syn.AdjustStamp();
			refreshcounter=0 ;
			while( (curStamp = syn.GetStamp()) < syncStamp ) {
				if( st_stream!=1 ) {
					goto quitStream ;
				}
				Sleep(2);
				if( (syncStamp-curStamp)>200 ) {
					syncState = BIGJUMP ;
					ENTER_CRITICAL ;
					syn.CheckState();
					LEAVE_CRITICAL ;
					if( ++refreshcounter==300 ) 
						Refresh();
				}
				if( playstat == ONEBYONE || playstat == SLOW || playstat==FAST ) {
					syn.AdjustStamp();
				}
			}
			syncState = RUN ;

			// submit frames
			PushStreamData( (PBYTE) frame, fsize ) ;
			delete (PBYTE) frame ;
			frame = NULL ;
		}
	}
quitStream:
	if( frame )
		delete (PBYTE) frame ;
	syncState = IDLE ;
	st_stream = 0 ;
	return 0 ;
}

// request to get Stream data
void HikPlayM4::GetStreamData()
{
	if( playMode == PLAYSTREAM  ) {			// network stream
//		client->GetRealData(PlayM4_GetSourceBufferRemain( nPort));
	}
	else if( playMode == PLAYMULTI ) {							// file playback stream
		return ;
		if( streamPointer<streamSize ) {
			int size ;
			PBYTE buf=new BYTE [8196] ;
			if( (streamSize-streamPointer)<8196 ) 
				size=streamSize-streamPointer ;
			else 
				size=8196 ;
//			ENTER_CRITICAL ;
//			DesSeek( streamHandle, streamStart+streamPointer, SEEK_SET);
//			size = DesRead( streamHandle, buf, size );
			fileSeek( streamHandle, streamStart+streamPointer, FILE_BEGIN);
			size = fileRead( streamHandle, buf, size );
//			LEAVE_CRITICAL ;
			streamPointer+=size;
			PushStreamData(buf, size);
			delete buf;
		}
	}
}


// On Stream Data Ready
int HikPlayM4::PushStreamData(PBYTE pBuf, DWORD nSize)
{
	return PlayM4_InputData( nPort, pBuf, nSize );
}

void CALLBACK SourceBufCallBack(long nPort, DWORD nBufSize, DWORD dwUser, void * pReserved)
{
	int i ;
	for( i=0; i<nPlayers; i++) {
		if( HikPlayer[i]==NULL ) return ;
		if( HikPlayer[i]->nPort==nPort ) {
			PlayM4_ResetSourceBufFlag(nPort);
			HikPlayer[i]->GetStreamData();
		}
	}
}

char   CaptureImage[128];

void CALLBACK DisplayCBFunc(long nPort,char * pBuf,long nSize,long nWidth,long nHeight,long nStamp,long nType,long nReserved)
{
	if( nType != T_YV12 ) return ;
	for( int i=0; i<nPlayers ; i++) {
		if( HikPlayer[i]==NULL ) continue;
		if( HikPlayer[i]->nPort==nPort ) {
			ENTER_CRITICAL ;
			HikPlayer[i]->CapBuf = pBuf ;
			HikPlayer[i]->CapSize=nSize ;
			HikPlayer[i]->CapWidth=nWidth ;
			HikPlayer[i]->CapHeight=nHeight ;
			HikPlayer[i]->CapStamp=nStamp ;
			HikPlayer[i]->CapType=nType ;
			LEAVE_CRITICAL ;
			break;
		}
	}
}

BOOL HikPlayM4::Capture() 
{
	BOOL result=FALSE ;
	if( nPort<=0 || playstat==STOP ) return FALSE ;
	char * tmp = getenv("TEMP");
	if( tmp==NULL )
		tmp=getenv("TMP");
	if( tmp==NULL )
		tmp=".";
	sprintf(CaptureImage, "%s\\dvrcap.bmp", tmp );

	char * tBuf ;
	long   tSize ;
	long   tWidth ;
	long   tHeight ;
	long   tType ;
	if( !IsBadReadPtr( CapBuf, CapSize ) ) {
		ENTER_CRITICAL ;
		tSize = CapSize ;
		tWidth = CapWidth ;
		tHeight = CapHeight ;
		tType = CapType ;
		tBuf = new char [tSize] ;
		memcpy( tBuf, CapBuf, tSize );
		LEAVE_CRITICAL ;
		result=PlayM4_ConvertToBmpFile( tBuf, tSize, tWidth, tHeight, tType, CaptureImage );
		delete tBuf ;	
	}
	return result;
}

CTime videofiletime(LPCSTR filepath)
{
	char year[5];
	char month[3];
	char day[3];
	char hour[3];
	char minute[3];
	char second[3];
	const char * pos=strstr(filepath, "\\CH" );
	if( pos ) {
		strncpy(year,   pos+11, 4); year[4]=0;
		strncpy(month,  pos+15, 2); month[2]=0;
		strncpy(day,    pos+17, 2); day[2]=0;
		strncpy(hour,   pos+19, 2); hour[2]=0;
		strncpy(minute, pos+21, 2); minute[2]=0;
		strncpy(second, pos+23, 2); second[2]=0;
		return CTime( atoi(year), atoi(month), atoi(day), atoi(hour), atoi(minute), atoi(second));
	}
	else {
		return CTime::GetCurrentTime();
	}
}

int videofilelength(LPCSTR filepath)
{
	int length=0 ;
	const char * pos=strstr(filepath, "\\CH" );
	if( pos ) {
		sscanf(pos+26, "%d", &length);
		return length ;
	}
	else return 0;
}

static int videofilecompare(const void * item1, const void * item2)
{
	video_file_item * p1;
	video_file_item * p2;
	p1 = *(video_file_item **)item1 ;
	p2 = *(video_file_item **)item2 ;
	if     ( p1->filetime > p2->filetime ) return 1 ;
	else if( p1->filetime < p2->filetime ) return -1 ;
	else                                   return 0 ;
}

void HikPlayM4::ClearPlayList()
{
	int i, size ;
	video_file_item * pfile ;
	size = PlayList.GetSize();
	for( i=0; i<size; i++) {
		pfile = (video_file_item *) PlayList[i] ;
		delete pfile ;
	}
	PlayList.SetSize(0);
	if( streamHandle ) {
		fileClose( streamHandle );
		streamHandle = NULL ;
	}
}

int videofilelength_1(LPCSTR fname)
{
	int i;
	struct hd_frame mainframe ;
	DWORD starttime, endtime ;
	int  filesize ;
	int  pos ;

//	int fr = _open( fname, _O_BINARY|_O_RDONLY );
	DVRFILE fr = fileOpen(fname, GENERIC_READ );
	if( fr==NULL ) return 0 ;
	fileSeek( fr, 40, FILE_BEGIN );

//	_read(fr, &mainframe, sizeof(mainframe));
	fileRead( fr, &mainframe, sizeof(mainframe));
	if( mainframe.flag != 1 ) {
//		_close(fr);
		fileClose(fr);
		if( g_hVideoKey ) {
			fr = fileOpen( fname, GENERIC_READ );
			fileSetKey( fr, g_hVideoKey );
			fileRead( fr, &mainframe, sizeof(mainframe));
			if( mainframe.flag != 1 ) {
				fileClose(fr);
				return 0;
			}
		}
		else 
			return 0;
	}
	starttime = mainframe.timestamp ;
//	filesize = _lseek(fr, 0, SEEK_END);
	filesize = fileSeek( fr, 0, FILE_END );
	if( filesize<10000 ) {
//		_close(fr);
		fileClose(fr);
		return 0;
	}

	pos = filesize-4096 ;
	pos = pos&(~3) ;
	int * pbuf ;
	pbuf = new int [1024] ;

	for( i=0; i<50; i++, pos-=4000) {
//		_lseek( fr, pos, SEEK_SET);
//		_read( fr, pbuf, 4096);
		fileSeek( fr, pos, FILE_BEGIN );
		fileRead( fr, pbuf, 4096 );
		for( int j=999; j>=0; j--){
			if( pbuf[j]==1 ) {		// mainframe found
				struct hd_frame * pmainframe ;
				pmainframe = (struct hd_frame *) &pbuf[j] ;
				endtime = pmainframe->timestamp ;
//				_close(fr);
				fileClose( fr );
				delete pbuf ;
				if( endtime>starttime )
					return (endtime-starttime)/64 ;
				else
					return 0;
			}
		}
	}
//	_close(fr);
	fileClose(fr);
	delete pbuf ;
	return 0 ;
}

void HikPlayM4::SearchLocal()
{
	video_file_item * pfile ;

	playMode = PLAYLIST ;

	if( ServerName=="") return ;

	ClearPlayList();
	if( nChannel<0 || nChannel>=MAXHIKPLAYER) return ;
	// search local drive
	DWORD drives = GetLogicalDrives();
	drives>>=2 ;		// Skip 'A', 'B' drive
	for( int drive='C'; drive<='Z'; drive++, drives>>=1 ) {
		CString datefindname ;
		CFileFind datefinder ;
		BOOL bdatefindworking ;
		if( (drives&1)==0 ) continue ;
		datefindname.Format("%c:\\_%s_\\20*", drive, ServerName);
		bdatefindworking = datefinder.FindFile( datefindname );
		while( bdatefindworking ){
			bdatefindworking = datefinder.FindNextFile();
			if( datefinder.IsDirectory() ) {
				CString videofindname ;
				CFileFind videofinder ;
				BOOL bvideofindworking ;
				videofindname.Format("%s\\CH%02d\\CH%02d*.264", datefinder.GetFilePath(), nChannel, nChannel );
				bvideofindworking=videofinder.FindFile( videofindname );
				while( bvideofindworking ) {
					bvideofindworking = videofinder.FindNextFile();
					pfile = new video_file_item ;
					pfile->filepath=videofinder.GetFilePath();
					pfile->filetime=videofiletime((LPCSTR)(pfile->filepath));
					pfile->filelength=videofilelength((LPCSTR)(pfile->filepath));
					if( pfile->filelength == 0 ) {
						pfile->filelength = videofilelength_1((LPCSTR)(pfile->filepath));
					}
					if( pfile->filelength > 0 ) {
						if( pWaitDlg->m_cancel ) return ;
						PlayList.Add(pfile);
						pWaitDlg->Step();
						pWaitDlg->SetMsg(videofinder.GetFileName());
					}
					else {
						delete pfile ;
					}
				}
			}
		}
	}

	// sort the file list
	if( PlayList.GetSize()>0 ) {
		qsort(&PlayList[0], PlayList.GetSize(), sizeof(void *),videofilecompare); 
	}
}

CTime  HikPlayM4::PlayTime() 
{
	if( playMode == PLAYSTREAM ) {
		return CTime::GetCurrentTime();
	}
	else if( playMode == PLAYLIST ) {
		if( nPort>0) {
			if( syncStamp <= startStamp )	
				return startTime ;
			DWORD diff  = syncStamp - startStamp ;
			CTimeSpan ts = CTimeSpan(diff /64);
			return startTime+ts;
		} else {
			return startTime ;
		}
	}
	else if( playMode == PLAYMULTI ) {
		if( nPort>0) {
			if( syncStamp <= startStamp ) {
				return streamStartTime ;
			}
			DWORD diff = syncStamp - startStamp ;
			CTimeSpan ts = CTimeSpan(diff/64 ) ;
			return streamStartTime+ts;
		} else {
			return streamStartTime ;
		}
	}
	else if( playMode == PLAYSERVER ) {
		if( nPort>0) {
			CTime t ;
			if( DVRStreamTime( dvrstream, t ) ) {
				return t ;
			}
			else {
				return startTime ;
			}
		} else {
			return startTime ;
		}
	}
	return CTime( 2005, 1, 1, 0, 0 , 0);
}

UINT checkMultidisk( LPVOID param )
{
	int version[8] ;
	version[0]=version[1]=version[2]=version[3]=0;
	DVR_Version( version );
	if( version[2]>=6 && version[3]>=510 ) 
		return 1 ;
	else 
		return 0 ;
}

int  HikPlayM4::GetDayInfo(struct dayinfoitem * * pdayinfo, CTime day)
{
	if( dayinfoday != day ) {
		if( dayinfosize>0 ) 
			delete dayinfo ;
		dayinfoday = day ;
		dayinfo = new struct dayinfoitem [300] ;
		dayinfosize = DVRStreamDayInfo(dvrstream, dayinfoday, dayinfo, 300 );
		if( dayinfosize>300 ) {		// get dayinfo again ;
			delete dayinfo ;
			dayinfo = new struct dayinfoitem [dayinfosize] ;
			dayinfosize = DVRStreamDayInfo(dvrstream, dayinfoday, dayinfo, dayinfosize );
		}
	}
	*pdayinfo = dayinfo ;
	return dayinfosize ;
}

DWORD  HikPlayM4::GetMonthInfo( CTime month )
{
	CTime monthday( month.GetYear(), 
					month.GetMonth(),
					1,
					0,
					0,
					0 );
	if( monthday != monthinfoday ) {
		monthinfoday = monthday ;
		monthinfo = DVRStreamMonthInfo( dvrstream, monthinfoday );
	}
	return monthinfo ;
}

void HikPlayM4::SearchNetwork()
{
	video_file_item * pfile ;
	playMode = PLAYLIST ;
	if( ServerName=="") return ;
	ClearPlayList();
	if( nChannel<0 || nChannel>=MAXHIKPLAYER) return ;
	CString datefindname ;

	int multiDisk ;
	if( pWaitDlg->m_cancel ) return ;

	// check DVR streamming support
	dvrstream = DVRStreamOpen(nChannel) ;
	if( dvrstream>=0 ) {
		playMode = PLAYSERVER ;
		return ;
	}

	multiDisk = syncProc( checkMultidisk, NULL );

	for(int drive='D'; drive<='Z'; drive++) {
		CFileFind datefinder ;
		BOOL bdatefindworking ;
		if( multiDisk )
			datefindname.Format("\\\\%s\\DISK%C$\\_%s_\\20*", TMEDVRServer, drive, ServerName);
		else
			datefindname.Format("\\\\%s\\share$\\_%s_\\20*", TMEDVRServer, ServerName);
		bdatefindworking = datefinder.FindFile( datefindname );
		while( bdatefindworking ){
			bdatefindworking = datefinder.FindNextFile();
			if( datefinder.IsDirectory() ) {
				CString videofindname ;
				CFileFind videofinder ;
				BOOL bvideofindworking ;
				videofindname.Format("%s\\CH%02d\\CH%02d*.264", datefinder.GetFilePath(), nChannel, nChannel );
				bvideofindworking=videofinder.FindFile( videofindname );
				while( bvideofindworking ) {
					bvideofindworking = videofinder.FindNextFile();
					pfile = new video_file_item ;
					pfile->filepath=videofinder.GetFilePath();
					pfile->filetime=videofiletime((LPCSTR)(pfile->filepath));
					pfile->filelength=videofilelength((LPCSTR)(pfile->filepath)); 
					if( pfile->filelength == 0 ) {
						pfile->filelength = videofilelength_1((LPCSTR)(pfile->filepath));
					}
					if( pfile->filelength > 0 ) {
						PlayList.Add(pfile);
						if( pWaitDlg->m_cancel ) return ;
						pWaitDlg->Step();
						pWaitDlg->SetMsg(videofinder.GetFileName());
					}
					else {
						delete pfile ;
					}
				}
			}
		}
		if(!multiDisk) break;
	}

	// sort the file list
	if( PlayList.GetSize()>0 ) {
		qsort(&PlayList[0], PlayList.GetSize(), sizeof(void *),videofilecompare); 
	}
}

void HikPlayM4::PlayCurrent()
{
	while( currentPlay>=0 && currentPlay<PlayList.GetSize() ){
		OpenList();
		if( nPort>0 ) {
			filestartTime=((video_file_item *)PlayList[currentPlay])->filetime ;
			return ;
		}
		currentPlay++ ;
	}
}

void HikPlayM4::PlayAt( CTime Time )
{
	playstat = SUSPEND ;
	if( playMode == PLAYMULTI ) {
		StopStreamData();
		PlayM4_ResetSourceBuffer(nPort);
		if( streamHandle<=0 ) return ;
		if( nPort<=0 ) { 
			// try reopen stream
//			ENTER_CRITICAL ;
			BYTE buf[40] ;
//			DesSeek( streamHandle, streamStart-40, SEEK_SET );
//			DesRead( streamHandle, buf, 40);
			fileSeek( streamHandle, streamStart-40, FILE_BEGIN );
			fileRead( streamHandle, buf, 40);
			OpenStream( buf, 40, 0 );
//			LEAVE_CRITICAL ;
		}
		syncState = IDLE ;
		if( Time<=streamStartTime) {
			Time=streamStartTime ;
			streamPointer = 0 ;
		}
		else if( (Time-streamStartTime).GetTotalSeconds() < streamLength ){
			ENTER_CRITICAL ;
			int step = streamSize/streamLength ;
			int diffseconds = CTimeSpan(Time-streamStartTime).GetTotalSeconds() ;
			if( diffseconds>2 ) diffseconds-=2 ;
			int targetstamp = startStamp + diffseconds * 64 ;

			streamPointer = (DWORD)( step * diffseconds );
			if( streamPointer > (streamSize-step) ) 
				streamPointer = streamSize-step ;
			streamPointer &= ~3 ;
			hd_frame frame ;
			for( int loop=10; loop>0; loop--) {
				if( streamPointer<step ) 
					break;
				frame.flag = 0 ;
//				_lseek( streamHandle, streamStart+streamPointer, SEEK_SET );
				fileSeek( streamHandle, streamStart+streamPointer, FILE_BEGIN );
				int pos = findmainframe( streamHandle ) ;
				if( pos>0 ) {
//					_read( streamHandle, &frame, sizeof(frame) );
					fileRead( streamHandle, &frame, sizeof(frame) );
					frame.timestamp &= 0x7fffffff ;
				}
				if( frame.flag ==1 && frame.framesize < 1000000 && frame.frames < 10 ) {
					streamPointer = pos - streamStart ;
					if( frame.timestamp > (targetstamp+64) ){
						DWORD stepback = (double)streamPointer *  (frame.timestamp-targetstamp+200) / ( frame.timestamp - startStamp )  ;
						if( streamPointer >= stepback ) {
							streamPointer -= stepback ;
						}
						else {
							break;
						}
					}
					else {
						break;
					}
				}
				else {
					streamPointer-=step ;
				}
			}
			LEAVE_CRITICAL ;
			if( syn.GetStamp()==0 )
				syn.SetStamp( targetstamp );
		}
		else {
			streamPointer = streamSize ;
		}
		StartStreamData();
	}
	else if( playMode == PLAYSERVER ) {
		if( dvrstream<=0 )
			dvrstream=DVRStreamOpen(nChannel);
		if( dvrstream<0 )
			return ;
		StopStreamData();
		PlayM4_ResetSourceBuffer(nPort);

		if( nPort<=0 ) {
			OpenStream( (PBYTE)&Head264, 40, 1 );
		}
		startTime = Time ;
		playstat = SUSPEND ;
		syncState = IDLE ;

		DVRStreamSeek( dvrstream, Time );
		StartStreamData();

	}
	else {		// PLAYLIST
		StopStreamData();
		PlayM4_ResetSourceBuffer(nPort);
		startTime = Time ;
		playstat = SUSPEND ;
		syncState = IDLE ;
//		filestartTime = Time ;
		video_file_item * p ;
		currentPlay = SearchFile(Time);
		if( currentPlay<0 ) {
			if( PlayList.GetSize()<=0 ) {
				Close();
				return ;
			}
			else {
				currentPlay=0 ;
			}
		}

		while( currentPlay<PlayList.GetSize() ) {
			OpenList();
			if( nPort>0 ) {
				p = (video_file_item *)PlayList[currentPlay] ;
				startTime = p->filetime ;
				startStamp = 0 ;

				if( startTime < Time && (startTime + CTimeSpan(p->filelength)) <= Time ) {
					currentPlay++;
					continue ;
				}

				struct hd_frame * frame ;
				if( hPlaylist ) {
//					_close( hPlaylist);
					fileClose( hPlaylist );
					hPlaylist = NULL ;
				}
//				hPlaylist = _open( p->filepath, _O_BINARY | _O_RDONLY ) ;
				hPlaylist = fileOpen( p->filepath, GENERIC_READ );
				if( hPlaylist ) {
					fileSetKey( hPlaylist, g_hVideoKey );
//					_lseek( hPlaylist, 40, SEEK_SET );
					fileSeek( hPlaylist, 40, FILE_BEGIN );
					frame = getmainframe(hPlaylist);
//					_lseek( hPlaylist, 40, SEEK_SET );
					fileSeek( hPlaylist, 40, FILE_BEGIN );
					if( frame ) {
						startStamp = frame->timestamp ;
						if( Time>=startTime && syn.GetStamp()==0 ) {
							syn.SetStamp( startStamp + ((Time-startTime).GetTotalSeconds()-1)*64 ) ;
						}
						delete (PBYTE) frame ;
						break;	
					}
				}
			}
			currentPlay++ ;
		}	

		if( nPort<=0 || startStamp == 0) {
			playstat = CLOSED ;
			syncState = IDLE ;
			Close();
			return ;
		}

		if( p->filetime < Time ) {
			// seek to play position
//			int filesize = _lseek( hPlaylist, 0, SEEK_END );
			int filesize = fileSeek( hPlaylist, 0, FILE_END );
			if( p->filelength > 2 ) {
				long offset ;
				int  diffseconds ;
				diffseconds = (Time - p->filetime).GetTotalSeconds() ;
				if( diffseconds < 2 ) {
					offset = 40 ;
				}
				else if( diffseconds > p->filelength ) {
					offset = filesize & (~3) ;
				}
				else {
					int step = filesize / p->filelength ;
					offset =step * diffseconds ;
					if( offset> (filesize-step) ) {
						offset = filesize-step ;
					}
					offset &= ~3 ;
					int targetstamp = startStamp + (diffseconds-1) * 64 ;
					hd_frame frame ;
					for( int loop=10; loop>0; loop--) {
						if( offset<step )
							break;
						frame.flag = 0 ;
//						_lseek( hPlaylist, offset, SEEK_SET );
						fileSeek( hPlaylist, offset, FILE_BEGIN );
						int pos = findmainframe( hPlaylist ) ;
						if( pos>0 ) {
//							_read( hPlaylist, &frame, sizeof(frame) );
							fileRead( hPlaylist, &frame, sizeof(frame) );
							frame.timestamp &= 0x7fffffff ;
						}
						if( frame.flag ==1 && frame.framesize < 1000000 && frame.frames < 10 ) {
							offset = pos ;
							if( frame.timestamp > (targetstamp+64) ){
								DWORD stepback = (double) pos * (frame.timestamp-targetstamp+200) / ( frame.timestamp - startStamp )  ;
								if( offset>stepback ) {
									offset -= stepback ;
								}
								else {
									break;
								}
							}
							else {
								break;
							}
						}
						else {
							offset -= step ;
						}
					}
				}
				if(offset<40) offset = 40 ;
				offset &= ~3 ;
//				_lseek( hPlaylist, offset, SEEK_SET );
				fileSeek(hPlaylist, offset, FILE_BEGIN );
			}
			else {
//				_lseek( hPlaylist, 40, SEEK_SET );
				fileSeek(hPlaylist, 40, FILE_BEGIN );
			}
		}
		StartStreamData();
	}
}

DWORD getVideoFilePosByTime( int handle, int searchtime )
{
	struct hd_frame mainframe ;
	struct hd_subframe subframe ;
	DWORD starttime ;
	DWORD pos ;
	DWORD readbytes ;
	pos = 40; 
	_lseek(handle, pos, SEEK_SET);
	readbytes=_read( handle, &mainframe, sizeof(mainframe));
	starttime = mainframe.timestamp ;

	// walk through frames
	while( readbytes==sizeof(mainframe) && mainframe.flag==1 ) {
		int subframes = mainframe.frames ;
		DWORD framesize = mainframe.framesize ;
		if( mainframe.frames ==1 && (mainframe.timestamp - starttime )/64 >= searchtime ) {	// found it
			return pos ;
		}
		pos = lseek(handle, framesize, SEEK_CUR);
		while( --subframes>0 ) {
			_read(handle, &subframe, sizeof(subframe));
			if( subframe.flag != 0x1001 && subframe.flag != 0x1005 ) {
				return 0;
			}
			pos = _lseek(handle, subframe.framesize , SEEK_CUR );
		}
		readbytes = _read(handle, &mainframe, sizeof(mainframe));
	}
	return 0 ;
}

DWORD getVideoFilePosByTime( DVRFILE handle, int searchtime )
{
	struct hd_frame mainframe ;
	struct hd_subframe subframe ;
	DWORD starttime ;
	DWORD pos ;
	DWORD readbytes ;
	pos = 40; 
	fileSeek(handle, pos, FILE_BEGIN);
	readbytes=fileRead( handle, &mainframe, sizeof(mainframe));
	starttime = mainframe.timestamp ;

	// walk through frames
	while( readbytes==sizeof(mainframe) && mainframe.flag==1 ) {
		int subframes = mainframe.frames ;
		DWORD framesize = mainframe.framesize ;
		if( mainframe.frames ==1 && (mainframe.timestamp - starttime )/64 >= searchtime ) {	// found it
			return pos ;
		}
		pos = fileSeek(handle, framesize, FILE_CURRENT);
		while( --subframes>0 ) {
			fileRead(handle, &subframe, sizeof(subframe));
			if( subframe.flag != 0x1001 && subframe.flag != 0x1005 ) {
				return 0;
			}
			pos = fileSeek(handle, subframe.framesize , FILE_CURRENT );
		}
		readbytes = fileRead(handle, &mainframe, sizeof(mainframe));
	}
	return 0 ;
}

int HikPlayM4::SearchFile( CTime Time )
{
	int i;
	video_file_item * p ;

	i=PlayList.GetSize();
	if( i<=0 ) {
		return -1;
	}

	// bsearch
	p = (video_file_item *)PlayList[0] ;
	if( Time<p->filetime ) return -1 ;
	i-- ;
	p = (video_file_item *)PlayList[i] ;
	if( Time>=p->filetime ) return i ;

	int high, low ;
	high=i ; low = 0 ;
	i=i/2 ;
	while( i>low ) {
		p= (video_file_item *)PlayList[i] ;
		if( Time<p->filetime ) {
			high=i;
		}
		else {
			low=i ;
		}
		i=(high+low)/2 ;
	}
	return i;

	// linear search
/*
	i=PlayList.GetSize();
	for(--i ; i>=0 ; i-- ) {
		p= (video_file_item *)PlayList[i] ;
		if( p->filetime<=Time ) {
			return i ;
		}
	}
	return -1 ;
*/
}

long getfilesize( LPCTSTR fileName)
{
    WIN32_FILE_ATTRIBUTE_DATA   fileInfo;
	if( fileName==NULL )
		return 0;
	if( ! GetFileAttributesEx(fileName, GetFileExInfoStandard, (void*)&fileInfo) ) {
		return 0 ;
	}
    return (long)fileInfo.nFileSizeLow;
}

long HikPlayM4::GetSaveSize(CTime BeginTime, CTime EndTime)
{
	extern int g_Copytype ;
	long totalsize = 0 ;
	int i;
	video_file_item * vfi ;
	if( BeginTime>=EndTime )
		return 0;
	if( playMode == PLAYSERVER ) {
		if( dvrstream<=0 )
			return 0 ;
		else {
			g_Copytype=1 ;			// count by file time, not size
			return (long)EndTime.GetTime()-(long)BeginTime.GetTime() ;
		}
	}
	else {		// PLAYLIST
		i=SearchFile( BeginTime ) ;
		if( i<0 ) 
			i=0;
		for( ; i<PlayList.GetSize(); i++ ) {
			vfi=(video_file_item *)PlayList[i] ;
			if( vfi->filetime >= EndTime ) break;

			if( vfi->filelength == 0 ) {
				continue ;
			}

			int fsize = getfilesize( vfi->filepath );
			fsize = (fsize+500000)/1000000 ;			// change to maga size

			int copylength = vfi->filelength ;
			CTime fileEndTime ;
			fileEndTime = vfi->filetime + copylength ;
			if( BeginTime > vfi->filetime ) {
				copylength -= ( BeginTime-vfi->filetime).GetTotalSeconds() ;
			}
			if( fileEndTime > EndTime ) {
				copylength -= ( fileEndTime-EndTime ).GetTotalSeconds();
			}
			if( copylength<=0 )
				continue ;
			fsize = fsize * copylength / vfi->filelength ;
			totalsize += fsize ;
		}
	}
	return totalsize;
}

int HikPlayM4::Save( DVRFILE hFile, CTime BeginTime, CTime EndTime )
{
	struct hd_frame mainframe ;
	struct hd_subframe subframe ;
	char * framebuf ;
	DVRFILE handle ;

	DWORD starttime = 0 ;
	DWORD pos ;
	DWORD readbytes ;
	int   w = 0 ;

	if( playMode == PLAYSERVER ) {
		CTime savet ;
		CTime t ;
		int bufsize ;
		void * buf ;
		int startcopy = g_CurrentCopied ;

		if( dvrstream<=0 )
			return 0 ;
		fileWrite( hFile, Head264, 40 );
		DVRStreamTime( dvrstream, savet );
		DVRStreamSeek( dvrstream, BeginTime);
		t=BeginTime ;
		while( t<=EndTime ) {
			if (g_CancelCopy) {
				break;
			}
			if( DVRStreamGetData( dvrstream, &buf, &bufsize)<=0 ) 
				break;
			fileWrite( hFile, buf, bufsize );
			DVRStreamTime( dvrstream, t );
			g_CurrentCopied = startcopy+(t-BeginTime).GetTotalSeconds() ;
		}
		DVRStreamSeek( dvrstream, savet );	// restore old location
	}
	else {
		pos = 40; 
		int i;
		video_file_item * vfi ;
		if( BeginTime>=EndTime )
			return 0;
		i=SearchFile( BeginTime ) ;
		if( i<0 ) 
			i=0;
		if( i>=PlayList.GetSize() )
			return 0;

		// write file header
		vfi = (video_file_item *)PlayList[i] ;
		handle = fileOpen( vfi->filepath, GENERIC_READ );
		if( handle == NULL )
			return FALSE ;
		fileSetKey( handle,  g_hVideoKey );
		framebuf = new char [80] ;
		fileRead( handle, framebuf, 40 );
		fileWrite( hFile, framebuf, 40 );
		fileClose( handle );
		delete framebuf ;

		for( ; i<PlayList.GetSize(); i++ ) {
			if (g_CancelCopy) {
				break;
			}
			vfi=(video_file_item *)PlayList[i] ;
			if( vfi->filetime >= EndTime ) break;

			if( vfi->filelength == 0 ) {
				continue ;
			}

			handle = fileOpen( vfi->filepath, GENERIC_READ );
			if( handle == NULL )
				continue ;

			fileSetKey( handle,  g_hVideoKey );
			pos = 40; 
			fileSeek(handle, pos, FILE_BEGIN);
			readbytes=fileRead( handle, &mainframe, sizeof(mainframe));
			starttime = mainframe.timestamp ;

			// walk through frames
			while( readbytes==sizeof(mainframe) && mainframe.flag==1 ) {
				int subframes = mainframe.frames ;
				int skip ;
				if (g_CancelCopy) {
					break;
				}
				CTime frametime = vfi->filetime +(mainframe.timestamp - starttime )/64 ;
				if( frametime>=EndTime )
					break;
				if( frametime<BeginTime ) {
					pos = fileSeek(handle, mainframe.framesize, FILE_CURRENT);
					skip = 1 ;
				}
				else {
					w+=fileWrite( hFile, &mainframe, sizeof(mainframe));
					framebuf = new char [mainframe.framesize] ;
					fileRead( handle, framebuf, mainframe.framesize );
					w+=fileWrite( hFile, framebuf, mainframe.framesize );
					delete framebuf ;
					skip = 0 ;
				}

				while( --subframes>0 ) {
					fileRead(handle, &subframe, sizeof(subframe));
					if( subframe.flag != 0x1001 && subframe.flag != 0x1005 ) {
						break;
					}
					if( skip ) {
						pos = fileSeek(handle, subframe.framesize , FILE_CURRENT );
					}
					else {
						w+=fileWrite( hFile, &subframe, sizeof(subframe));
						framebuf = new char [subframe.framesize] ;
						fileRead( handle, framebuf, subframe.framesize);
						w+=fileWrite( hFile, framebuf, subframe.framesize);
						delete framebuf ;
					}
				}
				if( w>=1000000 ) {
					g_CurrentCopied += w/1000000 ;			// mega bytes for g_CurrentCopied 
					w = w%1000000 ;
				}
				readbytes = fileRead(handle, &mainframe, sizeof(mainframe));
			}
			fileClose( handle );
		}
		if( w > 500000 )
			g_CurrentCopied++ ;
		return 0 ;
	}
}
