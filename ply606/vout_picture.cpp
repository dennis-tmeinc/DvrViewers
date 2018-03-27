#include "stdafx.h"
#include <crtdbg.h>
#include <assert.h>
#include "vout_picture.h"
#include "fourcc.h"
#include "trace.h"

/**
 * Display a picture
 *
 * Remove the reservation flag of a picture, which will cause it to be ready
 * for display. The picture won't be displayed until vout_DatePicture has been
 * called.
 */
void vout_DisplayPicture( CVideoOutput *p_vout, picture_t *p_pic )
{
    tme_mutex_lock( &p_vout->m_pictureLock );
    switch( p_pic->i_status )
    {
    case RESERVED_PICTURE:
        p_pic->i_status = RESERVED_DISP_PICTURE;
        break;
    case RESERVED_DATED_PICTURE:
        p_pic->i_status = READY_PICTURE;
        break;
    default:
        _RPT2( _CRT_WARN, "picture to display %p has invalid status %d\n",
                         p_pic, p_pic->i_status );
        break;
    }

    tme_mutex_unlock( &p_vout->m_pictureLock );
}

/**
 * Date a picture
 *
 * Remove the reservation flag of a picture, which will cause it to be ready
 * for display. The picture won't be displayed until vout_DisplayPicture has
 * been called.
 * \param p_vout The vout in question
 * \param p_pic The picture to date
 * \param date The date to display the picture
 */
void vout_DatePicture( CVideoOutput *p_vout,
                       picture_t *p_pic, mtime_t date )
{
    tme_mutex_lock( &p_vout->m_pictureLock );
    p_pic->date = date;
    switch( p_pic->i_status )
    {
    case RESERVED_PICTURE:
        p_pic->i_status = RESERVED_DATED_PICTURE;
        break;
    case RESERVED_DISP_PICTURE:
        p_pic->i_status = READY_PICTURE;
        break;
    default:
        _RPT2( _CRT_WARN, "picture to date %p has invalid status %d\n",
                         p_pic, p_pic->i_status );
        break;
    }

    tme_mutex_unlock( &p_vout->m_pictureLock );
}

/**
 * Allocate a picture in the video output heap.
 *
 * This function creates a reserved image in the video output heap.
 * A null pointer is returned if the function fails. This method provides an
 * already allocated zone of memory in the picture data fields.
 * It needs locking since several pictures can be created by several producers
 * threads.
 */
picture_t *vout_CreatePicture( CVideoOutput *p_vout,
                               bool b_progressive,
                               bool b_top_field_first,
                               unsigned int i_nb_fields )
{
    int         i_pic;                                      /* picture index */
    picture_t * p_pic;
    picture_t * p_freepic = NULL;                      /* first free picture */

    /* Get lock */
    tme_mutex_lock( &p_vout->m_pictureLock );

    /*
     * Look for an empty place in the picture heap.
     */
    for( i_pic = 0; i_pic < p_vout->m_render.i_pictures; i_pic++ )
    {
        p_pic = p_vout->m_render.pp_picture[(p_vout->m_render.i_last_used_pic + i_pic + 1)
                                 % p_vout->m_render.i_pictures];

        switch( p_pic->i_status )
        {
            case DESTROYED_PICTURE:
               /* Memory will not be reallocated, and function can end
                 * immediately - this is the best possible case, since no
                 * memory allocation needs to be done */
                p_pic->i_status   = RESERVED_PICTURE;
                p_pic->i_refcount = 0;
                p_pic->b_force    = 0;

                p_pic->b_progressive        = b_progressive;
                p_pic->i_nb_fields          = i_nb_fields;
                p_pic->b_top_field_first    = b_top_field_first;

                p_vout->m_iHeapSize++;
                p_vout->m_render.i_last_used_pic =
                    ( p_vout->m_render.i_last_used_pic + i_pic + 1 )
                    % p_vout->m_render.i_pictures;
                tme_mutex_unlock( &p_vout->m_pictureLock );
                return( p_pic );

            case FREE_PICTURE:
                /* Picture is empty and ready for allocation */
                p_vout->m_render.i_last_used_pic =
                    ( p_vout->m_render.i_last_used_pic + i_pic + 1 )
                    % p_vout->m_render.i_pictures;
                p_freepic = p_pic;
                break;

            default:
                break;
        }
    }

    /*
     * Prepare picture
     */
    if( p_freepic != NULL )
    {
        vout_AllocatePicture( p_freepic, p_vout->m_render.i_chroma,
                              p_vout->m_render.i_width, p_vout->m_render.i_height,
                              p_vout->m_render.i_aspect );

        if( p_freepic->i_planes )
        {
            /* Copy picture information, set some default values */
            p_freepic->i_status   = RESERVED_PICTURE;
            p_freepic->i_type     = MEMORY_PICTURE;
            p_freepic->b_slow     = 0;

            p_freepic->i_refcount = 0;
            p_freepic->b_force = 0;

            p_freepic->b_progressive        = b_progressive;
            p_freepic->i_nb_fields          = i_nb_fields;
            p_freepic->b_top_field_first    = b_top_field_first;

            p_freepic->i_matrix_coefficients = 1;

            p_vout->m_iHeapSize++;
        }
        else
        {
            /* Memory allocation failed : set picture as empty */
            p_freepic->i_status = FREE_PICTURE;
            p_freepic = NULL;

            _RPT0( _CRT_WARN, "picture allocation failed\n" );
        }

        tme_mutex_unlock( &p_vout->m_pictureLock );

        return( p_freepic );
    }

    /* No free or destroyed picture could be found, but the decoder
     * will try again in a while. */
    tme_mutex_unlock( &p_vout->m_pictureLock );

    return( NULL );
}

