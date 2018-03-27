#ifndef _AUDIO_OUTPUT_H_
#define _AUDIO_OUTPUT_H_

#include "mtime.h"
#include "tme_thread.h"
#include "tmeviewdef.h"
#include "aout_internal.h"
#include "aout_common.h"

#define AOUT_FMTS_IDENTICAL( p_first, p_second ) (                          \
    ((p_first)->i_format == (p_second)->i_format)                           \
      && ((p_first)->i_rate == (p_second)->i_rate)                          \
      && ((p_first)->i_physical_channels == (p_second)->i_physical_channels)\
      && ((p_first)->i_original_channels == (p_second)->i_original_channels) )

/* Check if i_rate == i_rate and i_channels == i_channels */
#define AOUT_FMTS_SIMILAR( p_first, p_second ) (                            \
    ((p_first)->i_rate == (p_second)->i_rate)                               \
      && ((p_first)->i_physical_channels == (p_second)->i_physical_channels)\
      && ((p_first)->i_original_channels == (p_second)->i_original_channels) )

#ifdef WORDS_BIGENDIAN
#   define AOUT_FMT_S16_NE TME_FOURCC('s','1','6','b')
#   define AOUT_FMT_U16_NE TME_FOURCC('u','1','6','b')
#   define AOUT_FMT_S24_NE TME_FOURCC('s','2','4','b')
#   define AOUT_FMT_SPDIF_NE TME_FOURCC('s','p','d','b')
#else
#   define AOUT_FMT_S16_NE TME_FOURCC('s','1','6','l')
#   define AOUT_FMT_U16_NE TME_FOURCC('u','1','6','l')
#   define AOUT_FMT_S24_NE TME_FOURCC('s','2','4','l')
#   define AOUT_FMT_SPDIF_NE TME_FOURCC('s','p','d','i')
#endif

#define AOUT_FMT_NON_LINEAR( p_format )                                    \
    ( ((p_format)->i_format == TME_FOURCC('s','p','d','i'))                \
       || ((p_format)->i_format == TME_FOURCC('s','p','d','b'))            \
       || ((p_format)->i_format == TME_FOURCC('a','5','2',' '))            \
       || ((p_format)->i_format == TME_FOURCC('d','t','s',' ')) )

/* This is heavily borrowed from libmad, by Robert Leslie <rob@mars.org> */
/*
 * Fixed-point format: 0xABBBBBBB
 * A == whole part      (sign + 3 bits)
 * B == fractional part (28 bits)
 *
 * Values are signed two's complement, so the effective range is:
 * 0x80000000 to 0x7fffffff
 *       -8.0 to +7.9999999962747097015380859375
 *
 * The smallest representable value is:
 * 0x00000001 == 0.0000000037252902984619140625 (i.e. about 3.725e-9)
 *
 * 28 bits of fractional accuracy represent about
 * 8.6 digits of decimal accuracy.
 *
 * Fixed-point numbers can be added or subtracted as normal
 * integers, but multiplication requires shifting the 64-bit result
 * from 56 fractional bits back to 28 (and rounding.)
 */
typedef INT32 tme_fixed_t;
#define FIXED32_FRACBITS 28
#define FIXED32_MIN ((tme_fixed_t) -0x80000000L)
#define FIXED32_MAX ((tme_fixed_t) +0x7fffffffL)
#define FIXED32_ONE ((tme_fixed_t) 0x10000000)


/*
 * Channels descriptions
 */

/* Values available for physical and original channels */
#define AOUT_CHAN_CENTER            0x1
#define AOUT_CHAN_LEFT              0x2
#define AOUT_CHAN_RIGHT             0x4
#define AOUT_CHAN_REARCENTER        0x10
#define AOUT_CHAN_REARLEFT          0x20
#define AOUT_CHAN_REARRIGHT         0x40
#define AOUT_CHAN_MIDDLELEFT        0x100
#define AOUT_CHAN_MIDDLERIGHT       0x200
#define AOUT_CHAN_LFE               0x1000

/* Values available for original channels only */
#define AOUT_CHAN_DOLBYSTEREO       0x10000
#define AOUT_CHAN_DUALMONO          0x20000
#define AOUT_CHAN_REVERSESTEREO     0x40000

#define AOUT_CHAN_PHYSMASK          0xFFFF
#define AOUT_CHAN_MAX               9

/* Values used for the audio-device and audio-channels object variables */
#define AOUT_VAR_MONO               1
#define AOUT_VAR_STEREO             2
#define AOUT_VAR_2F2R               4
#define AOUT_VAR_3F2R               5
#define AOUT_VAR_5_1                6
#define AOUT_VAR_6_1                7
#define AOUT_VAR_7_1                8
#define AOUT_VAR_SPDIF              10

