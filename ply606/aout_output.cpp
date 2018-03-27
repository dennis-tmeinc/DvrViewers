#include "stdafx.h"
#include <crtdbg.h>

#include "audio_output.h"
#include "aout_internal.h"
#include "aout_output.h"


CAoutOutput::CAoutOutput(tme_object_t *pIpc, CAudioOutput * pAout,
						 audio_format_t * pFormat)
{
	m_pIpc = pIpc;
    m_bError = true;
    m_bStarving = true;
	m_bResetPlayer = false;
	m_fifo.p_first = NULL;
	m_iNbFilters = 0;

	m_pAout = pAout;
	memcpy( &m_output, pFormat, sizeof(audio_sample_format_t) );
    aout_FormatPrepare( &m_output);
}

int CAoutOutput::PostInit()
{
	tme_mutex_lock( &m_pAout->m_outputFifoLock );

    /* Prepare FIFO. */
    aout_FifoInit( m_pAout, &m_fifo,
                   m_output.i_rate );

    tme_mutex_unlock( &m_pAout->m_outputFifoLock );

    aout_FormatPrint( m_pAout, _T("output"), &m_output );

    /* Calculate the resulting mixer output format. */
    memcpy( &m_pAout->m_mixer.mixer, &m_output,
            sizeof(audio_format_t) );

    if ( !AOUT_FMT_NON_LINEAR(&m_output) )
    {
        /* Non-S/PDIF mixer only deals with float32 or fixed32. */
        m_pAout->m_mixer.mixer.i_format
                     = /*(CPU_CAPABILITY_FPU)*/ true ?
                        TME_FOURCC('f','l','3','2') :
                        TME_FOURCC('f','i','3','2');
        aout_FormatPrepare( &m_pAout->m_mixer.mixer );
    }
    else
    {
        m_pAout->m_mixer.mixer.i_format = m_output.i_format;
    }

    aout_FormatPrint( m_pAout, _T("mixer"), &m_pAout->m_mixer.mixer );

	/* Create filters. */
	m_iNbFilters = 0;
    if ( aout_FiltersCreatePipeline( m_pAout, m_ppFilters,
                                     &m_iNbFilters,
                                     &m_pAout->m_mixer.mixer,
                                     &m_output ) < 0 )
    {
        _RPT0(_CRT_WARN,"couldn't create audio output pipeline\n" );
        return 1;
    }

    /* Prepare hints for the buffer allocator. */
    m_pAout->m_mixer.output_alloc.i_alloc_type = AOUT_ALLOC_HEAP;
    m_pAout->m_mixer.output_alloc.i_bytes_per_sec
                        = m_pAout->m_mixer.mixer.i_bytes_per_frame
                           * m_pAout->m_mixer.mixer.i_rate
                           / m_pAout->m_mixer.mixer.i_frame_length;

    aout_FiltersHintBuffers( m_pAout, m_ppFilters,
                             m_iNbFilters,
                             &m_pAout->m_mixer.output_alloc );

    m_bError = false;

	return 0;
}

CAoutOutput::~CAoutOutput()
{
    aout_FiltersDestroyPipeline( m_pAout, m_ppFilters,
                                 m_iNbFilters );
    aout_FifoDestroy( m_pAout, &m_fifo );
}

/*****************************************************************************
 * aout_OutputPlay : play a buffer
 *****************************************************************************
 * This function is entered with the mixer lock.
 * Called from MixBuffer() -- many times...
 *****************************************************************************/
void CAoutOutput::OutputPlay( aout_buffer_t * p_buffer )
{
    //_RPT0(_CRT_WARN,"OutputPlay\n" );
	aout_FiltersPlay( m_pAout, m_ppFilters, m_iNbFilters,
                      &p_buffer );

    if( p_buffer->i_nb_bytes == 0 )
    {
        aout_BufferFree( p_buffer );
        return;
    }

    tme_mutex_lock( &m_pAout->m_outputFifoLock );
    aout_FifoPush( m_pAout, &m_fifo, p_buffer );
	if (m_bResetPlayer) { /* nice try, but didn't help a lot */
		m_bResetPlayer = false;
	    Play(true);
	} else {
	    Play(false);
	}
    tme_mutex_unlock( &m_pAout->m_outputFifoLock );
}