/**
 * Remove a permanent or reserved picture from the heap
 *
 * This function frees a previously reserved picture or a permanent
 * picture. It is meant to be used when the construction of a picture aborted.
 * Note that the picture will be destroyed even if it is linked !
 */
void vout_DestroyPicture( CVideoOutput *p_vout, picture_t *p_pic )
{
    tme_mutex_lock( &p_vout->m_pictureLock );

#ifdef DEBUG
    /* Check if picture status is valid */
    if( (p_pic->i_status != RESERVED_PICTURE) &&
        (p_pic->i_status != RESERVED_DATED_PICTURE) &&
        (p_pic->i_status != RESERVED_DISP_PICTURE) )
    {
        _RPT2( _CRT_WARN, "picture to destroy %p has invalid status %d\n",
                         p_pic, p_pic->i_status );
    }
#endif

    p_pic->i_status = DESTROYED_PICTURE;
    p_vout->m_iHeapSize--;

    tme_mutex_unlock( &p_vout->m_pictureLock );
}

/**
 * Increment reference counter of a picture
 *
 * This function increments the reference counter of a picture in the video
 * heap. It needs a lock since several producer threads can access the picture.
 */
void vout_LinkPicture( CVideoOutput *p_vout, picture_t *p_pic )
{
    tme_mutex_lock( &p_vout->m_pictureLock );
    p_pic->i_refcount++;
    tme_mutex_unlock( &p_vout->m_pictureLock );
}

/**
 * Decrement reference counter of a picture
 *
 * This function decrement the reference counter of a picture in the video heap
 */
void vout_UnlinkPicture( CVideoOutput *p_vout, picture_t *p_pic )
{
    tme_mutex_lock( &p_vout->m_pictureLock );
    p_pic->i_refcount--;

    if( p_pic->i_refcount < 0 )
    {
        _RPT2( _CRT_WARN, "picture %p refcount is %i\n",
                 p_pic, p_pic->i_refcount );
        p_pic->i_refcount = 0;
    }

    if( ( p_pic->i_refcount == 0 ) &&
        ( p_pic->i_status == DISPLAYED_PICTURE ) )
    {
        p_pic->i_status = DESTROYED_PICTURE;
        p_vout->m_iHeapSize--;
    }

    tme_mutex_unlock( &p_vout->m_pictureLock );
}

/**
 * Render a picture
 *
 * This function chooses whether the current picture needs to be copied
 * before rendering, does the subpicture magic, and tells the video output
 * thread which direct buffer needs to be displayed.
 */
picture_t * vout_RenderPicture( CVideoOutput *p_vout, picture_t *p_pic,
                                                       subpicture_t *p_subpic )
{
    int i_scale_width, i_scale_height;

    if( p_pic == NULL )
    {
        /* XXX: subtitles */
        return NULL;
    }

    i_scale_width = p_vout->m_fmtOut.i_visible_width * 1000 /
        p_vout->m_fmtIn.i_visible_width;
    i_scale_height = p_vout->m_fmtOut.i_visible_height * 1000 /
        p_vout->m_fmtIn.i_visible_height;

    if( p_pic->i_type == DIRECT_PICTURE )
    {
        if( !p_vout->m_render.b_allow_modify_pics || p_pic->i_refcount ||
            p_pic->b_force )
        {
            /* Picture is in a direct buffer and is still in use,
             * we need to copy it to another direct buffer before
             * displaying it if there are subtitles. */
            if( p_subpic != NULL )
            {
                /* We have subtitles. First copy the picture to
                 * the spare direct buffer, then render the
                 * subtitles. */
                vout_CopyPicture( p_vout, p_vout->m_output.pp_picture[0], p_pic );

                p_vout->m_pSpu->RenderSubpictures( &p_vout->m_fmtOut,
                                       p_vout->m_output.pp_picture[0], p_pic, p_subpic,
                                       i_scale_width, i_scale_height );

				return p_vout->m_output.pp_picture[0];
            }
            /* No subtitles, picture is in a directbuffer so
             * we can display it directly even if it is still
             * in use. */
            return p_pic;
        }

        /* Picture is in a direct buffer but isn't used by the
         * decoder. We can safely render subtitles on it and
         * display it. */
        p_vout->m_pSpu->RenderSubpictures( &p_vout->m_fmtOut, p_pic, p_pic,
                               p_subpic, i_scale_width, i_scale_height );

        return p_pic;
    }

    /* Not a direct buffer. We either need to copy it to a direct buffer,
     * or render it if the chroma isn't the same. */
    if( p_vout->m_bDirect )
    {
        /* Picture is not in a direct buffer, but is exactly the
         * same size as the direct buffers. A memcpy() is enough,
         * then render the subtitles. */

        if( p_vout->Lock( p_vout->PP_OUTPUTPICTURE[0] ) )
            return NULL;

        vout_CopyPicture( p_vout, p_vout->PP_OUTPUTPICTURE[0], p_pic );

        p_vout->m_pSpu->RenderSubpictures( &p_vout->m_fmtOut,
                               p_vout->PP_OUTPUTPICTURE[0], p_pic,
                               p_subpic, i_scale_width, i_scale_height );

        p_vout->Unlock( p_vout->PP_OUTPUTPICTURE[0] );

        return p_vout->PP_OUTPUTPICTURE[0];
    }

	/* Picture is not in a direct buffer, and needs to be converted to
     * another size/chroma. Then the subtitles need to be rendered as
     * well. This usually means software YUV, or hardware YUV with a
     * different chroma. */
    if( p_subpic != NULL && p_vout->m_pPicture[0].b_slow )
    {
        /* The picture buffer is in slow memory. We'll use
         * the "2 * VOUT_MAX_PICTURES + 1" picture as a temporary
         * one for subpictures rendering. */
        picture_t *p_tmp_pic = &p_vout->m_pPicture[2 * VOUT_MAX_PICTURES];

		if( p_tmp_pic->i_status == FREE_PICTURE )
        {
            vout_AllocatePicture( p_tmp_pic, p_vout->m_fmtOut.i_chroma,
                                  p_vout->m_fmtOut.i_width,
                                  p_vout->m_fmtOut.i_height,
                                  p_vout->m_fmtOut.i_aspect );
            p_tmp_pic->i_type = MEMORY_PICTURE;
            p_tmp_pic->i_status = RESERVED_PICTURE;
        }

        /* Convert image to the first direct buffer */
		p_vout->m_chroma.pf_convert(p_vout, p_pic, p_tmp_pic );

        /* Render subpictures on the first direct buffer */
        p_vout->m_pSpu->RenderSubpictures( &p_vout->m_fmtOut, p_tmp_pic,
                               p_tmp_pic, p_subpic,
                               i_scale_width, i_scale_height );

        if( p_vout->Lock( &p_vout->m_pPicture[0] ) )
            return NULL;

        vout_CopyPicture( p_vout, &p_vout->m_pPicture[0], p_tmp_pic );
    }
    else
    {	
		if( p_vout->Lock( &p_vout->m_pPicture[0] ) )
			return NULL;

        /* Convert image to the first direct buffer */
		p_vout->m_chroma.pf_convert(p_vout, p_pic, &p_vout->m_pPicture[0] );

        /* Render subpictures on the first direct buffer */
        p_vout->m_pSpu->RenderSubpictures( &p_vout->m_fmtOut,
							   &p_vout->m_pPicture[0], &p_vout->m_pPicture[0],
                               p_subpic, i_scale_width, i_scale_height );
    }
	p_vout->Unlock( &p_vout->m_pPicture[0] );

    return &p_vout->m_pPicture[0];
}

