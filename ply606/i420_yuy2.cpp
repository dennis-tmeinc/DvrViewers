#include "stdafx.h"
#include "video_output.h"
#include "i420_yuy2.h"

#if defined (MODULE_NAME_IS_i420_yuy2)
#    define DEST_FOURCC "YUY2,YUNV,YVYU,UYVY,UYNV,Y422,IUYV,cyuv,Y211"
#elif defined (MODULE_NAME_IS_i420_yuy2_mmx)
#    define DEST_FOURCC "YUY2,YUNV,YVYU,UYVY,UYNV,Y422,IUYV,cyuv"
#elif defined (MODULE_NAME_IS_i420_yuy2_altivec)
#    define DEST_FOURCC "YUY2,YUNV"
#endif

/*****************************************************************************
 * Local and extern prototypes.
 *****************************************************************************/

static void I420_YUY2           ( CVideoOutput *, picture_t *, picture_t * );
#if !defined (MODULE_NAME_IS_i420_yuy2_altivec)
static void I420_YVYU           ( CVideoOutput *, picture_t *, picture_t * );
static void I420_UYVY           ( CVideoOutput *, picture_t *, picture_t * );
static void I420_IUYV           ( CVideoOutput *, picture_t *, picture_t * );
static void I420_cyuv           ( CVideoOutput *, picture_t *, picture_t * );
#endif
#if defined (MODULE_NAME_IS_i420_yuy2)
static void I420_Y211           ( CVideoOutput *, picture_t *, picture_t * );
#endif

#ifdef MODULE_NAME_IS_i420_yuy2_mmx
static UINT64 i_00ffw = 0x00ff00ff00ff00ffULL;
static UINT64 i_80w = 0x0000000080808080ULL;
#endif

/* Following functions are local */

/*****************************************************************************
 * I420_YUY2: planar YUV 4:2:0 to packed YUYV 4:2:2
 *****************************************************************************/