/*****************************************************************************
 * aout_OutputNextBuffer : give the audio output plug-in the right buffer
 *****************************************************************************
 * If b_can_sleek is 1, the aout core functions won't try to resample
 * new buffers to catch up - that is we suppose that the output plug-in can
 * compensate it by itself. S/PDIF outputs should always set b_can_sleek = 1.
 * This function is entered with no lock at all :-).
 *****************************************************************************/
aout_buffer_t * CAoutOutput::OutputNextBuffer( mtime_t start_date,
                                       bool b_can_sleek)
{
    aout_buffer_t * p_buffer;

    tme_mutex_lock( &m_pAout->m_outputFifoLock );

    p_buffer = m_fifo.p_first;

    /* Drop the audio sample if the audio output is really late.
     * In the case of b_can_sleek, we don't use a resampler so we need to be
     * a lot more severe. */
    while ( p_buffer && p_buffer->start_date <
            (b_can_sleek ? start_date : mdate()) - AOUT_PTS_TOLERANCE )
    {
        _RPT2(_CRT_WARN, "***************audio output is too slow (%I64d), trashing %I64dus\n", mdate() - p_buffer->start_date,
                 p_buffer->end_date - p_buffer->start_date );
        p_buffer = p_buffer->p_next;
        aout_BufferFree( m_fifo.p_first );
        m_fifo.p_first = p_buffer;
    }

    if ( p_buffer == NULL )
    {
        m_fifo.pp_last = &m_fifo.p_first;

        tme_mutex_unlock( &m_pAout->m_outputFifoLock );
        return NULL;
    }

	//_RPT4(_CRT_WARN, "start_date:%I64d,buffer->start_date:%I64d > %I64d,buffer->end_date%I64d\n", start_date,p_buffer->start_date,start_date+(p_buffer->end_date - p_buffer->start_date),p_buffer->end_date);
   /* Here we suppose that all buffers have the same duration - this is
     * generally true, and anyway if it's wrong it won't be a disaster.
     */
    if ( p_buffer->start_date > start_date
                         + (p_buffer->end_date - p_buffer->start_date) )
    /*
     *                   + AOUT_PTS_TOLERANCE )
     * There is no reason to want that, it just worsen the scheduling of
     * an audio sample after an output starvation (ie. on start or on resume)
     */
    {
        tme_mutex_unlock( &m_pAout->m_outputFifoLock );
        //if ( !m_bStarving )
          //  _RPT1(_CRT_WARN, "***************audio output is starving (%I64d), playing silence\n", p_buffer->start_date - start_date );
        m_bStarving = true;
        return NULL;
    }

    m_bStarving = false;

    if ( !b_can_sleek && 
          ( (p_buffer->start_date - start_date > AOUT_PTS_TOLERANCE)
             || (start_date - p_buffer->start_date > AOUT_PTS_TOLERANCE) ) )
    {
        /* Try to compensate the drift by doing some resampling. */
        int i;
        mtime_t difference = start_date - p_buffer->start_date;
		_RPT3(_CRT_WARN, "***************output date isn't PTS date, requesting resampling (%I64d),start:%I64d,bufstart:%I64d\n", difference,start_date,p_buffer->start_date );

        tme_mutex_lock( &m_pAout->m_inputFifosLock );
        for ( i = 0; i < m_pAout->m_iNbInputs; i++ )
        {
            aout_fifo_t * p_fifo = &m_pAout->m_ppInputs[i]->fifo;

            aout_FifoMoveDates( m_pAout, p_fifo, difference );
        }

        aout_FifoMoveDates( m_pAout, &m_fifo, difference );
        tme_mutex_unlock( &m_pAout->m_inputFifosLock );
    }

    m_fifo.p_first = p_buffer->p_next;
    if ( p_buffer->p_next == NULL )
    {
        m_fifo.pp_last = &m_fifo.p_first;
    }

    tme_mutex_unlock( &m_pAout->m_outputFifoLock );
    return p_buffer;
}
