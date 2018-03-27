#ifndef _TMEVIEWDEF_H
#define _TMEVIEWDEF_H

#define TME_VAR_VOID      0x0010
#define TME_VAR_BOOL      0x0020
#define TME_VAR_INTEGER   0x0030
#define TME_VAR_HOTKEY    0x0031
#define TME_VAR_STRING    0x0040
#define TME_VAR_MODULE    0x0041
#define TME_VAR_FILE      0x0042
#define TME_VAR_DIRECTORY 0x0043
#define TME_VAR_VARIABLE  0x0044
#define TME_VAR_FLOAT     0x0050
#define TME_VAR_TIME      0x0060
#define TME_VAR_ADDRESS   0x0070
#define TME_VAR_MUTEX     0x0080
#define TME_VAR_LIST      0x0090

typedef union
{
    int             i_int;
    bool      b_bool;
    float           f_float;
    char *          psz_string;
    void *          p_address;
    INT64     i_time;

   /* Make sure the structure is at least 64bits */
    struct { char a, b, c, d, e, f, g, h; } padding;

} tme_value_t;

/*****************************************************************************
 * General configuration
 *****************************************************************************/

#define CLOCK_FREQ 1000000


/* When creating or destroying threads in blocking mode, delay to poll thread
 * status */
#define THREAD_SLEEP                    ((mtime_t)(0.010*CLOCK_FREQ))

/* When a thread waits on a condition in debug mode, delay to wait before
 * outputting an error message (in second) */
#define THREAD_COND_TIMEOUT             1

/*****************************************************************************
 * Interface configuration
 *****************************************************************************/

/* Base delay in micro second for interface sleeps */
#define INTF_IDLE_SLEEP                 ((mtime_t)(0.050*CLOCK_FREQ))

/* Step for changing gamma, and minimum and maximum values */
#define INTF_GAMMA_STEP                 .1
#define INTF_GAMMA_LIMIT                3

/*****************************************************************************
 * Input thread configuration
 *****************************************************************************/

/* Used in ErrorThread */
#define INPUT_IDLE_SLEEP                ((mtime_t)(0.100*CLOCK_FREQ))

/* Time to wait in case of read error */
#define INPUT_ERROR_SLEEP               ((mtime_t)(0.10*CLOCK_FREQ))

/* Number of read() calls needed until we check the file size through
 * fstat() */
#define INPUT_FSTAT_NB_READS            10

/*
 * Time settings
 */

/* Time during which the thread will sleep if it has nothing to
 * display (in micro-seconds) */
#define VOUT_IDLE_SLEEP                 ((int)(0.020*CLOCK_FREQ))

/* Maximum lap of time allowed between the beginning of rendering and
 * display. If, compared to the current date, the next image is too
 * late, the thread will perform an idle loop. This time should be
 * at least VOUT_IDLE_SLEEP plus the time required to render a few
 * images, to avoid trashing of decoded images */
#define VOUT_DISPLAY_DELAY              ((int)(0.200*CLOCK_FREQ))

/* Pictures which are VOUT_BOGUS_DELAY or more in advance probably have
 * a bogus PTS and won't be displayed */
#define VOUT_BOGUS_DELAY                ((mtime_t)(DEFAULT_PTS_DELAY * 30))

/* Delay (in microseconds) before an idle screen is displayed */
#define VOUT_IDLE_DELAY                 (5*CLOCK_FREQ)

/* Number of pictures required to computes the FPS rate */
#define VOUT_FPS_SAMPLES                20

/* Better be in advance when awakening than late... */
#define VOUT_MWAIT_TOLERANCE            ((mtime_t)(0.020*CLOCK_FREQ))

/* Time to sleep when waiting for a buffer (from vout or the video fifo).
 * It should be approximately the time needed to perform a complete picture
 * loop. Since it only happens when the video heap is full, it does not need
 * to be too low, even if it blocks the decoder. */
#define VOUT_OUTMEM_SLEEP               ((mtime_t)(0.020*CLOCK_FREQ))

#define DEFAULT_PTS_DELAY               (mtime_t)(.3*CLOCK_FREQ)
#define VOUT_ASPECT_FACTOR              432000
#define VOUT_MIN_DIRECT_PICTURES        6

/** b_info changed */
#define VOUT_INFO_CHANGE        0x0001
/** b_grayscale changed */
#define VOUT_GRAYSCALE_CHANGE   0x0002
/** b_interface changed */
#define VOUT_INTF_CHANGE        0x0004
/** b_scale changed */
#define VOUT_SCALE_CHANGE       0x0008
/** gamma changed */
#define VOUT_GAMMA_CHANGE       0x0010
/** b_cursor changed */
#define VOUT_CURSOR_CHANGE      0x0020
/** b_fullscreen changed */
#define VOUT_FULLSCREEN_CHANGE  0x0040
/** size changed */
#define VOUT_SIZE_CHANGE        0x0200
/** depth changed */
#define VOUT_DEPTH_CHANGE       0x0400
/** change chroma tables */
#define VOUT_CHROMA_CHANGE      0x0800
/** cropping parameters changed */
#define VOUT_CROP_CHANGE        0x1000
/** aspect ratio changed */
#define VOUT_ASPECT_CHANGE      0x2000
/** change/recreate picture buffers */
#define VOUT_PICTURE_BUFFERS_CHANGE 0x4000