static void I420_YUY2( CVideoOutput *p_vout, picture_t *p_source,
                                              picture_t *p_dest )
{
    UINT8 *p_line1, *p_line2 = p_dest->p->p_pixels;
    UINT8 *p_y1, *p_y2 = p_source->Y_PIXELS;
    UINT8 *p_u = p_source->U_PIXELS;
    UINT8 *p_v = p_source->V_PIXELS;

    int i_x, i_y;

#if defined (MODULE_NAME_IS_i420_yuy2_altivec)
#define VEC_NEXT_LINES( ) \
    p_line1  = p_line2; \
    p_line2 += p_dest->p->i_pitch; \
    p_y1     = p_y2; \
    p_y2    += p_source->p[Y_PLANE].i_pitch;

#define VEC_LOAD_UV( ) \
    u_vec = vec_ld( 0, p_u ); p_u += 16; \
    v_vec = vec_ld( 0, p_v ); p_v += 16;

#define VEC_MERGE( a ) \
    uv_vec = a( u_vec, v_vec ); \
    y_vec = vec_ld( 0, p_y1 ); p_y1 += 16; \
    vec_st( vec_mergeh( y_vec, uv_vec ), 0, p_line1 ); p_line1 += 16; \
    vec_st( vec_mergel( y_vec, uv_vec ), 0, p_line1 ); p_line1 += 16; \
    y_vec = vec_ld( 0, p_y2 ); p_y2 += 16; \
    vec_st( vec_mergeh( y_vec, uv_vec ), 0, p_line2 ); p_line2 += 16; \
    vec_st( vec_mergel( y_vec, uv_vec ), 0, p_line2 ); p_line2 += 16;

    vector unsigned char u_vec;
    vector unsigned char v_vec;
    vector unsigned char uv_vec;
    vector unsigned char y_vec;

    if( !( ( p_vout->m_render.i_width % 32 ) |
           ( p_vout->m_render.i_height % 2 ) ) )
    {
        /* Width is a multiple of 32, we take 2 lines at a time */
        for( i_y = p_vout->m_render.i_height / 2 ; i_y-- ; )
        {
            VEC_NEXT_LINES( );
            for( i_x = p_vout->m_render.i_width / 32 ; i_x-- ; )
            {
                VEC_LOAD_UV( );
                VEC_MERGE( vec_mergeh );
                VEC_MERGE( vec_mergel );
            }
        }
    }
    else if( !( ( p_vout->m_render.i_width % 16 ) |
                ( p_vout->m_render.i_height % 4 ) ) )
    {
        /* Width is only a multiple of 16, we take 4 lines at a time */
        for( i_y = p_vout->m_render.i_height / 4 ; i_y-- ; )
        {
            /* Line 1 and 2, pixels 0 to ( width - 16 ) */
            VEC_NEXT_LINES( );
            for( i_x = p_vout->m_render.i_width / 32 ; i_x-- ; )
            {
                VEC_LOAD_UV( );
                VEC_MERGE( vec_mergeh );
                VEC_MERGE( vec_mergel );
            }

            /* Line 1 and 2, pixels ( width - 16 ) to ( width ) */
            VEC_LOAD_UV( );
            VEC_MERGE( vec_mergeh );

            /* Line 3 and 4, pixels 0 to 16 */
            VEC_NEXT_LINES( );
            VEC_MERGE( vec_mergel );

            /* Line 3 and 4, pixels 16 to ( width ) */
            for( i_x = p_vout->m_render.i_width / 32 ; i_x-- ; )
            {
                VEC_LOAD_UV( );
                VEC_MERGE( vec_mergeh );
                VEC_MERGE( vec_mergel );
            }
        }
    }
    else
    {
        /* Crap, use the C version */
#undef VEC_NEXT_LINES
#undef VEC_LOAD_UV
#undef VEC_MERGE
#endif

    const int i_source_margin = p_source->p[0].i_pitch
                                 - p_source->p[0].i_visible_pitch;
    const int i_source_margin_c = p_source->p[1].i_pitch
                                 - p_source->p[1].i_visible_pitch;
    const int i_dest_margin = p_dest->p->i_pitch
                               - p_dest->p->i_visible_pitch;

    for( i_y = p_vout->m_render.i_height / 2 ; i_y-- ; )
    {
        p_line1 = p_line2;
        p_line2 += p_dest->p->i_pitch;

        p_y1 = p_y2;
        p_y2 += p_source->p[Y_PLANE].i_pitch;

#if !defined (MODULE_NAME_IS_i420_yuy2_mmx)
        for( i_x = p_vout->m_render.i_width / 2 ; i_x-- ; )
        {
            C_YUV420_YUYV( );
        }
#else
        for( i_x = p_vout->m_render.i_width / 8 ; i_x-- ; )
        {
            MMX_CALL( MMX_YUV420_YUYV );
        }
        for( i_x = ( p_vout->m_render.i_width % 8 ) / 2; i_x-- ; )
        {
            C_YUV420_YUYV( );
        }
#endif

        p_y1 += i_source_margin;
        p_y2 += i_source_margin;
        p_u += i_source_margin_c;
        p_v += i_source_margin_c;
        p_line1 += i_dest_margin;
        p_line2 += i_dest_margin;
    }
#if defined (MODULE_NAME_IS_i420_yuy2_mmx)
    /* re-enable FPU registers */
    __asm __volatile__ ("emms");
#endif

#if defined (MODULE_NAME_IS_i420_yuy2_altivec)
    }
#endif
}

/*****************************************************************************
 * I420_YVYU: planar YUV 4:2:0 to packed YVYU 4:2:2
 *****************************************************************************/
#if !defined (MODULE_NAME_IS_i420_yuy2_altivec)
static void I420_YVYU( CVideoOutput *p_vout, picture_t *p_source,
                                              picture_t *p_dest )
{
    UINT8 *p_line1, *p_line2 = p_dest->p->p_pixels;
    UINT8 *p_y1, *p_y2 = p_source->Y_PIXELS;
    UINT8 *p_u = p_source->U_PIXELS;
    UINT8 *p_v = p_source->V_PIXELS;

    int i_x, i_y;

    const int i_source_margin = p_source->p[0].i_pitch
                                 - p_source->p[0].i_visible_pitch;
    const int i_source_margin_c = p_source->p[1].i_pitch
                                 - p_source->p[1].i_visible_pitch;
    const int i_dest_margin = p_dest->p->i_pitch
                               - p_dest->p->i_visible_pitch;

    for( i_y = p_vout->m_render.i_height / 2 ; i_y-- ; )
    {
        p_line1 = p_line2;
        p_line2 += p_dest->p->i_pitch;

        p_y1 = p_y2;
        p_y2 += p_source->p[Y_PLANE].i_pitch;

        for( i_x = p_vout->m_render.i_width / 8 ; i_x-- ; )
        {
#if defined (MODULE_NAME_IS_i420_yuy2)
            C_YUV420_YVYU( );
            C_YUV420_YVYU( );
            C_YUV420_YVYU( );
            C_YUV420_YVYU( );
#else
            MMX_CALL( MMX_YUV420_YVYU );
#endif
        }

        p_y1 += i_source_margin;
        p_y2 += i_source_margin;
        p_u += i_source_margin_c;
        p_v += i_source_margin_c;
        p_line1 += i_dest_margin;
        p_line2 += i_dest_margin;
    }
#if defined (MODULE_NAME_IS_i420_yuy2_mmx)
    /* re-enable FPU registers */
    __asm__ __volatile__ ("emms");
#endif
}

