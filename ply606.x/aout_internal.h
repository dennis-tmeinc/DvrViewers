#ifndef _AOUT_INTERNAL_H_
#define _AOUT_INTERNAL_H_

#include "tme_thread.h"
#include "es.h"
#include "tmeviewdef.h"
#include "aout_common.h"

#define HAVE_ALLOCA 1

#define AOUT_ALLOC_NONE     0
#define AOUT_ALLOC_STACK    1
#define AOUT_ALLOC_HEAP     2

#ifdef HAVE_ALLOCA
#   define ALLOCA_TEST( p_alloc, p_new_buffer )                             \
        if ( (p_alloc)->i_alloc_type == AOUT_ALLOC_STACK )                  \
        {                                                                   \
            (p_new_buffer) = (aout_buffer_t *)alloca( i_alloc_size + sizeof(aout_buffer_t) );\
            i_alloc_type = AOUT_ALLOC_STACK;                                \
        }                                                                   \
        else
#else
#   define ALLOCA_TEST( p_alloc, p_new_buffer )
#endif

#define aout_BufferAlloc( p_alloc, i_nb_usec, p_previous_buffer,            \
                          p_new_buffer )                                    \
    if ( (p_alloc)->i_alloc_type == AOUT_ALLOC_NONE )                       \
    {                                                                       \
        (p_new_buffer) = p_previous_buffer;                                 \
    }                                                                       \
    else                                                                    \
    {                                                                       \
        int i_alloc_size, i_alloc_type;                                     \
        i_alloc_size = (int)( (UINT64)(p_alloc)->i_bytes_per_sec          \
                                            * (i_nb_usec) / 1000000 + 1 );  \
        ALLOCA_TEST( p_alloc, p_new_buffer )                                \
        {                                                                   \
            (p_new_buffer) = (aout_buffer_t *)malloc( i_alloc_size + sizeof(aout_buffer_t) );\
            i_alloc_type = AOUT_ALLOC_HEAP;                                 \
        }                                                                   \
        if ( p_new_buffer != NULL )                                         \
        {                                                                   \
            (p_new_buffer)->i_alloc_type = i_alloc_type;                    \
            (p_new_buffer)->i_size = i_alloc_size;                          \
            (p_new_buffer)->p_buffer = (BYTE *)(p_new_buffer)             \
                                         + sizeof(aout_buffer_t);           \
            if ( (p_previous_buffer) != NULL )                              \
            {                                                               \
                (p_new_buffer)->start_date =                                \
                           ((aout_buffer_t *)p_previous_buffer)->start_date;\
                (p_new_buffer)->end_date =                                  \
                           ((aout_buffer_t *)p_previous_buffer)->end_date;  \
            }                                                               \
        }                                                                   \
        /* we'll keep that for a while --Meuuh */                           \
        /* else printf("%s:%d\n", __FILE__, __LINE__); */                   \
    }

#define aout_BufferFree( p_buffer )                                         \
    if( p_buffer != NULL && (p_buffer)->i_alloc_type == AOUT_ALLOC_HEAP )   \
    {                                                                       \
        free( p_buffer );                                                   \
    }                                                                       \
    p_buffer = NULL;

class CAudioOutput;
/*****************************************************************************
 * aout_filter_t : audio output filter
 *****************************************************************************/
struct aout_filter_t
{

    audio_format_t   input;
    audio_format_t   output;
    aout_alloc_t            output_alloc;

    struct aout_filter_sys_t * p_sys;
    void                 (* pf_do_work)( CAudioOutput *,
                                         struct aout_filter_t *,
                                         struct aout_buffer_t *,
                                         struct aout_buffer_t * );
    bool              b_in_place;
    bool              b_continuity;
};


/*****************************************************************************
 * aout_input_t : input stream for the audio output
 *****************************************************************************/
#define AOUT_RESAMPLING_NONE     0
#define AOUT_RESAMPLING_UP       1
#define AOUT_RESAMPLING_DOWN     2
struct aout_input_t
{
    /* When this lock is taken, the pipeline cannot be changed by a
     * third-party. */
    tme_mutex_t             lock;

    /* The input thread that spawned this input */

    audio_format_t   input;
    aout_alloc_t            input_alloc;

    /* pre-filters */
    aout_filter_t *         pp_filters[AOUT_MAX_FILTERS];
    int                     i_nb_filters;

    /* resamplers */
    aout_filter_t *         pp_resamplers[AOUT_MAX_FILTERS];
    int                     i_nb_resamplers;
    int                     i_resampling_type;
    mtime_t                 i_resamp_start_date;
    int                     i_resamp_start_drift;

    aout_fifo_t             fifo;