/* Alignment flags */
#define VOUT_ALIGN_LEFT         0x0001
#define VOUT_ALIGN_RIGHT        0x0002
#define VOUT_ALIGN_HMASK        0x0003
#define VOUT_ALIGN_TOP          0x0004
#define VOUT_ALIGN_BOTTOM       0x0008
#define VOUT_ALIGN_VMASK        0x000C

#define MAX_JITTER_SAMPLES      20

/*****************************************************************************
 * Audio configuration
 *****************************************************************************/

/* Volume */
#define AOUT_VOLUME_DEFAULT             256
#define AOUT_VOLUME_STEP                32
#define AOUT_VOLUME_MAX                 1024
#define AOUT_VOLUME_MIN                 0

/* Max number of pre-filters per input, and max number of post-filters */
#define AOUT_MAX_FILTERS                10

/* Max number of inputs */
#define AOUT_MAX_INPUTS                 5

/* Buffers which arrive in advance of more than AOUT_MAX_ADVANCE_TIME
 * will be considered as bogus and be trashed */
#define AOUT_MAX_ADVANCE_TIME           (mtime_t)(DEFAULT_PTS_DELAY * 3)

/* Buffers which arrive in advance of more than AOUT_MAX_PREPARE_TIME
 * will cause the calling thread to sleep */
#define AOUT_MAX_PREPARE_TIME           (mtime_t)(.5*CLOCK_FREQ)

/* Buffers which arrive after pts - AOUT_MIN_PREPARE_TIME will be trashed
 * to avoid too heavy resampling */
#define AOUT_MIN_PREPARE_TIME           (mtime_t)(.04*CLOCK_FREQ) /* 40000 */

/* Max acceptable delay between the coded PTS and the actual presentation
 * time, without resampling */
#define AOUT_PTS_TOLERANCE              (mtime_t)(.04*CLOCK_FREQ)

/* Max acceptable resampling (in %) */
#define AOUT_MAX_RESAMPLING             10


typedef UINT16            audio_volume_t;

typedef struct audio_format_t audio_sample_format_t;
typedef struct aout_buffer_t aout_buffer_t;
typedef struct aout_filter_t aout_filter_t;

typedef INT64 off64_t;

static inline UINT16 GetWLE( void const * _p )
{
    UINT8 * p = (UINT8 *)_p;
    return ( ((UINT16)p[1] << 8) | p[0] );
}
static inline UINT32 GetDWLE( void const * _p )
{
    UINT8 * p = (UINT8 *)_p;
    return ( ((UINT32)p[3] << 24) | ((UINT32)p[2] << 16)
              | ((UINT32)p[1] << 8) | p[0] );
}
static inline UINT64 GetQWLE( void const * _p )
{
    UINT8 * p = (UINT8 *)_p;
    return ( ((UINT64)p[7] << 56) | ((UINT64)p[6] << 48)
              | ((UINT64)p[5] << 40) | ((UINT64)p[4] << 32)
              | ((UINT64)p[3] << 24) | ((UINT64)p[2] << 16)
              | ((UINT64)p[1] << 8) | p[0] );
}

#define SetWLE( p, v ) _SetWLE( (UINT8*)p, v)
static inline void _SetWLE( UINT8 *p, UINT16 i_dw )
{
    p[1] = ( i_dw >>  8 )&0xff;
    p[0] = ( i_dw       )&0xff;
}

#define SetDWLE( p, v ) _SetDWLE( (UINT8*)p, v)
static inline void _SetDWLE( UINT8 *p, UINT32 i_dw )
{
    p[3] = ( i_dw >> 24 )&0xff;
    p[2] = ( i_dw >> 16 )&0xff;
    p[1] = ( i_dw >>  8 )&0xff;
    p[0] = ( i_dw       )&0xff;
}

static inline UINT16 U16_AT( void const * _p )
{
    UINT8 * p = (UINT8 *)_p;
    return ( ((UINT16)p[0] << 8) | p[1] );
}
static inline UINT32 U32_AT( void const * _p )
{
    UINT8 * p = (UINT8 *)_p;
    return ( ((UINT32)p[0] << 24) | ((UINT32)p[1] << 16)
              | ((UINT32)p[2] << 8) | p[3] );
}
static inline UINT64 U64_AT( void const * _p )
{
    UINT8 * p = (UINT8 *)_p;
    return ( ((UINT64)p[0] << 56) | ((UINT64)p[1] << 48)
              | ((UINT64)p[2] << 40) | ((UINT64)p[3] << 32)
              | ((UINT64)p[4] << 24) | ((UINT64)p[5] << 16)
              | ((UINT64)p[6] << 8) | p[7] );
}

#define GetWBE( p )     U16_AT( p )
#define GetDWBE( p )    U32_AT( p )
#define GetQWBE( p )    U64_AT( p )

#define PLAYLIST_INSERT          0x0001
#define PLAYLIST_REPLACE         0x0002
#define PLAYLIST_APPEND          0x0004
#define PLAYLIST_GO              0x0008
#define PLAYLIST_CHECK_INSERT    0x0010
#define PLAYLIST_PREPARSE        0x0020

#define PLAYLIST_END           -666

#define SUCCESS 0
#define EGENERIC 1

#endif
