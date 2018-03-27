
#ifndef __PLAYER_H__
#define __PLAYER_H__

#include "inputstream.h"

// player thread status
#define PLY_THREAD_NONE			(0)			// thread not started or ended
#define PLY_THREAD_RUN			(1)			// thread is running
#define PLY_THREAD_QUIT			(2)			// thread is required to quit

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
	virtual int inputdata( block_t *pBlock )=0 ;	// input data into player (decoder), return 1: success
	virtual int getremaindatasize(){											// get remaining data(internal buffer) haven't been decoded
		return 0;
	}
	virtual int resetbuffer(){													// clear remaining data
		return 0;
	}
	virtual int stop()=0;			    										// stop playing and show background screen (logo)
	virtual int	play()=0;   													// play mode
	virtual int pause()=0;														// pause mode
	virtual int slow(int speed)=0;                                              // slow play mode
	virtual int fast(int speed)=0;                                              // fast play mode
    virtual int oneframeforward()=0;                                            // play one frame forward
    //virtual int oneframeforward_delayed()=0;                                    // play one frame forward after seek
	virtual int suspend()=0;													// suspend diaplay and show background screen (logo)
	virtual int audioon( int audioon ) {										// set audio on/off state
		int oldaudio=m_audioon ;
		m_audioon=audioon ;
		return oldaudio ;
	}
	virtual int capturesnapshot(char * bmpfilename)=0;									// capture current screen into a bmp file
	virtual void  refresh(int nCmdShow = SW_HIDE)=0;		// refresh screen, show logo screen
	virtual int getBufferDelay() = 0;
	virtual void checkPendingShowCmd() = 0;

    virtual int  setblurarea(struct blur_area * ba, int banumber )=0;
    virtual int  clearblurarea( )=0;
    // blur process requested from .AVI encoder
    virtual void BlurDecFrame( picture_t * pic )=0;
};

#define SOURCE_BUF_MIN (50 * 1024)
#define SOURCEBUFSIZE	(SOURCE_BUF_MIN*8)
#define SOURCEBUFTHRESHOLD (SOURCEBUFSIZE/2)

// a common .264 file header
extern char Head264[40] ;
#define FOURCC_HKMI 0x46464952

// player play state
#define PLAY_STAT_STOP		(0)
#define PLAY_STAT_PAUSE		(1)
#define PLAY_STAT_SUSPEND	(2)
#define PLAY_STAT_ONEFRAME	(3)
#define PLAY_STAT_SLOWEST	(96)
#define PLAY_STAT_SLOW		(99)
#define PLAY_STAT_PLAY		(100)
#define PLAY_STAT_FAST		(101)
#define PLAY_STAT_FASTEST	(104)

// Hikvision player, implement a decoer interface
class MpegPlayer : public decoder
{
private:
	int			m_playstat ;			// playing state, play/pause/fast/slow
	int		m_channelID ;
	int			m_maxframesize ;		// maximum frame size

	CRITICAL_SECTION m_cs ;	
	//int m_bufSize, m_bufSizeEstimate;
	CInputStream *m_pInput;
	int m_pendingShowCmd;


public:
	MpegPlayer( HWND hplaywindow, int channelID=0 );
	~MpegPlayer();

// interfaces
	int inputdata( block_t *pBlock );	// input data into player (decoder)
	int getremaindatasize(){ // get remaining data(internal buffer) haven't been decoded
		return 0;
	}
	int resetbuffer();
	int stop();														// stop playing and show background screen (logo)
	int	play();   													// play mode
	int pause();													// pause mode
	int slow(int speed);                                            // slow play mode
	int fast(int speed);                                            // fast play mode
    int oneframeforward();                                          // play one frame forward
	int suspend();													// suspend diaplay and show background screen (logo)
	int audioon( int audioon );										// set audio on/off state
	int capturesnapshot(char * bmpfilename);						// capture current screen into a bmp file
	int getBufferDelay();

// other function
	void  refresh(int nCmdShow = SW_HIDE);		// refresh screen, show logo screen
	void checkPendingShowCmd();

    virtual int  setblurarea(struct blur_area * ba, int banumber );
    virtual int  clearblurarea( );
    // blur process requested from .AVI encoder
    virtual void BlurDecFrame( picture_t * pic );

};

#endif 