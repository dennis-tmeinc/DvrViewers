#ifndef _ADPCM_H_
#define _ADPCM_H_

#include "inttypedef.h"

#define DEF_T(type, name, bytes, read, write)                             \
static inline type bytestream_get_ ## name(const uint8_t **b){\
    (*b) += bytes;\
    return read(*b - bytes);\
}\
static inline void bytestream_put_ ##name(uint8_t **b, const type value){\
    write(*b, value);\
    (*b) += bytes;\
}

#define DEF(name, bytes, read, write) \
    DEF_T(unsigned int, name, bytes, read, write)
#define DEF64(name, bytes, read, write) \
    DEF_T(uint64_t, name, bytes, read, write)

//DEF64(le64, 8, AV_RL64, AV_WL64)
//DEF  (le32, 4, AV_RL32, AV_WL32)
//DEF  (le24, 3, AV_RL24, AV_WL24)
DEF  (le16, 2, AV_RL16, AV_WL16)
//DEF64(be64, 8, AV_RB64, AV_WB64)
//DEF  (be32, 4, AV_RB32, AV_WB32)
//DEF  (be24, 3, AV_RB24, AV_WB24)
//DEF  (be16, 2, AV_RB16, AV_WB16)
//DEF  (byte, 1, AV_RB8 , AV_WB8 )

typedef struct ADPCMChannelStatus {
    int predictor;
    short int step_index;
    int step;
    /* for encoding */
    int prev_sample;

    /* MS version */
    short sample1;
    short sample2;
    int coeff1;
    int coeff2;
    int idelta;
} ADPCMChannelStatus;

typedef struct AVCodecContext {
    int block_align;
	int channels;
	int frame_size;
    ADPCMChannelStatus status[6];
	PBYTE work_buf, frame_buf;
	int work_buf_size, data_size;
} AVCodecContext;

/**
 * Clip a signed integer value into the amin-amax range.
 * @param a value to clip
 * @param amin minimum value of the clip range
 * @param amax maximum value of the clip range
 * @return clipped value
 */
static inline int av_clip_c(int a, int amin, int amax)
{
    if      (a < amin) return amin;
    else if (a > amax) return amax;
    else               return a;
}

/**
 * Clip a signed integer value into the -32768,32767 range.
 * @param a value to clip
 * @return clipped value
 */
static inline int16_t av_clip_int16_c(int a)
{
    if ((a+0x8000) & ~0xFFFF) return (a>>31) ^ 0x7FFF;
    else                      return a;
}

#ifndef av_clip
#   define av_clip          av_clip_c
#endif
#ifndef av_clip_int16
#   define av_clip_int16    av_clip_int16_c
#endif

int adpcm_encode_init(AVCodecContext *avctx);
void adpcm_encode_close(AVCodecContext *avctx);
int adpcm_encode_frame(AVCodecContext *avctx,
                            unsigned char *frame, int buf_size, void *data);
#endif