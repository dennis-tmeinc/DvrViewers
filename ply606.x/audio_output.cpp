#include "stdafx.h"
#include <crtdbg.h>
#include <stdio.h>
#include "audio_output.h"
#include "aout_internal.h"
#include "directx_audio.h"
#include "trace.h"

#ifndef ptrdiff_t
typedef int ptrdiff_t;
#endif

static void DoTrivialMixer(CAudioOutput *pAout, aout_buffer_t * p_buffer );
static void DoFloat32Mixer( CAudioOutput *pAout, aout_buffer_t * p_buffer );

// called from AoutNewBuffer(decoder.c)
CAudioOutput::CAudioOutput(tme_object_t *pIpc, audio_format_t *pFormat, int channel)
{
    /* Initialize members. */
	m_iAudioDevice = -1;
	m_channelID = channel;

	m_pIpc = pIpc;
	//tme_thread_init(&m_ipc);
    tme_mutex_init(m_pIpc, &m_inputFifosLock );
    tme_mutex_init(m_pIpc, &m_mixerLock );
    tme_mutex_init(m_pIpc, &m_outputFifoLock );
    m_iNbInputs = 0;
    m_mixer.f_multiplier = 1.0;
    m_mixer.b_error = true; // can be used as audio on(false)/off(true)
	m_iVolume = AOUT_VOLUME_DEFAULT;
}

CAudioOutput::~CAudioOutput()
{
    tme_mutex_destroy( &m_inputFifosLock );
    tme_mutex_destroy( &m_mixerLock );
    tme_mutex_destroy( &m_outputFifoLock );

	//tme_mutex_destroy(&(m_ipc.object_lock));
	//tme_cond_destroy(&(m_ipc.object_wait));
}

aout_input_t * CAudioOutput::DecNew( audio_sample_format_t * p_format )
{
	int i;
    aout_input_t * p_input;

    /* We can only be called by the decoder, so no need to lock
     * p_input->lock. */
    tme_mutex_lock( &m_mixerLock );

    if ( m_iNbInputs >= AOUT_MAX_INPUTS )
    {
        _RPT1(_CRT_WARN, "too many inputs already (%d)\n", m_iNbInputs );
        return NULL;
    }

    p_input = (aout_input_t *)malloc(sizeof(aout_input_t));
    if ( p_input == NULL )
    {
        _RPT0(_CRT_WARN, "out of memory\n" );
        return NULL;
    }

    tme_mutex_init( m_pIpc, &p_input->lock );

    p_input->b_changed = false;
    p_input->b_error = true;
    aout_FormatPrepare( p_format );
    memcpy( &p_input->input, p_format,
            sizeof(audio_sample_format_t) );

    m_ppInputs[m_iNbInputs] = p_input;
    m_iNbInputs++;

    if ( m_mixer.b_error )
    {
		m_pOutput = new CDirectXAudio(m_pIpc, this, p_format);
        if ( m_pOutput == NULL )
        {  
			for ( i = 0; i < m_iNbInputs - 1; i++ )
            {
				tme_mutex_lock( &m_ppInputs[i]->lock );
                aout_InputDelete( this, m_ppInputs[i] );
				tme_mutex_unlock( &m_ppInputs[i]->lock );
            }
   
			tme_mutex_unlock( &m_mixerLock );   
			return p_input;
        }
        /* Create other input streams. */
        for ( i = 0; i < m_iNbInputs - 1; i++ )
        {
			tme_mutex_lock( &m_ppInputs[i]->lock );
            aout_InputDelete( this, m_ppInputs[i] );
            aout_InputNew( this, m_ppInputs[i] );
			tme_mutex_unlock( &m_ppInputs[i]->lock );
        }
    }
    else
    {
		MixerDelete();
	}

    if ( MixerNew() == -1 )
    {
		delete m_pOutput;
		m_pOutput = NULL;

		tme_mutex_unlock( &m_mixerLock );
        return p_input;
    }

    aout_InputNew( this, p_input );

    tme_mutex_unlock( &m_mixerLock );

	int audioDesync = 0; // make it configurable
    p_input->i_desync = audioDesync * 1000;

    p_input->i_pts_delay = DEFAULT_PTS_DELAY;//(int)m_pInput->m_iPtsDelay;
    p_input->i_pts_delay += p_input->i_desync;

	return p_input;
}

/*****************************************************************************
 * aout_DecDelete : delete a decoder
 *****************************************************************************/
int CAudioOutput::DecDelete( aout_input_t * p_input )
{
    int i_input;

    _RPT0(_CRT_WARN, "CAudioOutput::DecDelete\n" );
    /* This function can only be called by the decoder itself, so no need
     * to lock p_input->lock. */
    tme_mutex_lock( &m_mixerLock );

    for ( i_input = 0; i_input < m_iNbInputs; i_input++ )
    {
        if ( m_ppInputs[i_input] == p_input )
        {
            break;
        }
    }

    if ( i_input == m_iNbInputs )
    {
        _RPT0(_CRT_WARN, "cannot find an input to delete\n" );
        return -1;
    }

    /* Remove the input from the list. */
    memmove( &m_ppInputs[i_input], &m_ppInputs[i_input + 1],
             (AOUT_MAX_INPUTS - i_input - 1) * sizeof(aout_input_t *) );
    m_iNbInputs--;

    aout_InputDelete( this, p_input );

    tme_mutex_destroy( &p_input->lock );
    free( p_input );

    if ( !m_iNbInputs )
    {
        delete m_pOutput;
		m_pOutput = NULL;
		m_mixer.b_error = true; // MixerDelete
		if (m_iAudioDevice != -1)
			m_iAudioDevice = -1;
    }

    tme_mutex_unlock( &m_mixerLock );

    return 0;
}

