#include "stdafx.h"
#include <process.h>
#include "inputstream.h"
#include "ffmpeg.h"
#include "araw.h"
#include "subs.h"
#include "vout_vector.h"
#include "trace.h"
#include "ply606.h"

static void ClockInit( input_clock_t *cl, bool b_master, int i_cr_average );
static void ClockNewRef( input_clock_t *cl,
                         mtime_t i_clock, mtime_t i_sysdate );

unsigned WINAPI InputRunFunc(PVOID pvoid) {
	CInputStream *p = (CInputStream *)pvoid;
	p->InputRun();
	_endthreadex(0);
	return 0;
}

CInputStream::CInputStream(int channelID)
{
	//tme_thread_init(&m_ipc);
	m_hwnd = NULL;
	m_bEnd = false;
	m_bError = false;
	m_pFifo = block_FifoNew(&g_ipc);
	m_pAudioDecoder = m_pVideoDecoder = m_pTextDecoder = NULL;
	m_inputClock.b_added = false;
	m_iPtsDelay = 300000;
	m_iCrAverage = 40;
    m_iRate  = INPUT_RATE_DEFAULT;
	m_bCanPaceControl = true;
	m_bOutPaceControl = false;
    m_iState = PAUSE_S;
	m_bStepPlay = false;
	m_bFastPlay = false;
	m_bAudioOn = false;
	m_channelID = channelID;
	//m_bResetClock = false;
	m_lastDataTime = 0;
	m_lastBufferTime = 0;
	m_iBufferDelay = 0;
	m_bCreatingVideoOutput = false;

    m_iCrAverage *= (int)(10 * m_iPtsDelay / 200000);
    m_iCrAverage /= 10;
    if( m_iCrAverage < 10 ) m_iCrAverage = 10;

    tme_mutex_init( &g_ipc, &m_lockPts );
    tme_mutex_init( &g_ipc, &m_lockControl );
    tme_mutex_init( &g_ipc, &m_lock );
    tme_cond_init( &g_ipc, &m_wait );

	m_iControl = 0;
}

CInputStream::~CInputStream()
{
    /* Free all packets still in the fifo. */
    block_FifoEmpty( m_pFifo );
    block_FifoRelease( m_pFifo );

	tme_mutex_destroy(&m_lock);
	tme_cond_destroy(&m_wait);
    tme_mutex_destroy( &m_lockControl );
    tme_mutex_destroy( &m_lockPts );
	//tme_mutex_destroy(&(m_ipc.object_lock));
	//tme_cond_destroy(&(m_ipc.object_wait));
}

int CInputStream::Open(HWND hwnd)
{
	if (hwnd == NULL)
		return 1;

	m_hwnd = hwnd;
	m_hThread = tme_thread_create( &g_ipc, "Input RunThread",
									InputRunFunc, this, 
									TME_THREAD_PRIORITY_INPUT, true );
    if (m_hThread == NULL) {
        _RPT0(_CRT_WARN, "Input RunThread Creation failed\n" );
		return 1;
    }

	return 0;
}

int CInputStream::InputData(block_t *pBlock)
{
	if (m_pFifo->i_depth > 5) {
		return 1; // buffer overrun
	}

	//TRACE(_T("InputData ch[%d](%I64d:%d)\n"),m_channelID,pBlock->i_dts,pBlock->i_buffer);
    tme_mutex_lock( &m_lockPts );
	m_lastBufferTime = (pBlock->i_dts > 0) ? pBlock->i_dts : pBlock->i_pts;
    tme_mutex_unlock( &m_lockPts );
	block_FifoPut( m_pFifo, pBlock );

	return 0;
}

void CInputStream::CheckInputClock()
{
    //tme_mutex_lock( &m_lockClock );
	if (!m_inputClock.b_added) {
		ClockInit(&m_inputClock, false, m_iCrAverage);
		m_inputClock.b_master = true;
		m_inputClock.b_added = true;
	}
    //tme_mutex_unlock( &m_lockClock );
}

int CInputStream::ExtractOSD(block_t *pBlock, block_t **ppBlock, int size)
{
	PBYTE ptr;
	block_t *p_b;
	int toRead, lineSize;
	int nText = 0;

	pBlock->i_osd = 0;

	if ((pBlock->i_buffer < 16) ||
		(pBlock->p_buffer[0] != 'T') ||
		(pBlock->p_buffer[1] != 'X') ||
		(pBlock->p_buffer[2] != 'T')) {
		return 0;
	}

	ptr = pBlock->p_buffer + 6;
	toRead = *((unsigned short *)ptr);
	pBlock->i_osd = toRead + 8; /* 8:sizeof(struct OSDHeader):let's avoid memmove here, and use this in DecodeVideo */

	ptr += 2;
	while (toRead > 8) {
		if ((ptr[0] != 's') || (ptr[1] != 't')) {
			break;
		}

		ptr += 6;
		lineSize = *((unsigned short *)ptr);
		ptr += 2;
		if (lineSize > 4) {
			p_b = block_New( lineSize );
			if (p_b) {
				p_b->i_pts = pBlock->i_dts;
				p_b->i_dts = pBlock->i_dts;
				p_b->i_buffer = lineSize;
				p_b->i_length = 0;
				memcpy((char *)p_b->p_buffer, ptr, lineSize);
				if (nText < size) {
					ppBlock[nText++] = p_b;
				}
			}
		}
		toRead -= (8 + lineSize);
		ptr += lineSize;
	}

	return nText;
}

