#include "stdafx.h"

#include <gdiplus.h>
#include <GdiplusGraphics.h>
using namespace Gdiplus;

#include <winsock2.h>
#include <ws2tcpip.h>

#include "PLY301.h"
#include "player.h"

char Head264[40] = {
  0x34, 0x48, 0x4b, 0x48, 0xfe, 0xb3, 0xd0, 0xd6, 
  0x08, 0x03, 0x04, 0x20, 0x00, 0x00, 0x00, 0x00,
  0x03, 0x10, 0x02, 0x10, 0x01, 0x10, 0x10, 0x00,
  0x80, 0x3e, 0x00, 0x00, 0xc0, 0x02, 0xe0, 0x01,
  0x11, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 
} ;

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

static HikPlayer *	g_phikplyr[PLAYM4_MAX_SUPPORTS] ;

static void CALLBACK DisplayCBFunc(long nPort,char * pBuf,long nSize,long nWidth,long nHeight,long nStamp,long nType,long nReserved)
{
	if( g_phikplyr[nPort] != NULL )
		g_phikplyr[nPort]->displaycallback( pBuf, nSize, nWidth, nHeight, nStamp, nType, nReserved);
}

static void CALLBACK DrawFun(long nPort, HDC hDc, LONG nUser)
{
	if (g_phikplyr[nPort] != NULL) {
		g_phikplyr[nPort]->drawOffscreen(hDc);
	}
}

// constructor
HikPlayer::HikPlayer( HWND hplaywindow, int streammode ) 
: decoder(hplaywindow) 
, m_playstat(HIKPLAY_STAT_STOP)
, m_maxframesize(SOURCE_BUF_MIN)
{
    // init cap buffer
	m_save_CapBuf = NULL;
	m_CapBuf = NULL ;
    m_CapSize = 0 ;
    m_CapWidth = 0 ;
    m_CapHeight = 0 ;
    m_CapType = 0;
	m_CapClean = 0;

	m_ba_number = 0;   // no blur

    m_decframes = 0 ;

	// default draw color
	m_drawcolor = RGB(192, 60, 60);
	m_lines_number = 0;
	m_lines=NULL;

	if (PlayM4_GetPort(&m_port)) {
		if (PlayM4_OpenStream(m_port, (PBYTE)Head264, 40, SOURCEBUFSIZE) != 0) {
			if (streammode) {
				m_hikstreammode = STREAME_REALTIME;				// STREAME_REALTIME / STREAME_FILE
			}
			else {
				m_hikstreammode = STREAME_REALTIME;				// STREAME_REALTIME / STREAME_FILE
			}
			g_phikplyr[m_port] = this;

			PlayM4_SetDisplayType(m_port, DISPLAY_NORMAL);
			PlayM4_SetPicQuality(m_port, TRUE);
			PlayM4_SetStreamOpenMode(m_port, m_hikstreammode);
			PlayM4_SetDisplayCallBack(m_port, DisplayCBFunc);
			PlayM4_RigisterDrawFun(m_port, DrawFun, (LONG)this );

			return;
		}
	}
       
	m_port = -1 ;												// failed to open decoder
}

HikPlayer::~HikPlayer() 
{
	if( m_port>=0 && m_port<PLAYM4_MAX_SUPPORTS ){
		stop();
		g_phikplyr[m_port]=NULL ;
		PlayM4_SetDisplayCallBack( m_port , NULL );
		PlayM4_CloseStream(m_port);
		PlayM4_FreePort(m_port);
		m_port = -1;
	}
	if (m_lines != NULL) delete[] m_lines;
}