/**
 * Calculate image window coordinates
 *
 * This function will be accessed by plugins. It calculates the relative
 * position of the output window and the image window.
 */
void vout_PlacePicture( CVideoOutput *p_vout,
                        unsigned int i_width, unsigned int i_height,
                        unsigned int *pi_x, unsigned int *pi_y,
                        unsigned int *pi_width, unsigned int *pi_height )
{
    if( (i_width <= 0) || (i_height <=0) )
    {
        *pi_width = *pi_height = *pi_x = *pi_y = 0;
        return;
    }

    if( p_vout->m_bScale )
    {
        *pi_width = i_width;
        *pi_height = i_height;
    }
    else
    {
        *pi_width = min( i_width, p_vout->m_fmtIn.i_visible_width );
        *pi_height = min( i_height, p_vout->m_fmtIn.i_visible_height );
    }

    if( p_vout->m_fmtIn.i_visible_width * (INT64)p_vout->m_fmtIn.i_sar_num *
        *pi_height / p_vout->m_fmtIn.i_visible_height /
        p_vout->m_fmtIn.i_sar_den > *pi_width )
    {
        *pi_height = p_vout->m_fmtIn.i_visible_height *
            (INT64)p_vout->m_fmtIn.i_sar_den * *pi_width /
            p_vout->m_fmtIn.i_visible_width / p_vout->m_fmtIn.i_sar_num;
    }
    else
    {
        *pi_width = p_vout->m_fmtIn.i_visible_width *
            (INT64)p_vout->m_fmtIn.i_sar_num * *pi_height /
            p_vout->m_fmtIn.i_visible_height / p_vout->m_fmtIn.i_sar_den;
    }

    switch( p_vout->m_iAlignment & VOUT_ALIGN_HMASK )
    {
    case VOUT_ALIGN_LEFT:
        *pi_x = 0;
        break;
    case VOUT_ALIGN_RIGHT:
        *pi_x = i_width - *pi_width;
        break;
    default:
        *pi_x = ( i_width - *pi_width ) / 2;
    }

    switch( p_vout->m_iAlignment & VOUT_ALIGN_VMASK )
    {
    case VOUT_ALIGN_TOP:
        *pi_y = 0;
        break;
    case VOUT_ALIGN_BOTTOM:
        *pi_y = i_height - *pi_height;
        break;
    default:
        *pi_y = ( i_height - *pi_height ) / 2;
    }
}

/**
 * Allocate a new picture in the heap.
 *
 * This function allocates a fake direct buffer in memory, which can be
 * used exactly like a video buffer. The video output thread then manages
 * how it gets displayed.
 */