/*****************************************************************************
 * aout_DecNewBuffer : ask for a new empty buffer
 *****************************************************************************/
aout_buffer_t * CAudioOutput::DecNewBuffer( aout_input_t * p_input,
                                   size_t i_nb_samples )
{
    aout_buffer_t * p_buffer;
    mtime_t duration;

    tme_mutex_lock( &p_input->lock );

    if ( p_input->b_error )
    {
        tme_mutex_unlock( &p_input->lock );
        return NULL;
    }

    duration = (1000000 * (mtime_t)i_nb_samples) / p_input->input.i_rate;

    /* This necessarily allocates in the heap. */
    aout_BufferAlloc( &p_input->input_alloc, duration, NULL, p_buffer );
    p_buffer->i_nb_samples = (unsigned int)i_nb_samples;
    p_buffer->i_nb_bytes = i_nb_samples * p_input->input.i_bytes_per_frame
                              / p_input->input.i_frame_length;

    /* Suppose the decoder doesn't have more than one buffered buffer */
    p_input->b_changed = false;

    tme_mutex_unlock( &p_input->lock );

    if ( p_buffer == NULL )
    {
        _RPT0(_CRT_WARN, "NULL buffer !" );
    }
    else
    {
        p_buffer->start_date = p_buffer->end_date = 0;
    }

    return p_buffer;
}

/*****************************************************************************
 * aout_DecDeleteBuffer : destroy an undecoded buffer
 *****************************************************************************/
void CAudioOutput::DecDeleteBuffer( aout_input_t * p_input, aout_buffer_t * p_buffer )
{
    aout_BufferFree( p_buffer );
}

/*****************************************************************************
 * aout_DecPlay : filter & mix the decoded buffer
 * called from CDecoder::DecoderDecode
 *****************************************************************************/
int CAudioOutput::DecPlay( aout_input_t * p_input, aout_buffer_t * p_buffer )
{
    if ( p_buffer->start_date == 0 )
    {
        _RPT0(_CRT_WARN, "non-dated buffer received\n" );
        aout_BufferFree( p_buffer );
        return -1;
    }

	//_RPT2(_CRT_WARN, "DecPlay-start_date:%I64d, desync:%d\n", p_buffer->start_date, p_input->i_desync);
    /* Apply the desynchronisation requested by the user */
    p_buffer->start_date += p_input->i_desync;
    p_buffer->end_date += p_input->i_desync;

    if ( p_buffer->start_date > mdate() + p_input->i_pts_delay +
         AOUT_MAX_ADVANCE_TIME )
    {
        _RPT1(_CRT_WARN, "received buffer in the future (%I64d)\n",
                  p_buffer->start_date - mdate());
        aout_BufferFree( p_buffer );
        return -1;
    }

    p_buffer->end_date = p_buffer->start_date
                            + (mtime_t)(p_buffer->i_nb_samples * 1000000)
                                / p_input->input.i_rate;

    tme_mutex_lock( &p_input->lock );

    if ( p_input->b_error )
    {
        tme_mutex_unlock( &p_input->lock );
        aout_BufferFree( p_buffer );
        return -1;
    }

    if ( p_input->b_changed )
    {
        /* Maybe the allocation size has changed. Re-allocate a buffer. */
        aout_buffer_t * p_new_buffer;
        mtime_t duration = (1000000 * (mtime_t)p_buffer->i_nb_samples)
                            / p_input->input.i_rate;

        aout_BufferAlloc( &p_input->input_alloc, duration, NULL, p_new_buffer );
        memcpy( p_new_buffer->p_buffer, p_buffer->p_buffer,
                                  p_buffer->i_nb_bytes );
        p_new_buffer->i_nb_samples = p_buffer->i_nb_samples;
        p_new_buffer->i_nb_bytes = p_buffer->i_nb_bytes;
        p_new_buffer->start_date = p_buffer->start_date;
        p_new_buffer->end_date = p_buffer->end_date;
        aout_BufferFree( p_buffer );
        p_buffer = p_new_buffer;
        p_input->b_changed = false;
    }

    /* If the buffer is too early, wait a while. */
    mwait( p_buffer->start_date - AOUT_MAX_PREPARE_TIME );

    //_RPT0(_CRT_WARN, "calling aout_InputPlay\n");
    if ( aout_InputPlay( this, p_input, p_buffer ) == -1 )
    {
        tme_mutex_unlock( &p_input->lock );
        return -1;
    }

    tme_mutex_unlock( &p_input->lock );

    /* Run the mixer if it is able to run. */
    tme_mutex_lock( &m_mixerLock );
    MixerRun();
    tme_mutex_unlock( &m_mixerLock );

    //_RPT0(_CRT_WARN, "DecPlay end\n");
   return 0;
}

int CAudioOutput::MixerNew()
{
	if (Float32MixerNew())
		return TrivialMixerNew();

	return 0;
}

int CAudioOutput::AudioOn(bool on)
{
	if (on)
		VolumeSet(m_iVolume);
	else 
		VolumeSet(AOUT_VOLUME_MIN);

	return 0;
}

int CAudioOutput::TrivialMixerNew()
{
    if ( m_mixer.mixer.i_format != TME_FOURCC('f','l','3','2')
          && m_mixer.mixer.i_format != TME_FOURCC('f','i','3','2') )
    {
        return -1;
    }

	m_mixer.b_error = false;
	m_mixer.pf_do_work = DoTrivialMixer;
    return 0;
}

int CAudioOutput::Float32MixerNew()
{
    if ( m_mixer.mixer.i_format != TME_FOURCC('f','l','3','2'))
    {
        return -1;
    }

    if ( m_iNbInputs == 1 && m_mixer.f_multiplier == 1.0 )
    {
        /* Tell the trivial mixer to go for it. */
		/* Maybe should call TrivialMixerNew() here */
        return -1;
    }

	m_mixer.b_error = false;
	m_mixer.pf_do_work = DoFloat32Mixer;
    return 0;
}