// input data into player (decoder)
// return 1: success
//		  0: should retry, or failed
int HikPlayer::inputdata( char * framebuf, int framesize, int frametype )	
{
//		if( frametype==FRAME_TYPE_AUDIO && m_audioon==FALSE )		// don't decode audio frame
//			return 0 ;

	int remain ;

	if( frametype==FRAME_TYPE_264FILEHEADER) {
		return 1 ;
	}

	if( framesize>m_maxframesize ){
		m_maxframesize=framesize ;
	}
	remain = getremaindatasize() ;
	if( m_playstat < HIKPLAY_STAT_PLAY ) {
//		if(remain>m_maxframesize) {
		if(remain>0) {
			return 0 ;								// should retry
		}
	}
	if( SOURCEBUFSIZE-remain < framesize ) {
			return 0 ;
	}
	PlayM4_InputData( m_port, (PBYTE)framebuf, (DWORD)framesize);
	return 1;
}

// stop playing and show background screen (logo)
int HikPlayer::stop()
{
	if( m_playstat!=HIKPLAY_STAT_STOP ) {						// already playing?
		PlayM4_Stop( m_port );
		m_playstat=HIKPLAY_STAT_STOP ;
		// refresh screen
		InvalidateRect(m_playerwindow, NULL, FALSE);
	}
	return 0;
}

// play mode
int	HikPlayer::play() 
{
    //	if( m_playstat!=HIKPLAY_STAT_PLAY ) {						// already playing?
		if( m_playstat==HIKPLAY_STAT_PAUSE) {
			PlayM4_Pause(m_port, FALSE);
		}
		if( m_hikstreammode != STREAME_REALTIME ) {				// switch to realtime mode
			PlayM4_Stop( m_port );
			m_hikstreammode = STREAME_REALTIME ;
			PlayM4_SetStreamOpenMode(m_port,m_hikstreammode);
		}
		PlayM4_ThrowBFrameNum(m_port, 0);
		PlayM4_Play( m_port, m_playerwindow );
		m_playstat=HIKPLAY_STAT_PLAY ;
		if( m_audioon ) 
			PlayM4_PlaySoundShare(m_port);
		else
			PlayM4_StopSoundShare(m_port);
//	}
	return 0;
}
													
// pause mode
int HikPlayer::pause()
{
	if( m_playstat!=HIKPLAY_STAT_PAUSE ) {					// already paused?
		if( m_hikstreammode != STREAME_FILE ) {
			PlayM4_Stop( m_port );
			m_hikstreammode = STREAME_FILE ;
			PlayM4_SetStreamOpenMode(m_port,m_hikstreammode);
			PlayM4_Play( m_port, m_playerwindow );
			PlayM4_ThrowBFrameNum(m_port, 0);
			PlayM4_Pause(m_port, TRUE);
			PlayM4_OneByOne( m_port);
		}
		else {
			PlayM4_Pause(m_port, TRUE);
		}
		m_playstat = HIKPLAY_STAT_PAUSE ;
    }
	return 0;
}

// slow play mode
int HikPlayer::slow(int speed)
{
	if( m_playstat != HIKPLAY_STAT_SLOW ) {
		m_playstat=HIKPLAY_STAT_SLOW;
		if( m_hikstreammode != STREAME_FILE ) {
			PlayM4_Stop( m_port );
			m_hikstreammode = STREAME_FILE ;
			PlayM4_SetStreamOpenMode(m_port,m_hikstreammode);
			PlayM4_Play( m_port, m_playerwindow );
			PlayM4_ThrowBFrameNum(m_port, 0);
		}
	}
	return PlayM4_Slow( m_port);
}

// fast play mode
int HikPlayer::fast(int speed)  
{
	if( m_playstat != HIKPLAY_STAT_FAST ) {
		m_playstat=HIKPLAY_STAT_FAST;
		if( m_hikstreammode != STREAME_FILE ) {
			PlayM4_Stop( m_port );
			m_hikstreammode = STREAME_FILE ;
			PlayM4_SetStreamOpenMode(m_port,m_hikstreammode);
			PlayM4_Play( m_port, m_playerwindow );
		}
	}
	if( speed >=4 )
		PlayM4_ThrowBFrameNum(m_port, 2);
	else if( speed >=3 ) 
		PlayM4_ThrowBFrameNum(m_port, 1);
	else
		PlayM4_ThrowBFrameNum(m_port, 0);

	return PlayM4_Fast( m_port);
}

