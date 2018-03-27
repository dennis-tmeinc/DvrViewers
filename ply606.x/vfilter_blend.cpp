#include "stdafx.h"
#include <crtdbg.h>
#include "vfilter_blend.h"
#include "tmeviewdef.h"

static void Blend( filter_t *, picture_t *, picture_t *, picture_t *,
                   int, int, int );
static void BlendI420( filter_t *, picture_t *, picture_t *, picture_t *,
                       int, int, int, int, int );
static void BlendR16( filter_t *, picture_t *, picture_t *, picture_t *,
                      int, int, int, int, int );
static void BlendR24( filter_t *, picture_t *, picture_t *, picture_t *,
                      int, int, int, int, int );
static void BlendYUVPacked( filter_t *, picture_t *, picture_t *, picture_t *,
                            int, int, int, int, int );
static void BlendPalI420( filter_t *, picture_t *, picture_t *, picture_t *,
                          int, int, int, int, int );
static void BlendPalYUVPacked( filter_t *, picture_t *, picture_t *, picture_t *,
                               int, int, int, int, int );
static void BlendPalRV( filter_t *, picture_t *, picture_t *, picture_t *,
                        int, int, int, int, int );


int OpenFilterBlender( filter_t *p_filter )
{
    /* Check if we can handle that format.
     * We could try to use a chroma filter if we can't. */
    if( ( p_filter->fmt_in.video.i_chroma != TME_FOURCC('Y','U','V','A') &&
          p_filter->fmt_in.video.i_chroma != TME_FOURCC('Y','U','V','P') ) ||
        ( p_filter->fmt_out.video.i_chroma != TME_FOURCC('I','4','2','0') &&
          p_filter->fmt_out.video.i_chroma != TME_FOURCC('Y','U','Y','2') &&
          p_filter->fmt_out.video.i_chroma != TME_FOURCC('Y','V','1','2') &&
          p_filter->fmt_out.video.i_chroma != TME_FOURCC('U','Y','V','Y') &&
          p_filter->fmt_out.video.i_chroma != TME_FOURCC('Y','V','Y','U') &&
          p_filter->fmt_out.video.i_chroma != TME_FOURCC('R','V','1','6') &&
          p_filter->fmt_out.video.i_chroma != TME_FOURCC('R','V','2','4') &&
          p_filter->fmt_out.video.i_chroma != TME_FOURCC('R','V','3','2') ) )
    {
        return EGENERIC;
    }

    _RPT2( _CRT_WARN, "chroma: %4.4s -> %4.4s\n",
             (char *)&p_filter->fmt_in.video.i_chroma,
             (char *)&p_filter->fmt_out.video.i_chroma );


    return SUCCESS;
}

void VideoBlend( filter_t *p_filter, picture_t *p_dst,
                   picture_t *p_dst_orig, picture_t *p_src,
                   int i_x_offset, int i_y_offset, int i_alpha )
{
    int i_width, i_height;

    i_width = min((int)p_filter->fmt_out.video.i_visible_width - i_x_offset,
                    (int)p_filter->fmt_in.video.i_visible_width);

    i_height = min((int)p_filter->fmt_out.video.i_visible_height -i_y_offset,
                     (int)p_filter->fmt_in.video.i_visible_height);

    if( i_width <= 0 || i_height <= 0 ) return;

    if( p_filter->fmt_in.video.i_chroma == TME_FOURCC('Y','U','V','A') &&
        ( p_filter->fmt_out.video.i_chroma == TME_FOURCC('I','4','2','0') ||
          p_filter->fmt_out.video.i_chroma == TME_FOURCC('Y','V','1','2') ) )
    {
        BlendI420( p_filter, p_dst, p_dst_orig, p_src,
                   i_x_offset, i_y_offset, i_width, i_height, i_alpha );
        return;
    }
    if( p_filter->fmt_in.video.i_chroma == TME_FOURCC('Y','U','V','A') &&
        ( p_filter->fmt_out.video.i_chroma == TME_FOURCC('Y','U','Y','2') ||
          p_filter->fmt_out.video.i_chroma == TME_FOURCC('U','Y','V','Y') ||
          p_filter->fmt_out.video.i_chroma == TME_FOURCC('Y','V','Y','U') ) )
    {
        BlendYUVPacked( p_filter, p_dst, p_dst_orig, p_src,
                   i_x_offset, i_y_offset, i_width, i_height, i_alpha );
        return;
    }
    if( p_filter->fmt_in.video.i_chroma == TME_FOURCC('Y','U','V','A') &&
        p_filter->fmt_out.video.i_chroma == TME_FOURCC('R','V','1','6') )
    {
        BlendR16( p_filter, p_dst, p_dst_orig, p_src,
                  i_x_offset, i_y_offset, i_width, i_height, i_alpha );
        return;
    }
    if( p_filter->fmt_in.video.i_chroma == TME_FOURCC('Y','U','V','A') &&
        ( p_filter->fmt_out.video.i_chroma == TME_FOURCC('R','V','2','4') ||
          p_filter->fmt_out.video.i_chroma == TME_FOURCC('R','V','3','2') ) )
    {
        BlendR24( p_filter, p_dst, p_dst_orig, p_src,
                  i_x_offset, i_y_offset, i_width, i_height, i_alpha );
        return;
    }
    if( p_filter->fmt_in.video.i_chroma == TME_FOURCC('Y','U','V','P') &&
        ( p_filter->fmt_out.video.i_chroma == TME_FOURCC('I','4','2','0') ||
          p_filter->fmt_out.video.i_chroma == TME_FOURCC('Y','V','1','2') ) )
    {
        BlendPalI420( p_filter, p_dst, p_dst_orig, p_src,
                      i_x_offset, i_y_offset, i_width, i_height, i_alpha );
        return;
    }
    if( p_filter->fmt_in.video.i_chroma == TME_FOURCC('Y','U','V','P') &&
        ( p_filter->fmt_out.video.i_chroma == TME_FOURCC('Y','U','Y','2') ||
          p_filter->fmt_out.video.i_chroma == TME_FOURCC('U','Y','V','Y') ||
          p_filter->fmt_out.video.i_chroma == TME_FOURCC('Y','V','Y','U') ) )
    {
        BlendPalYUVPacked( p_filter, p_dst, p_dst_orig, p_src,
                      i_x_offset, i_y_offset, i_width, i_height, i_alpha );
        return;
    }
    if( p_filter->fmt_in.video.i_chroma == TME_FOURCC('Y','U','V','P') &&
        ( p_filter->fmt_out.video.i_chroma == TME_FOURCC('R','V','1','6') ||
          p_filter->fmt_out.video.i_chroma == TME_FOURCC('R','V','2','4') ||
          p_filter->fmt_out.video.i_chroma == TME_FOURCC('R','V','3','2') ) )
    {
        BlendPalRV( p_filter, p_dst, p_dst_orig, p_src,
                    i_x_offset, i_y_offset, i_width, i_height, i_alpha );
        return;
    }

    _RPT0( _CRT_WARN, "no matching alpha blending routine\n" );
}