void CAudioOutput::MixerDelete()
{
    if ( m_mixer.b_error) return;

	m_mixer.b_error = true;
}

/*****************************************************************************
 * MixBuffer: try to prepare one output buffer
 *****************************************************************************
 * Please note that you must hold the mixer lock.
 *****************************************************************************/
int CAudioOutput::MixBuffer()
{
    int             i, i_first_input = 0;
    aout_buffer_t * p_output_buffer;
    mtime_t start_date, end_date;
    audio_date_t exact_start_date;

	//_RPT1(_CRT_WARN, "MixBuffer(), m_iNbInputs:%d\n",m_iNbInputs);

	if ( m_mixer.b_error )
    {
        /* Free all incoming buffers. */
        tme_mutex_lock( &m_inputFifosLock );
        for ( i = 0; i < m_iNbInputs; i++ )
        {
            aout_input_t * p_input = m_ppInputs[i];
            aout_buffer_t * p_buffer = p_input->fifo.p_first;
            if ( p_input->b_error ) continue;
            while ( p_buffer != NULL )
            {
                aout_buffer_t * p_next = p_buffer->p_next;
                aout_BufferFree( p_buffer );
                p_buffer = p_next;
            }
        }
        tme_mutex_unlock( &m_inputFifosLock );
        return -1;
    }


    tme_mutex_lock( &m_outputFifoLock );
    tme_mutex_lock( &m_inputFifosLock );

    /* Retrieve the date of the next buffer. */
	memcpy( &exact_start_date, &m_pOutput->m_fifo.end_date,
            sizeof(audio_date_t) );
    start_date = aout_DateGet( &exact_start_date );

    if ( start_date != 0 && start_date < mdate() )
    {
        /* The output is _very_ late. This can only happen if the user
         * pauses the stream (or if the decoder is buggy, which cannot
         * happen :). */
        _RPT1(_CRT_WARN, "output PTS is out of range (%I64d), clearing out\n",
                  mdate() - start_date );
        aout_FifoSet( this, &m_pOutput->m_fifo, 0 );
        aout_DateSet( &exact_start_date, 0 );
        start_date = 0;
    }

    tme_mutex_unlock( &m_outputFifoLock );

    /* See if we have enough data to prepare a new buffer for the audio
     * output. First : start date. */
    if ( !start_date )
    {
        /* Find the latest start date available. */
        for ( i = 0; i < m_iNbInputs; i++ )
        {
            aout_input_t * p_input = m_ppInputs[i];
            aout_fifo_t * p_fifo = &p_input->fifo;
            aout_buffer_t * p_buffer;

            if ( p_input->b_error ) continue;

            p_buffer = p_fifo->p_first;
            while ( p_buffer != NULL && p_buffer->start_date < mdate() )
            {
                _RPT1(_CRT_WARN, "input PTS is out of range (%I64d), trashing\n", mdate() - p_buffer->start_date );
                p_buffer = aout_FifoPop( this, p_fifo );
                aout_BufferFree( p_buffer );
                p_buffer = p_fifo->p_first;
                p_input->p_first_byte_to_mix = NULL;
            }

            if ( p_buffer == NULL )
            {
                break;
            }

            if ( !start_date || start_date < p_buffer->start_date )
            {
                aout_DateSet( &exact_start_date, p_buffer->start_date );
                start_date = p_buffer->start_date;
            }
        }

        if ( i < m_iNbInputs )
        {
            /* Interrupted before the end... We can't run. */
            tme_mutex_unlock( &m_inputFifosLock );
            return -1;
        }
    }
	aout_DateIncrement( &exact_start_date, m_pOutput->m_iNbSamples );
    end_date = aout_DateGet( &exact_start_date );

    /* Check that start_date and end_date are available for all input
     * streams. */
    for ( i = 0; i < m_iNbInputs; i++ )
    {
        aout_input_t * p_input = m_ppInputs[i];
        aout_fifo_t * p_fifo = &p_input->fifo;
        aout_buffer_t * p_buffer;
        mtime_t prev_date;
        bool b_drop_buffers;

        if ( p_input->b_error )
        {
            if ( i_first_input == i ) i_first_input++;
            continue;
        }

        p_buffer = p_fifo->p_first;
        if ( p_buffer == NULL )
        {
            break;
        }

        /* Check for the continuity of start_date */
        while ( p_buffer != NULL && p_buffer->end_date < start_date - 1 )
        {
            /* We authorize a +-1 because rounding errors get compensated
             * regularly. */
            aout_buffer_t * p_next = p_buffer->p_next;
            _RPT1(_CRT_WARN, "the mixer got a packet in the past (%I64d)\n",
                      start_date - p_buffer->end_date );
            aout_BufferFree( p_buffer );
            p_fifo->p_first = p_buffer = p_next;
            p_input->p_first_byte_to_mix = NULL;
        }

        if ( p_buffer == NULL )
        {
            p_fifo->pp_last = &p_fifo->p_first;
            break;
        }

        /* Check that we have enough samples. */
        for ( ; ; )
        {
            p_buffer = p_fifo->p_first;
            if ( p_buffer == NULL ) break;
            if ( p_buffer->end_date >= end_date ) break;

            /* Check that all buffers are contiguous. */
            prev_date = p_fifo->p_first->end_date;
            p_buffer = p_buffer->p_next;
            b_drop_buffers = 0;
            for ( ; p_buffer != NULL; p_buffer = p_buffer->p_next )
            {
                if ( prev_date != p_buffer->start_date )
                {
                    _RPT1(_CRT_WARN,
                              "buffer hole, dropping packets (%I64d)\n",
                              p_buffer->start_date - prev_date );
                    b_drop_buffers = 1;
                    break;
                }
                if ( p_buffer->end_date >= end_date ) break;
                prev_date = p_buffer->end_date;
            }
            if ( b_drop_buffers )
            {
                aout_buffer_t * p_deleted = p_fifo->p_first;
                while ( p_deleted != NULL && p_deleted != p_buffer )
                {
                    aout_buffer_t * p_next = p_deleted->p_next;
                    aout_BufferFree( p_deleted );
                    p_deleted = p_next;
                }
                p_fifo->p_first = p_deleted; /* == p_buffer */
            }
            else break;
        }
        if ( p_buffer == NULL ) break;

        p_buffer = p_fifo->p_first;
        if ( !AOUT_FMT_NON_LINEAR( &m_mixer.mixer ) )
        {
            /* Additionally check that p_first_byte_to_mix is well
             * located. */
            mtime_t i_nb_bytes = (start_date - p_buffer->start_date)
                            * m_mixer.mixer.i_bytes_per_frame
                            * m_mixer.mixer.i_rate
                            / m_mixer.mixer.i_frame_length
                            / 1000000;
            ptrdiff_t mixer_nb_bytes;

            if ( p_input->p_first_byte_to_mix == NULL )
            {
                p_input->p_first_byte_to_mix = p_buffer->p_buffer;
            }
            mixer_nb_bytes = p_input->p_first_byte_to_mix - p_buffer->p_buffer;

            if ( !((i_nb_bytes + m_mixer.mixer.i_bytes_per_frame
                     > mixer_nb_bytes) &&
                   (i_nb_bytes < m_mixer.mixer.i_bytes_per_frame
                     + mixer_nb_bytes)) )
            {
                _RPT1(_CRT_WARN, "mixer start isn't output start (%I64d)\n",
                          i_nb_bytes - mixer_nb_bytes );

                /* Round to the nearest multiple */
                i_nb_bytes /= m_mixer.mixer.i_bytes_per_frame;
                i_nb_bytes *= m_mixer.mixer.i_bytes_per_frame;
                if( i_nb_bytes < 0 )
                {
                    /* Is it really the best way to do it ? */
                    aout_FifoSet( this, &m_pOutput->m_fifo, 0 );
                    aout_DateSet( &exact_start_date, 0 );
                    break;
                }

                p_input->p_first_byte_to_mix = p_buffer->p_buffer + i_nb_bytes;
            }
        }
    }

    if ( i < m_iNbInputs || i_first_input == m_iNbInputs )
    {
        /* Interrupted before the end... We can't run. */
        tme_mutex_unlock( &m_inputFifosLock );
        return -1;
    }

    /* Run the mixer. */
    aout_BufferAlloc( &m_mixer.output_alloc,
		((UINT64)m_pOutput->m_iNbSamples * 1000000)
                        / m_pOutput->m_output.i_rate,
                      /* This is a bit kludgy, but is actually only used
                       * for the S/PDIF dummy mixer : */
                      m_ppInputs[i_first_input]->fifo.p_first,
                      p_output_buffer );
    if ( p_output_buffer == NULL )
    {
        _RPT0(_CRT_WARN, "out of memory\n" );
        tme_mutex_unlock( &m_inputFifosLock );
        return -1;
    }
    /* This is again a bit kludgy - for the S/PDIF mixer. */
    if ( m_mixer.output_alloc.i_alloc_type != AOUT_ALLOC_NONE )
    {
        p_output_buffer->i_nb_samples = m_pOutput->m_iNbSamples;
        p_output_buffer->i_nb_bytes = m_pOutput->m_iNbSamples
                              * m_mixer.mixer.i_bytes_per_frame
                              / m_mixer.mixer.i_frame_length;
    }
    p_output_buffer->start_date = start_date;
    p_output_buffer->end_date = end_date;

	m_mixer.pf_do_work(this, p_output_buffer );

    tme_mutex_unlock( &m_inputFifosLock );

    m_pOutput->OutputPlay( p_output_buffer );

    return 0;
}