int CInputStream::InputRun()
{
	int i_type;
	tme_value_t val;
	const int textBufSize = 32;
	block_t *p_block, *pp_bText[textBufSize];
	int i, nText;
	mtime_t now;

	tme_thread_ready(&g_ipc);

	while (!m_bEnd && !m_bError)
	{
		//TRACE(_T(" ch<%d>, state:%d\n"), m_channelID, m_iState);
		/* Handle control */
		tme_mutex_lock( &m_lockControl );
		while( !ControlPopNoLock( &i_type, &val ) )
		{
			TRACE(_T(" ch<%d>, control type=%d\n"), m_channelID, i_type );
			Control( i_type, val );
		}
		tme_mutex_unlock( &m_lockControl );

		if( m_iState == PAUSE_S ) {
			msleep( 10*1000 );
			continue;
		}

		/* get a block put by InputData() from dvrplayer */
		if( ( p_block = block_FifoGet( m_pFifo ) ) == NULL ) {
			m_bError = true;
			break;
		}
#if 0
		if (!m_bFirst) {
			DUMP(p_block->p_buffer, p_block->i_buffer > 305 ? 305 : p_block->i_buffer);		
			m_bFirst = true;
		}
#endif
		//TRACE(_T("InputRun ch[%d](%I64d:%d)\n"),m_channelID,p_block->i_dts,p_block->i_buffer);
		/* dvrplayer tried to sync between channels by not sending data? */
		now = mdate();
		//if ((m_lastDataTime > 0) && ((now - m_lastDataTime) > AOUT_MAX_PREPARE_TIME)) {
		if (((now - m_lastDataTime) >  (AOUT_MAX_PREPARE_TIME * 4))) {
			TRACE(_T(" ch<%d>, XX:%I64d(%I64d)\n"), m_channelID, now - m_lastDataTime, AOUT_MAX_PREPARE_TIME);
			ResetPCR(true);
		}
		m_lastDataTime = now;

		if (p_block->i_buffer == 0) {
			TRACE(_T(" ch<%d>, Input Got stop signal\n"), m_channelID);
			block_Release( p_block );
			break; // told to stop looping
		}

		if (p_block->i_buffer == 1) {
			block_Release( p_block );
			continue; // check control again
		}

		mtime_t currentPts = (p_block->i_dts > 0) ? p_block->i_dts : p_block->i_pts;
		tme_mutex_lock( &m_lockPts );
		m_iBufferDelay = (int)((m_lastBufferTime - currentPts) / 1000);
		tme_mutex_unlock( &m_lockPts );
		//TRACE(_T(" ch<%d>, Got data:%d(%x,%I64d,%d)\n"), m_channelID, p_block->i_buffer, p_block->i_flags, p_block->i_dts, m_iBufferDelay);

		//ValidateDelayedRequest();
		if (p_block->i_flags & (BLOCK_FLAG_TYPE_I | BLOCK_FLAG_TYPE_PB)) { 
			if (m_pVideoDecoder == NULL) {
				CheckInputClock();

				es_format_t fmt;
				es_format_Init(&fmt, VIDEO_ES, TME_FOURCC('D','X','5','0'));
				m_pVideoDecoder = new CFFmpeg(&g_ipc, m_hwnd, &fmt, this, false);
				if (m_pVideoDecoder->OpenDecoder()) {
					delete m_pVideoDecoder;
					m_pVideoDecoder = NULL;
					block_Release( p_block );
					continue;
				}

				es_format_Init(&fmt, SPU_ES, TME_FOURCC('s','u','b','t'));
				m_pTextDecoder = new CSubs(&g_ipc, m_hwnd, &fmt, this);
				if (m_pTextDecoder) {
					if (m_pTextDecoder->OpenDecoder()) {
						delete m_pTextDecoder;
						m_pTextDecoder = NULL;
					}
				}
			}
			//tme_mutex_lock( &m_lockClock );
			ClockSetPCR(&m_inputClock, (p_block->i_dts + 11 ) * 9 / 100  );

			//TRACE(_T(" ch<%d>, (%d), original pts:%I64d, dts:%I64d, mdate:%I64d\n"), m_channelID, p_block->i_buffer, p_block->i_pts, p_block->i_dts, mdate());
			if( p_block->i_dts > 0 ) {  
				p_block->i_dts =
					ClockGetTS( &m_inputClock,
					( p_block->i_dts + 11 ) * 9 / 100 );
				//TRACE(_T("ch:%d(%d), modified pts:%I64d, dts:%I64d, mdate:%I64d\n"),m_channelID, p_block->i_buffer, p_block->i_pts, p_block->i_dts, mdate());
			}
			//tme_mutex_unlock( &m_lockClock );
			p_block->i_rate = m_iRate;

			// check OSD text
			nText = ExtractOSD(p_block, pp_bText, textBufSize);

			if (m_pVideoDecoder) {
				m_pVideoDecoder->DecoderData(p_block);
				if (m_bStepPlay) {
					Pause();
				}
			}

			for (i = 0; i < nText; i++) {
				m_pTextDecoder->DecoderData(pp_bText[i]);
			}

#if 0
			// osd test
			m_osdtest++;
			if (osd_test == 1 && m_pTextDecoder && t) {
				block_t *p_b;
				char *osdtext = "OSD Testing\nSecond line\n\nFourth line";
				char *osdtext2 = "\n\n\n\n\nOSD Testing2\nSecond line\n\nFourth line";
				p_b = block_New( strlen(osdtext) + 1 );
				p_b->i_pts = t;
				p_b->i_dts = p_b->i_pts;
				p_b->i_length = 0;
				strcpy((char *)p_b->p_buffer, osdtext);
				if (m_pTextDecoder) m_pTextDecoder->DecoderData(p_b);			

				p_b = block_New( strlen(osdtext2) + 1 );
				p_b->i_pts = t;
				p_b->i_dts = p_b->i_pts;
				p_b->i_length = 0;
				strcpy((char *)p_b->p_buffer, osdtext2);
				if (m_pTextDecoder) m_pTextDecoder->DecoderData(p_b);			
			}
			if (m_osdtest == 100 && m_pTextDecoder && t) {
				block_t *p_b;
				char *osdtext = "                            OSD Testing3";
				char *osdtext2 = "\nOSD Testing4";
				p_b = block_New( strlen(osdtext) + 1 );
				p_b->i_pts = t;
				p_b->i_dts = p_b->i_pts;
				p_b->i_length = 0;
				strcpy((char *)p_b->p_buffer, osdtext);
				if (m_pTextDecoder) m_pTextDecoder->DecoderData(p_b);			

				p_b = block_New( strlen(osdtext2) + 1 );
				p_b->i_pts = t;
				p_b->i_dts = p_b->i_pts;
				p_b->i_length = 0;
				strcpy((char *)p_b->p_buffer, osdtext2);
				if (m_pTextDecoder) m_pTextDecoder->DecoderData(p_b);			
			}
#endif
		} else if (!m_bFastPlay && p_block->i_flags & BLOCK_FLAG_TYPE_AUDIO){
			// don't decode audio when fast or slow play
			if((m_iRate != INPUT_RATE_DEFAULT) || m_bStepPlay) {
				block_Release( p_block );
				continue;
			}
#if 0 // nice try
			if (m_bResetClock) {
				m_bResetClock = false;
				ResetPCR(true);
				TRACE(_T("ch:%d, m_bResetClock2\n"), m_channelID);
			}
#endif
			if (m_pAudioDecoder == NULL) {
				int rate = p_block->i_rate; // temporarily set to sampling rate by mpeg4.cpp
				if ((rate != 8000) && (rate != 32000)) {
					rate = 32000;
				}

				CheckInputClock();
				es_format_t fmt;
				es_format_Init(&fmt, AUDIO_ES, TME_FOURCC('m', 'l', 'a', 'w'));
				fmt.audio.i_channels        = 1;
				fmt.audio.i_rate            = rate;
				fmt.i_bitrate               = rate*8;
				fmt.audio.i_blockalign      = 1;
				fmt.audio.i_bitspersample   = 8;
				fmt.i_extra = 0;
				fmt.p_extra = NULL;
				m_pAudioDecoder = new CARaw(&g_ipc, m_hwnd, &fmt, this);
				if (m_pAudioDecoder->OpenDecoder()) {
					delete m_pAudioDecoder;
					m_pAudioDecoder = NULL;
					block_Release( p_block );
					continue;
				}
			}
			//tme_mutex_lock( &m_lockClock );
			ClockSetPCR(&m_inputClock, (p_block->i_pts + 11 ) * 9 / 100  );
			//tme_mutex_unlock( &m_lockClock );
			if( p_block->i_pts > 0 ) {  
				//tme_mutex_lock( &m_lockClock );
				p_block->i_pts =
					ClockGetTS( &m_inputClock,
					( p_block->i_pts + 11 ) * 9 / 100 );
				//tme_mutex_unlock( &m_lockClock );
				//TRACE(_T("ch:%d, modified pts:%I64d, dts:%I64d, mdate:%I64d\n"),m_channelID, p_block->i_pts, p_block->i_dts, mdate());
			}

			p_block->i_rate = m_iRate; // now use it for its original use.
			if (m_pAudioDecoder) m_pAudioDecoder->DecoderData(p_block);			
		} else {
			block_Release( p_block );
		}
	}

	TRACE(_T(" ch<%d>, input thread end\n"), m_channelID);
	return 0;
}

