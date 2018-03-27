#include "stdafx.h"
#include <math.h>
#include "video_output.h"
#include "i420_rgb.h"

#define MODULE_NAME_IS_i420_rgb

/*****************************************************************************
 * SetOffset: build offset array for conversion functions
 *****************************************************************************
 * This function will build an offset array used in later conversion functions.
 * It will also set horizontal and vertical scaling indicators. The p_offset
 * structure has interleaved Y and U/V offsets.
 *****************************************************************************/
static void SetOffsetRGB8( int i_width, int i_height, int i_pic_width,
                       int i_pic_height, bool *pb_hscale,
                       int *pi_vscale, int *p_offset )
{
    int i_x;                                    /* x position in destination */
    int i_scale_count;                                     /* modulo counter */

    /*
     * Prepare horizontal offset array
     */
    if( i_pic_width - i_width == 0 )
    {
        /* No horizontal scaling: YUV conversion is done directly to picture */
        *pb_hscale = 0;
    }
    else if( i_pic_width - i_width > 0 )
    {
        int i_dummy = 0;

        /* Prepare scaling array for horizontal extension */
        *pb_hscale = 1;
        i_scale_count = i_pic_width;
        for( i_x = i_width; i_x--; )
        {
            while( (i_scale_count -= i_width) > 0 )
            {
                *p_offset++ = 0;
                *p_offset++ = 0;
            }
            *p_offset++ = 1;
            *p_offset++ = i_dummy;
            i_dummy = 1 - i_dummy;
            i_scale_count += i_pic_width;
        }
    }
    else /* if( i_pic_width - i_width < 0 ) */
    {
        int i_remainder = 0;
        int i_jump;

        /* Prepare scaling array for horizontal reduction */
        *pb_hscale = 1;
        i_scale_count = i_width;
        for( i_x = i_pic_width; i_x--; )
        {
            i_jump = 1;
            while( (i_scale_count -= i_pic_width) > 0 )
            {
                i_jump += 1;
            }
            *p_offset++ = i_jump;
            *p_offset++ = ( i_jump += i_remainder ) >> 1;
            i_remainder = i_jump & 1;
            i_scale_count += i_width;
        }
    }

    /*
     * Set vertical scaling indicator
     */
    if( i_pic_height - i_height == 0 )
    {
        *pi_vscale = 0;
    }
    else if( i_pic_height - i_height > 0 )
    {
        *pi_vscale = 1;
    }
    else /* if( i_pic_height - i_height < 0 ) */
    {
        *pi_vscale = -1;
    }
}

void I420_RGB8(CVideoOutput *pVout, picture_t *p_src, picture_t *p_dest )
{
    /* We got this one from the old arguments */
    UINT8 *p_pic = (UINT8*)p_dest->p->p_pixels;
    UINT8 *p_y   = p_src->Y_PIXELS;
    UINT8 *p_u   = p_src->U_PIXELS;
    UINT8 *p_v   = p_src->V_PIXELS;

    bool  b_hscale;                         /* horizontal scaling type */
    int         i_vscale;                          /* vertical scaling type */
    unsigned int i_x, i_y;                /* horizontal and vertical indexes */
    unsigned int i_real_y;                                          /* y % 4 */
    int          i_right_margin;
    int          i_scale_count;                      /* scale modulo counter */
    unsigned int i_chroma_width = pVout->m_render.i_width / 2;/* chroma width */

    /* Lookup table */
    UINT8 *        p_lookup = (UINT8 *)pVout->m_chroma.p_base;

    /* Offset array pointer */
    int *       p_offset_start = pVout->m_chroma.p_offset;
    int *       p_offset;

    const int i_source_margin = p_src->p[0].i_pitch
                                 - p_src->p[0].i_visible_pitch;
    const int i_source_margin_c = p_src->p[1].i_pitch
                                 - p_src->p[1].i_visible_pitch;

    /* The dithering matrices */
    static int dither10[4] = {  0x0,  0x8,  0x2,  0xa };
    static int dither11[4] = {  0xc,  0x4,  0xe,  0x6 };
    static int dither12[4] = {  0x3,  0xb,  0x1,  0x9 };
    static int dither13[4] = {  0xf,  0x7,  0xd,  0x5 };

    static int dither20[4] = {  0x0, 0x10,  0x4, 0x14 };
    static int dither21[4] = { 0x18,  0x8, 0x1c,  0xc };
    static int dither22[4] = {  0x6, 0x16,  0x2, 0x12 };
    static int dither23[4] = { 0x1e,  0xe, 0x1a,  0xa };

    SetOffsetRGB8( pVout->m_render.i_width, pVout->m_render.i_height,
               pVout->m_output.i_width, pVout->m_output.i_height,
               &b_hscale, &i_vscale, p_offset_start );

    i_right_margin = p_dest->p->i_pitch - p_dest->p->i_visible_pitch;

    /*
     * Perform conversion
     */
    i_scale_count = ( i_vscale == 1 ) ?
                    pVout->m_output.i_height : pVout->m_render.i_height;
    for( i_y = 0, i_real_y = 0; i_y < pVout->m_render.i_height; i_y++ )
    {
        /* Do horizontal and vertical scaling */
		SCALE_WIDTH_DITHER( 420 );
        SCALE_HEIGHT_DITHER( 420 );
    }

    p_y += i_source_margin;
    if( i_y % 2 )
    {
        p_u += i_source_margin_c;
        p_v += i_source_margin_c;
    }
}