/*****************************************************************************
 * aout_MixerRun: entry point for the mixer & post-filters processing
 *****************************************************************************
 * Please note that you must hold the mixer lock.
 *****************************************************************************/
void CAudioOutput::MixerRun()
{
    while( MixBuffer() != -1 );
}

/*
 * Formats management (internal and external)
 */

/*****************************************************************************
 * aout_FormatNbChannels : return the number of channels
 *****************************************************************************/
unsigned int aout_FormatNbChannels( const audio_sample_format_t * p_format )
{
    static const UINT32 pi_channels[] =
        { AOUT_CHAN_CENTER, AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT,
          AOUT_CHAN_REARCENTER, AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT,
          AOUT_CHAN_MIDDLELEFT, AOUT_CHAN_MIDDLERIGHT, AOUT_CHAN_LFE };
    unsigned int i_nb = 0, i;

    for ( i = 0; i < sizeof(pi_channels)/sizeof(UINT32); i++ )
    {
        if ( p_format->i_physical_channels & pi_channels[i] ) i_nb++;
    }

    return i_nb;
}

/*****************************************************************************
 * aout_FormatPrepare : compute the number of bytes per frame & frame length
 *****************************************************************************/
void aout_FormatPrepare( audio_sample_format_t * p_format )
{
    int i_result;

    switch ( p_format->i_format )
    {
    case TME_FOURCC('u','8',' ',' '):
    case TME_FOURCC('s','8',' ',' '):
        i_result = 1;
        break;

    case TME_FOURCC('u','1','6','l'):
    case TME_FOURCC('s','1','6','l'):
    case TME_FOURCC('u','1','6','b'):
    case TME_FOURCC('s','1','6','b'):
        i_result = 2;
        break;

    case TME_FOURCC('u','2','4','l'):
    case TME_FOURCC('s','2','4','l'):
    case TME_FOURCC('u','2','4','b'):
    case TME_FOURCC('s','2','4','b'):
        i_result = 3;
        break;

    case TME_FOURCC('f','l','3','2'):
    case TME_FOURCC('f','i','3','2'):
        i_result = 4;
        break;

    case TME_FOURCC('s','p','d','i'):
    case TME_FOURCC('s','p','d','b'): /* Big endian spdif output */
    case TME_FOURCC('a','5','2',' '):
    case TME_FOURCC('d','t','s',' '):
    case TME_FOURCC('m','p','g','a'):
    case TME_FOURCC('m','p','g','3'):
    default:
        /* For these formats the caller has to indicate the parameters
         * by hand. */
        return;
    }

    p_format->i_bytes_per_frame = i_result * aout_FormatNbChannels( p_format );
    p_format->i_frame_length = 1;
}