static void BlendI420( filter_t *p_filter, picture_t *p_dst,
                       picture_t *p_dst_orig, picture_t *p_src,
                       int i_x_offset, int i_y_offset,
                       int i_width, int i_height, int i_alpha )
{
    int i_src1_pitch, i_src2_pitch, i_dst_pitch;
    UINT8 *p_src1_y, *p_src2_y, *p_dst_y;
    UINT8 *p_src1_u, *p_src2_u, *p_dst_u;
    UINT8 *p_src1_v, *p_src2_v, *p_dst_v;
    UINT8 *p_trans;
    int i_x, i_y, i_trans;
    BOOL b_even_scanline = i_y_offset % 2;

    i_dst_pitch = p_dst->p[Y_PLANE].i_pitch;
    p_dst_y = p_dst->p[Y_PLANE].p_pixels + i_x_offset +
              p_filter->fmt_out.video.i_x_offset +
              p_dst->p[Y_PLANE].i_pitch *
              ( i_y_offset + p_filter->fmt_out.video.i_y_offset );
    p_dst_u = p_dst->p[U_PLANE].p_pixels + i_x_offset/2 +
              p_filter->fmt_out.video.i_x_offset/2 +
              ( i_y_offset + p_filter->fmt_out.video.i_y_offset ) / 2 *
              p_dst->p[U_PLANE].i_pitch;
    p_dst_v = p_dst->p[V_PLANE].p_pixels + i_x_offset/2 +
              p_filter->fmt_out.video.i_x_offset/2 +
              ( i_y_offset + p_filter->fmt_out.video.i_y_offset ) / 2 *
              p_dst->p[V_PLANE].i_pitch;

    i_src1_pitch = p_dst_orig->p[Y_PLANE].i_pitch;
    p_src1_y = p_dst_orig->p[Y_PLANE].p_pixels + i_x_offset +
               p_filter->fmt_out.video.i_x_offset +
               p_dst_orig->p[Y_PLANE].i_pitch *
               ( i_y_offset + p_filter->fmt_out.video.i_y_offset );
    p_src1_u = p_dst_orig->p[U_PLANE].p_pixels + i_x_offset/2 +
               p_filter->fmt_out.video.i_x_offset/2 +
               ( i_y_offset + p_filter->fmt_out.video.i_y_offset ) / 2 *
               p_dst_orig->p[U_PLANE].i_pitch;
    p_src1_v = p_dst_orig->p[V_PLANE].p_pixels + i_x_offset/2 +
               p_filter->fmt_out.video.i_x_offset/2 +
               ( i_y_offset + p_filter->fmt_out.video.i_y_offset ) / 2 *
               p_dst_orig->p[V_PLANE].i_pitch;

    i_src2_pitch = p_src->p[Y_PLANE].i_pitch;
    p_src2_y = p_src->p[Y_PLANE].p_pixels +
               p_filter->fmt_in.video.i_x_offset +
               p_src->p[Y_PLANE].i_pitch * p_filter->fmt_in.video.i_y_offset;
    p_src2_u = p_src->p[U_PLANE].p_pixels +
               p_filter->fmt_in.video.i_x_offset/2 +
               p_src->p[U_PLANE].i_pitch * p_filter->fmt_in.video.i_y_offset/2;
    p_src2_v = p_src->p[V_PLANE].p_pixels +
               p_filter->fmt_in.video.i_x_offset/2 +
               p_src->p[V_PLANE].i_pitch * p_filter->fmt_in.video.i_y_offset/2;

    p_trans = p_src->p[A_PLANE].p_pixels +
              p_filter->fmt_in.video.i_x_offset +
              p_src->p[A_PLANE].i_pitch * p_filter->fmt_in.video.i_y_offset;

#define MAX_TRANS 255
#define TRANS_BITS  8

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++, p_trans += i_src2_pitch,
         p_dst_y += i_dst_pitch, p_src1_y += i_src1_pitch,
         p_src2_y += i_src2_pitch,
         p_dst_u += b_even_scanline ? i_dst_pitch/2 : 0,
         p_src1_u += b_even_scanline ? i_src1_pitch/2 : 0,
         p_src2_u += i_src2_pitch,
         p_dst_v += b_even_scanline ? i_dst_pitch/2 : 0,
         p_src1_v += b_even_scanline ? i_src1_pitch/2 : 0,
         p_src2_v += i_src2_pitch )
    {
        b_even_scanline = !b_even_scanline;

        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++ )
        {
            i_trans = ( p_trans[i_x] * i_alpha ) / 255;
            if( !i_trans )
            {
                /* Completely transparent. Don't change pixel */
                continue;
            }
            else if( i_trans == MAX_TRANS )
            {
                /* Completely opaque. Completely overwrite underlying pixel */
                p_dst_y[i_x] = p_src2_y[i_x];

                if( b_even_scanline && i_x % 2 == 0 )
                {
                    p_dst_u[i_x/2] = p_src2_u[i_x];
                    p_dst_v[i_x/2] = p_src2_v[i_x];
                }
                continue;
            }

            /* Blending */
            p_dst_y[i_x] = ( (UINT16)p_src2_y[i_x] * i_trans +
                (UINT16)p_src1_y[i_x] * (MAX_TRANS - i_trans) )
                >> TRANS_BITS;

            if( b_even_scanline && i_x % 2 == 0 )
            {
                p_dst_u[i_x/2] = ( (UINT16)p_src2_u[i_x] * i_trans +
                (UINT16)p_src1_u[i_x/2] * (MAX_TRANS - i_trans) )
                >> TRANS_BITS;
                p_dst_v[i_x/2] = ( (UINT16)p_src2_v[i_x] * i_trans +
                (UINT16)p_src1_v[i_x/2] * (MAX_TRANS - i_trans) )
                >> TRANS_BITS;
            }
        }
    }