int HikPlayer::oneframeforward()                                          // play one frame forward
{
	if( m_playstat!=HIKPLAY_STAT_PAUSE ) 
		return -1 ;												// one frame forward on paused state only
	if( m_hikstreammode != STREAME_FILE ) {
		PlayM4_Stop( m_port );
		m_hikstreammode = STREAME_FILE ;
		PlayM4_SetStreamOpenMode(m_port,m_hikstreammode);
	    PlayM4_Play( m_port, m_playerwindow );
		PlayM4_ThrowBFrameNum(m_port, 0);
		PlayM4_Pause( m_port, TRUE );
	}
    return PlayM4_OneByOne( m_port);
}

// suspend diaplay and show background screen (logo)
int HikPlayer::suspend()							
{
	if( m_playstat!=HIKPLAY_STAT_SUSPEND ) {
		m_playstat=HIKPLAY_STAT_SUSPEND ;
		// show background screen
		showbackground();
	}
	return 0;
}


// set audio on/off state
int HikPlayer::audioon( int audioon ) 
{
	int oldaudio=m_audioon ;
	m_audioon=audioon ;
	if( oldaudio!=audioon ) {
		if( audioon ) {
			PlayM4_PlaySoundShare(m_port);
		}
		else {
			PlayM4_StopSoundShare(m_port);
		}
	}
	return oldaudio ;
}

void HikPlayer::refresh()
{
	if( m_port >=0 )
		PlayM4_RefreshPlay(m_port);
}

void HikPlayer::showbackground()
{
	if( m_playerwindow )
		InvalidateRect(m_playerwindow, NULL, FALSE);
}

int  HikPlayer::refreshdecoder()
{
	if (!m_CapClean) {
		restoreCap();
	}
	
	if (m_playstat == HIKPLAY_STAT_PAUSE &&
		m_CapBuf && m_CapSize>0)
	{
		if (m_ba_number > 0) {
			blurYV12Buf();
		}
	}

	refresh();
    return (m_CapBuf!=NULL) ;
}

int  HikPlayer::setblurarea(struct blur_area * ba, int banumber )
{
	m_ba_number = banumber;
	if (m_ba_number<0 || ba == NULL) {
		m_ba_number = 0;
	}
	else if (m_ba_number>MAX_BLUR_AREA) {
		m_ba_number = MAX_BLUR_AREA;
	}

	if(m_ba_number>0)
		memcpy( m_blur_area, ba, m_ba_number *sizeof(struct blur_area));
	if (m_playstat == HIKPLAY_STAT_PAUSE)
		refreshdecoder();
    return 0;
}