/*****************************************************************************
 * aout_FormatPrintChannels : print a channel in a human-readable form
 *****************************************************************************/
LPCTSTR aout_FormatPrintChannels( const audio_sample_format_t * p_format )
{
    switch ( p_format->i_physical_channels & AOUT_CHAN_PHYSMASK )
    {
    case AOUT_CHAN_CENTER:
        if ( (p_format->i_original_channels & AOUT_CHAN_CENTER)
              || (p_format->i_original_channels
                   & (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT)) )
            return _T("Mono");
        else if ( p_format->i_original_channels & AOUT_CHAN_LEFT )
            return _T("Left");
        return _T("Right");
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT:
        if ( p_format->i_original_channels & AOUT_CHAN_REVERSESTEREO )
        {
            if ( p_format->i_original_channels & AOUT_CHAN_DOLBYSTEREO )
                return _T("Dolby/Reverse");
            return _T("Stereo/Reverse");
        }
        else
        {
            if ( p_format->i_original_channels & AOUT_CHAN_DOLBYSTEREO )
                return _T("Dolby");
            else if ( p_format->i_original_channels & AOUT_CHAN_DUALMONO )
                return _T("Dual-mono");
            else if ( p_format->i_original_channels == AOUT_CHAN_CENTER )
                return _T("Stereo/Mono");
            else if ( !(p_format->i_original_channels & AOUT_CHAN_RIGHT) )
                return _T("Stereo/Left");
            else if ( !(p_format->i_original_channels & AOUT_CHAN_LEFT) )
                return _T("Stereo/Right");
            return _T("Stereo");
        }
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER:
        return _T("3F");
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARCENTER:
        return _T("2F1R");
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_REARCENTER:
        return _T("3F1R");
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
          | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT:
        return _T("2F2R");
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT:
        return _T("3F2R");

    case AOUT_CHAN_CENTER | AOUT_CHAN_LFE:
        if ( (p_format->i_original_channels & AOUT_CHAN_CENTER)
              || (p_format->i_original_channels
                   & (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT)) )
            return _T("Mono/LFE");
        else if ( p_format->i_original_channels & AOUT_CHAN_LEFT )
            return _T("Left/LFE");
        return _T("Right/LFE");
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_LFE:
        if ( p_format->i_original_channels & AOUT_CHAN_DOLBYSTEREO )
            return _T("Dolby/LFE");
        else if ( p_format->i_original_channels & AOUT_CHAN_DUALMONO )
            return _T("Dual-mono/LFE");
        else if ( p_format->i_original_channels == AOUT_CHAN_CENTER )
            return _T("Mono/LFE");
        else if ( !(p_format->i_original_channels & AOUT_CHAN_RIGHT) )
            return _T("Stereo/Left/LFE");
        else if ( !(p_format->i_original_channels & AOUT_CHAN_LEFT) )
            return _T("Stereo/Right/LFE");
         return _T("Stereo/LFE");
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER | AOUT_CHAN_LFE:
        return _T("3F/LFE");
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARCENTER
          | AOUT_CHAN_LFE:
        return _T("2F1R/LFE");
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_REARCENTER | AOUT_CHAN_LFE:
        return _T("3F1R/LFE");
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
          | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE:
        return _T("2F2R/LFE");
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE:
        return _T("3F2R/LFE");
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_MIDDLELEFT
          | AOUT_CHAN_MIDDLERIGHT:
        return _T("3F2M2R");
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_MIDDLELEFT
          | AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_LFE:
        return _T("3F2M2R/LFE");
    }

    return _T("ERROR");
}

/*****************************************************************************
 * aout_FormatPrint : print a format in a human-readable form
 *****************************************************************************/
void aout_FormatPrint( CAudioOutput * p_aout, const TCHAR * psz_text,
                       const audio_sample_format_t * p_format )
{
    TRACE(_T("%s '%x' %d Hz %s frame=%d samples/%d bytes\n"), psz_text,
             p_format->i_format, p_format->i_rate,
             aout_FormatPrintChannels( p_format ),
             p_format->i_frame_length, p_format->i_bytes_per_frame );
}

/*****************************************************************************
 * aout_FormatsPrint : print two formats in a human-readable form
 *****************************************************************************/
void aout_FormatsPrint( CAudioOutput * p_aout, const TCHAR * psz_text,
                        const audio_sample_format_t * p_format1,
                        const audio_sample_format_t * p_format2 )
{
    TRACE(_T("%s '%x'->'%x' %d Hz->%d Hz %s->%s\n"),
             psz_text,
             p_format1->i_format, p_format2->i_format,
             p_format1->i_rate, p_format2->i_rate,
             aout_FormatPrintChannels( p_format1 ),
             aout_FormatPrintChannels( p_format2 ) );
}