#undef MAX_TRANS
#undef TRANS_BITS

    return;
}

static inline void yuv_to_rgb( int *r, int *g, int *b,
                               UINT8 y1, UINT8 u1, UINT8 v1 )
{
    /* macros used for YUV pixel conversions */
#   define SCALEBITS 10
#   define ONE_HALF  (1 << (SCALEBITS - 1))
#   define FIX(x)    ((int) ((x) * (1<<SCALEBITS) + 0.5))
#   define CLAMP( x ) (((x) > 255) ? 255 : ((x) < 0) ? 0 : (x));

    int y, cb, cr, r_add, g_add, b_add;

    cb = u1 - 128;
    cr = v1 - 128;
    r_add = FIX(1.40200*255.0/224.0) * cr + ONE_HALF;
    g_add = - FIX(0.34414*255.0/224.0) * cb
            - FIX(0.71414*255.0/224.0) * cr + ONE_HALF;
    b_add = FIX(1.77200*255.0/224.0) * cb + ONE_HALF;
    y = (y1 - 16) * FIX(255.0/219.0);
    *r = CLAMP((y + r_add) >> SCALEBITS);
    *g = CLAMP((y + g_add) >> SCALEBITS);
    *b = CLAMP((y + b_add) >> SCALEBITS);
}

static void BlendR16( filter_t *p_filter, picture_t *p_dst_pic,
                      picture_t *p_dst_orig, picture_t *p_src,
                      int i_x_offset, int i_y_offset,
                      int i_width, int i_height, int i_alpha )
{
    int i_src1_pitch, i_src2_pitch, i_dst_pitch;
    UINT8 *p_dst, *p_src1, *p_src2_y;
    UINT8 *p_src2_u, *p_src2_v;
    UINT8 *p_trans;
    int i_x, i_y, i_pix_pitch;
    int r, g, b;

    i_pix_pitch = p_dst_pic->p->i_pixel_pitch;
    i_dst_pitch = p_dst_pic->p->i_pitch;
    p_dst = p_dst_pic->p->p_pixels + i_x_offset * i_pix_pitch +
            p_filter->fmt_out.video.i_x_offset * i_pix_pitch +
            p_dst_pic->p->i_pitch *
            ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    i_src1_pitch = p_dst_orig->p[Y_PLANE].i_pitch;
    p_src1 = p_dst_orig->p->p_pixels + i_x_offset * i_pix_pitch +
               p_filter->fmt_out.video.i_x_offset * i_pix_pitch +
               p_dst_orig->p->i_pitch *
               ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    i_src2_pitch = p_src->p[Y_PLANE].i_pitch;
    p_src2_y = p_src->p[Y_PLANE].p_pixels +
               p_filter->fmt_in.video.i_x_offset +
               p_src->p[Y_PLANE].i_pitch * p_filter->fmt_in.video.i_y_offset;
    p_src2_u = p_src->p[U_PLANE].p_pixels +
               p_filter->fmt_in.video.i_x_offset/2 +
               p_src->p[U_PLANE].i_pitch * p_filter->fmt_in.video.i_y_offset/2;
    p_src2_v = p_src->p[V_PLANE].p_pixels +
               p_filter->fmt_in.video.i_x_offset/2 +
               p_src->p[V_PLANE].i_pitch * p_filter->fmt_in.video.i_y_offset/2;

    p_trans = p_src->p[A_PLANE].p_pixels +
              p_filter->fmt_in.video.i_x_offset +
              p_src->p[A_PLANE].i_pitch * p_filter->fmt_in.video.i_y_offset;

#define MAX_TRANS 255
#define TRANS_BITS  8

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++, p_trans += i_src2_pitch,
         p_dst += i_dst_pitch, p_src1 += i_src1_pitch,
         p_src2_y += i_src2_pitch, p_src2_u += i_src2_pitch,
         p_src2_v += i_src2_pitch )
    {
        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++ )
        {
            if( !p_trans[i_x] )
            {
                /* Completely transparent. Don't change pixel */
                continue;
            }
            else if( p_trans[i_x] == MAX_TRANS )
            {
                /* Completely opaque. Completely overwrite underlying pixel */
                yuv_to_rgb( &r, &g, &b,
                            p_src2_y[i_x], p_src2_u[i_x], p_src2_v[i_x] );

    ((UINT16 *)(&p_dst[i_x * i_pix_pitch]))[0] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
                continue;
            }

            /* Blending */
            yuv_to_rgb( &r, &g, &b,
                        p_src2_y[i_x], p_src2_u[i_x], p_src2_v[i_x] );

    ((UINT16 *)(&p_dst[i_x * i_pix_pitch]))[0] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        }
    }

