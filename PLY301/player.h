
#ifndef __PLAYER_H__
#define __PLAYER_H__

#include <mmsystem.h>

// #define HIKAPI

// redefine HIK API
#ifdef	HIKAPI
#include "HikPlayMpeg4.h"
#define PlayM4_SetDisplayCallBack	Hik_PlayM4_SetDisplayCallBack
#define	PlayM4_SetStreamOpenMode	Hik_PlayM4_SetStreamOpenMode
#define	PlayM4_OpenStream			Hik_PlayM4_OpenStream
#define	PlayM4_GetSourceBufferRemain	Hik_PlayM4_GetSourceBufferRemain
#define	PlayM4_ResetSourceBuffer	Hik_PlayM4_ResetSourceBuffer
#define	PlayM4_SetSourceBufCallBack	Hik_PlayM4_SetSourceBufCallBack
#define PlayM4_ResetSourceBufFlag	Hik_PlayM4_ResetSourceBufFlag
#define PlayM4_SetDisplayBuf		Hik_PlayM4_SetDisplayBuf
#define	PlayM4_CloseStream			Hik_PlayM4_CloseStream
#define	PlayM4_InputData			Hik_PlayM4_InputData
#define	PlayM4_Stop					Hik_PlayM4_Stop
#define	PlayM4_Stop					Hik_PlayM4_Stop
#define	PlayM4_PlaySoundShare		Hik_PlayM4_PlaySoundShare
#define	PlayM4_Play					Hik_PlayM4_Play
#define	PlayM4_ThrowBFrameNum		Hik_PlayM4_ThrowBFrameNum
#define	PlayM4_Pause				Hik_PlayM4_Pause
#define	PlayM4_OneByOne				Hik_PlayM4_OneByOne
#define	PlayM4_Slow					Hik_PlayM4_Slow
#define	PlayM4_Fast					Hik_PlayM4_Fast
#define	PlayM4_ConvertToBmpFile		Hik_PLayM4_ConvertToBmpFile
#define	PlayM4_GetVolume			Hik_PlayM4_GetVolume
#define PlayM4_SetVolume			Hik_PlayM4_SetVolume
#define	PlayM4_PlaySoundShare		Hik_PlayM4_PlaySoundShare
#define PlayM4_StopSoundShare		Hik_PlayM4_StopSoundShare
#define PlayM4_SetPicQuality		Hik_PlayM4_SetPicQuality
#define PlayM4_SetDisplayType		Hik_PlayM4_SetDisplayType
#define PlayM4_RefreshPlay      Hik_PlayM4_RefreshPlay

#define	PLAYM4_MAX_SUPPORTS			HIK_PLAYM4_MAX_SUPPORTS
#else
#include "PlayM4.h"
#endif

#include "PLY301.h"

// dvr frame type
#define FRAME_TYPE_UNKNOWN		(0)
#define FRAME_TYPE_KEYVIDEO		(1)
#define FRAME_TYPE_VIDEO		(2)
#define FRAME_TYPE_AUDIO		(3)
#define FRAME_TYPE_264FILEHEADER		(10)

// DVR decoder interface
class decoder {
public:
	decoder * m_next ;				// next decoder
	int		m_decframes ;			// number of frames decoded ;
protected:
	HWND m_playerwindow ;			// display on this window 
	int  m_audioon ;				// if audio is on

	// thread safe
	CRITICAL_SECTION m_cs;
	inline void lock() {
		EnterCriticalSection(&m_cs);
	}
	inline void unlock() {
		LeaveCriticalSection(&m_cs);
	}

public:
// constructor
	decoder( HWND hplaywindow ) {
		InitializeCriticalSectionAndSpinCount(&m_cs, 1024);
		m_next = NULL ;
		m_playerwindow = hplaywindow ;
		m_audioon=0;
		m_decframes=0;
	}
	virtual ~decoder(){
		// Release resources used by the critical section object.
		DeleteCriticalSection(&m_cs);
	};

// interfaces
	HWND		window() { return m_playerwindow; }
	virtual int inputdata( char * framebuf, int framesize, int frametype )=0 ;	// input data into player (decoder), return 1: success
	virtual int getremaindatasize(){											// get remaining data(internal buffer) haven't been decoded
		return 0;
	}
	virtual int resetbuffer(){													// clear remaining data
		return 0;
	}
	virtual int getres(int * xres, int * yres )=0;								// get play current resolution
	virtual int getdecframes() { return m_decframes ; }							// get frames been decoded
	virtual int stop()=0;			    										// stop playing and show background screen (logo)
	virtual int	play()=0;   													// play mode
	virtual int pause()=0;														// pause mode
	virtual int slow(int speed)=0;                                              // slow play mode
	virtual int fast(int speed)=0;                                              // fast play mode
    virtual int oneframeforward()=0;                                            // play one frame forward
	virtual int suspend()=0;													// suspend diaplay and show background screen (logo)
	virtual int audioon( int audioon ) {										// set audio on/off state
		int oldaudio=m_audioon ;
		m_audioon=audioon ;
		return oldaudio ;
	}
	virtual int capturesnapshot(char * bmpfilename)=0;							// capture current screen into a bmp file
	virtual void  refresh()=0;		                                            // refresh screen, show logo screen
    virtual int refreshdecoder()=0 ;                                           // refresh screen by decoder
    virtual int  setblurarea(struct blur_area * ba, int banumber )=0;
    virtual int  clearblurarea( )=0;

    // blur process requested from .AVI encoder
    virtual void BlurDecFrame( char * pBuf, int nSize, int nWidth, int nHeight, int nType)=0;