/*
 * FIFO management (internal) - please understand that solving race conditions
 * is _your_ job, ie. in the audio output you should own the mixer lock
 * before calling any of these functions.
 */

/*****************************************************************************
 * aout_FifoInit : initialize the members of a FIFO
 *****************************************************************************/
void aout_FifoInit( CAudioOutput * p_aout, aout_fifo_t * p_fifo,
                    UINT32 i_rate )
{
    if( i_rate == 0 )
    {
        _RPT0(_CRT_WARN, "initialising fifo with zero divider\n" );
    }

    p_fifo->p_first = NULL;
    p_fifo->pp_last = &p_fifo->p_first;
    aout_DateInit( &p_fifo->end_date, i_rate );
}

/*****************************************************************************
 * aout_FifoPush : push a packet into the FIFO
 *****************************************************************************/
void aout_FifoPush( CAudioOutput * p_aout, aout_fifo_t * p_fifo,
                    aout_buffer_t * p_buffer )
{
    *p_fifo->pp_last = p_buffer;
    p_fifo->pp_last = &p_buffer->p_next;
    *p_fifo->pp_last = NULL;
    /* Enforce the continuity of the stream. */
    if ( aout_DateGet( &p_fifo->end_date ) )
    {
        p_buffer->start_date = aout_DateGet( &p_fifo->end_date );
        p_buffer->end_date = aout_DateIncrement( &p_fifo->end_date,
                                                 p_buffer->i_nb_samples );
    }
    else
    {
        aout_DateSet( &p_fifo->end_date, p_buffer->end_date );
    }
}

/*****************************************************************************
 * aout_FifoSet : set end_date and trash all buffers (because they aren't
 * properly dated)
 *****************************************************************************/
void aout_FifoSet( CAudioOutput * p_aout, aout_fifo_t * p_fifo,
                   mtime_t date )
{
    aout_buffer_t * p_buffer;

    aout_DateSet( &p_fifo->end_date, date );
    p_buffer = p_fifo->p_first;
    while ( p_buffer != NULL )
    {
        aout_buffer_t * p_next = p_buffer->p_next;
        aout_BufferFree( p_buffer );
        p_buffer = p_next;
    }
    p_fifo->p_first = NULL;
    p_fifo->pp_last = &p_fifo->p_first;
}

/*****************************************************************************
 * aout_FifoMoveDates : Move forwards or backwards all dates in the FIFO
 *****************************************************************************/
void aout_FifoMoveDates( CAudioOutput * p_aout, aout_fifo_t * p_fifo,
                         mtime_t difference )
{
    aout_buffer_t * p_buffer;

    aout_DateMove( &p_fifo->end_date, difference );
    p_buffer = p_fifo->p_first;
    while ( p_buffer != NULL )
    {
        p_buffer->start_date += difference;
        p_buffer->end_date += difference;
        p_buffer = p_buffer->p_next;
    }
}

/*****************************************************************************
 * aout_FifoNextStart : return the current end_date
 *****************************************************************************/
mtime_t aout_FifoNextStart( CAudioOutput * p_aout, aout_fifo_t * p_fifo )
{
    return aout_DateGet( &p_fifo->end_date );
}

/*****************************************************************************
 * aout_FifoFirstDate : return the playing date of the first buffer in the
 * FIFO
 *****************************************************************************/
mtime_t aout_FifoFirstDate( CAudioOutput * p_aout, aout_fifo_t * p_fifo )
{
    return p_fifo->p_first ?  p_fifo->p_first->start_date : 0;
}

/*****************************************************************************
 * aout_FifoPop : get the next buffer out of the FIFO
 *****************************************************************************/
aout_buffer_t * aout_FifoPop( CAudioOutput * p_aout, aout_fifo_t * p_fifo )
{
    aout_buffer_t * p_buffer;

    p_buffer = p_fifo->p_first;
    if ( p_buffer == NULL ) return NULL;
    p_fifo->p_first = p_buffer->p_next;
    if ( p_fifo->p_first == NULL )
    {
        p_fifo->pp_last = &p_fifo->p_first;
    }

    return p_buffer;
}

/*****************************************************************************
 * aout_FifoDestroy : destroy a FIFO and its buffers
 *****************************************************************************/
void aout_FifoDestroy( CAudioOutput * p_aout, aout_fifo_t * p_fifo )
{
    aout_buffer_t * p_buffer;

    p_buffer = p_fifo->p_first;
    while ( p_buffer != NULL )
    {
        aout_buffer_t * p_next = p_buffer->p_next;
        aout_BufferFree( p_buffer );
        p_buffer = p_next;
    }

    p_fifo->p_first = NULL;
    p_fifo->pp_last = &p_fifo->p_first;
}


/*
 * Date management (internal and external)
 */

/*****************************************************************************
 * aout_DateInit : set the divider of an audio_date_t
 *****************************************************************************/
void aout_DateInit( audio_date_t * p_date, UINT32 i_divider )
{
    p_date->date = 0;
    p_date->i_divider = i_divider;
    p_date->i_remainder = 0;
}

/*****************************************************************************
 * aout_DateSet : set the date of an audio_date_t
 *****************************************************************************/
void aout_DateSet( audio_date_t * p_date, mtime_t new_date )
{
    p_date->date = new_date;
    p_date->i_remainder = 0;
}

/*****************************************************************************
 * aout_DateMove : move forwards or backwards the date of an audio_date_t
 *****************************************************************************/
void aout_DateMove( audio_date_t * p_date, mtime_t difference )
{
    p_date->date += difference;
}

/*****************************************************************************
 * aout_DateGet : get the date of an audio_date_t
 *****************************************************************************/
mtime_t aout_DateGet( const audio_date_t * p_date )
{
    return p_date->date;
}