#undef MAX_TRANS
#undef TRANS_BITS

    return;
}

static void BlendR24( filter_t *p_filter, picture_t *p_dst_pic,
                      picture_t *p_dst_orig, picture_t *p_src,
                      int i_x_offset, int i_y_offset,
                      int i_width, int i_height, int i_alpha )
{
    int i_src1_pitch, i_src2_pitch, i_dst_pitch;
    UINT8 *p_dst, *p_src1, *p_src2_y;
    UINT8 *p_src2_u, *p_src2_v;
    UINT8 *p_trans;
    int i_x, i_y, i_pix_pitch, i_trans;
    int r, g, b;

    i_pix_pitch = p_dst_pic->p->i_pixel_pitch;
    i_dst_pitch = p_dst_pic->p->i_pitch;
    p_dst = p_dst_pic->p->p_pixels + i_x_offset * i_pix_pitch +
            p_filter->fmt_out.video.i_x_offset * i_pix_pitch +
            p_dst_pic->p->i_pitch *
            ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    i_src1_pitch = p_dst_orig->p[Y_PLANE].i_pitch;
    p_src1 = p_dst_orig->p->p_pixels + i_x_offset * i_pix_pitch +
               p_filter->fmt_out.video.i_x_offset * i_pix_pitch +
               p_dst_orig->p->i_pitch *
               ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    i_src2_pitch = p_src->p[Y_PLANE].i_pitch;
    p_src2_y = p_src->p[Y_PLANE].p_pixels +
               p_filter->fmt_in.video.i_x_offset +
               p_src->p[Y_PLANE].i_pitch * p_filter->fmt_in.video.i_y_offset;
    p_src2_u = p_src->p[U_PLANE].p_pixels +
               p_filter->fmt_in.video.i_x_offset/2 +
               p_src->p[U_PLANE].i_pitch * p_filter->fmt_in.video.i_y_offset/2;
    p_src2_v = p_src->p[V_PLANE].p_pixels +
               p_filter->fmt_in.video.i_x_offset/2 +
               p_src->p[V_PLANE].i_pitch * p_filter->fmt_in.video.i_y_offset/2;

    p_trans = p_src->p[A_PLANE].p_pixels +
              p_filter->fmt_in.video.i_x_offset +
              p_src->p[A_PLANE].i_pitch * p_filter->fmt_in.video.i_y_offset;

#define MAX_TRANS 255
#define TRANS_BITS  8

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++, p_trans += i_src2_pitch,
         p_dst += i_dst_pitch, p_src1 += i_src1_pitch,
         p_src2_y += i_src2_pitch, p_src2_u += i_src2_pitch,
         p_src2_v += i_src2_pitch )
    {
        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++ )
        {
            i_trans = ( p_trans[i_x] * i_alpha ) / 255;
            if( !i_trans )
            {
                /* Completely transparent. Don't change pixel */
                continue;
            }
            else if( i_trans == MAX_TRANS )
            {
                /* Completely opaque. Completely overwrite underlying pixel */
                yuv_to_rgb( &r, &g, &b,
                            p_src2_y[i_x], p_src2_u[i_x], p_src2_v[i_x] );

                p_dst[i_x * i_pix_pitch]     = r;
                p_dst[i_x * i_pix_pitch + 1] = g;
                p_dst[i_x * i_pix_pitch + 2] = b;
                continue;
            }

            /* Blending */
            yuv_to_rgb( &r, &g, &b,
                        p_src2_y[i_x], p_src2_u[i_x], p_src2_v[i_x] );

            p_dst[i_x * i_pix_pitch]     = ( r * i_trans +
                (UINT16)p_src1[i_x * i_pix_pitch] *
                (MAX_TRANS - i_trans) ) >> TRANS_BITS;
            p_dst[i_x * i_pix_pitch + 1] = ( g * i_trans +
                (UINT16)p_src1[i_x * i_pix_pitch + 1] *
                (MAX_TRANS - i_trans) ) >> TRANS_BITS;
            p_dst[i_x * i_pix_pitch + 2] = ( b * i_trans +
                (UINT16)p_src1[i_x * i_pix_pitch + 2] *
                (MAX_TRANS - i_trans) ) >> TRANS_BITS;
        }
    }