void CInputStream::DestroyThread()
{
	m_bEnd = true;

	/* if Direct3d is being created, 
	 * thread_join cannot be called.
	 * So, make sure video output creation is complete if already started.
	 */
	while (m_bCreatingVideoOutput) {
		TRACE(_T(" ch<%d>, waiting video output creation to complete\n"), m_channelID);
		Sleep(20);
	}

	// release the fifo waiting.
	block_t *p_block = block_New( 0 );
	block_FifoPut( m_pFifo, p_block );

	if (m_hThread != NULL) {
		TRACE(_T(" ch<%d>,Joining input thread:%d\n"), m_channelID, m_hThread);
		tme_thread_join(m_hThread);
        TRACE(_T(" ch<%d>,Joining input thread done\n"), m_channelID);
	}
	m_hThread = NULL;

	if (m_pVideoDecoder != NULL) {
		delete m_pVideoDecoder;
		m_pVideoDecoder = NULL;
	}
	if (m_pAudioDecoder != NULL) {
		delete m_pAudioDecoder;
		m_pAudioDecoder = NULL;
	}
	if (m_pTextDecoder != NULL) {
		delete m_pTextDecoder;
		m_pTextDecoder = NULL;
	}
}

int CInputStream::Play()
{
	tme_value_t val;

	TRACE(_T(" ch<%d>, Play()\n"), m_channelID);
	/* go to normal speed */
	if (m_iRate != INPUT_RATE_DEFAULT) {
		val.i_int = 0;
		ControlPush(INPUT_CONTROL_SET_FORWARD_SPEED, &val);
	}

	/* when input loop is stuck at getFifo, pause command can be stuck, too */
	//if (m_iState == PLAYING_S ) {
	//	return 0;
	//} else if (m_iState == PAUSE_S ) {
		val.i_int = PLAYING_S;
	//}
	ControlPush(INPUT_CONTROL_SET_STATE, &val);

	return 0;
}