/*****************************************************************************
 * I420_UYVY: planar YUV 4:2:0 to packed UYVY 4:2:2
 *****************************************************************************/
static void I420_UYVY( CVideoOutput *p_vout, picture_t *p_source,
                                              picture_t *p_dest )
{
    UINT8 *p_line1, *p_line2 = p_dest->p->p_pixels;
    UINT8 *p_y1, *p_y2 = p_source->Y_PIXELS;
    UINT8 *p_u = p_source->U_PIXELS;
    UINT8 *p_v = p_source->V_PIXELS;

    int i_x, i_y;

    const int i_source_margin = p_source->p[0].i_pitch
                                 - p_source->p[0].i_visible_pitch;
    const int i_source_margin_c = p_source->p[1].i_pitch
                                 - p_source->p[1].i_visible_pitch;
    const int i_dest_margin = p_dest->p->i_pitch
                               - p_dest->p->i_visible_pitch;

    for( i_y = p_vout->m_render.i_height / 2 ; i_y-- ; )
    {
        p_line1 = p_line2;
        p_line2 += p_dest->p->i_pitch;

        p_y1 = p_y2;
        p_y2 += p_source->p[Y_PLANE].i_pitch;

        for( i_x = p_vout->m_render.i_width / 8 ; i_x-- ; )
        {
#if defined (MODULE_NAME_IS_i420_yuy2)
            C_YUV420_UYVY( );
            C_YUV420_UYVY( );
            C_YUV420_UYVY( );
            C_YUV420_UYVY( );
#else
            MMX_CALL( MMX_YUV420_UYVY );
#endif
        }
        for( i_x = ( p_vout->m_render.i_width % 8 ) / 2; i_x--; )
        {
            C_YUV420_UYVY( );
        }

        p_y1 += i_source_margin;
        p_y2 += i_source_margin;
        p_u += i_source_margin_c;
        p_v += i_source_margin_c;
        p_line1 += i_dest_margin;
        p_line2 += i_dest_margin;
    }
#if defined (MODULE_NAME_IS_i420_yuy2_mmx)
    /* re-enable FPU registers */
    __asm__ __volatile__ ("emms");
#endif
}

/*****************************************************************************
 * I420_IUYV: planar YUV 4:2:0 to interleaved packed UYVY 4:2:2
 *****************************************************************************/
static void I420_IUYV( CVideoOutput *p_vout, picture_t *p_source,
                                              picture_t *p_dest )
{
    /* FIXME: TODO ! */
    //msg_Err( p_vout, "I420_IUYV unimplemented, please harass <sam@zoy.org>" );
}

/*****************************************************************************
 * I420_cyuv: planar YUV 4:2:0 to upside-down packed UYVY 4:2:2
 *****************************************************************************/
static void I420_cyuv( CVideoOutput *p_vout, picture_t *p_source,
                                              picture_t *p_dest )
{
    UINT8 *p_line1 = p_dest->p->p_pixels +
                       p_dest->p->i_visible_lines * p_dest->p->i_pitch
                       + p_dest->p->i_pitch;
    UINT8 *p_line2 = p_dest->p->p_pixels +
                       p_dest->p->i_visible_lines * p_dest->p->i_pitch;
    UINT8 *p_y1, *p_y2 = p_source->Y_PIXELS;
    UINT8 *p_u = p_source->U_PIXELS;
    UINT8 *p_v = p_source->V_PIXELS;

    int i_x, i_y;

    const int i_source_margin = p_source->p[0].i_pitch
                                 - p_source->p[0].i_visible_pitch;
    const int i_source_margin_c = p_source->p[1].i_pitch
                                 - p_source->p[1].i_visible_pitch;
    const int i_dest_margin = p_dest->p->i_pitch
                               - p_dest->p->i_visible_pitch;

    for( i_y = p_vout->m_render.i_height / 2 ; i_y-- ; )
    {
        p_line1 -= 3 * p_dest->p->i_pitch;
        p_line2 -= 3 * p_dest->p->i_pitch;

        p_y1 = p_y2;
        p_y2 += p_source->p[Y_PLANE].i_pitch;

        for( i_x = p_vout->m_render.i_width / 8 ; i_x-- ; )
        {
#if defined (MODULE_NAME_IS_i420_yuy2)
            C_YUV420_UYVY( );
            C_YUV420_UYVY( );
            C_YUV420_UYVY( );
            C_YUV420_UYVY( );
#else
            MMX_CALL( MMX_YUV420_UYVY );
#endif
        }

        p_y1 += i_source_margin;
        p_y2 += i_source_margin;
        p_u += i_source_margin_c;
        p_v += i_source_margin_c;
        p_line1 += i_dest_margin;
        p_line2 += i_dest_margin;
    }
#if defined (MODULE_NAME_IS_i420_yuy2_mmx)
    /* re-enable FPU registers */
    __asm__ __volatile__ ("emms");
#endif
}
#endif // !defined (MODULE_NAME_IS_i420_yuy2_altivec)