int vout_AllocatePicture( picture_t *p_pic,
                            fourcc_t i_chroma,
                            int i_width, int i_height, int i_aspect )
{
    int i_bytes, i_index, i_width_aligned, i_height_aligned;

    /* Make sure the real dimensions are a multiple of 16 */
    i_width_aligned = (i_width + 15) >> 4 << 4;
    i_height_aligned = (i_height + 15) >> 4 << 4;

    if( vout_InitPicture( p_pic, i_chroma,
                          i_width, i_height, i_aspect ) != SUCCESS )
    {
        p_pic->i_planes = 0;
        return EGENERIC;
    }

    /* Calculate how big the new image should be */
    i_bytes = p_pic->format.i_bits_per_pixel *
        i_width_aligned * i_height_aligned / 8;

    p_pic->p_data = (PBYTE)memalign( &p_pic->p_data_orig, 16, i_bytes );

    if( p_pic->p_data == NULL )
    {
        p_pic->i_planes = 0;
        return EGENERIC;
    }

    /* Fill the p_pixels field for each plane */
    p_pic->p[ 0 ].p_pixels = p_pic->p_data;

    for( i_index = 1; i_index < p_pic->i_planes; i_index++ )
    {
        p_pic->p[i_index].p_pixels = p_pic->p[i_index-1].p_pixels +
            p_pic->p[i_index-1].i_lines * p_pic->p[i_index-1].i_pitch;
    }

    return SUCCESS;
}


/**
 * Initialise the video format fields given chroma/size.
 *
 * This function initializes all the video_frame_format_t fields given the
 * static properties of a picture (chroma and size).
 * \param p_format Pointer to the format structure to initialize
 * \param i_chroma Chroma to set
 * \param i_width Width to set
 * \param i_height Height to set
 * \param i_aspect Aspect ratio
 */
void vout_InitFormat( video_format_t *p_format, fourcc_t i_chroma,
                      int i_width, int i_height, int i_aspect )
{
    p_format->i_chroma   = i_chroma;
    p_format->i_width    = p_format->i_visible_width  = i_width;
    p_format->i_height   = p_format->i_visible_height = i_height;
    p_format->i_x_offset = p_format->i_y_offset = 0;
    p_format->i_aspect   = i_aspect;

    switch( i_chroma )
    {
        case FOURCC_YUVA:
            p_format->i_bits_per_pixel = 32;
            break;
        case FOURCC_I444:
        case FOURCC_J444:
            p_format->i_bits_per_pixel = 24;
            break;
        case FOURCC_I422:
        case FOURCC_YUY2:
        case FOURCC_UYVY:
        case FOURCC_J422:
            p_format->i_bits_per_pixel = 16;
            p_format->i_bits_per_pixel = 16;
            break;
        case FOURCC_I411:
        case FOURCC_YV12:
        case FOURCC_I420:
        case FOURCC_J420:
        case FOURCC_IYUV:
            p_format->i_bits_per_pixel = 12;
            break;
        case FOURCC_I410:
        case FOURCC_YVU9:
            p_format->i_bits_per_pixel = 9;
            break;
        case FOURCC_Y211:
            p_format->i_bits_per_pixel = 8;
            break;
        case FOURCC_YUVP:
            p_format->i_bits_per_pixel = 8;
            break;

        case FOURCC_RV32:
            p_format->i_bits_per_pixel = 32;
            break;
        case FOURCC_RV24:
            p_format->i_bits_per_pixel = 24;
            break;
        case FOURCC_RV15:
        case FOURCC_RV16:
            p_format->i_bits_per_pixel = 16;
            break;
        case FOURCC_RGB2:
            p_format->i_bits_per_pixel = 8;
            break;
        default:
            p_format->i_bits_per_pixel = 0;
            break;
    }
}

/**
 * Initialise the picture_t fields given chroma/size.
 *
 * This function initializes most of the picture_t fields given a chroma and
 * size. It makes the assumption that stride == width.
 * \param p_this The calling object
 * \param p_pic Pointer to the picture to initialize
 * \param i_chroma The chroma fourcc to set
 * \param i_width The width of the picture
 * \param i_height The height of the picture
 * \param i_aspect The aspect ratio of the picture
 */