int CInputStream::PlayStep() 
{
	tme_value_t val;

	/* go to normal speed */
	if (m_iRate != INPUT_RATE_DEFAULT) {
		val.i_int = 0;
		ControlPush(INPUT_CONTROL_SET_FORWARD_SPEED, &val);
	}

	ControlPush(INPUT_CONTROL_SET_FORWARD_STEP, NULL);
	return 0;
}

int CInputStream::Pause() 
{
	tme_value_t val;
	if (m_iState == PAUSE_S ) {
		return 0;
	} else if (m_iState == PLAYING_S ) {
		val.i_int = PAUSE_S;
	}
	ControlPush(INPUT_CONTROL_SET_STATE, &val);

	return 0;
}

int CInputStream::PlayForwardSpeed(int speed) 
{
	tme_value_t val;

	val.i_int = speed;
	ControlPush(INPUT_CONTROL_SET_FORWARD_SPEED, &val);
	return 0;
}

int CInputStream::AudioOn(bool on)
{
	m_bAudioOn = on;

	if ((m_pAudioDecoder != NULL) && (m_pAudioDecoder->m_pAout != NULL)) {
		return m_pAudioDecoder->m_pAout->AudioOn(on);
	}

	return 1;
}

void CInputStream::SetState(int state)
{
	TRACE(_T("SetState(%d):%d\n"), m_channelID, state);
	m_iState = state;
	//setVoutStateForWindowHandle(m_hwnd, m_iState);
}

bool CInputStream::IsAudioOn()
{
	return m_bAudioOn;
}

int CInputStream::GetChannelID()
{
	return m_channelID;
}

/*****************************************************************************
 * Control
 *****************************************************************************/