#define AOUT_VAR_CHAN_STEREO        1
#define AOUT_VAR_CHAN_RSTEREO       2
#define AOUT_VAR_CHAN_LEFT          3
#define AOUT_VAR_CHAN_RIGHT         4
#define AOUT_VAR_CHAN_DOLBYS        5

/*****************************************************************************
 * aout_buffer_t : audio output buffer
 *****************************************************************************/
struct aout_buffer_t
{
    BYTE *                p_buffer;
    int                     i_alloc_type;
    /* i_size is the real size of the buffer (used for debug ONLY), i_nb_bytes
     * is the number of significative bytes in it. */
    size_t                  i_size, i_nb_bytes;
    unsigned int            i_nb_samples;
    mtime_t                 start_date, end_date;

    struct aout_buffer_t *  p_next;

    /** Private data (aout_buffer_t will disappear soon so no need for an
     * aout_buffer_sys_t type) */
    void * p_sys;

    /** This way the release can be overloaded */
    void (*pf_release)( aout_buffer_t * );
};

/* Size of a frame for S/PDIF output. */
#define AOUT_SPDIF_SIZE 6144

/* Number of samples in an A/52 frame. */
#define A52_FRAME_NB 1536


typedef struct aout_instance_t aout_instance_t;
typedef struct aout_input_t aout_input_t;


class CAoutOutput;

class CAudioOutput
{
public:
	CAudioOutput(tme_object_t *pIpc, audio_format_t *pFormat, int channel = 0);
	~CAudioOutput();

	int m_iAudioDevice;
	int m_channelID;

	tme_object_t *m_pIpc;
    /* Locks : please note that if you need several of these locks, it is
     * mandatory (to avoid deadlocks) to take them in the following order :
     * mixer_lock, p_input->lock, output_fifo_lock, input_fifos_lock.
     * --Meuuh */
    /* When input_fifos_lock is taken, none of the p_input->fifo structures
     * can be read or modified by a third-party thread. */
    tme_mutex_t             m_inputFifosLock;
    /* When mixer_lock is taken, all decoder threads willing to mix a
     * buffer must wait until it is released. The output pipeline cannot
     * be modified. No input stream can be added or removed. */
    tme_mutex_t             m_mixerLock;
    /* When output_fifo_lock is taken, the p_aout->output.fifo structure
     * cannot be read or written  by a third-party thread. */
    tme_mutex_t             m_outputFifoLock;

    /* Input streams & pre-filters */
    aout_input_t *          m_ppInputs[AOUT_MAX_INPUTS];
    int                     m_iNbInputs;

    /* Mixer */
    aout_mixer_t            m_mixer;

    CAoutOutput           *m_pOutput;
	int						m_iVolume;

	aout_input_t * DecNew( audio_sample_format_t * p_format );
	int DecDelete( aout_input_t * p_input );
	aout_buffer_t * DecNewBuffer( aout_input_t * p_input,
	                                   size_t i_nb_samples );
	void DecDeleteBuffer( aout_input_t * p_input, aout_buffer_t * p_buffer );
	int DecPlay( aout_input_t * p_input, aout_buffer_t * p_buffer );
	int MixBuffer();
	void MixerRun();
	int MixerNew();
	void MixerDelete();
	int TrivialMixerNew();
	int Float32MixerNew();
	int AudioOn(bool on);
	int MixerMultiplierSet( float f_multiplier );
	int VolumeSet(int iVolume);
};

void aout_DateInit( audio_date_t * p_date, UINT32 i_divider );
void aout_DateSet( audio_date_t * p_date, mtime_t new_date );
void aout_DateMove( audio_date_t * p_date, mtime_t difference );
mtime_t aout_DateGet( const audio_date_t * p_date );
mtime_t aout_DateIncrement( audio_date_t * p_date, UINT32 i_nb_samples );
int aout_CheckChannelReorder( const UINT32 *pi_chan_order_in,
                              const UINT32 *pi_chan_order_out,
                              UINT32 i_channel_mask,
                              int i_channels, int *pi_chan_table );
void aout_ChannelReorder( UINT8 *p_buf, int i_buffer,
                          int i_channels, const int *pi_chan_table,
                          int i_bits_per_sample );

/* aout_Dec */
int aout_DecDelete( CAudioOutput * p_aout, aout_input_t * p_input );
void aout_DecDeleteBuffer( CAudioOutput * p_aout, aout_input_t * p_input,
                           aout_buffer_t * p_buffer );
int aout_DecPlay( CAudioOutput * p_aout, aout_input_t * p_input,
                  aout_buffer_t * p_buffer );

#endif