int vout_InitPicture( picture_t *p_pic,
                        fourcc_t i_chroma,
                        int i_width, int i_height, int i_aspect )
{
    int i_index, i_width_aligned, i_height_aligned;

    /* Store default values */
    for( i_index = 0; i_index < VOUT_MAX_PLANES; i_index++ )
    {
        p_pic->p[i_index].p_pixels = NULL;
        p_pic->p[i_index].i_pixel_pitch = 1;
    }

    p_pic->pf_release = 0;
    //p_pic->pf_lock = 0;
    //p_pic->pf_unlock = 0;
    p_pic->i_refcount = 0;

    vout_InitFormat( &p_pic->format, i_chroma, i_width, i_height, i_aspect );

    /* Make sure the real dimensions are a multiple of 16 */
    i_width_aligned = (i_width + 15) >> 4 << 4;
    i_height_aligned = (i_height + 15) >> 4 << 4;

    /* Calculate coordinates */
    switch( i_chroma )
    {
        case FOURCC_I411:
            p_pic->p[ Y_PLANE ].i_lines = i_height_aligned;
            p_pic->p[ Y_PLANE ].i_visible_lines = i_height;
            p_pic->p[ Y_PLANE ].i_pitch = i_width_aligned;
            p_pic->p[ Y_PLANE ].i_visible_pitch = i_width;
            p_pic->p[ U_PLANE ].i_lines = i_height_aligned;
            p_pic->p[ U_PLANE ].i_visible_lines = i_height;
            p_pic->p[ U_PLANE ].i_pitch = i_width_aligned / 4;
            p_pic->p[ U_PLANE ].i_visible_pitch = i_width / 4;
            p_pic->p[ V_PLANE ].i_lines = i_height_aligned;
            p_pic->p[ V_PLANE ].i_visible_lines = i_height;
            p_pic->p[ V_PLANE ].i_pitch = i_width_aligned / 4;
            p_pic->p[ V_PLANE ].i_visible_pitch = i_width / 4;
            p_pic->i_planes = 3;
            break;

        case FOURCC_I410:
        case FOURCC_YVU9:
            p_pic->p[ Y_PLANE ].i_lines = i_height_aligned;
            p_pic->p[ Y_PLANE ].i_visible_lines = i_height;
            p_pic->p[ Y_PLANE ].i_pitch = i_width_aligned;
            p_pic->p[ Y_PLANE ].i_visible_pitch = i_width;
            p_pic->p[ U_PLANE ].i_lines = i_height_aligned / 4;
            p_pic->p[ U_PLANE ].i_visible_lines = i_height / 4;
            p_pic->p[ U_PLANE ].i_pitch = i_width_aligned / 4;
            p_pic->p[ U_PLANE ].i_visible_pitch = i_width / 4;
            p_pic->p[ V_PLANE ].i_lines = i_height_aligned / 4;
            p_pic->p[ V_PLANE ].i_visible_lines = i_height / 4;
            p_pic->p[ V_PLANE ].i_pitch = i_width_aligned / 4;
            p_pic->p[ V_PLANE ].i_visible_pitch = i_width / 4;
            p_pic->i_planes = 3;
            break;

        case FOURCC_YV12:
        case FOURCC_I420:
        case FOURCC_IYUV:
        case FOURCC_J420:
            p_pic->p[ Y_PLANE ].i_lines = i_height_aligned;
            p_pic->p[ Y_PLANE ].i_visible_lines = i_height;
            p_pic->p[ Y_PLANE ].i_pitch = i_width_aligned;
            p_pic->p[ Y_PLANE ].i_visible_pitch = i_width;
            p_pic->p[ U_PLANE ].i_lines = i_height_aligned / 2;
            p_pic->p[ U_PLANE ].i_visible_lines = i_height / 2;
            p_pic->p[ U_PLANE ].i_pitch = i_width_aligned / 2;
            p_pic->p[ U_PLANE ].i_visible_pitch = i_width / 2;
            p_pic->p[ V_PLANE ].i_lines = i_height_aligned / 2;
            p_pic->p[ V_PLANE ].i_visible_lines = i_height / 2;
            p_pic->p[ V_PLANE ].i_pitch = i_width_aligned / 2;
            p_pic->p[ V_PLANE ].i_visible_pitch = i_width / 2;
            p_pic->i_planes = 3;
            break;

        case FOURCC_I422:
        case FOURCC_J422:
            p_pic->p[ Y_PLANE ].i_lines = i_height_aligned;
            p_pic->p[ Y_PLANE ].i_visible_lines = i_height;
            p_pic->p[ Y_PLANE ].i_pitch = i_width_aligned;
            p_pic->p[ Y_PLANE ].i_visible_pitch = i_width;
            p_pic->p[ U_PLANE ].i_lines = i_height_aligned;
            p_pic->p[ U_PLANE ].i_visible_lines = i_height;
            p_pic->p[ U_PLANE ].i_pitch = i_width_aligned / 2;
            p_pic->p[ U_PLANE ].i_visible_pitch = i_width / 2;
            p_pic->p[ V_PLANE ].i_lines = i_height_aligned;
            p_pic->p[ V_PLANE ].i_visible_lines = i_height;
            p_pic->p[ V_PLANE ].i_pitch = i_width_aligned / 2;
            p_pic->p[ V_PLANE ].i_visible_pitch = i_width / 2;
            p_pic->i_planes = 3;
            break;

        case FOURCC_I444:
        case FOURCC_J444:
            p_pic->p[ Y_PLANE ].i_lines = i_height_aligned;
            p_pic->p[ Y_PLANE ].i_visible_lines = i_height;
            p_pic->p[ Y_PLANE ].i_pitch = i_width_aligned;
            p_pic->p[ Y_PLANE ].i_visible_pitch = i_width;
            p_pic->p[ U_PLANE ].i_lines = i_height_aligned;
            p_pic->p[ U_PLANE ].i_visible_lines = i_height;
            p_pic->p[ U_PLANE ].i_pitch = i_width_aligned;
            p_pic->p[ U_PLANE ].i_visible_pitch = i_width;
            p_pic->p[ V_PLANE ].i_lines = i_height_aligned;
            p_pic->p[ V_PLANE ].i_visible_lines = i_height;
            p_pic->p[ V_PLANE ].i_pitch = i_width_aligned;
            p_pic->p[ V_PLANE ].i_visible_pitch = i_width;
            p_pic->i_planes = 3;
            break;

        case FOURCC_YUVA:
            p_pic->p[ Y_PLANE ].i_lines = i_height_aligned;
            p_pic->p[ Y_PLANE ].i_visible_lines = i_height;
            p_pic->p[ Y_PLANE ].i_pitch = i_width_aligned;
            p_pic->p[ Y_PLANE ].i_visible_pitch = i_width;
            p_pic->p[ U_PLANE ].i_lines = i_height_aligned;
            p_pic->p[ U_PLANE ].i_visible_lines = i_height;
            p_pic->p[ U_PLANE ].i_pitch = i_width_aligned;
            p_pic->p[ U_PLANE ].i_visible_pitch = i_width;
            p_pic->p[ V_PLANE ].i_lines = i_height_aligned;
            p_pic->p[ V_PLANE ].i_visible_lines = i_height;
            p_pic->p[ V_PLANE ].i_pitch = i_width_aligned;
            p_pic->p[ V_PLANE ].i_visible_pitch = i_width;
            p_pic->p[ A_PLANE ].i_lines = i_height_aligned;
            p_pic->p[ A_PLANE ].i_visible_lines = i_height;
            p_pic->p[ A_PLANE ].i_pitch = i_width_aligned;
            p_pic->p[ A_PLANE ].i_visible_pitch = i_width;
            p_pic->i_planes = 4;
            break;

        case FOURCC_YUVP:
            p_pic->p->i_lines = i_height_aligned;
            p_pic->p->i_visible_lines = i_height;
            p_pic->p->i_pitch = i_width_aligned;
            p_pic->p->i_visible_pitch = i_width;
            p_pic->p->i_pixel_pitch = 8;
            p_pic->i_planes = 1;
            break;

        case FOURCC_Y211:
            p_pic->p->i_lines = i_height_aligned;
            p_pic->p->i_visible_lines = i_height;
            p_pic->p->i_pitch = i_width_aligned;
            p_pic->p->i_visible_pitch = i_width;
            p_pic->p->i_pixel_pitch = 4;
            p_pic->i_planes = 1;
            break;

        case FOURCC_UYVY:
        case FOURCC_YUY2:
            p_pic->p->i_lines = i_height_aligned;
            p_pic->p->i_visible_lines = i_height;
            p_pic->p->i_pitch = i_width_aligned * 2;
            p_pic->p->i_visible_pitch = i_width * 2;
            p_pic->p->i_pixel_pitch = 4;
            p_pic->i_planes = 1;
            break;

        case FOURCC_RGB2:
            p_pic->p->i_lines = i_height_aligned;
            p_pic->p->i_visible_lines = i_height;
            p_pic->p->i_pitch = i_width_aligned;
            p_pic->p->i_visible_pitch = i_width;
            p_pic->p->i_pixel_pitch = 1;
            p_pic->i_planes = 1;
            break;

        case FOURCC_RV15:
            p_pic->p->i_lines = i_height_aligned;
            p_pic->p->i_visible_lines = i_height;
            p_pic->p->i_pitch = i_width_aligned * 2;
            p_pic->p->i_visible_pitch = i_width * 2;
            p_pic->p->i_pixel_pitch = 2;
            p_pic->i_planes = 1;
            break;

        case FOURCC_RV16:
            p_pic->p->i_lines = i_height_aligned;
            p_pic->p->i_visible_lines = i_height;
            p_pic->p->i_pitch = i_width_aligned * 2;
            p_pic->p->i_visible_pitch = i_width * 2;
            p_pic->p->i_pixel_pitch = 2;
            p_pic->i_planes = 1;
            break;

        case FOURCC_RV24:
            p_pic->p->i_lines = i_height_aligned;
            p_pic->p->i_visible_lines = i_height;
            p_pic->p->i_pitch = i_width_aligned * 3;
            p_pic->p->i_visible_pitch = i_width * 3;
            p_pic->p->i_pixel_pitch = 3;
            p_pic->i_planes = 1;
            break;

        case FOURCC_RV32:
            p_pic->p->i_lines = i_height_aligned;
            p_pic->p->i_visible_lines = i_height;
            p_pic->p->i_pitch = i_width_aligned * 4;
            p_pic->p->i_visible_pitch = i_width * 4;
            p_pic->p->i_pixel_pitch = 4;
            p_pic->i_planes = 1;
            break;

        default:
            _RPT2( _CRT_WARN, "unknown chroma type 0x%.8x (%4.4s)\n",
                             i_chroma, (char*)&i_chroma );
            p_pic->i_planes = 0;
            return EGENERIC;
    }

    return SUCCESS;
}