    /* Mixer information */
    BYTE *                p_first_byte_to_mix;

    /* If b_restart == 1, the input pipeline will be re-created. */
    bool              b_restart;

    /* If b_error == 1, there is no input pipeline. */
    bool              b_error;

    /* Did we just change the output format? (expect buffer inconsistencies) */
    bool              b_changed;

    /* internal caching delay from input */
    int                     i_pts_delay;
    /* desynchronisation delay request by the user */
    int                     i_desync;

};



/*****************************************************************************
 * Prototypes
 *****************************************************************************/
/* From input.c : */
int aout_InputNew( CAudioOutput * p_aout, aout_input_t * p_input );
int aout_InputDelete( CAudioOutput * p_aout, aout_input_t * p_input );
int aout_InputPlay( CAudioOutput * p_aout, aout_input_t * p_input,
                    aout_buffer_t * p_buffer );

/* From filters.c : */
int aout_FiltersCreatePipeline( CAudioOutput * p_aout,
                                aout_filter_t ** pp_filters_start,
                                int * pi_nb_filters,
                                const audio_format_t * p_input_format,
                                const audio_format_t * p_output_format );
//int aout_FiltersCreatePipeline( CAudioOutput * p_aout, aout_filter_t ** pp_filters, int * pi_nb_filters, const audio_sample_format_t * p_input_format, const audio_sample_format_t * p_output_format );
void aout_FiltersDestroyPipeline( CAudioOutput * p_aout, aout_filter_t ** pp_filters, int i_nb_filters );
void aout_FiltersPlay( CAudioOutput * p_aout, aout_filter_t ** pp_filters, int i_nb_filters, aout_buffer_t ** pp_input_buffer );
void aout_FiltersHintBuffers( CAudioOutput * p_aout, aout_filter_t ** pp_filters, int i_nb_filters, aout_alloc_t * p_first_alloc );

/* From mixer.c : */
int aout_MixerNew( CAudioOutput * p_aout );
void aout_MixerDelete( CAudioOutput * p_aout );
void aout_MixerRun( CAudioOutput * p_aout );
int aout_MixerMultiplierSet( CAudioOutput * p_aout, float f_multiplier );
int aout_MixerMultiplierGet( CAudioOutput * p_aout, float * pf_multiplier );

/* From output.c : */
int aout_OutputNew( CAudioOutput * p_aout,
                    audio_sample_format_t * p_format );
void aout_OutputPlay( CAudioOutput * p_aout, aout_buffer_t * p_buffer );
void aout_OutputDelete( CAudioOutput * p_aout );
aout_buffer_t *aout_OutputNextBuffer( CAudioOutput *, mtime_t, bool );

/* From common.c : */
unsigned int aout_FormatNbChannels( const audio_sample_format_t * p_format );
void aout_FormatPrepare( audio_sample_format_t * p_format );
void aout_FormatPrint( CAudioOutput * p_aout, const TCHAR * psz_text, const audio_sample_format_t * p_format );
void aout_FormatsPrint( CAudioOutput * p_aout, const TCHAR * psz_text, const audio_sample_format_t * p_format1, const audio_sample_format_t * p_format2 );
LPCTSTR aout_FormatPrintChannels( const audio_sample_format_t * );
void aout_FifoInit( CAudioOutput *, aout_fifo_t *, UINT32 );
mtime_t aout_FifoNextStart( CAudioOutput *, aout_fifo_t * );
void aout_FifoPush( CAudioOutput *, aout_fifo_t *, aout_buffer_t * );
void aout_FifoSet( CAudioOutput *, aout_fifo_t *, mtime_t );
void aout_FifoMoveDates( CAudioOutput *, aout_fifo_t *, mtime_t );
aout_buffer_t *aout_FifoPop( CAudioOutput * p_aout, aout_fifo_t * p_fifo );
void aout_FifoDestroy( CAudioOutput * p_aout, aout_fifo_t * p_fifo );
mtime_t aout_FifoFirstDate( CAudioOutput *, aout_fifo_t * );

/* From intf.c :*/
void aout_VolumeSoftInit( CAudioOutput * );
int aout_VolumeSoftGet( CAudioOutput *, audio_volume_t * );
int aout_VolumeSoftSet( CAudioOutput *, audio_volume_t );
int aout_VolumeSoftInfos( CAudioOutput *, audio_volume_t * );
void aout_VolumeNoneInit( CAudioOutput * );
int aout_VolumeNoneGet( CAudioOutput *, audio_volume_t * );
int aout_VolumeNoneSet( CAudioOutput *, audio_volume_t );
int aout_VolumeNoneInfos( CAudioOutput *, audio_volume_t * );


#endif