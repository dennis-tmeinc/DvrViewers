#include "stdafx.h"
#include "adpcm.h"

#define BLKSIZE 324
// BLKSize   Rawsize
// 1024   -> 4082 (framesize * 2)
// 324    -> 1282
// 320    -> 1266

/* step_table[] and index_table[] are from the ADPCM reference source */
/* This is the index table: */
static const int index_table[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8,
};

/**
 * This is the step table. Note that many programs use slight deviations from
 * this table, but such deviations are negligible:
 */
static const int step_table[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

/* These are for MS-ADPCM */
/* AdaptationTable[], AdaptCoeff1[], and AdaptCoeff2[] are from libsndfile */
static const int AdaptationTable[] = {
        230, 230, 230, 230, 307, 409, 512, 614,
        768, 614, 512, 409, 307, 230, 230, 230
};

/** Divided by 4 to fit in 8-bit integers */
static const uint8_t AdaptCoeff1[] = {
        64, 128, 0, 48, 60, 115, 98
};

/** Divided by 4 to fit in 8-bit integers */
static const int8_t AdaptCoeff2[] = {
        0, -64, 0, 16, 0, -52, -58
};

/* These are for CD-ROM XA ADPCM */
static const int xa_adpcm_table[5][2] = {
   {   0,   0 },
   {  60,   0 },
   { 115, -52 },
   {  98, -55 },
   { 122, -60 }
};

static const int ea_adpcm_table[] = {
    0, 240, 460, 392, 0, 0, -208, -220, 0, 1,
    3, 4, 7, 8, 10, 11, 0, -1, -3, -4
};

// padded to zero where table size is less then 16
static const int swf_index_tables[4][16] = {
    /*2*/ { -1, 2 },
    /*3*/ { -1, -1, 2, 4 },
    /*4*/ { -1, -1, -1, -1, 2, 4, 6, 8 },
    /*5*/ { -1, -1, -1, -1, -1, -1, -1, -1, 1, 2, 4, 6, 8, 10, 13, 16 }
};

static const int yamaha_indexscale[] = {
    230, 230, 230, 230, 307, 409, 512, 614,
    230, 230, 230, 230, 307, 409, 512, 614
};

static const int yamaha_difflookup[] = {
    1, 3, 5, 7, 9, 11, 13, 15,
    -1, -3, -5, -7, -9, -11, -13, -15
};

/* end of tables */

int adpcm_encode_init(AVCodecContext *avctx)
{
	memset(avctx, 0, sizeof(AVCodecContext));
	avctx->channels = 1;
	/* each 16 bits sample gives one nibble */    
	/* and we have 4 bytes per channel overhead */
	avctx->block_align = BLKSIZE;
	avctx->frame_size = (BLKSIZE - 4 * avctx->channels) * 8 / (4 * avctx->channels) + 1; 

	/* need work_buf */
	avctx->frame_buf = new BYTE[BLKSIZE];
	avctx->work_buf = new BYTE[avctx->frame_size * 2];
	avctx->work_buf_size = avctx->frame_size * 2;
	avctx->data_size = 0;

    return 0;
}

void adpcm_encode_close(AVCodecContext *avctx)
{
	delete [] avctx->work_buf;
	delete [] avctx->frame_buf;
}

static unsigned char adpcm_ima_compress_sample(ADPCMChannelStatus *c, short sample)
{
    int delta = sample - c->prev_sample;
    int nibble = min(7, abs(delta)*4/step_table[c->step_index]) + (delta<0)*8;
    c->prev_sample += ((step_table[c->step_index] * yamaha_difflookup[nibble]) / 8);
    c->prev_sample = av_clip_int16(c->prev_sample);
    c->step_index = av_clip(c->step_index + index_table[nibble], 0, 88);
    return nibble;
}

int adpcm_encode_frame(AVCodecContext *avctx,
                            unsigned char *frame, int buf_size, void *data)
{
    int n, i, st;
    short *samples;
    unsigned char *dst;
    uint8_t *buf;

    dst = frame;
    samples = (short *)data;
    st= avctx->channels == 2;

    n = avctx->frame_size / 8;
    avctx->status[0].prev_sample = (signed short)samples[0]; /* XXX */
    bytestream_put_le16(&dst, avctx->status[0].prev_sample);
    *dst++ = (unsigned char)avctx->status[0].step_index;
    *dst++ = 0; /* unknown */
    samples++;
    if (avctx->channels == 2) {
        avctx->status[1].prev_sample = (signed short)samples[0];
        bytestream_put_le16(&dst, avctx->status[1].prev_sample);
        *dst++ = (unsigned char)avctx->status[1].step_index;
        *dst++ = 0;
        samples++;
    }

    for (; n>0; n--) {
        *dst = adpcm_ima_compress_sample(&avctx->status[0], samples[0]);
        *dst |= adpcm_ima_compress_sample(&avctx->status[0], samples[avctx->channels]) << 4;
        dst++;
        *dst = adpcm_ima_compress_sample(&avctx->status[0], samples[avctx->channels * 2]);
        *dst |= adpcm_ima_compress_sample(&avctx->status[0], samples[avctx->channels * 3]) << 4;
        dst++;
        *dst = adpcm_ima_compress_sample(&avctx->status[0], samples[avctx->channels * 4]);
        *dst |= adpcm_ima_compress_sample(&avctx->status[0], samples[avctx->channels * 5]) << 4;
        dst++;
        *dst = adpcm_ima_compress_sample(&avctx->status[0], samples[avctx->channels * 6]);
        *dst |= adpcm_ima_compress_sample(&avctx->status[0], samples[avctx->channels * 7]) << 4;
        dst++;
        /* right channel */
        if (avctx->channels == 2) {
            *dst = adpcm_ima_compress_sample(&avctx->status[1], samples[1]);
            *dst |= adpcm_ima_compress_sample(&avctx->status[1], samples[3]) << 4;
            dst++;
            *dst = adpcm_ima_compress_sample(&avctx->status[1], samples[5]);
            *dst |= adpcm_ima_compress_sample(&avctx->status[1], samples[7]) << 4;
            dst++;
            *dst = adpcm_ima_compress_sample(&avctx->status[1], samples[9]);
            *dst |= adpcm_ima_compress_sample(&avctx->status[1], samples[11]) << 4;
            dst++;
            *dst = adpcm_ima_compress_sample(&avctx->status[1], samples[13]);
            *dst |= adpcm_ima_compress_sample(&avctx->status[1], samples[15]) << 4;
            dst++;
        }
        samples += 8 * avctx->channels;
    }
    return dst - frame;
}