/**
 * Compare two chroma values
 *
 * This function returns 1 if the two fourcc values given as argument are
 * the same format (eg. UYVY/UYNV) or almost the same format (eg. I420/YV12)
 */
int vout_ChromaCmp( fourcc_t i_chroma, fourcc_t i_amorhc )
{
    /* If they are the same, they are the same ! */
    if( i_chroma == i_amorhc )
    {
        return 1;
    }

    /* Check for equivalence classes */
    switch( i_chroma )
    {
        case FOURCC_I420:
        case FOURCC_IYUV:
        case FOURCC_YV12:
            switch( i_amorhc )
            {
                case FOURCC_I420:
                case FOURCC_IYUV:
                case FOURCC_YV12:
                    return 1;

                default:
                    return 0;
            }

        case FOURCC_UYVY:
        case FOURCC_UYNV:
        case FOURCC_Y422:
            switch( i_amorhc )
            {
                case FOURCC_UYVY:
                case FOURCC_UYNV:
                case FOURCC_Y422:
                    return 1;

                default:
                    return 0;
            }

        case FOURCC_YUY2:
        case FOURCC_YUNV:
            switch( i_amorhc )
            {
                case FOURCC_YUY2:
                case FOURCC_YUNV:
                    return 1;

                default:
                    return 0;
            }

        default:
            return 0;
    }
}

/*****************************************************************************
 * vout_CopyPicture: copy a picture to another one
 *****************************************************************************
 * This function takes advantage of the image format, and reduces the
 * number of calls to memcpy() to the minimum. Source and destination
 * images must have same width (hence i_visible_pitch), height, and chroma.
 *****************************************************************************/