int CInputStream::ControlPopNoLock( int *pi_type, tme_value_t *p_val )
{
	int i;

	if( m_iControl <= 0 )
    {
        return EGENERIC;
    }

#if 0
	if (m_control[0].i_type == INPUT_CONTROL_SET_FORWARD_STEP_DELAYED) {
		if (m_control[0].val.i_int == 1) {
			*pi_type = m_control[0].i_type;
			*p_val   = m_control[0].val;

			m_iControl--;
			if( m_iControl > 0 )
			{
		        for( i = 0; i < m_iControl; i++ )
				{
					m_control[i].i_type = m_control[i+1].i_type;
					m_control[i].val    = m_control[i+1].val;
				}
			}
		} else {
			*pi_type = m_control[0].i_type;
			*p_val   = m_control[0].val;

			if( m_iControl > 1 )
			{
				*pi_type = m_control[1].i_type;
				*p_val   = m_control[1].val;

				m_iControl--;
		        for( i = 1; i < m_iControl; i++ )
				{
					m_control[i].i_type = m_control[i+1].i_type;
					m_control[i].val    = m_control[i+1].val;
				}
			}
		}
	} else {
#endif
	    *pi_type = m_control[0].i_type;
		*p_val   = m_control[0].val;

		m_iControl--;
		if( m_iControl > 0 )
		{
			for( i = 0; i < m_iControl; i++ )
			{
				m_control[i].i_type = m_control[i+1].i_type;
				m_control[i].val    = m_control[i+1].val;
			}
		}
#if 0
	}
#endif

    return SUCCESS;
}
void CInputStream::ControlPush(int i_type, tme_value_t *p_val)
{
	TRACE(_T(" ch<%d>, ControlPush:%d\n"), m_channelID, i_type);
    tme_mutex_lock( &m_lockControl );
    if( i_type == INPUT_CONTROL_SET_DIE )
    {
        /* Special case, empty the control */
        m_iControl = 1;
        m_control[0].i_type = i_type;
        memset( &m_control[0].val, 0, sizeof( tme_value_t ) );
    }
    else
    {
        if( m_iControl >= INPUT_CONTROL_FIFO_SIZE )
        {
            _RPT1(_CRT_WARN, "input control fifo overflow, trashing type=%d\n",
                     i_type );
            tme_mutex_unlock( &m_lockControl );
            return;
        }
        m_control[m_iControl].i_type = i_type;
        if( p_val )
            m_control[m_iControl].val = *p_val;
        else
            memset( &m_control[m_iControl].val, 0,
                    sizeof( tme_value_t ) );

        m_iControl++;
		TRACE(_T(" ch<%d>, m_iControl:%d\n"), m_channelID, m_iControl);
    }
    tme_mutex_unlock( &m_lockControl );
}
#if 0
void CInputStream::ValidateDelayedRequest()
{
    if( m_iControl > 0 )
    {
        int i;

        for( i = 0; i < m_iControl; i++ )
        {
			if (m_control[i].i_type == INPUT_CONTROL_SET_FORWARD_STEP_DELAYED) {
				m_control[i].val.i_int = 1;
			}
        }
    }
}
#endif
bool CInputStream::Control( int i_type, tme_value_t val )
{
    bool b_force_update = false;

    switch( i_type )
    {
        case INPUT_CONTROL_SET_STATE:
            if( ( val.i_int == PLAYING_S && m_iState == PAUSE_S ) ||
                ( val.i_int == PAUSE_S && m_iState == PAUSE_S ) )
            {
				TRACE(_T("ch:%d, pause ----> playing or pause ----> pause\n"), m_channelID);
				/* pause ----> playing or pause ----> pause */
				if (m_bStepPlay) {
					m_bStepPlay = false;
					//setVoutStepPlayForWindowHandle(m_hwnd, m_bStepPlay);
				}

                //SetPauseState(false);

                b_force_update = true;

                /* Switch to play */
				SetState(PLAYING_S);
                val.i_int = PLAYING_S;

                /* Reset clock */
				ResetPCR(true);
		        /* Send a dummy block to let decoder know that
				 * there is a discontinuity */
		        AllDecodersDiscontinuity();
#if 0 // nice try
				/* resetPCR again when 1st audio data arrives */
				if (m_pAudioDecoder) {
					m_bResetClock = true;
					TRACE(_T("ch:%d, m_bResetClock\n"), m_channelID);
				}
#endif
            }
            else if( val.i_int == PAUSE_S && m_iState == PLAYING_S )
            {
				TRACE(_T("ch:%d, playing ----> pause\n"), m_channelID);
				/* playing ----> pause */
                //SetPauseState(true);
                b_force_update = true;
                val.i_int = PAUSE_S;
                /* Switch to new state */
				SetState(val.i_int);
            }
            else if( val.i_int != PLAYING_S && val.i_int != PAUSE_S )
            {
                _RPT0(_CRT_WARN, "invalid state in INPUT_CONTROL_SET_STATE\n" );
            }
            break;
		case INPUT_CONTROL_SET_FORWARD_SPEED:
			{	
				int speed, newRate;

				speed = val.i_int;
				TRACE(_T("ch:%d, speed:%d\n"), m_channelID, speed);
				if (speed > 0) {
					if (speed > 3)
						speed = 3;
					m_bFastPlay = true;
					if (speed == 1)
						newRate = INPUT_RATE_DEFAULT / 2;
					else if (speed == 2)
						newRate = INPUT_RATE_DEFAULT / 4;
					else
						newRate = INPUT_RATE_DEFAULT / 8;
				} else if (speed == 0) {
					m_bFastPlay = false;
					newRate = INPUT_RATE_DEFAULT;
				} else if (speed < 0) {
					if (speed < -3)
						speed = -3;
					m_bFastPlay = false;
					if (speed == -1)
						newRate = INPUT_RATE_DEFAULT * 2;
					else if (speed == -2)
						newRate = INPUT_RATE_DEFAULT * 4;
					else 
						newRate = INPUT_RATE_DEFAULT * 6;
				}
				if (m_iRate != newRate) {
					_RPT2(_CRT_WARN, "m_iRate:%d,newRate:%d\n",m_iRate,newRate);

					/* in case fast --> normal transition,
					 * clear buffers to avoid "received future data error" 
					 */
					if ((m_iRate < INPUT_RATE_DEFAULT) && (newRate == INPUT_RATE_DEFAULT)) { 
						if( m_pVideoDecoder)
				            m_pVideoDecoder->DecoderDiscontinuity();
						if( m_pTextDecoder)
				            m_pTextDecoder->DecoderDiscontinuity();
					}

					m_iRate = newRate;
					if (m_iRate == INPUT_RATE_DEFAULT) {
						if( m_pAudioDecoder)
				            m_pAudioDecoder->DecoderDiscontinuity();
					}
					ResetPCR(true);
	                b_force_update = true;
				}
				break;
			}
        case INPUT_CONTROL_SET_FORWARD_STEP:
			TRACE(_T("ch:%d, FORWARD_STEP:%d\n"), m_channelID, m_iState);
			m_bStepPlay = true;
			//setVoutStepPlayForWindowHandle(m_hwnd, m_bStepPlay);
            if (m_iState == PAUSE_S)
            {
                //SetPauseState(false);

                b_force_update = true;

                /* Switch to play */
				SetState(PLAYING_S);
                /* Reset clock */
				ResetPCR(true);
		        /* Send a dummy block to let decoder know that
				 * there is a discontinuity */
				AllDecodersDiscontinuity();	
            } else if(m_iState == PLAYING_S)
            {
                //SetPauseState(true);
                b_force_update = true;
                /* Switch to new state */
				SetState(PAUSE_S);
            }
            break;
        case INPUT_CONTROL_SET_RESET_BUFFER:
			/* empty input buffer */
			block_FifoEmpty( m_pFifo );

			ResetPCR(true);
			/* empty decoder buffers */
			AllDecodersDiscontinuity();

			/* signal that it's done */
			tme_mutex_lock(&m_lock);
		    tme_cond_signal( &m_wait );
			tme_mutex_unlock(&m_lock);

#if 0 // nice try
			/* resetPCR again when 1st audio data arrives */
			m_bResetClock = true;
#endif
            break;
		default:
			break;
	}
	return b_force_update;
}