#undef MAX_TRANS
#undef TRANS_BITS

    return;
}

static void BlendYUVPacked( filter_t *p_filter, picture_t *p_dst_pic,
                            picture_t *p_dst_orig, picture_t *p_src,
                            int i_x_offset, int i_y_offset,
                            int i_width, int i_height, int i_alpha )
{
    int i_src1_pitch, i_src2_pitch, i_dst_pitch;
    UINT8 *p_dst, *p_src1, *p_src2_y;
    UINT8 *p_src2_u, *p_src2_v;
    UINT8 *p_trans;
    int i_x, i_y, i_pix_pitch, i_trans;
    BOOL b_even = !((i_x_offset + p_filter->fmt_out.video.i_x_offset)%2);
    int i_l_offset = 0, i_u_offset = 0, i_v_offset = 0;

    if( p_filter->fmt_out.video.i_chroma == TME_FOURCC('Y','U','Y','2') )
    {
        i_l_offset = 0;
        i_u_offset = 1;
        i_v_offset = 3;
    }
    else if( p_filter->fmt_out.video.i_chroma == TME_FOURCC('U','Y','V','Y') )
    {
        i_l_offset = 1;
        i_u_offset = 0;
        i_v_offset = 2;
    }
    else if( p_filter->fmt_out.video.i_chroma == TME_FOURCC('Y','V','Y','U') )
    {
        i_l_offset = 0;
        i_u_offset = 3;
        i_v_offset = 1;
    }

    i_pix_pitch = 2;
    i_dst_pitch = p_dst_pic->p->i_pitch;
    p_dst = p_dst_pic->p->p_pixels + i_x_offset * i_pix_pitch +
            p_filter->fmt_out.video.i_x_offset * i_pix_pitch +
            p_dst_pic->p->i_pitch *
            ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    i_src1_pitch = p_dst_orig->p[Y_PLANE].i_pitch;
    p_src1 = p_dst_orig->p->p_pixels + i_x_offset * i_pix_pitch +
               p_filter->fmt_out.video.i_x_offset * i_pix_pitch +
               p_dst_orig->p->i_pitch *
               ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    i_src2_pitch = p_src->p[Y_PLANE].i_pitch;
    p_src2_y = p_src->p[Y_PLANE].p_pixels +
               p_filter->fmt_in.video.i_x_offset +
               p_src->p[Y_PLANE].i_pitch * p_filter->fmt_in.video.i_y_offset;
    p_src2_u = p_src->p[U_PLANE].p_pixels +
               p_filter->fmt_in.video.i_x_offset/2 +
               p_src->p[U_PLANE].i_pitch * p_filter->fmt_in.video.i_y_offset/2;
    p_src2_v = p_src->p[V_PLANE].p_pixels +
               p_filter->fmt_in.video.i_x_offset/2 +
               p_src->p[V_PLANE].i_pitch * p_filter->fmt_in.video.i_y_offset/2;

    p_trans = p_src->p[A_PLANE].p_pixels +
              p_filter->fmt_in.video.i_x_offset +
              p_src->p[A_PLANE].i_pitch * p_filter->fmt_in.video.i_y_offset;

    i_width = (i_width >> 1) << 1; /* Needs to be a multiple of 2 */

#define MAX_TRANS 255
#define TRANS_BITS  8

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++, p_trans += i_src2_pitch,
         p_dst += i_dst_pitch, p_src1 += i_src1_pitch,
         p_src2_y += i_src2_pitch, p_src2_u += i_src2_pitch,
         p_src2_v += i_src2_pitch )
    {
        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++, b_even = !b_even )
        {
            i_trans = ( p_trans[i_x] * i_alpha ) / 255;
            if( !i_trans )
            {
                /* Completely transparent. Don't change pixel */
            }
            else if( i_trans == MAX_TRANS )
            {
                /* Completely opaque. Completely overwrite underlying pixel */
                p_dst[i_x * 2 + i_l_offset]     = p_src2_y[i_x];

                if( b_even )
                {
                    if( p_trans[i_x+1] > 0xaa )
                    {
                        p_dst[i_x * 2 + i_u_offset] = (p_src2_u[i_x]+p_src2_u[i_x+1])>>1;
                        p_dst[i_x * 2 + i_v_offset] = (p_src2_v[i_x]+p_src2_v[i_x+1])>>1;
                    }
                    else
                    {
                        p_dst[i_x * 2 + i_u_offset] = p_src2_u[i_x];
                        p_dst[i_x * 2 + i_v_offset] = p_src2_v[i_x];
                    }
                }
            }
            else
            {
                /* Blending */
                p_dst[i_x * 2 + i_l_offset]     = ( (UINT16)p_src2_y[i_x] * i_trans +
                    (UINT16)p_src1[i_x * 2 + i_l_offset] * (MAX_TRANS - i_trans) )
                    >> TRANS_BITS;

                if( b_even )
                {
                    UINT16 i_u = 0;
                    UINT16 i_v = 0;
                    if( p_trans[i_x+1] > 0xaa )
                    {
                        i_u = (p_src2_u[i_x]+p_src2_u[i_x+1])>>1;
                        i_v = (p_src2_v[i_x]+p_src2_v[i_x+1])>>1;
                    }
                    else 
                    {
                        i_u = p_src2_u[i_x];
                        i_v = p_src2_v[i_x];
                    }
                    p_dst[i_x * 2 + i_u_offset] = ( (UINT16)i_u * i_trans +
                        (UINT16)p_src1[i_x * 2 + i_u_offset] * (MAX_TRANS - i_trans) )
                        >> TRANS_BITS;
                    p_dst[i_x * 2 + i_v_offset] = ( (UINT16)i_v * i_trans +
                        (UINT16)p_src1[i_x * 2 + i_v_offset] * (MAX_TRANS - i_trans) )
                        >> TRANS_BITS;
                }
            }
        }
    }