/*****************************************************************************
 * aout_DateIncrement : increment the date and return the result, taking
 * into account rounding errors
 *****************************************************************************/
mtime_t aout_DateIncrement( audio_date_t * p_date, UINT32 i_nb_samples )
{
    mtime_t i_dividend = (mtime_t)i_nb_samples * 1000000;
    p_date->date += i_dividend / p_date->i_divider;
    p_date->i_remainder += (int)(i_dividend % p_date->i_divider);
    if ( p_date->i_remainder >= p_date->i_divider )
    {
        /* This is Bresenham algorithm. */
        p_date->date++;
        p_date->i_remainder -= p_date->i_divider;
    }
    return p_date->date;
}

/*****************************************************************************
 * aout_CheckChannelReorder : Check if we need to do some channel re-ordering
 *****************************************************************************/
int aout_CheckChannelReorder( const UINT32 *pi_chan_order_in,
                              const UINT32 *pi_chan_order_out,
                              UINT32 i_channel_mask,
                              int i_channels, int *pi_chan_table )
{
    bool b_chan_reorder = false;
    int i, j, k, l;

    if( i_channels > AOUT_CHAN_MAX ) return false;

    for( i = 0, j = 0; pi_chan_order_in[i]; i++ )
    {
        if( !(i_channel_mask & pi_chan_order_in[i]) ) continue;

        for( k = 0, l = 0; pi_chan_order_in[i] != pi_chan_order_out[k]; k++ )
        {
            if( i_channel_mask & pi_chan_order_out[k] ) l++;
        }

        pi_chan_table[j++] = l;
    }

    for( i = 0; i < i_channels; i++ )
    {
        if( pi_chan_table[i] != i ) b_chan_reorder = true;
    }

    return b_chan_reorder;
}

/*****************************************************************************
 * aout_ChannelReorder :
 *****************************************************************************/
void aout_ChannelReorder( UINT8 *p_buf, int i_buffer,
                          int i_channels, const int *pi_chan_table,
                          int i_bits_per_sample )
{
    UINT8 p_tmp[AOUT_CHAN_MAX * 4];
    int i, j;

    if( i_bits_per_sample == 8 )
    {
        for( i = 0; i < i_buffer / i_channels; i++ )
        {
            for( j = 0; j < i_channels; j++ )
            {
                p_tmp[pi_chan_table[j]] = p_buf[j];
            }

            memcpy( p_buf, p_tmp, i_channels );
            p_buf += i_channels;
        }
    }
    else if( i_bits_per_sample == 16 )
    {
        for( i = 0; i < i_buffer / i_channels / 2; i++ )
        {
            for( j = 0; j < i_channels; j++ )
            {
                p_tmp[2 * pi_chan_table[j]]     = p_buf[2 * j];
                p_tmp[2 * pi_chan_table[j] + 1] = p_buf[2 * j + 1];
            }

            memcpy( p_buf, p_tmp, 2 * i_channels );
            p_buf += 2 * i_channels;
        }
    }
    else if( i_bits_per_sample == 24 )
    {
        for( i = 0; i < i_buffer / i_channels / 3; i++ )
        {
            for( j = 0; j < i_channels; j++ )
            {
                p_tmp[3 * pi_chan_table[j]]     = p_buf[3 * j];
                p_tmp[3 * pi_chan_table[j] + 1] = p_buf[3 * j + 1];
                p_tmp[3 * pi_chan_table[j] + 2] = p_buf[3 * j + 2];
            }

            memcpy( p_buf, p_tmp, 3 * i_channels );
            p_buf += 3 * i_channels;
        }
    }
    else if( i_bits_per_sample == 32 )
    {
        for( i = 0; i < i_buffer / i_channels / 4; i++ )
        {
            for( j = 0; j < i_channels; j++ )
            {
                p_tmp[4 * pi_chan_table[j]]     = p_buf[4 * j];
                p_tmp[4 * pi_chan_table[j] + 1] = p_buf[4 * j + 1];
                p_tmp[4 * pi_chan_table[j] + 2] = p_buf[4 * j + 2];
                p_tmp[4 * pi_chan_table[j] + 3] = p_buf[4 * j + 3];
            }

            memcpy( p_buf, p_tmp, 4 * i_channels );
            p_buf += 4 * i_channels;
        }
    }
}

/*****************************************************************************
 * DoWork: mix a new output buffer
 *****************************************************************************/
static void DoTrivialMixer(CAudioOutput *pAout, aout_buffer_t * p_buffer )
{
    int i = 0;
    aout_input_t * p_input = pAout->m_ppInputs[i];
    int i_nb_channels = aout_FormatNbChannels( &pAout->m_mixer.mixer );
    int i_nb_bytes = p_buffer->i_nb_samples * sizeof(INT32)
                      * i_nb_channels;
    BYTE * p_in;
    BYTE * p_out;

    while ( p_input->b_error )
    {
        p_input = pAout->m_ppInputs[++i];
        /* This can't crash because if no input has b_error == 0, the
         * audio mixer cannot run and we can't be here. */
    }
    p_in = p_input->p_first_byte_to_mix;
    p_out = p_buffer->p_buffer;

    for ( ; ; )
    {
        ptrdiff_t i_available_bytes = (p_input->fifo.p_first->p_buffer
                                        - p_in)
                                        + p_input->fifo.p_first->i_nb_samples
                                           * sizeof(INT32)
                                           * i_nb_channels;

        if ( i_available_bytes < i_nb_bytes )
        {
            aout_buffer_t * p_old_buffer;

            if ( i_available_bytes > 0 )
                memcpy( p_out, p_in, i_available_bytes );
            i_nb_bytes -= (int)i_available_bytes;
            p_out += i_available_bytes;

            /* Next buffer */
            p_old_buffer = aout_FifoPop( pAout, &p_input->fifo );
            aout_BufferFree( p_old_buffer );
            if ( p_input->fifo.p_first == NULL )
            {
                _RPT0(_CRT_WARN, "internal amix error" );
                return;
            }
            p_in = p_input->fifo.p_first->p_buffer;
        }
        else
        {
            if ( i_nb_bytes > 0 )
                memcpy( p_out, p_in, i_nb_bytes );
            p_input->p_first_byte_to_mix = p_in + i_nb_bytes;
            break;
        }
    }

    /* Empty other FIFOs to avoid a memory leak. */
    for ( i++; i < pAout->m_iNbInputs; i++ )
    {
        aout_fifo_t * p_fifo;
        aout_buffer_t * p_deleted;

        p_input = pAout->m_ppInputs[i];
        if ( p_input->b_error ) continue;
        p_fifo = &p_input->fifo;
        p_deleted = p_fifo->p_first;
        while ( p_deleted != NULL )
        {
            aout_buffer_t * p_next = p_deleted->p_next;
            aout_BufferFree( p_deleted );
            p_deleted = p_next;
        }
        p_fifo->p_first = NULL;
        p_fifo->pp_last = &p_fifo->p_first;
    }
}