	// display zoom in area
	virtual int showzoomin(struct zoom_area * za) = 0;

	// ROC/AOI support
	virtual int drawline(float x1, float y1, float x2, float y2 ) = 0;
	virtual int clearlines() = 0;
	virtual int setdrawcolor(COLORREF color) = 0;
};

#define SOURCEBUFSIZE	(SOURCE_BUF_MIN*8)
#define SOURCEBUFTHRESHOLD (SOURCEBUFSIZE/2)

// .264 file struct
struct hd_file {
	DWORD	flag ;		//	"4HKH", 0x484b4834
	DWORD	res1[6] ;
	WORD	width;
	WORD	height;
	DWORD	res2[2];
} ;

// a common .264 file header
extern char Head264[40] ;

struct hd_frame {
	DWORD	flag ;		// 1
	DWORD   serial ;
	DWORD   timestamp ;
	DWORD	res1 ;
	BYTE    frames ;	// sub frames number
	BYTE    res2[3] ;
	WORD    width ;
	WORD    height ;
	DWORD   res3[6] ;
	BYTE	type ;		// 3  keyframe, 1, audio, 4: B frame
	BYTE	res4[3] ;
	DWORD	res5[3] ;
	DWORD   framesize ;
};

struct hd_subframe {
	WORD    flag ;      // audio: 0x1001  B frame: 0x1005
	WORD    res1[7] ;
	DWORD   framesize ;		// 0x50 for audio
};

#define HIKPLAY_STAT_STOP		(0)
#define HIKPLAY_STAT_PAUSE		(1)
#define HIKPLAY_STAT_SUSPEND	(2)
#define HIKPLAY_STAT_SLOW		(99)
#define HIKPLAY_STAT_PLAY		(100)
#define HIKPLAY_STAT_FAST		(101)

#define MAX_BLUR_AREA   (10)

#define MAX_DRAW_LINES	(10000)

struct draw_line {
	float x1, y1, x2, y2;
	COLORREF color;
};

// Hikvision player, implement a decoer interface
class HikPlayer : public decoder
{
private:
	LONG		m_port ;				// hik player port (Decoder handle). Valid from 0 to 64
	int			m_playstat ;			// playing state, play/pause/fast/slow
	DWORD		m_hikstreammode ;
	int			m_maxframesize ;		// maximum frame size

	// capture support member
	char * m_CapBuf ;
	long   m_CapSize ;
	long   m_CapWidth ;
	long   m_CapHeight ;
	long   m_CapStamp ;
	long   m_CapType ;
	int    m_CapClean;		// if Cap buffer is clean 

	// saved display frame
	char * m_save_CapBuf;
	inline void saveCap() {
		if (m_CapClean && m_save_CapBuf && m_CapBuf) {
			memcpy(m_save_CapBuf, m_CapBuf, m_CapSize);
		}
	}
	inline void restoreCap() {
		if (m_save_CapBuf && m_CapBuf && m_CapClean==0) {
			memcpy(m_CapBuf, m_save_CapBuf, m_CapSize);
			m_CapClean = 1;
		}
	}

    // blur area support
    struct blur_area m_blur_area[MAX_BLUR_AREA] ;
    int    m_ba_number ;
    void   blurYV12Buf();

	// drawing lines
	COLORREF	m_drawcolor;
	int			m_lines_number;
	draw_line * m_lines;

public:
	HikPlayer( HWND hplaywindow, int streammode=0 );
	virtual ~HikPlayer();

// interfaces
	int inputdata( char * framebuf, int framesize, int frametype );	// input data into player (decoder)
	int getremaindatasize(){										// get remaining data(internal buffer) haven't been decoded
		return PlayM4_GetSourceBufferRemain(m_port);
	}
	int resetbuffer(){
		return PlayM4_ResetSourceBuffer(m_port);
	}
	virtual int getres(int * xres, int * yres )					// get play current resolution
	{
		*xres = m_CapWidth;
		*yres = m_CapHeight;
		return 0 ;
	}
	int stop();														// stop playing and show background screen (logo)
	int	play();   													// play mode
	int pause();													// pause mode
	int slow(int speed);                                            // slow play mode
	int fast(int speed);                                            // fast play mode
    int oneframeforward();                                          // play one frame forward
	int suspend();													// suspend diaplay and show background screen (logo)
	int audioon( int audioon );										// set audio on/off state
	int capturesnapshot(char * bmpfilename);						// capture current screen into a bmp file

// other function
	void displaycallback(char * pBuf,long nSize,long nWidth,long nHeight,long nStamp,long nType,long nReserved);	//display call back
	void drawOffscreen(HDC hDc);
	BOOL  isPause()					{     return ( m_playstat == HIKPLAY_STAT_PAUSE ); } 
	WORD  GetVolume()			   {	 return PlayM4_GetVolume(m_port); }
	BOOL  SetVolume(WORD nVolume = 0xffff){ return PlayM4_SetVolume(m_port,nVolume); }
	void  showbackground();
	void  refresh();		// refresh screen, show logo screen
    int   refreshdecoder() ;                                           // refresh screen by decoder
    int   setblurarea(struct blur_area * ba, int banumber );
    int   clearblurarea( );

    // blur process requested from .AVI encoder
    virtual void BlurDecFrame( char * pBuf, int nSize, int nWidth, int nHeight, int nType);

	// display zoom in area
	virtual int showzoomin(struct zoom_area * za);

	// Drawing support, for Zoom / ROC/AOI 
	virtual int drawline(float x1, float y1, float x2, float y2);
	virtual int clearlines();
	virtual int setdrawcolor(COLORREF color) ;

};

#endif 