#undef MAX_TRANS
#undef TRANS_BITS

    return;
}

static void BlendPalI420( filter_t *p_filter, picture_t *p_dst,
                          picture_t *p_dst_orig, picture_t *p_src,
                          int i_x_offset, int i_y_offset,
                          int i_width, int i_height, int i_alpha )
{
    int i_src1_pitch, i_src2_pitch, i_dst_pitch;
    UINT8 *p_src1_y, *p_src2, *p_dst_y;
    UINT8 *p_src1_u, *p_dst_u;
    UINT8 *p_src1_v, *p_dst_v;
    int i_x, i_y, i_trans;
    BOOL b_even_scanline = i_y_offset % 2;

    i_dst_pitch = p_dst->p[Y_PLANE].i_pitch;
    p_dst_y = p_dst->p[Y_PLANE].p_pixels + i_x_offset +
              p_filter->fmt_out.video.i_x_offset +
              p_dst->p[Y_PLANE].i_pitch *
              ( i_y_offset + p_filter->fmt_out.video.i_y_offset );
    p_dst_u = p_dst->p[U_PLANE].p_pixels + i_x_offset/2 +
              p_filter->fmt_out.video.i_x_offset/2 +
              ( i_y_offset + p_filter->fmt_out.video.i_y_offset ) / 2 *
              p_dst->p[U_PLANE].i_pitch;
    p_dst_v = p_dst->p[V_PLANE].p_pixels + i_x_offset/2 +
              p_filter->fmt_out.video.i_x_offset/2 +
              ( i_y_offset + p_filter->fmt_out.video.i_y_offset ) / 2 *
              p_dst->p[V_PLANE].i_pitch;

    i_src1_pitch = p_dst_orig->p[Y_PLANE].i_pitch;
    p_src1_y = p_dst_orig->p[Y_PLANE].p_pixels + i_x_offset +
               p_filter->fmt_out.video.i_x_offset +
               p_dst_orig->p[Y_PLANE].i_pitch *
               ( i_y_offset + p_filter->fmt_out.video.i_y_offset );
    p_src1_u = p_dst_orig->p[U_PLANE].p_pixels + i_x_offset/2 +
               p_filter->fmt_out.video.i_x_offset/2 +
               ( i_y_offset + p_filter->fmt_out.video.i_y_offset ) / 2 *
               p_dst_orig->p[U_PLANE].i_pitch;
    p_src1_v = p_dst_orig->p[V_PLANE].p_pixels + i_x_offset/2 +
               p_filter->fmt_out.video.i_x_offset/2 +
               ( i_y_offset + p_filter->fmt_out.video.i_y_offset ) / 2 *
               p_dst_orig->p[V_PLANE].i_pitch;

    i_src2_pitch = p_src->p->i_pitch;
    p_src2 = p_src->p->p_pixels + p_filter->fmt_in.video.i_x_offset +
             i_src2_pitch * p_filter->fmt_in.video.i_y_offset;

#define MAX_TRANS 255
#define TRANS_BITS  8
#define p_trans p_src2
#define p_pal p_filter->fmt_in.video.p_palette->palette

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++,
         p_dst_y += i_dst_pitch, p_src1_y += i_src1_pitch,
         p_src2 += i_src2_pitch,
         p_dst_u += b_even_scanline ? i_dst_pitch/2 : 0,
         p_src1_u += b_even_scanline ? i_src1_pitch/2 : 0,
         p_dst_v += b_even_scanline ? i_dst_pitch/2 : 0,
         p_src1_v += b_even_scanline ? i_src1_pitch/2 : 0 )
    {
        b_even_scanline = !b_even_scanline;

        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++ )
        {
            i_trans = ( p_pal[p_trans[i_x]][3] * i_alpha ) / 255;
            if( !i_trans )
            {
                /* Completely transparent. Don't change pixel */
                continue;
            }
            else if( i_trans == MAX_TRANS )
            {
                /* Completely opaque. Completely overwrite underlying pixel */
                p_dst_y[i_x] = p_pal[p_src2[i_x]][0];

                if( b_even_scanline && i_x % 2 == 0 )
                {
                    p_dst_u[i_x/2] = p_pal[p_src2[i_x]][1];
                    p_dst_v[i_x/2] = p_pal[p_src2[i_x]][2];
                }
                continue;
            }

            /* Blending */
            p_dst_y[i_x] = ( (UINT16)p_pal[p_src2[i_x]][0] * i_trans +
                (UINT16)p_src1_y[i_x] * (MAX_TRANS - i_trans) )
                >> TRANS_BITS;

            if( b_even_scanline && i_x % 2 == 0 )
            {
                p_dst_u[i_x/2] = ( (UINT16)p_pal[p_src2[i_x]][1] * i_trans +
                    (UINT16)p_src1_u[i_x/2] * (MAX_TRANS - i_trans) )
                    >> TRANS_BITS;
                p_dst_v[i_x/2] = ( (UINT16)p_pal[p_src2[i_x]][2] * i_trans +
                    (UINT16)p_src1_v[i_x/2] * (MAX_TRANS - i_trans) )
                    >> TRANS_BITS;
            }
        }
    }