/*****************************************************************************
 * SetOffset: build offset array for conversion functions
 *****************************************************************************
 * This function will build an offset array used in later conversion functions.
 * It will also set horizontal and vertical scaling indicators.
 *****************************************************************************/
static void SetOffsetRGB16( int i_width, int i_height, int i_pic_width,
                       int i_pic_height, bool *pb_hscale,
                       unsigned int *pi_vscale, int *p_offset )
{
    int i_x;                                    /* x position in destination */
    int i_scale_count;                                     /* modulo counter */

    /*
     * Prepare horizontal offset array
     */
    if( i_pic_width - i_width == 0 )
    {
        /* No horizontal scaling: YUV conversion is done directly to picture */
        *pb_hscale = 0;
    }
    else if( i_pic_width - i_width > 0 )
    {
        /* Prepare scaling array for horizontal extension */
        *pb_hscale = 1;
        i_scale_count = i_pic_width;
        for( i_x = i_width; i_x--; )
        {
            while( (i_scale_count -= i_width) > 0 )
            {
                *p_offset++ = 0;
            }
            *p_offset++ = 1;
            i_scale_count += i_pic_width;
        }
    }
    else /* if( i_pic_width - i_width < 0 ) */
    {
        /* Prepare scaling array for horizontal reduction */
        *pb_hscale = 1;
        i_scale_count = i_width;
        for( i_x = i_pic_width; i_x--; )
        {
            *p_offset = 1;
            while( (i_scale_count -= i_pic_width) > 0 )
            {
                *p_offset += 1;
            }
            p_offset++;
            i_scale_count += i_width;
        }
    }

    /*
     * Set vertical scaling indicator
     */
    if( i_pic_height - i_height == 0 )
    {
        *pi_vscale = 0;
    }
    else if( i_pic_height - i_height > 0 )
    {
        *pi_vscale = 1;
    }
    else /* if( i_pic_height - i_height < 0 ) */
    {
        *pi_vscale = -1;
    }
}

/*****************************************************************************
 * I420_RGB16: color YUV 4:2:0 to RGB 16 bpp
 *****************************************************************************
 * Horizontal alignment needed:
 *  - input: 8 pixels (8 Y bytes, 4 U/V bytes), margins not allowed
 *  - output: 1 pixel (2 bytes), margins allowed
 * Vertical alignment needed:
 *  - input: 2 lines (2 Y lines, 1 U/V line)
 *  - output: 1 line
 *****************************************************************************/