void CInputStream::AllDecodersDiscontinuity()
{
	if( m_pVideoDecoder)	
		m_pVideoDecoder->DecoderDiscontinuity();				
	if( m_pAudioDecoder)	
		m_pAudioDecoder->DecoderDiscontinuity();			
	if( m_pTextDecoder)
		m_pTextDecoder->DecoderDiscontinuity();
}

int CInputStream::ResetBuffer()
{
	TRACE(_T("ch:%d,ResetBuffer:%d\n"), m_channelID, m_pFifo->i_depth);
#if 0
	/* empty input buffer */
	block_FifoEmpty( m_pFifo );
	TRACE(_T("ch:%d,Empty:%d\n"), m_channelID, m_pFifo->i_depth);

	ResetPCR(true);

	TRACE(_T("ch:%d, Decoder:%d\n"), m_channelID, m_pFifo->i_depth);
	/* empty decoder buffers */
	AllDecodersDiscontinuity();
#else
	tme_mutex_lock(&m_lock);
	ControlPush(INPUT_CONTROL_SET_RESET_BUFFER, NULL);

	// release the fifo waiting.
	block_t *p_block = block_New( 1 );
	block_FifoPut( m_pFifo, p_block );

	/* wait until processed */
	tme_cond_wait(&m_wait, &m_lock);
	tme_mutex_unlock(&m_lock);
#endif

	tme_mutex_lock( &m_lockPts );
	m_iBufferDelay = 0;
	tme_mutex_unlock( &m_lockPts );

	return 0;
}

void CInputStream::ResetPCR(bool synchro_reinit)
{
    //tme_mutex_lock( &m_lockClock );
	if (synchro_reinit)
		m_inputClock.i_synchro_state = SYNCHRO_REINIT;
	m_inputClock.last_pts = 0;
	//m_lastDataTime = 0;
    //tme_mutex_unlock( &m_lockClock );
}

