
#ifndef __PLAYER_H__
#define __PLAYER_H__

#include <mmsystem.h>

#include "tme_thread.h"
//#define AUDIO_TEST
//#define HIKAPI

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

#define	PLAYM4_MAX_SUPPORTS			HIK_PLAYM4_MAX_SUPPORTS
#else
#include "PlayM4.h"
#endif

//#define USE_REALMODE

// dvr frame type
#define FRAME_TYPE_UNKNOWN		(0)
#define FRAME_TYPE_KEYVIDEO		(1)
#define FRAME_TYPE_VIDEO		(2)
#define FRAME_TYPE_AUDIO		(3)

// player thread status
#define PLY_THREAD_NONE			(0)			// thread not started or ended
#define PLY_THREAD_RUN			(1)			// thread is running
#define PLY_THREAD_QUIT			(2)			// thread is required to quit

// player play state
#define PLAY_STAT_STOP		(0)
#define PLAY_STAT_PAUSE		(1)
#define PLAY_STAT_SUSPEND	(2)
#define PLAY_STAT_SLOWEST	(96)
#define PLAY_STAT_SLOW		(99)
#define PLAY_STAT_PLAY		(100)
#define PLAY_STAT_FAST		(101)
#define PLAY_STAT_FASTEST	(104)

// DVR decoder interface
class decoder {
public:
	decoder * m_next ;				// next decoder

protected:
	HWND m_playerwindow ;			// display on this window 
	int  m_audioon ;				// if audio is on
public:
// constructor
	decoder( HWND hplaywindow ) {
		m_next = NULL ;
		m_playerwindow = hplaywindow ;
		m_audioon=0;
	}
	virtual ~decoder(){};

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
	virtual int capturesnapshot(char * bmpfilename)=0;									// capture current screen into a bmp file
	virtual void  refresh(int nCmd)=0;		// refresh screen, show logo screen

    virtual void  refreshdecoder()=0 ;                                           // refresh screen by decoder
    virtual int  setblurarea(struct blur_area * ba, int banumber )=0;
    virtual int  clearblurarea( )=0;
    // blur process requested from .AVI encoder
    virtual void BlurDecFrame( char * pBuf, int nSize, int nWidth, int nHeight, int nType)=0;
};

#define SOURCEBUFSIZE	(SOURCE_BUF_MIN*2)
#define SOURCEBUFTHRESHOLD (SOURCEBUFSIZE/2)

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

// a common .264 file header
//extern char Head264[40] ;

#define MAX_BLUR_AREA   (10)

// blur area definition
//   coordinates are base on 256x256 picture
struct blur_area {
    int x;
    int y;
    int w;
    int h;
    int radius;
    int shape;
};

// Hikvision player, implement a decoer interface
class HikPlayer : public decoder
{
private:
	LONG		m_port ;											// hik player port (Decoder handle). Valid from 0 to 64
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

    // blur area support
    struct blur_area m_blur_area[MAX_BLUR_AREA] ;
    int    m_ba_number ;
    char * m_blur_save_CapBuf  ;
    void   blurYV12Buf();

	//CRITICAL_SECTION m_cs ;	
	bool m_bAudioOnly;
	//int m_bufSize, m_bufSizeEstimate;
	int m_iWindowX, m_iWindowY;
	int m_iWindowWidth, m_iWindowHeight;
	TCHAR m_class_main[256], m_class_video[256];
  tme_mutex_t		m_lock;
	bool m_bEventDie, m_bDie;
	HANDLE m_hEventThread, m_hThread;
	bool m_bPlay, m_bReady;
	int m_ch, m_show;
	RECT         m_rectParent;
public:
	HWND m_hparent, m_hwnd, m_hvideownd;


public:
	HikPlayer( int channel, HWND hplaywindow, int streammode=0, bool audio_only = false );
	~HikPlayer();

#ifdef AUDIO_TEST
	FILE *m_fp;
#endif
// interfaces
	int inputdata( char * framebuf, int framesize, int frametype );	// input data into player (decoder)
	int getremaindatasize();
	int resetbuffer();
	int getres(int * xres, int * yres )								// get play current resolution
	{
		*xres = m_CapWidth ;
		*yres = m_CapHeight ;
		return 0;
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
	BOOL  isPause()					{     return ( m_playstat == PLAY_STAT_PAUSE ); } 
	WORD  GetVolume()			   {	 return PlayM4_GetVolume(m_port); }
	BOOL  SetVolume(WORD nVolume = 0xffff){ return PlayM4_SetVolume(m_port,nVolume); }
	void  refresh(int nCmdShow);		// refresh screen, show logo screen
	//void SourceBufCB(DWORD);
	int player_CreateWindow();
	int WindowEventThread();
	void CloseVideo();
	int PlayerThread();
	void WindowUpdateRects( bool b_force );
	int Manage();

// blur functions
    void  refreshdecoder() ;                                           // refresh screen by decoder
    int   setblurarea(struct blur_area * ba, int banumber );
    int   clearblurarea( );

    // blur process requested from .AVI encoder
    virtual void BlurDecFrame( char * pBuf, int nSize, int nWidth, int nHeight, int nType);
};

#endif 