void I420_RGB16(CVideoOutput *pVout, picture_t *p_src, picture_t *p_dest )
{
    /* We got this one from the old arguments */
    UINT16 *p_pic = (UINT16*)p_dest->p->p_pixels;
    UINT8  *p_y   = p_src->Y_PIXELS;
    UINT8  *p_u   = p_src->U_PIXELS;
    UINT8  *p_v   = p_src->V_PIXELS;

    bool  b_hscale;                         /* horizontal scaling type */
    unsigned int i_vscale;                          /* vertical scaling type */
    unsigned int i_x, i_y;                /* horizontal and vertical indexes */

    int         i_right_margin;
    int         i_rewind;
    int         i_scale_count;                       /* scale modulo counter */
    int         i_chroma_width = pVout->m_render.i_width / 2; /* chroma width */
    UINT16 *  p_pic_start;       /* beginning of the current line for copy */
#if defined (MODULE_NAME_IS_i420_rgb)
    int         i_uval, i_vval;                           /* U and V samples */
    int         i_red, i_green, i_blue;          /* U and V modified samples */
    UINT16 *  p_yuv = pVout->m_chroma.p_rgb16;
    UINT16 *  p_ybase;                     /* Y dependant conversion table */
#endif

    /* Conversion buffer pointer */
    UINT16 *  p_buffer_start = (UINT16*)pVout->m_chroma.p_buffer;
    UINT16 *  p_buffer;

    /* Offset array pointer */
    int *       p_offset_start = pVout->m_chroma.p_offset;
    int *       p_offset;

    const int i_source_margin = p_src->p[0].i_pitch
                                 - p_src->p[0].i_visible_pitch;
    const int i_source_margin_c = p_src->p[1].i_pitch
                                 - p_src->p[1].i_visible_pitch;

    i_right_margin = p_dest->p->i_pitch - p_dest->p->i_visible_pitch;

    if( pVout->m_render.i_width & 7 )
    {
        i_rewind = 8 - ( pVout->m_render.i_width & 7 );
    }
    else
    {
        i_rewind = 0;
    }

    /* Rule: when a picture of size (x1,y1) with aspect ratio r1 is rendered
     * on a picture of size (x2,y2) with aspect ratio r2, if x1 grows to x1'
     * then y1 grows to y1' = x1' * y2/x2 * r2/r1 */
    SetOffsetRGB16( pVout->m_render.i_width, pVout->m_render.i_height,
               pVout->m_output.i_width, pVout->m_output.i_height,
               &b_hscale, &i_vscale, p_offset_start );

    /*
     * Perform conversion
     */
    i_scale_count = ( i_vscale == 1 ) ?
                    pVout->m_output.i_height : pVout->m_render.i_height;
    for( i_y = 0; i_y < pVout->m_render.i_height; i_y++ )
    {
        p_pic_start = p_pic;
        p_buffer = b_hscale ? p_buffer_start : p_pic;

#if defined (MODULE_NAME_IS_i420_rgb)
        for ( i_x = pVout->m_render.i_width / 8; i_x--; )
        {
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
       }
#elif defined (MODULE_NAME_IS_i420_rgb_mmx)
        if( pVout->m_output.i_rmask == 0x7c00 )
        {
            /* 15bpp 5/5/5 */
            for ( i_x = pVout->m_render.i_width / 8; i_x--; )
            {
#   if defined (HAVE_MMX_INTRINSICS)
                __m64 mm0, mm1, mm2, mm3, mm4, mm5, mm6, mm7;
                uint64_t tmp64;
                INTRINSICS_INIT_16
                INTRINSICS_YUV_MUL
                INTRINSICS_YUV_ADD
                INTRINSICS_UNPACK_15
 #   else
                __asm__( MMX_INIT_16
                         : : "r" (p_y), "r" (p_u), "r" (p_v), "r" (p_buffer) );

                __asm__( ".p2align 3"
                         MMX_YUV_MUL
                         MMX_YUV_ADD
                         MMX_UNPACK_15
                         : : "r" (p_y), "r" (p_u), "r" (p_v), "r" (p_buffer) );
#   endif

                p_y += 8;
                p_u += 4;
                p_v += 4;
                p_buffer += 8;
            }
        }
        else
        {
            /* 16bpp 5/6/5 */
            for ( i_x = pVout->m_render.i_width / 8; i_x--; )
            {
#   if defined (HAVE_MMX_INTRINSICS)
                __m64 mm0, mm1, mm2, mm3, mm4, mm5, mm6, mm7;
                uint64_t tmp64;
                INTRINSICS_INIT_16
                INTRINSICS_YUV_MUL
                INTRINSICS_YUV_ADD
                INTRINSICS_UNPACK_16
#   else
                __asm__( MMX_INIT_16
                         : : "r" (p_y), "r" (p_u), "r" (p_v), "r" (p_buffer) );

                __asm__( ".p2align 3"
                         MMX_YUV_MUL
                         MMX_YUV_ADD
                         MMX_UNPACK_16
                         : : "r" (p_y), "r" (p_u), "r" (p_v), "r" (p_buffer) );
#   endif

                p_y += 8;
                p_u += 4;
                p_v += 4;
                p_buffer += 8;
            }
        }
#endif

        /* Here we do some unaligned reads and duplicate conversions, but
         * at least we have all the pixels */
        if( i_rewind )
        {
#if defined (MODULE_NAME_IS_i420_rgb_mmx)
#   if defined (HAVE_MMX_INTRINSICS)
            __m64 mm0, mm1, mm2, mm3, mm4, mm5, mm6, mm7;
            uint64_t tmp64;
#   endif
#endif
            p_y -= i_rewind;
            p_u -= i_rewind >> 1;
            p_v -= i_rewind >> 1;
            p_buffer -= i_rewind;
#if defined (MODULE_NAME_IS_i420_rgb)
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
#elif defined (MODULE_NAME_IS_i420_rgb_mmx)

#   if defined (HAVE_MMX_INTRINSICS)
            INTRINSICS_INIT_16
#   else
            __asm__( MMX_INIT_16
                     : : "r" (p_y), "r" (p_u), "r" (p_v), "r" (p_buffer) );
#   endif

            if( pVout->m_output.i_rmask == 0x7c00 )
            {
                /* 15bpp 5/5/5 */
#   if defined (HAVE_MMX_INTRINSICS)
                INTRINSICS_YUV_MUL
                INTRINSICS_YUV_ADD
                INTRINSICS_UNPACK_15
#   else
                __asm__( ".p2align 3"
                         MMX_YUV_MUL
                         MMX_YUV_ADD
                         MMX_UNPACK_15
                         : : "r" (p_y), "r" (p_u), "r" (p_v), "r" (p_buffer) );
#   endif
            }
            else
            {
#   if defined (HAVE_MMX_INTRINSICS)
                INTRINSICS_YUV_MUL
                INTRINSICS_YUV_ADD
                INTRINSICS_UNPACK_16
#   else
                /* 16bpp 5/6/5 */
                __asm__( ".p2align 3"
                         MMX_YUV_MUL
                         MMX_YUV_ADD
                         MMX_UNPACK_16
                         : : "r" (p_y), "r" (p_u), "r" (p_v), "r" (p_buffer) );
#   endif
            }

            p_y += 8;
            p_u += 4;
            p_v += 4;
            p_buffer += 8;
#endif
        }
        SCALE_WIDTH16;
        SCALE_HEIGHT16( 420, 2 );

        p_y += i_source_margin;
        if( i_y % 2 )
        {
            p_u += i_source_margin_c;
            p_v += i_source_margin_c;
        }
    }
#if defined (MODULE_NAME_IS_i420_rgb_mmx)
    /* re-enable FPU registers */
#   if defined (HAVE_MMX_INTRINSICS)
    _mm_empty();
#   else
    __asm__ __volatile__ ("emms");
#   endif
#endif
}