/*****************************************************************************
 * ScaleWords: prepare input words for averaging
 *****************************************************************************/
static void ScaleWords( float * p_out, const float * p_in, size_t i_nb_words,
                        int i_nb_inputs, float f_multiplier )
{
    int i;
    f_multiplier /= i_nb_inputs;

    for ( i = (int)i_nb_words; i--; )
    {
        *p_out++ = *p_in++ * f_multiplier;
    }
}

/*****************************************************************************
 * MeanWords: average input words
 *****************************************************************************/
static void MeanWords( float * p_out, const float * p_in, size_t i_nb_words,
                       int i_nb_inputs, float f_multiplier )
{
    int i;
    f_multiplier /= i_nb_inputs;

    for ( i = (int)i_nb_words; i--; )
    {
        *p_out++ += *p_in++ * f_multiplier;
    }
}
/*****************************************************************************
 * DoFloat32Work: mix a new output buffer
 *****************************************************************************
 * Terminology : in this function a word designates a single float32, eg.
 * a stereo sample is consituted of two words.
 *****************************************************************************/
static void DoFloat32Mixer( CAudioOutput *pAout, aout_buffer_t * p_buffer )
{
	int i_nb_inputs = pAout->m_iNbInputs;
    float f_multiplier = pAout->m_mixer.f_multiplier;
    int i_input;
    int i_nb_channels = aout_FormatNbChannels( &pAout->m_mixer.mixer );

    for ( i_input = 0; i_input < i_nb_inputs; i_input++ )
    {
        int i_nb_words = p_buffer->i_nb_samples * i_nb_channels;
		aout_input_t * p_input = pAout->m_ppInputs[i_input];
        float * p_out = (float *)p_buffer->p_buffer;
        float * p_in = (float *)p_input->p_first_byte_to_mix;

        if ( p_input->b_error ) continue;

        for ( ; ; )
        {
            ptrdiff_t i_available_words = (
                 (float *)p_input->fifo.p_first->p_buffer - p_in)
                                   + p_input->fifo.p_first->i_nb_samples
                                   * i_nb_channels;

            if ( i_available_words < i_nb_words )
            {
                aout_buffer_t * p_old_buffer;

                if ( i_available_words > 0 )
                {
                    if ( !i_input )
                    {
                        ScaleWords( p_out, p_in, i_available_words,
                                    i_nb_inputs, f_multiplier );
                    }
                    else
                    {
                        MeanWords( p_out, p_in, i_available_words,
                                   i_nb_inputs, f_multiplier );
                    }
                }

                i_nb_words -= (int)i_available_words;
                p_out += i_available_words;

                /* Next buffer */
                p_old_buffer = aout_FifoPop( pAout, &p_input->fifo );
                aout_BufferFree( p_old_buffer );
                if ( p_input->fifo.p_first == NULL )
                {
	                _RPT0(_CRT_WARN, "internal amix error" );
                    return;
                }
                p_in = (float *)p_input->fifo.p_first->p_buffer;
            }
            else
            {
                if ( i_nb_words > 0 )
                {
                    if ( !i_input )
                    {
                        ScaleWords( p_out, p_in, i_nb_words, i_nb_inputs,
                                    f_multiplier );
                    }
                    else
                    {
                        MeanWords( p_out, p_in, i_nb_words, i_nb_inputs,
                                   f_multiplier );
                    }
                }
                p_input->p_first_byte_to_mix = (BYTE *)(p_in
                                            + i_nb_words);
                break;
            }
        }
    }
}

int CAudioOutput::VolumeSet(int iVolume)
{
    tme_mutex_lock( &m_mixerLock );
	MixerMultiplierSet((float)iVolume / AOUT_VOLUME_DEFAULT);
	tme_mutex_unlock( &m_mixerLock );

	return 0;
}

/*****************************************************************************
 * MixerMultiplierSet: set m_mixer.f_multiplier
 *****************************************************************************
 * Please note that we assume that you own the mixer lock when entering this
 * function. This function returns -1 on error.
 *****************************************************************************/
int CAudioOutput::MixerMultiplierSet( float f_multiplier )
{
    float f_old = m_mixer.f_multiplier;
    bool b_new_mixer = false;

    if ( !m_mixer.b_error )
    {
        MixerDelete();
        b_new_mixer = true;
    }

    m_mixer.f_multiplier = f_multiplier;

    if ( b_new_mixer && MixerNew() )
    {
        m_mixer.f_multiplier = f_old;
        MixerNew();
        return -1;
    }

    return 0;
}