#undef MAX_TRANS
#undef TRANS_BITS
#undef p_trans
#undef p_pal

    return;
}

static void BlendPalYUVPacked( filter_t *p_filter, picture_t *p_dst_pic,
                               picture_t *p_dst_orig, picture_t *p_src,
                               int i_x_offset, int i_y_offset,
                               int i_width, int i_height, int i_alpha )
{
    int i_src1_pitch, i_src2_pitch, i_dst_pitch;
    UINT8 *p_src1, *p_src2, *p_dst;
    int i_x, i_y, i_pix_pitch, i_trans;
    BOOL b_even = !((i_x_offset + p_filter->fmt_out.video.i_x_offset)%2);
    int i_l_offset = 0, i_u_offset = 0, i_v_offset = 0;

    if( p_filter->fmt_out.video.i_chroma == TME_FOURCC('Y','U','Y','2') )
    {
        i_l_offset = 0;
        i_u_offset = 1;
        i_v_offset = 3;
    }
    else if( p_filter->fmt_out.video.i_chroma == TME_FOURCC('U','Y','V','Y') )
    {
        i_l_offset = 1;
        i_u_offset = 0;
        i_v_offset = 2;
    }
    else if( p_filter->fmt_out.video.i_chroma == TME_FOURCC('Y','V','Y','U') )
    {
        i_l_offset = 0;
        i_u_offset = 3;
        i_v_offset = 1;
    }

    i_pix_pitch = 2;
    i_dst_pitch = p_dst_pic->p->i_pitch;
    p_dst = p_dst_pic->p->p_pixels + i_pix_pitch * (i_x_offset +
            p_filter->fmt_out.video.i_x_offset) + p_dst_pic->p->i_pitch *
            ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    i_src1_pitch = p_dst_orig->p->i_pitch;
    p_src1 = p_dst_orig->p->p_pixels + i_pix_pitch * (i_x_offset +
             p_filter->fmt_out.video.i_x_offset) + p_dst_orig->p->i_pitch *
             ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    i_src2_pitch = p_src->p->i_pitch;
    p_src2 = p_src->p->p_pixels + p_filter->fmt_in.video.i_x_offset +
             i_src2_pitch * p_filter->fmt_in.video.i_y_offset;

    i_width = (i_width >> 1) << 1; /* Needs to be a multiple of 2 */

#define MAX_TRANS 255
#define TRANS_BITS  8
#define p_trans p_src2
#define p_pal p_filter->fmt_in.video.p_palette->palette

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++,
         p_dst += i_dst_pitch, p_src1 += i_src1_pitch, p_src2 += i_src2_pitch )
    {
        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++, b_even = !b_even )
        {
            i_trans = ( p_pal[p_trans[i_x]][3] * i_alpha ) / 255;
            if( !i_trans )
            {
                /* Completely transparent. Don't change pixel */
            }
            else if( i_trans == MAX_TRANS )
            {
                /* Completely opaque. Completely overwrite underlying pixel */
                p_dst[i_x * 2 + i_l_offset]     = p_pal[p_src2[i_x]][0];

                if( b_even )
                {
                    if( p_trans[i_x+1] > 0xaa )
                    {
                        p_dst[i_x * 2 + i_u_offset] = (p_pal[p_src2[i_x]][1] + p_pal[p_src2[i_x+1]][1]) >> 1;
                        p_dst[i_x * 2 + i_v_offset] = (p_pal[p_src2[i_x]][2] + p_pal[p_src2[i_x+1]][2]) >> 1;
                    }
                    else
                    {
                        p_dst[i_x * 2 + i_u_offset] = p_pal[p_src2[i_x]][1];
                        p_dst[i_x * 2 + i_v_offset] = p_pal[p_src2[i_x]][2];
                    }
                }
            }
            else
            {
                /* Blending */
                p_dst[i_x * 2 + i_l_offset]     = ( (UINT16)p_pal[p_src2[i_x]][0] *
                    i_trans + (UINT16)p_src1[i_x * 2 + i_l_offset] *
                    (MAX_TRANS - i_trans) ) >> TRANS_BITS;

                if( b_even )
                {
                    UINT16 i_u = 0;
                    UINT16 i_v = 0;
                    if( p_trans[i_x+1] > 0xaa )
                    {
                        i_u = (p_pal[p_src2[i_x]][1] + p_pal[p_src2[i_x+1]][1]) >> 1;
                        i_v = (p_pal[p_src2[i_x]][2] + p_pal[p_src2[i_x+1]][2]) >> 1;
                    }
                    else 
                    {
                        i_u = p_pal[p_src2[i_x]][1];
                        i_v = p_pal[p_src2[i_x]][2];
                    }

                    p_dst[i_x * 2 + i_u_offset] = ( (UINT16)i_u *
                        i_trans + (UINT16)p_src1[i_x * 2 + i_u_offset] *
                        (MAX_TRANS - i_trans) ) >> TRANS_BITS;
                    p_dst[i_x * 2 + i_v_offset] = ( (UINT16)i_v *
                        i_trans + (UINT16)p_src1[i_x * 2 + i_v_offset] *
                        (MAX_TRANS - i_trans) ) >> TRANS_BITS;
                }
            }
        }
    }