/*****************************************************************************
 * I420_RGB32: color YUV 4:2:0 to RGB 32 bpp
 *****************************************************************************
 * Horizontal alignment needed:
 *  - input: 8 pixels (8 Y bytes, 4 U/V bytes), margins not allowed
 *  - output: 1 pixel (2 bytes), margins allowed
 * Vertical alignment needed:
 *  - input: 2 lines (2 Y lines, 1 U/V line)
 *  - output: 1 line
 *****************************************************************************/
void I420_RGB32(CVideoOutput *pVout, picture_t *p_src, picture_t *p_dest )
{
    /* We got this one from the old arguments */
    UINT32 *p_pic = (UINT32*)p_dest->p->p_pixels;
    UINT8  *p_y   = p_src->Y_PIXELS;
    UINT8  *p_u   = p_src->U_PIXELS;
    UINT8  *p_v   = p_src->V_PIXELS;

    bool  b_hscale;                         /* horizontal scaling type */
    unsigned int i_vscale;                          /* vertical scaling type */
    unsigned int i_x, i_y;                /* horizontal and vertical indexes */

    int         i_right_margin;
    int         i_rewind;
    int         i_scale_count;                       /* scale modulo counter */
    int         i_chroma_width = pVout->m_render.i_width / 2; /* chroma width */
    UINT32 *  p_pic_start;       /* beginning of the current line for copy */
#if defined (MODULE_NAME_IS_i420_rgb)
    int         i_uval, i_vval;                           /* U and V samples */
    int         i_red, i_green, i_blue;          /* U and V modified samples */
    UINT32 *  p_yuv = pVout->m_chroma.p_rgb32;
    UINT32 *  p_ybase;                     /* Y dependant conversion table */
#endif

    /* Conversion buffer pointer */
    UINT32 *  p_buffer_start = (UINT32*)pVout->m_chroma.p_buffer;
    UINT32 *  p_buffer;

    /* Offset array pointer */
    int *       p_offset_start = pVout->m_chroma.p_offset;
    int *       p_offset;

	const int i_source_margin = p_src->p[0].i_pitch
                                 - p_src->p[0].i_visible_pitch;
    const int i_source_margin_c = p_src->p[1].i_pitch
                                 - p_src->p[1].i_visible_pitch;

    i_right_margin = p_dest->p->i_pitch - p_dest->p->i_visible_pitch;

    if( pVout->m_render.i_width & 7 )
    {
        i_rewind = 8 - ( pVout->m_render.i_width & 7 );
    }
    else
    {
        i_rewind = 0;
    }

    /* Rule: when a picture of size (x1,y1) with aspect ratio r1 is rendered
     * on a picture of size (x2,y2) with aspect ratio r2, if x1 grows to x1'
     * then y1 grows to y1' = x1' * y2/x2 * r2/r1 */
    SetOffsetRGB16( pVout->m_render.i_width, pVout->m_render.i_height,
               pVout->m_output.i_width, pVout->m_output.i_height,
               &b_hscale, &i_vscale, p_offset_start );

    /*
     * Perform conversion
     */
    i_scale_count = ( i_vscale == 1 ) ?
                    pVout->m_output.i_height : pVout->m_render.i_height;
    for( i_y = 0; i_y < pVout->m_render.i_height; i_y++ )
    {
        p_pic_start = p_pic;
        p_buffer = b_hscale ? p_buffer_start : p_pic;

        for ( i_x = pVout->m_render.i_width / 8; i_x--; )
        {
#if defined (MODULE_NAME_IS_i420_rgb)
            CONVERT_YUV_PIXEL(4);  CONVERT_Y_PIXEL(4);
            CONVERT_YUV_PIXEL(4);  CONVERT_Y_PIXEL(4);
            CONVERT_YUV_PIXEL(4);  CONVERT_Y_PIXEL(4);
            CONVERT_YUV_PIXEL(4);  CONVERT_Y_PIXEL(4);
#elif defined (MODULE_NAME_IS_i420_rgb_mmx)
#   if defined (HAVE_MMX_INTRINSICS)
            __m64 mm0, mm1, mm2, mm3, mm4, mm5, mm6, mm7;
            UINT64 tmp64;
            INTRINSICS_INIT_32
            INTRINSICS_YUV_MUL
            INTRINSICS_YUV_ADD
            INTRINSICS_UNPACK_32
#   else
            __asm__( MMX_INIT_32
                     : : "r" (p_y), "r" (p_u), "r" (p_v), "r" (p_buffer) );

            __asm__( ".p2align 3"
                     MMX_YUV_MUL
                     MMX_YUV_ADD
                     MMX_UNPACK_32
                     : : "r" (p_y), "r" (p_u), "r" (p_v), "r" (p_buffer) );
#   endif

            p_y += 8;
            p_u += 4;
            p_v += 4;
            p_buffer += 8;
#endif
        }

        /* Here we do some unaligned reads and duplicate conversions, but
         * at least we have all the pixels */
        if( i_rewind )
        {
#if defined (MODULE_NAME_IS_i420_rgb_mmx)
#   if defined (HAVE_MMX_INTRINSICS)
            __m64 mm0, mm1, mm2, mm3, mm4, mm5, mm6, mm7;
            uint64_t tmp64;
#   endif
#endif
            p_y -= i_rewind;
            p_u -= i_rewind >> 1;
            p_v -= i_rewind >> 1;
            p_buffer -= i_rewind;
#if defined (MODULE_NAME_IS_i420_rgb)
            CONVERT_YUV_PIXEL(4);  CONVERT_Y_PIXEL(4);
            CONVERT_YUV_PIXEL(4);  CONVERT_Y_PIXEL(4);
            CONVERT_YUV_PIXEL(4);  CONVERT_Y_PIXEL(4);
            CONVERT_YUV_PIXEL(4);  CONVERT_Y_PIXEL(4);
#elif defined (MODULE_NAME_IS_i420_rgb_mmx)
#   if defined (HAVE_MMX_INTRINSICS)
            INTRINSICS_INIT_32
            INTRINSICS_YUV_MUL
            INTRINSICS_YUV_ADD
            INTRINSICS_UNPACK_32
#   else
            __asm__( MMX_INIT_32
                     : : "r" (p_y), "r" (p_u), "r" (p_v), "r" (p_buffer) );

            __asm__( ".p2align 3"
                     MMX_YUV_MUL
                     MMX_YUV_ADD
                     MMX_UNPACK_32
                     : : "r" (p_y), "r" (p_u), "r" (p_v), "r" (p_buffer) );
#   endif

            p_y += 8;
            p_u += 4;
            p_v += 4;
            p_buffer += 8;
#endif
        }
        SCALE_WIDTH;
        SCALE_HEIGHT( 420, 4 );

        p_y += i_source_margin;
        if( i_y % 2 )
        {
            p_u += i_source_margin_c;
            p_v += i_source_margin_c;
        }
    }
#if defined (MODULE_NAME_IS_i420_rgb_mmx)
    /* re-enable FPU registers */
#   if defined (HAVE_MMX_INTRINSICS)
    _mm_empty();
#   else
    __asm__ __volatile__ ("emms");
#   endif
#endif
}

