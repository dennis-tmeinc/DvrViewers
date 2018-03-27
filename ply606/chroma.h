#ifndef _CHROMA_H_
#define _CHROMA_H_

enum{CHROMA_NONE, CHROMA__I420_YUY2, CHROMA_I420_RGB};

/** Number of entries in RGB palette/colormap */
#define CMAP_RGB2_SIZE 256

class CVideoOutput;
typedef void (vout_chroma_convert_t)(CVideoOutput *, picture_t *, picture_t *);
/**
 * chroma_sys_t: chroma method descriptor

 * This structure is part of the chroma transformation descriptor, it
 * describes the yuv2rgb specific properties.
 */
struct vout_chroma_t
{
	vout_chroma_convert_t *pf_convert;
    UINT8  *p_buffer;
    int *p_offset;

    /**< Pre-calculated conversion tables */
    void *p_base;                      /**< base for all conversion tables */
    UINT8   *p_rgb8;                 /**< RGB 8 bits table */
    UINT16  *p_rgb16;                /**< RGB 16 bits table */
    UINT32  *p_rgb32;                /**< RGB 32 bits table */

    /**< To get RGB value for palette entry i, use (p_rgb_r[i], p_rgb_g[i],
       p_rgb_b[i]). Note these are 16 bits per pixel. For 8bpp entries,
       shift right 8 bits.
    */
    UINT16  p_rgb_r[CMAP_RGB2_SIZE];  /**< Red values of palette */
    UINT16  p_rgb_g[CMAP_RGB2_SIZE];  /**< Green values of palette */
    UINT16  p_rgb_b[CMAP_RGB2_SIZE];  /**< Blue values of palette */
};

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
//#ifdef MODULE_NAME_IS_i420_rgb
//void E_(I420_RGB8)         ( vout_thread_t *, picture_t *, picture_t * );
//void E_(I420_RGB16_dither) ( vout_thread_t *, picture_t *, picture_t * );
//#endif
//void E_(I420_RGB16)        ( vout_thread_t *, picture_t *, picture_t * );
//void E_(I420_RGB32)        ( vout_thread_t *, picture_t *, picture_t * );

/*****************************************************************************
 * CONVERT_*_PIXEL: pixel conversion macros
 *****************************************************************************
 * These conversion routines are used by YUV conversion functions.
 * conversion are made from p_y, p_u, p_v, which are modified, to p_buffer,
 * which is also modified. CONVERT_4YUV_PIXEL is used for 8bpp dithering,
 * CONVERT_4YUV_PIXEL_SCALE does the same but also scales the output.
 *****************************************************************************/