#undef MAX_TRANS
#undef TRANS_BITS
#undef p_trans
#undef p_pal

    return;
}

static void BlendPalRV( filter_t *p_filter, picture_t *p_dst_pic,
                        picture_t *p_dst_orig, picture_t *p_src,
                        int i_x_offset, int i_y_offset,
                        int i_width, int i_height, int i_alpha )
{
    int i_src1_pitch, i_src2_pitch, i_dst_pitch;
    UINT8 *p_src1, *p_src2, *p_dst;
    int i_x, i_y, i_pix_pitch, i_trans;
    int r, g, b;
    video_palette_t rgbpalette;

    i_pix_pitch = p_dst_pic->p->i_pixel_pitch;
    i_dst_pitch = p_dst_pic->p->i_pitch;
    p_dst = p_dst_pic->p->p_pixels + i_pix_pitch * (i_x_offset +
            p_filter->fmt_out.video.i_x_offset) + p_dst_pic->p->i_pitch *
            ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    i_src1_pitch = p_dst_orig->p->i_pitch;
    p_src1 = p_dst_orig->p->p_pixels + i_pix_pitch * (i_x_offset +
             p_filter->fmt_out.video.i_x_offset) + p_dst_orig->p->i_pitch *
             ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    i_src2_pitch = p_src->p->i_pitch;
    p_src2 = p_src->p->p_pixels + p_filter->fmt_in.video.i_x_offset +
             i_src2_pitch * p_filter->fmt_in.video.i_y_offset;

#define MAX_TRANS 255
#define TRANS_BITS  8
#define p_trans p_src2
#define p_pal p_filter->fmt_in.video.p_palette->palette
#define rgbpal rgbpalette.palette

    /* Convert palette first */
    for( i_y = 0; i_y < p_filter->fmt_in.video.p_palette->i_entries &&
         i_y < 256; i_y++ )
    {
        yuv_to_rgb( &r, &g, &b, p_pal[i_y][0], p_pal[i_y][1], p_pal[i_y][2] );

        if( p_filter->fmt_out.video.i_chroma == TME_FOURCC('R','V','1','6') )
        {
            *(UINT16 *)rgbpal[i_y] =
                ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        }
        else
        {
            rgbpal[i_y][0] = r; rgbpal[i_y][1] = g; rgbpal[i_y][2] = b;
        }
    }

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++,
         p_dst += i_dst_pitch, p_src1 += i_src1_pitch, p_src2 += i_src2_pitch )
    {
        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++ )
        {
            i_trans = ( p_pal[p_trans[i_x]][3] * i_alpha ) / 255;
            if( !i_trans )
            {
                /* Completely transparent. Don't change pixel */
                continue;
            }
            else if( i_trans == MAX_TRANS ||
                     p_filter->fmt_out.video.i_chroma ==
                     TME_FOURCC('R','V','1','6') )
            {
                /* Completely opaque. Completely overwrite underlying pixel */
                p_dst[i_x * i_pix_pitch]     = rgbpal[p_src2[i_x]][0];
                p_dst[i_x * i_pix_pitch + 1] = rgbpal[p_src2[i_x]][1];
                if( p_filter->fmt_out.video.i_chroma !=
                    TME_FOURCC('R','V','1','6') )
                p_dst[i_x * i_pix_pitch + 2] = rgbpal[p_src2[i_x]][2];
                continue;
            }

            /* Blending */
            p_dst[i_x * i_pix_pitch]     = ( (UINT16)rgbpal[p_src2[i_x]][0] *
                i_trans + (UINT16)p_src1[i_x * i_pix_pitch] *
                (MAX_TRANS - i_trans) ) >> TRANS_BITS;
            p_dst[i_x * i_pix_pitch + 1] = ( (UINT16)rgbpal[p_src2[i_x]][1] *
                i_trans + (UINT16)p_src1[i_x * i_pix_pitch + 1] *
                (MAX_TRANS - i_trans) ) >> TRANS_BITS;
            p_dst[i_x * i_pix_pitch + 2] = ( (UINT16)rgbpal[p_src2[i_x]][2] *
                i_trans + (UINT16)p_src1[i_x * i_pix_pitch + 2] *
                (MAX_TRANS - i_trans) ) >> TRANS_BITS;
        }
    }

#undef MAX_TRANS
#undef TRANS_BITS
#undef p_trans
#undef p_pal
#undef rgbpal

    return;
}