/*****************************************************************************
 * RGB2PIXEL: assemble RGB components to a pixel value, returns a UINT32
 *****************************************************************************/
#define RGB2PIXEL( i_r, i_g, i_b )          \
    (((((UINT32)i_r) >> pVout->m_output.i_rrshift) \
                       << pVout->m_output.i_lrshift) \
   | ((((UINT32)i_g) >> pVout->m_output.i_rgshift) \
                       << pVout->m_output.i_lgshift) \
   | ((((UINT32)i_b) >> pVout->m_output.i_rbshift) \
                       << pVout->m_output.i_lbshift))

void Set8bppPalette(CVideoOutput *pVout, UINT8 *p_rgb8 )
{
    #define CLIP( x ) ( ((x < 0) ? 0 : (x > 255) ? 255 : x) << 8 )

    int y,u,v;
    int r,g,b;
    int i = 0, j = 0;
    UINT16 *p_cmap_r=pVout->m_chroma.p_rgb_r;
    UINT16 *p_cmap_g=pVout->m_chroma.p_rgb_g;
    UINT16 *p_cmap_b=pVout->m_chroma.p_rgb_b;

    unsigned char p_lookup[PALETTE_TABLE_SIZE];

    /* This loop calculates the intersection of an YUV box and the RGB cube. */
    for ( y = 0; y <= 256; y += 16, i += 128 - 81 )
    {
        for ( u = 0; u <= 256; u += 32 )
        {
            for ( v = 0; v <= 256; v += 32 )
            {
                r = y + ( (V_RED_COEF*(v-128)) >> SHIFT );
                g = y + ( (U_GREEN_COEF*(u-128)
                         + V_GREEN_COEF*(v-128)) >> SHIFT );
                b = y + ( (U_BLUE_COEF*(u-128)) >> SHIFT );

                if( r >= 0x00 && g >= 0x00 && b >= 0x00
                        && r <= 0xff && g <= 0xff && b <= 0xff )
                {
                    /* This one should never happen unless someone
                     * fscked up my code */
                    if( j == 256 )
                    {
						_RPT0(_CRT_WARN, "no colors left in palette\n");
                        break;
                    }

                    /* Clip the colors */
                    p_cmap_r[ j ] = CLIP( r );
                    p_cmap_g[ j ] = CLIP( g );
                    p_cmap_b[ j ] = CLIP( b );

#if 0
		    printf("+++Alloc RGB cmap %d (%d, %d, %d)\n", j,
			   p_cmap_r[ j ] >>8, p_cmap_g[ j ] >>8, 
			   p_cmap_b[ j ] >>8);
#endif

                    /* Allocate color */
                    p_lookup[ i ] = 1;
                    p_rgb8[ i++ ] = j;
                    j++;
                }
                else
                {
                    p_lookup[ i ] = 0;
                    p_rgb8[ i++ ] = 0;
                }
            }
        }
    }

    /* The colors have been allocated, we can set the palette */
    pVout->m_output.pf_setpalette( p_cmap_r, p_cmap_g, p_cmap_b );

#if 0
    /* There will eventually be a way to know which colors
     * couldn't be allocated and try to find a replacement */
    p_vout->i_white_pixel = 0xff;
    p_vout->i_black_pixel = 0x00;
    p_vout->i_gray_pixel = 0x44;
    p_vout->i_blue_pixel = 0x3b;
#endif

    /* This loop allocates colors that got outside the RGB cube */
    for ( i = 0, y = 0; y <= 256; y += 16, i += 128 - 81 )
    {
        for ( u = 0; u <= 256; u += 32 )
        {
            for ( v = 0; v <= 256; v += 32, i++ )
            {
                int u2, v2, dist, mindist = 100000000;

                if( p_lookup[ i ] || y == 0 )
                {
                    continue;
                }

                /* Heavy. yeah. */
                for( u2 = 0; u2 <= 256; u2 += 32 )
                {
                    for( v2 = 0; v2 <= 256; v2 += 32 )
                    {
                        j = ((y>>4)<<7) + (u2>>5)*9 + (v2>>5);
                        dist = (u-u2)*(u-u2) + (v-v2)*(v-v2);

                        /* Find the nearest color */
                        if( p_lookup[ j ] && dist < mindist )
                        {
                            p_rgb8[ i ] = p_rgb8[ j ];
                            mindist = dist;
                        }

                        j -= 128;

                        /* Find the nearest color */
                        if( p_lookup[ j ] && dist + 128 < mindist )
                        {
                            p_rgb8[ i ] = p_rgb8[ j ];
                            mindist = dist + 128;
                        }
                    }
                }
            }
        }
    }
}

