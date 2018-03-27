#include "stdafx.h"
#include <winsock2.h>
#include <crtdbg.h>
#include "ply606.h"
#include "player.h"
#include "vout_vector.h"
#include "video_output.h"

#define PAUSE_TIME 100

// constructor
MpegPlayer::MpegPlayer( HWND hplaywindow, int channelID ) 
: decoder(hplaywindow) 
, m_playstat(PLAY_STAT_STOP)
, m_channelID(channelID)
, m_maxframesize(SOURCE_BUF_MIN)
, m_pendingShowCmd(-1)
{
	InitializeCriticalSection(&m_cs);
	m_pInput = new CInputStream(channelID);
	m_pInput->Open(m_playerwindow);
}

MpegPlayer::~MpegPlayer() 
{
	stop();

	struct vout_set *pvs, vs;
	pvs = getVoutSetForWindowHandle(m_playerwindow, &vs);
	if (pvs && vs.vout) {
		CVideoOutput *pVout = vs.vout;
		if (pVout) {
			pVout->m_bDie = true;
			_RPT0(_CRT_WARN, "joining vout thread\n");
			tme_thread_join(pVout->m_hThread);
			delete pVout;
		}
		removeVoutForWindowHandle(m_playerwindow);
	}

	DeleteCriticalSection(&m_cs);
}

// input data into player (decoder)
// return 1: success
//		  0: should retry, or failed
int MpegPlayer::inputdata( block_t *pBlock )	
{
	if(m_pInput->InputData(pBlock)) {
		//_RPT0(_CRT_WARN, "Buffer full=========\n");
		return 0;
	}

	return 1;
}

int MpegPlayer::resetbuffer()	
{
	m_pInput->ResetBuffer();
	return 0;
}
// stop playing and show background screen (logo)
int MpegPlayer::stop()
{
	if (m_pInput) {
		m_pInput->DestroyThread();
		delete m_pInput;
		m_pInput = NULL;
	}
		
	m_playstat=PLAY_STAT_STOP ;

	// refresh screen
	InvalidateRect(m_playerwindow, NULL, FALSE);

	return 0;
}

// play mode
int	MpegPlayer::play() 
{
		EnterCriticalSection(&m_cs); 
		m_pInput->Play();

		m_playstat=PLAY_STAT_PLAY ;
		m_pInput->AudioOn(m_audioon ? true : false);
		LeaveCriticalSection(&m_cs); 
	return 0;
}
													
// pause mode
int MpegPlayer::pause()
{
	if( m_playstat!=PLAY_STAT_PAUSE ) {					// already paused?
		EnterCriticalSection(&m_cs); 
		m_playstat = PLAY_STAT_PAUSE ;
		LeaveCriticalSection(&m_cs); 
		m_pInput->Pause();
	}
	return 0;
}

// slow play mode
int MpegPlayer::slow(int speed)
{
	EnterCriticalSection(&m_cs); 
	if( m_playstat != PLAY_STAT_SLOW ) {
		m_playstat=PLAY_STAT_SLOW;
	}
	m_pInput->PlayForwardSpeed(-speed);
	LeaveCriticalSection(&m_cs); 
	return 1;
}

// fast play mode
int MpegPlayer::fast(int speed)  
{
	EnterCriticalSection(&m_cs); 
	if( m_playstat != PLAY_STAT_FAST ) {
		m_playstat=PLAY_STAT_FAST;
	}
	m_pInput->PlayForwardSpeed(speed);
	LeaveCriticalSection(&m_cs); 
	return 1;
}

int MpegPlayer::oneframeforward()                                          // play one frame forward
{
	if( m_playstat!=PLAY_STAT_PAUSE ) 
		return -1 ;												// one frame forward on paused state only

	EnterCriticalSection(&m_cs); 
	m_pInput->PlayStep();
	LeaveCriticalSection(&m_cs); 
	return 1;
}

// suspend diaplay and show background screen (logo)
int MpegPlayer::suspend()							
{
	if( m_playstat!=PLAY_STAT_SUSPEND ) {
		m_playstat=PLAY_STAT_SUSPEND ;
		// refresh screen.
		InvalidateRect(m_playerwindow, NULL, FALSE);
	}
	return 0;
}

void MpegPlayer::refresh(int nCmdShow)
{
	struct vout_set *pvs, vs;
	pvs = getVoutSetForWindowHandle(m_playerwindow, &vs);
	if (pvs && vs.vout) {
		CVideoOutput *pVout = vs.vout;
		pVout->ShowVideoWindow(nCmdShow); // hide our window (different from hWnd)
		m_pendingShowCmd = -1;
	} else {
		m_pendingShowCmd = nCmdShow;
	}
	//InvalidateRect(m_playerwindow, NULL, FALSE);
}

// set audio on/off state
int MpegPlayer::audioon( int audioon ) 
{
	int oldaudio=m_audioon ;
	m_audioon=audioon ;
	if( oldaudio!=audioon ) {
		EnterCriticalSection(&m_cs); 
		m_pInput->AudioOn(audioon ? true : false);
		LeaveCriticalSection(&m_cs); 
	}
	return oldaudio ;
}

// capture current screen into a bmp file
int MpegPlayer::capturesnapshot(char * bmpfilename)
{
	BOOL result=FALSE ;
	if( m_playstat==PLAY_STAT_STOP ) return FALSE ;

	EnterCriticalSection(&m_cs); 
	result = m_pInput->SnapShot(bmpfilename);
	LeaveCriticalSection(&m_cs); 
	
	return result;
}

int MpegPlayer::getBufferDelay()
{
	if (m_pInput) {
		return m_pInput->GetBufferDelay();
	}

	return 0;
}

void MpegPlayer::checkPendingShowCmd()
{
	if (m_pendingShowCmd != -1) {
		refresh(m_pendingShowCmd);
	}
}

// BLUR features

int  MpegPlayer::setblurarea(struct blur_area * ba, int banumber )
{
    struct vout_set *pvs, vs;
    pvs = getVoutSetForWindowHandle(m_playerwindow, &vs);
    if (pvs && vs.vout) {
        CVideoOutput *pVout = vs.vout;
        pVout->m_ba_number = 0 ;
        if( banumber<=0 || 
            banumber>MAX_BLUR_AREA ||
            ba==NULL ) 
        {
                return 0;
        }
        memcpy( pVout->m_blur_area, ba, banumber*sizeof(struct blur_area));
        pVout->m_ba_number = banumber ;
        pVout->i_idle_loops = 10 ;      // to quickly refresh previous picture
    }
    return 0;
}

int  MpegPlayer::clearblurarea( )
{
    struct vout_set *pvs, vs;
    pvs = getVoutSetForWindowHandle(m_playerwindow, &vs);
    if (pvs && vs.vout) {
        CVideoOutput *pVout = vs.vout;
        pVout->m_ba_number = 0 ;
        //        refreshdecoder();
        pVout->i_idle_loops = 10 ;      // to quickly refresh previous picture
    }
    return 0 ;
}

// blur process requested from .AVI encoder
void MpegPlayer::BlurDecFrame( picture_t * pic )
{
    struct vout_set *pvs, vs;
    pvs = getVoutSetForWindowHandle(m_playerwindow, &vs);
    if (pvs && vs.vout) {
        CVideoOutput *pVout = vs.vout;
        if( pVout->m_ba_number > 0 ) {
            pVout->blurPicture(pic);
        }
    }
}