void vout_CopyPicture( CVideoOutput *p_this,
                         picture_t *p_dest, picture_t *p_src )
{
    int i;

    for( i = 0; i < p_src->i_planes ; i++ )
    {
        if( p_src->p[i].i_pitch == p_dest->p[i].i_pitch )
        {
            /* There are margins, but with the same width : perfect ! */
            memcpy(p_dest->p[i].p_pixels, p_src->p[i].p_pixels,
                         p_src->p[i].i_pitch * p_src->p[i].i_visible_lines );
        }
        else
        {
            /* We need to proceed line by line */
            UINT8 *p_in = p_src->p[i].p_pixels;
            UINT8 *p_out = p_dest->p[i].p_pixels;
            int i_line;

            for( i_line = p_src->p[i].i_visible_lines; i_line--; )
            {
                memcpy( p_out, p_in, p_src->p[i].i_visible_pitch );
                p_in += p_src->p[i].i_pitch;
                p_out += p_dest->p[i].i_pitch;
            }
        }
    }

    p_dest->date = p_src->date;
    p_dest->b_force = p_src->b_force;
    p_dest->i_nb_fields = p_src->i_nb_fields;
    p_dest->b_progressive = p_src->b_progressive;
    p_dest->b_top_field_first = p_src->b_top_field_first;
}


static UINT64 GCD( UINT64 a, UINT64 b )
{
    if( b ) return GCD( b, a % b );
    else return a;
}

bool ureduce( unsigned *pi_dst_nom, unsigned *pi_dst_den,
                        UINT64 i_nom, UINT64 i_den, UINT64 i_max )
{
    bool b_exact = 1;
    UINT64 i_gcd;

    if( i_den == 0 )
    {
        *pi_dst_nom = 0;
        *pi_dst_den = 1;
        return 1;
    }

    i_gcd = GCD( i_nom, i_den );
    i_nom /= i_gcd;
    i_den /= i_gcd;

    if( i_max == 0 ) i_max = 0xFFFFFFFFLL;

    if( i_nom > i_max || i_den > i_max )
    {
        UINT64 i_a0_num = 0, i_a0_den = 1, i_a1_num = 1, i_a1_den = 0;
        b_exact = 0;

        for( ; ; )
        {
            UINT64 i_x = i_nom / i_den;
            UINT64 i_a2n = i_x * i_a1_num + i_a0_num;
            UINT64 i_a2d = i_x * i_a1_den + i_a0_den;

            if( i_a2n > i_max || i_a2d > i_max ) break;

            i_nom %= i_den;

            i_a0_num = i_a1_num; i_a0_den = i_a1_den;
            i_a1_num = i_a2n; i_a1_den = i_a2d;
            if( i_nom == 0 ) break;
            i_x = i_nom; i_nom = i_den; i_den = i_x;
        }
        i_nom = i_a1_num;
        i_den = i_a1_den;
    }

    *pi_dst_nom = i_nom;
    *pi_dst_den = i_den;

    return b_exact;
}

void video_format_Setup( video_format_t *p_fmt, fourcc_t i_chroma,
                         int i_width, int i_height,
                         int i_sar_num, int i_sar_den )
{
    p_fmt->i_chroma         = fourcc_GetCodec( VIDEO_ES, i_chroma );
    p_fmt->i_width          =
    p_fmt->i_visible_width  = i_width;
    p_fmt->i_height         =
    p_fmt->i_visible_height = i_height;
    p_fmt->i_x_offset       =
    p_fmt->i_y_offset       = 0;
    ureduce( &p_fmt->i_sar_num, &p_fmt->i_sar_den,
                 i_sar_num, i_sar_den, 0 );

    switch( p_fmt->i_chroma )
    {
    case CODEC_YUVA:
        p_fmt->i_bits_per_pixel = 32;
        break;
    case CODEC_I444:
    case CODEC_J444:
        p_fmt->i_bits_per_pixel = 24;
        break;
    case CODEC_I422:
    case CODEC_YUYV:
    case CODEC_YVYU:
    case CODEC_UYVY:
    case CODEC_VYUY:
    case CODEC_J422:
        p_fmt->i_bits_per_pixel = 16;
        break;
    case CODEC_I440:
    case CODEC_J440:
        p_fmt->i_bits_per_pixel = 16;
        break;
    case CODEC_I411:
    case CODEC_YV12:
    case CODEC_I420:
    case CODEC_J420:
        p_fmt->i_bits_per_pixel = 12;
        break;
    case CODEC_YV9:
    case CODEC_I410:
        p_fmt->i_bits_per_pixel = 9;
        break;
    case CODEC_Y211:
        p_fmt->i_bits_per_pixel = 8;
        break;
    case CODEC_YUVP:
        p_fmt->i_bits_per_pixel = 8;
        break;

    case CODEC_RGB32:
    case CODEC_RGBA:
        p_fmt->i_bits_per_pixel = 32;
        break;
    case CODEC_RGB24:
        p_fmt->i_bits_per_pixel = 24;
        break;
    case CODEC_RGB15:
    case CODEC_RGB16:
        p_fmt->i_bits_per_pixel = 16;
        break;
    case CODEC_RGB8:
        p_fmt->i_bits_per_pixel = 8;
        break;

    case CODEC_GREY:
    case CODEC_RGBP:
        p_fmt->i_bits_per_pixel = 8;
        break;

    default:
        p_fmt->i_bits_per_pixel = 0;
        break;
    }
}

/*****************************************************************************
 *
 *****************************************************************************/
typedef struct
{
    unsigned     i_plane_count;
    struct
    {
        struct
        {
            unsigned i_num;
            unsigned i_den;
        } w;
        struct
        {
            unsigned i_num;
            unsigned i_den;
        } h;
    } p[VOUT_MAX_PLANES];
    unsigned i_pixel_size;

} chroma_description_t;

#define PLANAR(n, w_den, h_den) \
    { n, { {{1,1}, {1,1}}, {{1,w_den}, {1,h_den}}, {{1,w_den}, {1,h_den}}, {{1,1}, {1,1}} }, 1 }