/*****************************************************************************
 * SetGammaTable: return intensity table transformed by gamma curve.
 *****************************************************************************
 * pi_table is a table of 256 entries from 0 to 255.
 *****************************************************************************/
static void SetGammaTable( int *pi_table, double f_gamma )
{
    int i_y;                                               /* base intensity */

    /* Use exp(gamma) instead of gamma */
    f_gamma = exp( f_gamma );

    /* Build gamma table */
    for( i_y = 0; i_y < 256; i_y++ )
    {
        pi_table[ i_y ] = (int)( pow( (double)i_y / 256, f_gamma ) * 256 );
    }
}

/*****************************************************************************
 * SetYUV: compute tables and set function pointers
 *****************************************************************************/
void SetYUV(CVideoOutput *pVout)
{
    int          pi_gamma[256];                               /* gamma table */
    volatile int i_index;                                 /* index in tables */
                   /* We use volatile here to work around a strange gcc-3.3.4
                    * optimization bug */
    /* Build gamma table */
    SetGammaTable( pi_gamma, pVout->m_fGamma );

    /*
     * Set pointers and build YUV tables
     */

    /* Color: build red, green and blue tables */
    switch( pVout->m_output.i_chroma )
    {
    case TME_FOURCC('R','G','B','2'):
        pVout->m_chroma.p_rgb8 = (UINT8 *)pVout->m_chroma.p_base;
        Set8bppPalette(pVout, pVout->m_chroma.p_rgb8 );
        break;

    case TME_FOURCC('R','V','1','5'):
    case TME_FOURCC('R','V','1','6'):
        pVout->m_chroma.p_rgb16 = (UINT16 *)pVout->m_chroma.p_base;
        for( i_index = 0; i_index < RED_MARGIN; i_index++ )
        {
            pVout->m_chroma.p_rgb16[RED_OFFSET - RED_MARGIN + i_index] = RGB2PIXEL( pi_gamma[0], 0, 0 );
            pVout->m_chroma.p_rgb16[RED_OFFSET + 256 + i_index] =        RGB2PIXEL( pi_gamma[255], 0, 0 );
        }
        for( i_index = 0; i_index < GREEN_MARGIN; i_index++ )
        {
            pVout->m_chroma.p_rgb16[GREEN_OFFSET - GREEN_MARGIN + i_index] = RGB2PIXEL( 0, pi_gamma[0], 0 );
            pVout->m_chroma.p_rgb16[GREEN_OFFSET + 256 + i_index] =          RGB2PIXEL( 0, pi_gamma[255], 0 );
        }
        for( i_index = 0; i_index < BLUE_MARGIN; i_index++ )
        {
            pVout->m_chroma.p_rgb16[BLUE_OFFSET - BLUE_MARGIN + i_index] = RGB2PIXEL( 0, 0, pi_gamma[0] );
            pVout->m_chroma.p_rgb16[BLUE_OFFSET + BLUE_MARGIN + i_index] = RGB2PIXEL( 0, 0, pi_gamma[255] );
        }
        for( i_index = 0; i_index < 256; i_index++ )
        {
            pVout->m_chroma.p_rgb16[RED_OFFSET + i_index] =   RGB2PIXEL( pi_gamma[ i_index ], 0, 0 );
            pVout->m_chroma.p_rgb16[GREEN_OFFSET + i_index] = RGB2PIXEL( 0, pi_gamma[ i_index ], 0 );
            pVout->m_chroma.p_rgb16[BLUE_OFFSET + i_index] =  RGB2PIXEL( 0, 0, pi_gamma[ i_index ] );
        }
        break;

    case TME_FOURCC('R','V','2','4'):
    case TME_FOURCC('R','V','3','2'):
        pVout->m_chroma.p_rgb32 = (UINT32 *)pVout->m_chroma.p_base;
        for( i_index = 0; i_index < RED_MARGIN; i_index++ )
        {
            pVout->m_chroma.p_rgb32[RED_OFFSET - RED_MARGIN + i_index] = RGB2PIXEL( pi_gamma[0], 0, 0 );
            pVout->m_chroma.p_rgb32[RED_OFFSET + 256 + i_index] =        RGB2PIXEL( pi_gamma[255], 0, 0 );
        }
        for( i_index = 0; i_index < GREEN_MARGIN; i_index++ )
        {
            pVout->m_chroma.p_rgb32[GREEN_OFFSET - GREEN_MARGIN + i_index] = RGB2PIXEL( 0, pi_gamma[0], 0 );
            pVout->m_chroma.p_rgb32[GREEN_OFFSET + 256 + i_index] =          RGB2PIXEL( 0, pi_gamma[255], 0 );
        }
        for( i_index = 0; i_index < BLUE_MARGIN; i_index++ )
        {
            pVout->m_chroma.p_rgb32[BLUE_OFFSET - BLUE_MARGIN + i_index] = RGB2PIXEL( 0, 0, pi_gamma[0] );
            pVout->m_chroma.p_rgb32[BLUE_OFFSET + BLUE_MARGIN + i_index] = RGB2PIXEL( 0, 0, pi_gamma[255] );
        }
        for( i_index = 0; i_index < 256; i_index++ )
        {
            pVout->m_chroma.p_rgb32[RED_OFFSET + i_index] =   RGB2PIXEL( pi_gamma[ i_index ], 0, 0 );
            pVout->m_chroma.p_rgb32[GREEN_OFFSET + i_index] = RGB2PIXEL( 0, pi_gamma[ i_index ], 0 );
            pVout->m_chroma.p_rgb32[BLUE_OFFSET + i_index] =  RGB2PIXEL( 0, 0, pi_gamma[ i_index ] );
        }
        break;
    }
}