/* convert 90000Hz clock (1 sec == 90000) to usec resolution system counter */
mtime_t CInputStream::ClockToSysdate( input_clock_t *cl, mtime_t i_clock )
{
    mtime_t     i_sysdate = 0;

	//_RPT4(_CRT_WARN, "ClockToSysdate(clock:%I64d):cl_ref:%I64d,i_rate:%d,sysdata_ref:%I64d\n", i_clock,cl->cr_ref,m_iRate,cl->sysdate_ref);

	if( cl->i_synchro_state == SYNCHRO_OK )
    {
        i_sysdate = (mtime_t)(i_clock - cl->cr_ref)
                        * (mtime_t)m_iRate
                        * (mtime_t)300;
		//_RPT1(_CRT_WARN, "i_sysdate:%I64d\n", i_sysdate);
        i_sysdate /= 27;
        i_sysdate /= 1000;
        i_sysdate += (mtime_t)cl->sysdate_ref;
    }

	//_RPT1(_CRT_WARN, "i_sysdate/27/1000+sysdate_ref:%I64d\n", i_sysdate);
    return( i_sysdate );
}

/* get current time in 90000Hz tick from the reference time(cr_ref in 90000Hz clock)
 */
mtime_t CInputStream::ClockCurrent(input_clock_t *cl )
{
    return( (mdate() - cl->sysdate_ref) * 27 * INPUT_RATE_DEFAULT
             / m_iRate / 300
             + cl->cr_ref );
}

/*
 * last_pts is 0 on 1st call and previous pts value(in 90000Hz clock) for others
 */
void CInputStream::ClockSetPCR( input_clock_t *cl, mtime_t i_clock )
{
	//TRACE(_T("ClockSetPCR(%I64d),last_pts:%I64d,last_cr:%I64d,state:%d\n"), i_clock,cl->last_pts,cl->last_cr,cl->i_synchro_state);

    if( ( cl->i_synchro_state != SYNCHRO_OK ) ||
        ( i_clock == 0 && cl->last_cr != 0 ) )
    {
		//_RPT3(_CRT_WARN, "Creating new Ref,state:%d,clock:%I64d,last_cr:%I64d\n", cl->i_synchro_state,i_clock,cl->last_cr);
        /* Feed synchro with a new reference point. usually with mdate() */
        ClockNewRef( cl, i_clock,
                     cl->last_pts + CR_MEAN_PTS_GAP > mdate() ?
                     cl->last_pts + CR_MEAN_PTS_GAP : mdate() );
        cl->i_synchro_state = SYNCHRO_OK;

        if( m_bCanPaceControl && cl->b_master ) /* m_bCanPaceControl:true, b_master:1 */
        {
            cl->last_cr = i_clock;
            if( !m_bOutPaceControl ) /* m_bOutPaceControl:false */
            {
                mtime_t i_wakeup = ClockToSysdate( cl, i_clock );
                while( (i_wakeup - mdate()) / CLOCK_FREQ > 1 )
                {
                    msleep( CLOCK_FREQ );
                    if( m_bEnd) i_wakeup = mdate();
                }
				mwait( i_wakeup );
            }
        }
        else
        {
            cl->last_cr = 0;
            cl->last_sysdate = 0;
            cl->delta_cr = 0;
            cl->i_delta_cr_residue = 0;
        }
    }
    else
    {
        if ( cl->last_cr != 0 &&
               (    (cl->last_cr - i_clock) > CR_MAX_GAP
                 || (cl->last_cr - i_clock) < - CR_MAX_GAP ) )
        {
            /* Stream discontinuity, for which we haven't received a
             * warning from the stream control facilities (dd-edited
             * stream ?). */
			_RPT3(_CRT_WARN, "clock gap, unexpected stream discontinuity:%I64d,%I64d,%I64d\n",cl->last_cr,i_clock,cl->last_cr - i_clock);
            ClockInit( cl, cl->b_master, cl->i_cr_average );
            /* FIXME needed ? */
        }

        cl->last_cr = i_clock;

        if( m_bCanPaceControl && cl->b_master )
        {
            /* Wait a while before delivering the packets to the decoder.
             * In case of multiple programs, we arbitrarily follow the
             * clock of the selected program. */
            if( !m_bOutPaceControl )
            {
                mtime_t i_wakeup = ClockToSysdate( cl, i_clock );
                while( (i_wakeup - mdate()) / CLOCK_FREQ > 1 )
                {
                    msleep( CLOCK_FREQ );
                    if( m_bEnd ) i_wakeup = mdate();
                }
                mwait( i_wakeup );
            }
            /* FIXME Not needed anymore ? */
        }
        else if ( mdate() - cl->last_sysdate > 200000 )
        {
            /* Smooth clock reference variations. */
            mtime_t i_extrapoled_clock = ClockCurrent( cl );
            mtime_t delta_cr;

            /* Bresenham algorithm to smooth variations. */
            delta_cr = ( cl->delta_cr * (cl->i_cr_average - 1)
                               + ( i_extrapoled_clock - i_clock )
                               + cl->i_delta_cr_residue )
                           / cl->i_cr_average;
			cl->i_delta_cr_residue = ( cl->delta_cr * (cl->i_cr_average - 1)
                                       + ( i_extrapoled_clock - i_clock )
                                       + cl->i_delta_cr_residue )
                                    % cl->i_cr_average;
            cl->delta_cr = delta_cr;
            cl->last_sysdate = mdate();
        }
    }
}