int  HikPlayer::clearblurarea( )
{
    m_ba_number = 0 ;
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
	if (m_CapType != T_YV12)
		return;

	// save original Cap buffer
	saveCap();

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
            //blur( (unsigned char *)m_CapBuf, m_CapWidth, bx, by, bw, bh, radius/2, m_blur_area[i].shape );
            blur( (unsigned char *)m_CapBuf, m_CapWidth, bx, by, bw, bh, radius, m_blur_area[i].shape );
            blur( (unsigned char *)m_CapBuf+m_CapWidth*m_CapHeight, m_CapWidth/2, bx/2, by/2, bw/2, bh/2, radius/2, m_blur_area[i].shape );
            blur( (unsigned char *)m_CapBuf+m_CapWidth*m_CapHeight+m_CapWidth*m_CapHeight/4, m_CapWidth/2, bx/2, by/2, bw/2, bh/2, radius/2, m_blur_area[i].shape );
			m_CapClean = 0;
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

int HikPlayer::showzoomin(struct zoom_area * za)
{
	if (za && za->z < 1.0) {
		// zoom in
		//zooming();
		RECT r;
		r.left = (int)( za->x * (float)m_CapWidth );
		r.right = r.left + (int)(za->z * (float)m_CapWidth);
		r.top = (int)(za->y * (float)m_CapHeight) ;
		r.bottom = r.top + (int)(za->z * (float)m_CapHeight);
		PlayM4_SetDisplayRegion(m_port, 0, &r, m_playerwindow, 1);
	}
	else {
		PlayM4_SetDisplayRegion(m_port, 0, NULL, m_playerwindow, 1);
	}

	return 0;
}

void HikPlayer::drawOffscreen(HDC hDc)
{
	using namespace Gdiplus;

	int lines = m_lines_number;
	if (lines>0 ) {
		RECT r;
		GetClientRect(m_playerwindow, &r);

		Gdiplus::Graphics g(hDc);
		ARGB xcolor = m_drawcolor ;
		Gdiplus::Pen * p = new Gdiplus::Pen(Color(xcolor ^ 0xff000000), 1.6);

		g.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeHighQuality);

		int l = 0;
		float x1, y1;
		float x2 = -1, y2=-1;
		while (l < lines) {
			if (m_lines[l].color != xcolor) {
				xcolor = m_lines[l].color;
				p->SetColor(Color(xcolor ^ 0xff000000));
			}
			g.DrawLine( p, m_lines[l].x1 * r.right, m_lines[l].y1 * r.bottom, m_lines[l].x2 * r.right, m_lines[l].y2 * r.bottom );
			l++;
		}

		delete p;
	}
}

int HikPlayer::drawline(float x1, float y1, float x2, float y2)
{
	if (m_lines == NULL) {
		m_lines = new draw_line[MAX_DRAW_LINES];
		m_lines_number = 0;
	}

	if (m_lines_number < MAX_DRAW_LINES) {
		m_lines[m_lines_number].color = m_drawcolor;
		m_lines[m_lines_number].x1 = x1;
		m_lines[m_lines_number].y1 = y1;
		m_lines[m_lines_number].x2 = x2;
		m_lines[m_lines_number].y2 = y2;
		m_lines_number++;
	}
	return 0;
}

int HikPlayer::clearlines()
{
	m_lines_number = 0;
	return 0;
}

int HikPlayer::setdrawcolor(COLORREF color)
{
	m_drawcolor = color;
	return 0;
}

void HikPlayer::displaycallback(char * pBuf,long nSize,long nWidth,long nHeight,long nStamp,long nType,long nReserved)
{
//	ENTER_CRITICAL ;
	lock();
	m_save_CapBuf = m_CapBuf;
	m_CapBuf = pBuf ;
	m_CapSize = nSize ;
	m_CapWidth = nWidth ;
	m_CapHeight = nHeight ;
	m_CapStamp = nStamp;
	m_CapType = nType;
	m_CapClean = 1;
	unlock();

	if (m_ba_number > 0 ) {
		blurYV12Buf();
	}

	m_decframes++ ;
//	LEAVE_CRITICAL ;
}

// capture current screen into a bmp file
int HikPlayer::capturesnapshot(char * bmpfilename)
{
	BOOL result=FALSE ;
	if( m_port<0 || m_port>PLAYM4_MAX_SUPPORTS || m_playstat==HIKPLAY_STAT_STOP ) 
		return FALSE ;

	if( !IsBadReadPtr( m_CapBuf, m_CapSize ) ) {
//		ENTER_CRITICAL ;
		char * dupBuf = new char[m_CapSize];
		lock();
		memcpy(dupBuf, m_CapBuf, m_CapSize);
		unlock();
//		LEAVE_CRITICAL ;
		result=PlayM4_ConvertToBmpFile(dupBuf, m_CapSize, m_CapWidth, m_CapHeight, m_CapType, bmpfilename );
		delete dupBuf;
	}
	return result;
}