int ActivateChromaRGB(CVideoOutput *pVout)
{
    size_t i_tables_size;

    if( pVout->m_render.i_width & 1 || pVout->m_render.i_height & 1 )
    {
        return -1;
    }

    switch( pVout->m_render.i_chroma )
    {
        case TME_FOURCC('Y','V','1','2'):
        case TME_FOURCC('I','4','2','0'):
        case TME_FOURCC('I','Y','U','V'):
            switch( pVout->m_output.i_chroma )
            {
                case TME_FOURCC('R','G','B','2'):
					pVout->m_chroma.pf_convert = I420_RGB8;
                    break;
                case TME_FOURCC('R','V','1','5'):
                case TME_FOURCC('R','V','1','6'):
                    pVout->m_chroma.pf_convert = I420_RGB16;
                    break;

                case TME_FOURCC('R','V','3','2'):
                    pVout->m_chroma.pf_convert = I420_RGB32;
                   break;

                default:
                    return -1;
            }
            break;

        default:
            return -1;
    }
#if 0
    p_vout->chroma.p_sys = malloc( sizeof( chroma_sys_t ) );
    if( p_vout->chroma.p_sys == NULL )
    {
        return -1;
    }
#endif

    switch( pVout->m_output.i_chroma )
    {
        case TME_FOURCC('R','G','B','2'):
            pVout->m_chroma.p_buffer = (UINT8 *)malloc( VOUT_MAX_WIDTH );
            break;

        case TME_FOURCC('R','V','1','5'):
        case TME_FOURCC('R','V','1','6'):
            pVout->m_chroma.p_buffer = (UINT8 *)malloc( VOUT_MAX_WIDTH * 2 );
            break;

        case TME_FOURCC('R','V','2','4'):
        case TME_FOURCC('R','V','3','2'):
            pVout->m_chroma.p_buffer = (UINT8 *)malloc( VOUT_MAX_WIDTH * 4 );
            break;

        default:
            pVout->m_chroma.p_buffer = NULL;
            break;
    }

    if( pVout->m_chroma.p_buffer == NULL )
    {
        //free( p_vout->chroma.p_sys );
        return -1;
    }

    pVout->m_chroma.p_offset = (int *)malloc( pVout->m_output.i_width
                    * ( ( pVout->m_output.i_chroma
                           == TME_FOURCC('R','G','B','2') ) ? 2 : 1 )
                    * sizeof( int ) );
    if( pVout->m_chroma.p_offset == NULL )
    {
        free( pVout->m_chroma.p_buffer );
        //free( p_vout->chroma.p_sys );
        return -1;
    }

    switch( pVout->m_output.i_chroma )
    {
    case TME_FOURCC('R','G','B','2'):
        i_tables_size = sizeof( UINT8 ) * PALETTE_TABLE_SIZE;
        break;
    case TME_FOURCC('R','V','1','5'):
    case TME_FOURCC('R','V','1','6'):
        i_tables_size = sizeof( UINT16 ) * RGB_TABLE_SIZE;
        break;
    default: /* RV24, RV32 */
        i_tables_size = sizeof( UINT32 ) * RGB_TABLE_SIZE;
        break;
    }

    pVout->m_chroma.p_base = malloc( i_tables_size );
    if( pVout->m_chroma.p_base == NULL )
    {
        free( pVout->m_chroma.p_offset );
        free( pVout->m_chroma.p_buffer );
        //free( p_vout->chroma.p_sys );
        return -1;
    }

    SetYUV(pVout);
    return 0;
}

