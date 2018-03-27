#ifndef _BLOCK_H_
#define _BLOCK_H_

#include "tme_thread.h"
#include "mtime.h"

typedef struct block_sys_t block_sys_t;

/** The content doesn't follow the last block, or is probably broken */
#define BLOCK_FLAG_DISCONTINUITY 0x0001
/** Intra frame */
#define BLOCK_FLAG_TYPE_I        0x0002
/** Inter frame with backward reference only */
#define BLOCK_FLAG_TYPE_P        0x0004
/** Inter frame with backward and forward reference */
#define BLOCK_FLAG_TYPE_B        0x0008
/** For inter frame when you don't know the real type */
#define BLOCK_FLAG_TYPE_PB       0x0010
/** Warn that this block is a header one */
#define BLOCK_FLAG_HEADER        0x0020
/** This is the last block of the frame */
#define BLOCK_FLAG_END_OF_FRAME  0x0040
/** This is not a key frame for bitrate shaping */
#define BLOCK_FLAG_NO_KEYFRAME   0x0080
/** This block contains a clock reference */
#define BLOCK_FLAG_CLOCK         0x0200
/** This block is scrambled */
#define BLOCK_FLAG_SCRAMBLED     0x0400
/** This block has to be decoded but not be displayed */
#define BLOCK_FLAG_PREROLL       0x0800
/** This block is corrupted and/or there is data loss  */
#define BLOCK_FLAG_CORRUPTED     0x1000
#define BLOCK_FLAG_TYPE_AUDIO    0x2000
#define BLOCK_FLAG_TYPE_TEXT     0x4000
#define BLOCK_FLAG_TYPE_KEYTEXT  0x8000 /* text for video key frame: to facilitate key frame scanning */

#define BLOCK_FLAG_PRIVATE_MASK  0xffff0000
#define BLOCK_FLAG_PRIVATE_SHIFT 16

struct block_t
{
    block_t     *p_next;
    block_t     *p_prev;

    UINT32    i_flags;

    mtime_t     i_pts;
    mtime_t     i_dts;
    mtime_t     i_length;

    int         i_samples; /* Used for audio */
    int         i_rate;

    int         i_buffer;
    BYTE     *p_buffer;

	int i_osd; /* osd text size in the beginning of frame, used in decodevideo to avoid memmove */

    /* Following fields are private, user should never touch it */
    /* XXX never touch that OK !!! the first that access that will
     * have cvs account removed ;) XXX */
    block_sys_t *p_sys;
};

static inline void block_Release( block_t *p_block )
{
    free( p_block );
}

struct block_fifo_t
{
    tme_mutex_t         lock;                         /* fifo data lock */
    tme_cond_t          wait;         /* fifo data conditional variable */

    int                 i_depth;      /* # of blocks in the fifo */
    block_t             *p_first;
    block_t             **pp_last;
    int                 i_size;			/* total bytes in all blocks in the fifo */
};

block_t *block_New( /*tme_object_t *p_obj, */int i_size );

block_fifo_t *block_FifoNew( tme_object_t *p_obj );
void block_FifoRelease( block_fifo_t *p_fifo );
void block_FifoEmpty( block_fifo_t *p_fifo );
int block_FifoPut( block_fifo_t *p_fifo, block_t *p_block );
block_t *block_FifoGet( block_fifo_t *p_fifo );
void block_FifoUpdateTimestamp(block_fifo_t *p_fifo, INT64 delay);

#endif