/*****************************************************************************
 * I420_Y211: planar YUV 4:2:0 to packed YUYV 2:1:1
 *****************************************************************************/
#if defined (MODULE_NAME_IS_i420_yuy2)
static void I420_Y211( CVideoOutput *p_vout, picture_t *p_source,
                                              picture_t *p_dest )
{
    UINT8 *p_line1, *p_line2 = p_dest->p->p_pixels;
    UINT8 *p_y1, *p_y2 = p_source->Y_PIXELS;
    UINT8 *p_u = p_source->U_PIXELS;
    UINT8 *p_v = p_source->V_PIXELS;

    int i_x, i_y;

    const int i_source_margin = p_source->p[0].i_pitch
                                 - p_source->p[0].i_visible_pitch;
    const int i_source_margin_c = p_source->p[1].i_pitch
                                 - p_source->p[1].i_visible_pitch;
    const int i_dest_margin = p_dest->p->i_pitch
                               - p_dest->p->i_visible_pitch;

    for( i_y = p_vout->m_render.i_height / 2 ; i_y-- ; )
    {
        p_line1 = p_line2;
        p_line2 += p_dest->p->i_pitch;

        p_y1 = p_y2;
        p_y2 += p_source->p[Y_PLANE].i_pitch;

        for( i_x = p_vout->m_render.i_width / 8 ; i_x-- ; )
        {
            C_YUV420_Y211( );
            C_YUV420_Y211( );
        }

        p_y1 += i_source_margin;
        p_y2 += i_source_margin;
        p_u += i_source_margin_c;
        p_v += i_source_margin_c;
        p_line1 += i_dest_margin;
        p_line2 += i_dest_margin;
    }
}
#endif

/*****************************************************************************
 * Activate: allocate a chroma function
 *****************************************************************************
 * This function allocates and initializes a chroma function
 *****************************************************************************/
int ActivateChromaYUY2( CVideoOutput *p_vout )
{
    if( p_vout->m_render.i_width & 1 || p_vout->m_render.i_height & 1 )
    {
        return -1;
    }

    switch( p_vout->m_render.i_chroma )
    {
        case TME_FOURCC('Y','V','1','2'):
        case TME_FOURCC('I','4','2','0'):
        case TME_FOURCC('I','Y','U','V'):
            switch( p_vout->m_output.i_chroma )
            {
                case TME_FOURCC('Y','U','Y','2'):
                case TME_FOURCC('Y','U','N','V'):
                    p_vout->m_chroma.pf_convert = I420_YUY2;
                    break;

#if !defined (MODULE_NAME_IS_i420_yuy2_altivec)
                case TME_FOURCC('Y','V','Y','U'):
                    p_vout->m_chroma.pf_convert = I420_YVYU;
                    break;

                case TME_FOURCC('U','Y','V','Y'):
                case TME_FOURCC('U','Y','N','V'):
                case TME_FOURCC('Y','4','2','2'):
                    p_vout->m_chroma.pf_convert = I420_UYVY;
                    break;

                case TME_FOURCC('I','U','Y','V'):
                    p_vout->m_chroma.pf_convert = I420_IUYV;
                    break;

                case TME_FOURCC('c','y','u','v'):
                    p_vout->m_chroma.pf_convert = I420_cyuv;
                    break;
#endif

#if defined (MODULE_NAME_IS_i420_yuy2)
                case TME_FOURCC('Y','2','1','1'):
                    p_vout->m_chroma.pf_convert = I420_Y211;
                    break;
#endif

                default:
                    return -1;
            }
            break;

        default:
            return -1;
    }

    return 0;
}