/* returns TS in our mdate() high res sys clock format + ptsDelay
 */
mtime_t CInputStream::ClockGetTS( input_clock_t *cl, mtime_t i_ts )
{
	//_RPT3(_CRT_WARN, "ClockGetTS(ts:%I64d,delta:%I64d,state:%d)\n", i_ts, cl->delta_cr,cl->i_synchro_state);
    if( cl->i_synchro_state != SYNCHRO_OK )
        return 0;

	//_RPT2(_CRT_WARN, "ts:%I64d, delta_cr:%I64d\n", i_ts, cl->delta_cr);
	/* delta_cr is only used when there is abrupt change in system time
	 * it is 0 otherwise
	 */
    cl->last_pts = ClockToSysdate( cl, i_ts + cl->delta_cr );
	//_RPT1(_CRT_WARN, "last_pts:%I64d\n", cl->last_pts);

	return cl->last_pts + m_iPtsDelay;
}

static void ClockInit( input_clock_t *cl, bool b_master, int i_cr_average )
{
    cl->i_synchro_state = SYNCHRO_START;

    cl->last_cr = 0;
    cl->last_pts = 0;
    cl->last_sysdate = 0;
    cl->cr_ref = 0;
    cl->sysdate_ref = 0;
    cl->delta_cr = 0;
    cl->i_delta_cr_residue = 0;

    cl->i_cr_average = i_cr_average;

    cl->b_master = b_master;
}

/* create refrence point
 * cr_ref: (server_wallclock+11)*9/100 i.e. in 90000 Hz clock
 * sysdate_ref: time in mdate() format(high res sys timer) value when ref was created
 */
static void ClockNewRef( input_clock_t *cl,
                         mtime_t i_clock, mtime_t i_sysdate )
{
    cl->cr_ref = i_clock;
    cl->sysdate_ref = i_sysdate ;
}

int CInputStream::SnapShot(char * bmpfilename)
{
	int res=0 ;

	struct vout_set *pvs, vs;
	pvs = getVoutSetForWindowHandle(m_hwnd, &vs);
	if (pvs && vs.vout) {
		CVideoOutput *pVout = vs.vout;
		int width = pVout->m_render.i_width;
		int height = pVout->m_render.i_height;
		int rgbsize = width * height * 3;
		PBYTE rgb = new BYTE[rgbsize];
		if( pVout->GetSnapShotPic(rgb, width, height)>=0 ) {
			// save to BMP file
			BITMAPFILEHEADER bmfheader ;
			BITMAPINFOHEADER bmiheader ;
			bmfheader.bfType='MB' ;
			bmfheader.bfOffBits = sizeof(bmfheader)+sizeof(bmiheader) ;
			bmfheader.bfSize = bmfheader.bfOffBits+rgbsize ;
			bmfheader.bfReserved1=bmfheader.bfReserved2=0 ;
			bmiheader.biSize = sizeof(bmiheader);
			bmiheader.biWidth = width; 
			bmiheader.biHeight = -height; 
			bmiheader.biPlanes = 1; 
			bmiheader.biBitCount=24	;		// 24 bits RGB 
			bmiheader.biCompression=BI_RGB; 
			bmiheader.biSizeImage=rgbsize; 
			bmiheader.biXPelsPerMeter=0; 
			bmiheader.biYPelsPerMeter=0; 
			bmiheader.biClrUsed=0; 
			bmiheader.biClrImportant=0; 
			FILE * bmpfile ;
			errno_t err;
			err = fopen_s(&bmpfile, bmpfilename, "wb");
			if( err == 0 ) { 
				fwrite(&bmfheader,1,sizeof(bmfheader), bmpfile);
				fwrite(&bmiheader,1,sizeof(bmiheader), bmpfile);
				fwrite(rgb, 1, rgbsize, bmpfile);
				fclose(bmpfile);
				res=1 ;
			}
			delete rgb ;		// release rgb buffer
		}
	}
	return res ;
}

int CInputStream::GetBufferDelay()
{
	int delay;
    tme_mutex_lock( &m_lockPts );
	delay =	m_iBufferDelay;
    tme_mutex_unlock( &m_lockPts );
	return delay;
}