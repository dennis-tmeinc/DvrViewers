#include "stdafx.h"
#include <crtdbg.h>
#include "audio_output.h"
#include "aout_internal.h"
#include "trace.h"

static void inputFailure( CAudioOutput *, aout_input_t *, char * );

/*****************************************************************************
 * aout_InputNew : allocate a new input and rework the filter pipeline
 *****************************************************************************/
int aout_InputNew( CAudioOutput * p_aout, aout_input_t * p_input )
{
    audio_sample_format_t chain_input_format;
    audio_sample_format_t chain_output_format;
    char * psz_filters, *psz_visual;
    //int i_visual;

    aout_FormatPrint( p_aout, _T("input"), &p_input->input );

    p_input->i_nb_filters = 0;

    /* Prepare FIFO. */
    aout_FifoInit( p_aout, &p_input->fifo, p_aout->m_mixer.mixer.i_rate );
    p_input->p_first_byte_to_mix = NULL;

    /* Prepare format structure */
    memcpy( &chain_input_format, &p_input->input,
            sizeof(audio_sample_format_t) );
    memcpy( &chain_output_format, &p_aout->m_mixer.mixer,
            sizeof(audio_sample_format_t) );
    chain_output_format.i_rate = p_input->input.i_rate;
    aout_FormatPrepare( &chain_output_format );
    psz_filters = NULL;
    psz_visual = NULL;

	/* complete the filter chain if necessary */
    if ( !AOUT_FMTS_IDENTICAL( &chain_input_format, &chain_output_format ) )
    {
        if ( aout_FiltersCreatePipeline( p_aout, p_input->pp_filters,
                                         &p_input->i_nb_filters,
                                         &chain_input_format,
                                         &chain_output_format ) < 0 )
        {
            inputFailure( p_aout, p_input, "couldn't set an input pipeline" );
            return -1;
        }
    }

    /* Prepare hints for the buffer allocator. */
    p_input->input_alloc.i_alloc_type = AOUT_ALLOC_HEAP;
    p_input->input_alloc.i_bytes_per_sec = -1;

    /* Create resamplers. */
    if ( AOUT_FMT_NON_LINEAR( &p_aout->m_mixer.mixer ) )
    {
        p_input->i_nb_resamplers = 0;
    }
    else
    {
        chain_output_format.i_rate = (max(p_input->input.i_rate,
                                            p_aout->m_mixer.mixer.i_rate)
                                 * (100 + AOUT_MAX_RESAMPLING)) / 100;
        if ( chain_output_format.i_rate == p_aout->m_mixer.mixer.i_rate )
        {
            /* Just in case... */
            chain_output_format.i_rate++;
        }
        p_input->i_nb_resamplers = 0;
        if ( aout_FiltersCreatePipeline( p_aout, p_input->pp_resamplers,
                                         &p_input->i_nb_resamplers,
                                         &chain_output_format,
                                         &p_aout->m_mixer.mixer ) < 0 )
        {
            inputFailure( p_aout, p_input, "couldn't set a resampler pipeline");
            return -1;
        }

        aout_FiltersHintBuffers( p_aout, p_input->pp_resamplers,
                                 p_input->i_nb_resamplers,
                                 &p_input->input_alloc );
        p_input->input_alloc.i_alloc_type = AOUT_ALLOC_HEAP;

        /* Setup the initial rate of the resampler */
        p_input->pp_resamplers[0]->input.i_rate = p_input->input.i_rate;
    }
    p_input->i_resampling_type = AOUT_RESAMPLING_NONE;

    aout_FiltersHintBuffers( p_aout, p_input->pp_filters,
                             p_input->i_nb_filters,
                             &p_input->input_alloc );
    p_input->input_alloc.i_alloc_type = AOUT_ALLOC_HEAP;

    /* i_bytes_per_sec is still == -1 if no filters */
    p_input->input_alloc.i_bytes_per_sec = max(
                                    p_input->input_alloc.i_bytes_per_sec,
                                    (int)(p_input->input.i_bytes_per_frame
                                     * p_input->input.i_rate
                                     / p_input->input.i_frame_length) );

    /* Success */
    p_input->b_error = false;
    p_input->b_restart = false;

    return 0;
}

