#ifndef _AOUT_COMMON_H_
#define _AOUT_COMMON_H_

#include "es.h"

class CAudioOutput;
/*****************************************************************************
 * audio_date_t : date incrementation without long-term rounding errors
 *****************************************************************************/
struct audio_date_t
{
    mtime_t  date;
    UINT32	 i_divider;
    UINT32	 i_remainder;
};

/*****************************************************************************
 * aout_alloc_t : allocation of memory in the audio output
 *****************************************************************************/
typedef struct aout_alloc_t
{
    int                     i_alloc_type;
    int                     i_bytes_per_sec;
} aout_alloc_t;

/*****************************************************************************
 * aout_mixer_t : audio output mixer
 *****************************************************************************/
typedef struct aout_mixer_t
{
    audio_format_t   mixer;
    aout_alloc_t            output_alloc;

    struct aout_mixer_sys_t * p_sys;
    void                 (* pf_do_work)( CAudioOutput *,
                                         struct aout_buffer_t * );

    /* If b_error == 1, there is no mixer. */
    bool              b_error;
    /* Multiplier used to raise or lower the volume of the sound in
     * software. Beware, this creates sound distortion and should be avoided
     * as much as possible. This isn't available for non-float32 mixer. */
    float                   f_multiplier;
} aout_mixer_t;

/*****************************************************************************
 * aout_fifo_t : audio output buffer FIFO
 *****************************************************************************/
struct aout_fifo_t
{
    aout_buffer_t *         p_first;
    aout_buffer_t **        pp_last;
    audio_date_t            end_date;
};

#endif