#define CONVERT_Y_PIXEL( BPP )                                                \
    /* Only Y sample is present */                                            \
    p_ybase = p_yuv + *p_y++;                                                 \
    *p_buffer++ = p_ybase[RED_OFFSET-((V_RED_COEF*128)>>SHIFT) + i_red] |     \
        p_ybase[GREEN_OFFSET-(((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT)       \
        + i_green ] | p_ybase[BLUE_OFFSET-((U_BLUE_COEF*128)>>SHIFT) + i_blue];

#define CONVERT_YUV_PIXEL( BPP )                                              \
    /* Y, U and V samples are present */                                      \
    i_uval =    *p_u++;                                                       \
    i_vval =    *p_v++;                                                       \
    i_red =     (V_RED_COEF * i_vval) >> SHIFT;                               \
    i_green =   (U_GREEN_COEF * i_uval + V_GREEN_COEF * i_vval) >> SHIFT;     \
    i_blue =    (U_BLUE_COEF * i_uval) >> SHIFT;                              \
    CONVERT_Y_PIXEL( BPP )                                                    \

#define CONVERT_Y_PIXEL_DITHER( BPP )                                         \
    /* Only Y sample is present */                                            \
    p_ybase = p_yuv + *p_y++;                                                 \
    *p_buffer++ = p_ybase[RED_OFFSET-((V_RED_COEF*128+p_dither[i_real_y])>>SHIFT) + i_red] |     \
        p_ybase[GREEN_OFFSET-(((U_GREEN_COEF+V_GREEN_COEF)*128+p_dither[i_real_y])>>SHIFT)       \
        + i_green ] | p_ybase[BLUE_OFFSET-((U_BLUE_COEF*128+p_dither[i_real_y])>>SHIFT) + i_blue];

#define CONVERT_YUV_PIXEL_DITHER( BPP )                                       \
    /* Y, U and V samples are present */                                      \
    i_uval =    *p_u++;                                                       \
    i_vval =    *p_v++;                                                       \
    i_red =     (V_RED_COEF * i_vval) >> SHIFT;                               \
    i_green =   (U_GREEN_COEF * i_uval + V_GREEN_COEF * i_vval) >> SHIFT;     \
    i_blue =    (U_BLUE_COEF * i_uval) >> SHIFT;                              \
    CONVERT_Y_PIXEL_DITHER( BPP )                                             \

#define CONVERT_4YUV_PIXEL( CHROMA )                                          \
    *p_pic++ = p_lookup[                                                      \
        (((*p_y++ + dither10[i_real_y]) >> 4) << 7)                           \
      + ((*p_u + dither20[i_real_y]) >> 5) * 9                                \
      + ((*p_v + dither20[i_real_y]) >> 5) ];                                 \
    *p_pic++ = p_lookup[                                                      \
        (((*p_y++ + dither11[i_real_y]) >> 4) << 7)                           \
      + ((*p_u++ + dither21[i_real_y]) >> 5) * 9                              \
      + ((*p_v++ + dither21[i_real_y]) >> 5) ];                               \
    *p_pic++ = p_lookup[                                                      \
        (((*p_y++ + dither12[i_real_y]) >> 4) << 7)                           \
      + ((*p_u + dither22[i_real_y]) >> 5) * 9                                \
      + ((*p_v + dither22[i_real_y]) >> 5) ];                                 \
    *p_pic++ = p_lookup[                                                      \
        (((*p_y++ + dither13[i_real_y]) >> 4) << 7)                           \
      + ((*p_u++ + dither23[i_real_y]) >> 5) * 9                              \
      + ((*p_v++ + dither23[i_real_y]) >> 5) ];                               \

#define CONVERT_4YUV_PIXEL_SCALE( CHROMA )                                    \
    *p_pic++ = p_lookup[                                                      \
        ( ((*p_y + dither10[i_real_y]) >> 4) << 7)                            \
        + ((*p_u + dither20[i_real_y]) >> 5) * 9                              \
        + ((*p_v + dither20[i_real_y]) >> 5) ];                               \
    p_y += *p_offset++;                                                       \
    p_u += *p_offset;                                                         \
    p_v += *p_offset++;                                                       \
    *p_pic++ = p_lookup[                                                      \
        ( ((*p_y + dither11[i_real_y]) >> 4) << 7)                            \
        + ((*p_u + dither21[i_real_y]) >> 5) * 9                              \
        + ((*p_v + dither21[i_real_y]) >> 5) ];                               \
    p_y += *p_offset++;                                                       \
    p_u += *p_offset;                                                         \
    p_v += *p_offset++;                                                       \
    *p_pic++ = p_lookup[                                                      \
        ( ((*p_y + dither12[i_real_y]) >> 4) << 7)                            \
        + ((*p_u + dither22[i_real_y]) >> 5) * 9                              \
        + ((*p_v + dither22[i_real_y]) >> 5) ];                               \
    p_y += *p_offset++;                                                       \
    p_u += *p_offset;                                                         \
    p_v += *p_offset++;                                                       \
    *p_pic++ = p_lookup[                                                      \
        ( ((*p_y + dither13[i_real_y]) >> 4) << 7)                            \
        + ((*p_u + dither23[i_real_y]) >> 5) * 9                              \
        + ((*p_v + dither23[i_real_y]) >> 5) ];                               \
    p_y += *p_offset++;                                                       \
    p_u += *p_offset;                                                         \
    p_v += *p_offset++;                                                       \

/*****************************************************************************
 * SCALE_WIDTH: scale a line horizontally
 *****************************************************************************
 * This macro scales a line using rendering buffer and offset array. It works
 * for 1, 2 and 4 Bpp.
 *****************************************************************************/
#define SCALE_WIDTH                                                           \
    if( b_hscale )                                                            \
    {                                                                         \
        /* Horizontal scaling, conversion has been done to buffer.            \
         * Rewind buffer and offset, then copy and scale line */              \
        p_buffer = p_buffer_start;                                            \
        p_offset = p_offset_start;                                            \
        for( i_x = pVout->m_output.i_width / 16; i_x--; )                      \
        {                                                                     \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
        }                                                                     \
        for( i_x = pVout->m_output.i_width & 15; i_x--; )                      \
        {                                                                     \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
        }                                                                     \
        p_pic = (UINT32 *)((UINT8*)p_pic + i_right_margin );                   \
    }                                                                         \
    else                                                                      \
    {                                                                         \
        /* No scaling, conversion has been done directly in picture memory.   \
         * Increment of picture pointer to end of line is still needed */     \
        p_pic = (UINT32 *)((UINT8*)p_pic + p_dest->p->i_pitch );               \
    }                                                                         \

#define SCALE_WIDTH16                                                           \
    if( b_hscale )                                                            \
    {                                                                         \
        /* Horizontal scaling, conversion has been done to buffer.            \
         * Rewind buffer and offset, then copy and scale line */              \
        p_buffer = p_buffer_start;                                            \
        p_offset = p_offset_start;                                            \
        for( i_x = pVout->m_output.i_width / 16; i_x--; )                      \
        {                                                                     \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
        }                                                                     \
        for( i_x = pVout->m_output.i_width & 15; i_x--; )                      \
        {                                                                     \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
        }                                                                     \
        p_pic = (UINT16 *)((UINT8*)p_pic + i_right_margin );                   \
    }                                                                         \
    else                                                                      \
    {                                                                         \
        /* No scaling, conversion has been done directly in picture memory.   \
         * Increment of picture pointer to end of line is still needed */     \
        p_pic = (UINT16 *)((UINT8*)p_pic + p_dest->p->i_pitch );               \
    }                                                                         \

/*****************************************************************************
 * SCALE_WIDTH_DITHER: scale a line horizontally for dithered 8 bpp
 *****************************************************************************
 * This macro scales a line using an offset array.
 *****************************************************************************/
#define SCALE_WIDTH_DITHER( CHROMA )                                          \
    if( b_hscale )                                                            \
    {                                                                         \
        /* Horizontal scaling - we can't use a buffer due to dithering */     \
        p_offset = p_offset_start;                                            \
        for( i_x = pVout->m_output.i_width / 16; i_x--; )                      \
        {                                                                     \
            CONVERT_4YUV_PIXEL_SCALE( CHROMA )                                \
            CONVERT_4YUV_PIXEL_SCALE( CHROMA )                                \
            CONVERT_4YUV_PIXEL_SCALE( CHROMA )                                \
            CONVERT_4YUV_PIXEL_SCALE( CHROMA )                                \
        }                                                                     \
    }                                                                         \
    else                                                                      \
    {                                                                         \
        for( i_x = pVout->m_render.i_width / 16; i_x--;  )                     \
        {                                                                     \
            CONVERT_4YUV_PIXEL( CHROMA )                                      \
            CONVERT_4YUV_PIXEL( CHROMA )                                      \
            CONVERT_4YUV_PIXEL( CHROMA )                                      \
            CONVERT_4YUV_PIXEL( CHROMA )                                      \
        }                                                                     \
    }                                                                         \
    /* Increment of picture pointer to end of line is still needed */         \
    p_pic = (UINT8*)((UINT8*)p_pic + i_right_margin );                       \
                                                                              \
    /* Increment the Y coordinate in the matrix, modulo 4 */                  \
    i_real_y = (i_real_y + 1) & 0x3;                                          \

/*****************************************************************************
 * SCALE_HEIGHT: handle vertical scaling
 *****************************************************************************
 * This macro handle vertical scaling for a picture. CHROMA may be 420, 422 or
 * 444 for RGB conversion, or 400 for gray conversion. It works for 1, 2, 3
 * and 4 Bpp.
 *****************************************************************************/
#define SCALE_HEIGHT( CHROMA, BPP )                                           \
    /* If line is odd, rewind 4:2:0 U and V samples */                        \
    if( ((CHROMA == 420) || (CHROMA == 422)) && !(i_y & 0x1) )                \
    {                                                                         \
        p_u -= i_chroma_width;                                                \
        p_v -= i_chroma_width;                                                \
    }                                                                         \
                                                                              \
    /*                                                                        \
     * Handle vertical scaling. The current line can be copied or next one    \
     * can be ignored.                                                        \
     */                                                                       \
    switch( i_vscale )                                                        \
    {                                                                         \
    case -1:                             /* vertical scaling factor is < 1 */ \
        while( (i_scale_count -= pVout->m_output.i_height) > 0 )               \
        {                                                                     \
            /* Height reduction: skip next source line */                     \
            p_y += pVout->m_render.i_width;                                    \
            i_y++;                                                            \
            if( (CHROMA == 420) || (CHROMA == 422) )                          \
            {                                                                 \
                if( i_y & 0x1 )                                               \
                {                                                             \
                    p_u += i_chroma_width;                                    \
                    p_v += i_chroma_width;                                    \
                }                                                             \
            }                                                                 \
            else if( CHROMA == 444 )                                          \
            {                                                                 \
                p_u += pVout->m_render.i_width;                                \
                p_v += pVout->m_render.i_width;                                \
            }                                                                 \
        }                                                                     \
        i_scale_count += pVout->m_render.i_height;                             \
        break;                                                                \
    case 1:                              /* vertical scaling factor is > 1 */ \
        while( (i_scale_count -= pVout->m_render.i_height) > 0 )               \
        {                                                                     \
            /* Height increment: copy previous picture line */                \
            memcpy( p_pic, p_pic_start,                     \
                                      pVout->m_output.i_width * BPP );         \
            p_pic = (UINT32*)((UINT8*)p_pic + p_dest->p->i_pitch );           \
        }                                                                     \
        i_scale_count += pVout->m_output.i_height;                             \
        break;                                                                \
    }                                                                         \

#define SCALE_HEIGHT16( CHROMA, BPP )                                           \
    /* If line is odd, rewind 4:2:0 U and V samples */                        \
    if( ((CHROMA == 420) || (CHROMA == 422)) && !(i_y & 0x1) )                \
    {                                                                         \
        p_u -= i_chroma_width;                                                \
        p_v -= i_chroma_width;                                                \
    }                                                                         \
                                                                              \
    /*                                                                        \
     * Handle vertical scaling. The current line can be copied or next one    \
     * can be ignored.                                                        \
     */                                                                       \
    switch( i_vscale )                                                        \
    {                                                                         \
    case -1:                             /* vertical scaling factor is < 1 */ \
        while( (i_scale_count -= pVout->m_output.i_height) > 0 )               \
        {                                                                     \
            /* Height reduction: skip next source line */                     \
            p_y += pVout->m_render.i_width;                                    \
            i_y++;                                                            \
            if( (CHROMA == 420) || (CHROMA == 422) )                          \
            {                                                                 \
                if( i_y & 0x1 )                                               \
                {                                                             \
                    p_u += i_chroma_width;                                    \
                    p_v += i_chroma_width;                                    \
                }                                                             \
            }                                                                 \
            else if( CHROMA == 444 )                                          \
            {                                                                 \
                p_u += pVout->m_render.i_width;                                \
                p_v += pVout->m_render.i_width;                                \
            }                                                                 \
        }                                                                     \
        i_scale_count += pVout->m_render.i_height;                             \
        break;                                                                \
    case 1:                              /* vertical scaling factor is > 1 */ \
        while( (i_scale_count -= pVout->m_render.i_height) > 0 )               \
        {                                                                     \
            /* Height increment: copy previous picture line */                \
            memcpy( p_pic, p_pic_start,                     \
                                      pVout->m_output.i_width * BPP );         \
            p_pic = (UINT16*)((UINT8*)p_pic + p_dest->p->i_pitch );           \
        }                                                                     \
        i_scale_count += pVout->m_output.i_height;                             \
        break;                                                                \
    }                                                                         \

/*****************************************************************************
 * SCALE_HEIGHT_DITHER: handle vertical scaling for dithered 8 bpp
 *****************************************************************************
 * This macro handles vertical scaling for a picture. CHROMA may be 420,
 * 422 or 444 for RGB conversion, or 400 for gray conversion.
 *****************************************************************************/
#define SCALE_HEIGHT_DITHER( CHROMA )                                         \
                                                                              \
    /* If line is odd, rewind 4:2:0 U and V samples */                        \
    if( ((CHROMA == 420) || (CHROMA == 422)) && !(i_y & 0x1) )                \
    {                                                                         \
        p_u -= i_chroma_width;                                                \
        p_v -= i_chroma_width;                                                \
    }                                                                         \
                                                                              \
    /*                                                                        \
     * Handle vertical scaling. The current line can be copied or next one    \
     * can be ignored.                                                        \
     */                                                                       \
                                                                              \
    switch( i_vscale )                                                        \
    {                                                                         \
    case -1:                             /* vertical scaling factor is < 1 */ \
        while( (i_scale_count -= pVout->m_output.i_height) > 0 )               \
        {                                                                     \
            /* Height reduction: skip next source line */                     \
            p_y += pVout->m_render.i_width;                                    \
            i_y++;                                                            \
            if( (CHROMA == 420) || (CHROMA == 422) )                          \
            {                                                                 \
                if( i_y & 0x1 )                                               \
                {                                                             \
                    p_u += i_chroma_width;                                    \
                    p_v += i_chroma_width;                                    \
                }                                                             \
            }                                                                 \
            else if( CHROMA == 444 )                                          \
            {                                                                 \
                p_u += pVout->m_render.i_width;                                \
                p_v += pVout->m_render.i_width;                                \
            }                                                                 \
        }                                                                     \
        i_scale_count += pVout->m_render.i_height;                             \
        break;                                                                \
    case 1:                              /* vertical scaling factor is > 1 */ \
        while( (i_scale_count -= pVout->m_render.i_height) > 0 )               \
        {                                                                     \
            p_y -= pVout->m_render.i_width;                                    \
            p_u -= i_chroma_width;                                            \
            p_v -= i_chroma_width;                                            \
            SCALE_WIDTH_DITHER( CHROMA );                                     \
        }                                                                     \
        i_scale_count += pVout->m_output.i_height;                             \
        break;                                                                \
    }                                                                         \

#endif