/****************************************************************************
 * aout_InputDelete : delete an input
 *****************************************************************************
 * This function must be entered with the mixer lock.
 *****************************************************************************/
int aout_InputDelete( CAudioOutput * p_aout, aout_input_t * p_input )
{
    if ( p_input->b_error ) return 0;

    aout_FiltersDestroyPipeline( p_aout, p_input->pp_filters,
                                 p_input->i_nb_filters );
    p_input->i_nb_filters = 0;
    aout_FiltersDestroyPipeline( p_aout, p_input->pp_resamplers,
                                 p_input->i_nb_resamplers );
    p_input->i_nb_resamplers = 0;
    aout_FifoDestroy( p_aout, &p_input->fifo );

    return 0;
}

/*****************************************************************************
 * aout_InputPlay : play a buffer
 *****************************************************************************
 * This function must be entered with the input lock.
 *****************************************************************************/
int aout_InputPlay( CAudioOutput * p_aout, aout_input_t * p_input,
                    aout_buffer_t * p_buffer )
{
    mtime_t start_date;

    if( p_input->b_restart )
    {
        aout_fifo_t fifo, dummy_fifo;
        BYTE      *p_first_byte_to_mix;

        tme_mutex_lock( &p_aout->m_mixerLock );

        /* A little trick to avoid loosing our input fifo */
        aout_FifoInit( p_aout, &dummy_fifo, p_aout->m_mixer.mixer.i_rate );
        p_first_byte_to_mix = p_input->p_first_byte_to_mix;
        fifo = p_input->fifo;
        p_input->fifo = dummy_fifo;
        aout_InputDelete( p_aout, p_input );
        aout_InputNew( p_aout, p_input );
        p_input->p_first_byte_to_mix = p_first_byte_to_mix;
        p_input->fifo = fifo;

        tme_mutex_unlock( &p_aout->m_mixerLock );
    }

    /* We don't care if someone changes the start date behind our back after
     * this. We'll deal with that when pushing the buffer, and compensate
     * with the next incoming buffer. */
    tme_mutex_lock( &p_aout->m_inputFifosLock );
    start_date = aout_FifoNextStart( p_aout, &p_input->fifo );
    tme_mutex_unlock( &p_aout->m_inputFifosLock );

		mtime_t now = mdate();
	//_RPT3(_CRT_WARN, "startdate:%I64d,now:%I64d,bufferdate:%I64d\n", start_date,mdate(), p_buffer->start_date);
    if ( start_date != 0 && start_date < mdate() )
    {
        /* The decoder is _very_ late. This can only happen if the user
         * pauses the stream (or if the decoder is buggy, which cannot
         * happen :). */
		TRACE(_T("ch:%d, computed PTS is out of range (%I64d), clearing out\n"), p_aout->m_channelID, mdate() - start_date );
        tme_mutex_lock( &p_aout->m_inputFifosLock );
        aout_FifoSet( p_aout, &p_input->fifo, 0 );
        p_input->p_first_byte_to_mix = NULL;
        tme_mutex_unlock( &p_aout->m_inputFifosLock );
				if ( p_input->i_resampling_type != AOUT_RESAMPLING_NONE ) {
            TRACE(_T("ch:%d, timing screwed, stopping resampling\n"), p_aout->m_channelID );
				}
        p_input->i_resampling_type = AOUT_RESAMPLING_NONE;
        if ( p_input->i_nb_resamplers != 0 )
        {
            p_input->pp_resamplers[0]->input.i_rate = p_input->input.i_rate;
            p_input->pp_resamplers[0]->b_continuity = false;
        }
        start_date = 0;
    }

		now = mdate();
		/* avoid intermittent mute channel */
		if (p_buffer->start_date < now) {
			mtime_t diff = p_buffer->end_date - p_buffer->start_date;
			p_buffer->start_date = now + AOUT_MIN_PREPARE_TIME;
			p_buffer->end_date = p_buffer->start_date + diff;
		}

	//_RPT3(_CRT_WARN, "start_date:%I64d, diff:%I64d, min:%I64d\n",
    //              p_buffer->start_date, p_buffer->start_date-now, AOUT_MIN_PREPARE_TIME );
    if ( p_buffer->start_date < now + AOUT_MIN_PREPARE_TIME )
    {
        /* The decoder gives us f*cked up PTS. It's its business, but we
         * can't present it anyway, so drop the buffer. */
		TRACE(_T("ch:%d,PTS is out of range (%I64d,%I64d,%I64d,%I64d), dropping buffer\n"), p_aout->m_channelID, now,p_buffer->start_date,p_buffer->start_date-now,start_date );
        aout_BufferFree( p_buffer );
        p_input->i_resampling_type = AOUT_RESAMPLING_NONE;
        if ( p_input->i_nb_resamplers != 0 )
        {
            p_input->pp_resamplers[0]->input.i_rate = p_input->input.i_rate;
            p_input->pp_resamplers[0]->b_continuity = false;
        }
        return 0;
    }

    /* If the audio drift is too big then it's not worth trying to resample
     * the audio. */
    if ( start_date != 0 &&
         ( start_date < p_buffer->start_date - 3 * AOUT_PTS_TOLERANCE ) )
    {
        TRACE(_T("ch:%d,audio drift is too big (%I64d), clearing out\n"),p_aout->m_channelID,
                  start_date - p_buffer->start_date );
        tme_mutex_lock( &p_aout->m_inputFifosLock );
        aout_FifoSet( p_aout, &p_input->fifo, 0 );
        p_input->p_first_byte_to_mix = NULL;
        tme_mutex_unlock( &p_aout->m_inputFifosLock );
        if ( p_input->i_resampling_type != AOUT_RESAMPLING_NONE )
            TRACE(_T("ch:%d,timing screwed, stopping resampling\n"), p_aout->m_channelID );
        p_input->i_resampling_type = AOUT_RESAMPLING_NONE;
        if ( p_input->i_nb_resamplers != 0 )
        {
            p_input->pp_resamplers[0]->input.i_rate = p_input->input.i_rate;
            p_input->pp_resamplers[0]->b_continuity = false;
        }
        start_date = 0;
    }
    else if ( start_date != 0 &&
              ( start_date > p_buffer->start_date + 3 * AOUT_PTS_TOLERANCE ) )
    {
        TRACE(_T("ch:%d,audio drift is too big (%I64d), dropping buffer\n"), p_aout->m_channelID,
                  start_date - p_buffer->start_date );
        aout_BufferFree( p_buffer );
        return 0;
    }

    if ( start_date == 0 ) start_date = p_buffer->start_date;

    /* Run pre-filters. */

    aout_FiltersPlay( p_aout, p_input->pp_filters, p_input->i_nb_filters,
                      &p_buffer );

    /* Run the resampler if needed.
     * We first need to calculate the output rate of this resampler. */
    if ( ( p_input->i_resampling_type == AOUT_RESAMPLING_NONE ) &&
         ( start_date < p_buffer->start_date - AOUT_PTS_TOLERANCE
           || start_date > p_buffer->start_date + AOUT_PTS_TOLERANCE ) &&
         p_input->i_nb_resamplers > 0 )
    {
        /* Can happen in several circumstances :
         * 1. A problem at the input (clock drift)
         * 2. A small pause triggered by the user
         * 3. Some delay in the output stage, causing a loss of lip
         *    synchronization
         * Solution : resample the buffer to avoid a scratch.
         */
        mtime_t drift = p_buffer->start_date - start_date;

        p_input->i_resamp_start_date = mdate();
        p_input->i_resamp_start_drift = (int)drift;

        if ( drift > 0 )
            p_input->i_resampling_type = AOUT_RESAMPLING_DOWN;
        else
            p_input->i_resampling_type = AOUT_RESAMPLING_UP;

        TRACE(_T("ch:%d, buffer is %I64d %s, triggering %ssampling\n"),
                          p_aout->m_channelID, drift > 0 ? drift : -drift,
                          drift > 0 ? _T("in advance") : _T("late"),
                          drift > 0 ? _T("down") : _T("up"));
    }

    if ( p_input->i_resampling_type != AOUT_RESAMPLING_NONE )
    {
        /* Resampling has been triggered previously (because of dates
         * mismatch). We want the resampling to happen progressively so
         * it isn't too audible to the listener. */

        if( p_input->i_resampling_type == AOUT_RESAMPLING_UP )
        {
            p_input->pp_resamplers[0]->input.i_rate += 2; /* Hz */
        }
        else
        {
            p_input->pp_resamplers[0]->input.i_rate -= 2; /* Hz */
        }

        /* Check if everything is back to normal, in which case we can stop the
         * resampling */
        if( p_input->pp_resamplers[0]->input.i_rate ==
              p_input->input.i_rate )
        {
            p_input->i_resampling_type = AOUT_RESAMPLING_NONE;
            TRACE(_T("ch:%d, resampling stopped after %I64i usec (drift: %I64i)\n"),
                      p_aout->m_channelID, mdate() - p_input->i_resamp_start_date,
                      p_buffer->start_date - start_date);
        }
        else if( abs( (int)(p_buffer->start_date - start_date) ) <
                 abs( p_input->i_resamp_start_drift ) / 2 )
        {
            /* if we reduced the drift from half, then it is time to switch
             * back the resampling direction. */
            if( p_input->i_resampling_type == AOUT_RESAMPLING_UP )
                p_input->i_resampling_type = AOUT_RESAMPLING_DOWN;
            else
                p_input->i_resampling_type = AOUT_RESAMPLING_UP;
            p_input->i_resamp_start_drift = 0;
        }
        else if( p_input->i_resamp_start_drift &&
                 ( abs( (int)(p_buffer->start_date - start_date) ) >
                   abs( p_input->i_resamp_start_drift ) * 3 / 2 ) )
        {
            /* If the drift is increasing and not decreasing, than something
             * is bad. We'd better stop the resampling right now. */
            TRACE(_T("ch:%d, timing screwed, stopping resampling\n"), p_aout->m_channelID);
            p_input->i_resampling_type = AOUT_RESAMPLING_NONE;
            p_input->pp_resamplers[0]->input.i_rate = p_input->input.i_rate;
        }
    }

    /* Adding the start date will be managed by aout_FifoPush(). */
    p_buffer->end_date = start_date +
        (p_buffer->end_date - p_buffer->start_date);
    p_buffer->start_date = start_date;

    /* Actually run the resampler now. */
    if ( p_input->i_nb_resamplers > 0 )
    {
        aout_FiltersPlay( p_aout, p_input->pp_resamplers,
                          p_input->i_nb_resamplers,
                          &p_buffer );
    }

    tme_mutex_lock( &p_aout->m_inputFifosLock );
    aout_FifoPush( p_aout, &p_input->fifo, p_buffer );
    tme_mutex_unlock( &p_aout->m_inputFifosLock );

    return 0;
}

/*****************************************************************************
 * static functions
 *****************************************************************************/

static void inputFailure( CAudioOutput * p_aout, aout_input_t * p_input,
                          char * psz_error_message )
{
    /* error message */
    _RPT0(_CRT_WARN, "couldn't set an input pipeline\n" );

    /* clean up */
    aout_FiltersDestroyPipeline( p_aout, p_input->pp_filters,
                                 p_input->i_nb_filters );
    aout_FiltersDestroyPipeline( p_aout, p_input->pp_resamplers,
                                 p_input->i_nb_resamplers );
    aout_FifoDestroy( p_aout, &p_input->fifo );
    /* error flag */
    p_input->b_error = 1;
}