#define PACKED(size) \
    { 1, { {{1,1}, {1,1}} }, size }

static const struct
{
    fourcc_t            p_fourcc[5];
    chroma_description_t    description;
} p_chromas[] = {
    { { CODEC_I411, 0 },                                 PLANAR(3, 4, 1) },
    { { CODEC_I410, CODEC_YV9, 0 },                  PLANAR(3, 4, 4) },
    { { CODEC_YV12, CODEC_I420, CODEC_J420, 0 }, PLANAR(3, 2, 2) },
    { { CODEC_I422, CODEC_J422, 0 },                 PLANAR(3, 2, 1) },
    { { CODEC_I440, CODEC_J440, 0 },                 PLANAR(3, 1, 2) },
    { { CODEC_I444, CODEC_J444, 0 },                 PLANAR(3, 1, 1) },
    { { CODEC_YUVA, 0 },                                 PLANAR(4, 1, 1) },

    { { CODEC_UYVY, CODEC_VYUY, CODEC_YUYV, CODEC_YVYU, 0 }, PACKED(2) },
    { { CODEC_RGB8, CODEC_GREY, CODEC_YUVP, CODEC_RGBP, 0 }, PACKED(1) },
    { { CODEC_RGB16, CODEC_RGB15, 0 },                               PACKED(2) },
    { { CODEC_RGB24, 0 },                                                PACKED(3) },
    { { CODEC_RGB32, CODEC_RGBA, 0 },                                PACKED(4) },

    { { CODEC_Y211, 0 }, { 1, { {{1,4}, {1,1}} }, 4 } },

    { {0}, { 0, {}, 0 } }
};

#undef PACKED
#undef PLANAR

static const chroma_description_t *fourcc_GetChromaDescription( fourcc_t i_fourcc )
{
    for( unsigned i = 0; p_chromas[i].p_fourcc[0]; i++ )
    {
        const fourcc_t *p_fourcc = p_chromas[i].p_fourcc;
        for( unsigned j = 0; p_fourcc[j]; j++ )
        {
            if( p_fourcc[j] == i_fourcc )
                return &p_chromas[i].description;
        }
    }
    return NULL;
}

static int LCM( int a, int b )
{
    return a * b / GCD( a, b );
}

int picture_Setup( picture_t *p_picture, fourcc_t i_chroma,
                   int i_width, int i_height, int i_sar_num, int i_sar_den )
{
    /* Store default values */
    p_picture->i_planes = 0;
    for( unsigned i = 0; i < VOUT_MAX_PLANES; i++ )
    {
        plane_t *p = &p_picture->p[i];
        p->p_pixels = NULL;
        p->i_pixel_pitch = 0;
    }

    p_picture->pf_release = NULL;
    //p_picture->p_release_sys = NULL;
    p_picture->i_refcount = 0;

    //p_picture->i_qtype = QTYPE_NONE;
    //p_picture->i_qstride = 0;
    //p_picture->p_q = NULL;

    video_format_Setup( &p_picture->format, i_chroma, i_width, i_height,
                        i_sar_num, i_sar_den );

    const chroma_description_t *p_dsc =
        fourcc_GetChromaDescription( p_picture->format.i_chroma );
    if( !p_dsc )
        return EGENERIC;

    /* We want V (width/height) to respect:
        (V * p_dsc->p[i].w.i_num) % p_dsc->p[i].w.i_den == 0
        (V * p_dsc->p[i].w.i_num/p_dsc->p[i].w.i_den * p_dsc->i_pixel_size) % 16 == 0
       Which is respected if you have
       V % lcm( p_dsc->p[0..planes].w.i_den * 16) == 0
    */
    int i_modulo_w = 1;
    int i_modulo_h = 1;
    int i_ratio_h  = 1;
    for( unsigned i = 0; i < p_dsc->i_plane_count; i++ )
    {
        i_modulo_w = LCM( i_modulo_w, 16 * p_dsc->p[i].w.i_den );
        i_modulo_h = LCM( i_modulo_h, 16 * p_dsc->p[i].h.i_den );
        if( i_ratio_h < p_dsc->p[i].h.i_den )
            i_ratio_h = p_dsc->p[i].h.i_den;
    }

    const int i_width_aligned  = ( i_width  + i_modulo_w - 1 ) / i_modulo_w * i_modulo_w;
    const int i_height_aligned = ( i_height + i_modulo_h - 1 ) / i_modulo_h * i_modulo_h;
    const int i_height_extra   = 2 * i_ratio_h; /* This one is a hack for some ASM functions */
    for( unsigned i = 0; i < p_dsc->i_plane_count; i++ )
    {
        plane_t *p = &p_picture->p[i];

        p->i_lines         = (i_height_aligned + i_height_extra ) * p_dsc->p[i].h.i_num / p_dsc->p[i].h.i_den;
        p->i_visible_lines = i_height * p_dsc->p[i].h.i_num / p_dsc->p[i].h.i_den;
        p->i_pitch         = i_width_aligned * p_dsc->p[i].w.i_num / p_dsc->p[i].w.i_den * p_dsc->i_pixel_size;
        p->i_visible_pitch = i_width * p_dsc->p[i].w.i_num / p_dsc->p[i].w.i_den * p_dsc->i_pixel_size;
        p->i_pixel_pitch   = p_dsc->i_pixel_size;

        assert( (p->i_pitch % 16) == 0 );
    }
    p_picture->i_planes  = p_dsc->i_plane_count;

    return SUCCESS;
}
