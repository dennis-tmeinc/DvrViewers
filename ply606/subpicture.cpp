#include "stdafx.h"
#include "subpicture.h"
#include "vout_picture.h"
#include "vfilter_blend.h"
#include "freetype.h"

static es_format_t null_es_format = {0};

CSubPicture::CSubPicture(tme_object_t *p_this)
{
    int i_index;

    for (i_index = 0; i_index < VOUT_MAX_SUBPICTURES; i_index++) {
        m_pSubpicture[i_index].i_status = FREE_SUBPICTURE;
    }

    m_iMargin = 0;
	m_pBlend = NULL;
    m_pText = NULL;
    m_pScale = NULL;
	m_bForcePalette = false; // always
	m_bForceCrop = false;    // always

    /* Register the default subpicture channel */
    m_iChannel = 2;

    tme_mutex_init(p_this, &m_lock);
}

CSubPicture::~CSubPicture()
{
    int i_index;

    /* Destroy all remaining subpictures */
    for( i_index = 0; i_index < VOUT_MAX_SUBPICTURES; i_index++ )
    {
        if( m_pSubpicture[i_index].i_status != FREE_SUBPICTURE )
        {
            DestroySubpicture( &m_pSubpicture[i_index] );
        }
    }

    if( m_pBlend )
    {
		free(m_pBlend);
    }

    if( m_pText )
    {
		delete m_pText;
    }

    if( m_pScale )
    {
		// do cleaning here
    }

    tme_mutex_destroy( &m_lock );
}

int CSubPicture::Init()
{
    m_iMargin = 0;
    return 0;
}

/**
 * Display a subpicture
 *
 * Remove the reservation flag of a subpicture, which will cause it to be
 * ready for display.
 * \param p_subpic the subpicture to display
 */
void CSubPicture::DisplaySubpicture( subpicture_t *p_subpic )
{
    /* Check if status is valid */
    if( p_subpic->i_status != RESERVED_SUBPICTURE )
    {
        _RPT2(_CRT_WARN, "subpicture %p has invalid status #%d\n",
                 p_subpic, p_subpic->i_status );
    }

    /* Remove reservation flag */
    p_subpic->i_status = READY_SUBPICTURE;

    if( p_subpic->i_channel == DEFAULT_CHAN )
    {
        p_subpic->i_channel = 0xFFFF;
		int default_channel = DEFAULT_CHAN;
        Control( SPU_CHANNEL_CLEAR, &default_channel );
        p_subpic->i_channel = DEFAULT_CHAN;
    }
}

/**
 * Allocate a subpicture in the spu heap.
 *
 * This function create a reserved subpicture in the spu heap.
 * A null pointer is returned if the function fails. This method provides an
 * already allocated zone of memory in the spu data fields. It needs locking
 * since several pictures can be created by several producers threads.
 * \return NULL on error, a reserved subpicture otherwise
 */
subpicture_t *CSubPicture::CreateSubpicture()
{
    int                 i_subpic;                        /* subpicture index */
    subpicture_t *      p_subpic = NULL;            /* first free subpicture */

    /* Get lock */
    tme_mutex_lock( &m_lock );

    /*
     * Look for an empty place
     */
    p_subpic = NULL;
    for( i_subpic = 0; i_subpic < VOUT_MAX_SUBPICTURES; i_subpic++ )
    {
        if( m_pSubpicture[i_subpic].i_status == FREE_SUBPICTURE )
        {
            /* Subpicture is empty and ready for allocation */
            p_subpic = &m_pSubpicture[i_subpic];
            m_pSubpicture[i_subpic].i_status = RESERVED_SUBPICTURE;
            break;
        }
    }

    /* If no free subpicture could be found */
    if( p_subpic == NULL )
    {
        _RPT0(_CRT_WARN, "subpicture heap is full\n");
        tme_mutex_unlock( &m_lock );
        return NULL;
    }

    /* Copy subpicture information, set some default values */
    memset( p_subpic, 0, sizeof(subpicture_t) );
    p_subpic->i_status   = RESERVED_SUBPICTURE;
    p_subpic->b_absolute = true;
    p_subpic->b_pausable = false;
    p_subpic->b_fade     = false;
    p_subpic->i_alpha    = 0xFF;
    p_subpic->p_region   = 0;
    //p_subpic->pf_render  = 0;
    //p_subpic->pf_destroy = 0;
    //p_subpic->p_sys      = 0;
    tme_mutex_unlock( &m_lock );

    //p_subpic->pf_create_region = __spu_CreateRegion;
    //p_subpic->pf_make_region = __spu_MakeRegion;
    //p_subpic->pf_destroy_region = __spu_DestroyRegion;

    return p_subpic;
}

/**
 * Remove a subpicture from the heap
 *
 * This function frees a previously reserved subpicture.
 * It is meant to be used when the construction of a picture aborted.
 * This function does not need locking since reserved subpictures are ignored
 * by the spu.
 */
void CSubPicture::DestroySubpicture( subpicture_t *p_subpic )
{
    /* Get lock */
    tme_mutex_lock( &m_lock );

    /* There can be race conditions so we need to check the status */
    if( p_subpic->i_status == FREE_SUBPICTURE )
    {
		tme_mutex_unlock( &m_lock );
        return;
    }

    /* Check if status is valid */
    if( ( p_subpic->i_status != RESERVED_SUBPICTURE )
           && ( p_subpic->i_status != READY_SUBPICTURE ) )
    {
        _RPT2(_CRT_WARN, "subpicture %p has invalid status %d\n",
                         p_subpic, p_subpic->i_status );
    }

    while( p_subpic->p_region )
    {
        subpicture_region_t *p_region = p_subpic->p_region;
        p_subpic->p_region = p_region->p_next;
        DestroyRegion( p_region );
    }

    //if( p_subpic->pf_destroy ) p_subpic->pf_destroy( p_subpic );

    p_subpic->i_status = FREE_SUBPICTURE;

    tme_mutex_unlock( &m_lock );
}

/*****************************************************************************
 * spu_RenderSubpictures: render a subpicture list
 *****************************************************************************
 * This function renders all sub picture units in the list.
 *****************************************************************************/
void CSubPicture::RenderSubpictures( video_format_t *p_fmt,
                            picture_t *p_pic_dst, picture_t *p_pic_src,
                            subpicture_t *p_subpic,
                            int i_scale_width_orig, int i_scale_height_orig )
{
    /* Get lock */
    tme_mutex_lock( &m_lock );

    /* Check i_status again to make sure spudec hasn't destroyed the subpic */
    while( p_subpic != NULL && p_subpic->i_status != FREE_SUBPICTURE )
    {
        subpicture_region_t *p_region = p_subpic->p_region;
        int i_scale_width, i_scale_height;
        int i_subpic_x = p_subpic->i_x;

        /* Load the blending module */
        if( !m_pBlend && p_region )
        {
            m_pBlend = (filter_t *)malloc( sizeof(filter_t) );
 			es_format_Copy(&m_pBlend->fmt_in, &null_es_format); 
			es_format_Copy(&m_pBlend->fmt_out, &null_es_format); 
           m_pBlend->fmt_out.video.i_x_offset =
                m_pBlend->fmt_out.video.i_y_offset = 0;
            m_pBlend->fmt_out.video.i_aspect = p_fmt->i_aspect;
            m_pBlend->fmt_out.video.i_chroma = p_fmt->i_chroma;
            m_pBlend->fmt_in.video.i_chroma = TME_FOURCC('Y','U','V','P');
			if (OpenFilterBlender(m_pBlend)) {
				free(m_pBlend);
				m_pBlend = NULL;
			}
        }

        /* Load the text rendering module */
        if( !m_pText && p_region && p_region->fmt.i_chroma == TME_FOURCC('T','E','X','T') )
        { 
			filter_t text;
			es_format_Copy(&text.fmt_in, &null_es_format); 
			es_format_Copy(&text.fmt_out, &null_es_format); 
			text.fmt_out.video.i_width =
                text.fmt_out.video.i_visible_width =
                p_fmt->i_width;
            text.fmt_out.video.i_height =
                text.fmt_out.video.i_visible_height =
                p_fmt->i_height;

			m_pText = new CFreetype(&text);
			if (m_pText->Create()) {
				delete m_pText;
				m_pText = NULL;
			}
            //p_spu->p_text->pf_sub_buffer_new = spu_new_buffer;
            //p_spu->p_text->pf_sub_buffer_del = spu_del_buffer;

        }
        if( m_pText )
        {
            if( p_subpic->i_original_picture_height > 0 &&
                p_subpic->i_original_picture_width  > 0 )
            {
                m_pText->m_fmt_out.video.i_width =
                    m_pText->m_fmt_out.video.i_visible_width =
                    p_subpic->i_original_picture_width;
                m_pText->m_fmt_out.video.i_height =
                    m_pText->m_fmt_out.video.i_visible_height =
                    p_subpic->i_original_picture_height;
            }
            else
            {
                m_pText->m_fmt_out.video.i_width =
                    m_pText->m_fmt_out.video.i_visible_width =
                    p_fmt->i_width;
                m_pText->m_fmt_out.video.i_height =
                    m_pText->m_fmt_out.video.i_visible_height =
                    p_fmt->i_height;
            }
        }

        i_scale_width = i_scale_width_orig;
        i_scale_height = i_scale_height_orig;

       if( p_subpic->i_original_picture_height > 0 &&
            p_subpic->i_original_picture_width  > 0 )
        {
            i_scale_width = i_scale_width * p_fmt->i_width /
                             p_subpic->i_original_picture_width;
            i_scale_height = i_scale_height * p_fmt->i_height /
                             p_subpic->i_original_picture_height;
        }
        else if( p_subpic->i_original_picture_height > 0 )
        {
            i_scale_height = i_scale_height * p_fmt->i_height /
                             p_subpic->i_original_picture_height;
            i_scale_width = i_scale_height * i_scale_height / p_fmt->i_height; 
        }

       /* Set default subpicture aspect ratio */
        if( p_region && p_region->fmt.i_aspect &&
            (!p_region->fmt.i_sar_num || !p_region->fmt.i_sar_den) )
        {
            p_region->fmt.i_sar_den = p_region->fmt.i_aspect;
            p_region->fmt.i_sar_num = VOUT_ASPECT_FACTOR;
        }
        if( p_region &&
            (!p_region->fmt.i_sar_num || !p_region->fmt.i_sar_den) )
        {
            p_region->fmt.i_sar_den = p_fmt->i_sar_den;
            p_region->fmt.i_sar_num = p_fmt->i_sar_num;
        }

        /* Take care of the aspect ratio */
        if( p_region && p_region->fmt.i_sar_num * p_fmt->i_sar_den !=
            p_region->fmt.i_sar_den * p_fmt->i_sar_num )
        {
            i_scale_width = i_scale_width *
                (INT64)p_region->fmt.i_sar_num * p_fmt->i_sar_den /
                p_region->fmt.i_sar_den / p_fmt->i_sar_num;
            i_subpic_x = p_subpic->i_x * i_scale_width / 1000;
        }

       /* Load the scaling module */
        if( !m_pScale && (i_scale_width != 1000 ||
            i_scale_height != 1000) )
        {
			_RPT0(_CRT_WARN, "Need scaling module\n");
        }

		while( p_region && m_pBlend )
        {
            int i_fade_alpha = 255;
            int i_x_offset = p_region->i_x + i_subpic_x;
            int i_y_offset = p_region->i_y + p_subpic->i_y;

            if( p_region->fmt.i_chroma == TME_FOURCC('T','E','X','T') )
            {
                if( m_pText )
                {
                    p_region->i_align = p_subpic->i_flags;
                    m_pText->RenderText( p_region, p_region ); 
                }
            }

            /* Force palette if requested */
            if( m_bForcePalette && TME_FOURCC('Y','U','V','P') ==
                p_region->fmt.i_chroma )
            {
                memcpy( p_region->fmt.p_palette->palette,
                        m_palette, 16 );
            }

            /* Scale SPU if necessary */
            if( p_region->p_cache )
            {
                if( i_scale_width * p_region->fmt.i_width / 1000 !=
                    p_region->p_cache->fmt.i_width ||
                    i_scale_height * p_region->fmt.i_height / 1000 !=
                    p_region->p_cache->fmt.i_height )
                {
                    DestroyRegion( p_region->p_cache );
                    p_region->p_cache = 0;
                }
            }

            if( (i_scale_width != 1000 || i_scale_height != 1000) &&
                m_pScale && !p_region->p_cache )
            {
				_RPT0(_CRT_WARN, "Need scaling module2\n");
            }
            if( (i_scale_width != 1000 || i_scale_height != 1000) &&
                m_pScale && p_region->p_cache )
            {
                p_region = p_region->p_cache;
            }

            if( p_subpic->i_flags & SUBPICTURE_ALIGN_BOTTOM )
            {
                i_y_offset = p_fmt->i_height - p_region->fmt.i_height -
                    p_subpic->i_y;
            }
            else if ( !(p_subpic->i_flags & SUBPICTURE_ALIGN_TOP) )
            {
                i_y_offset = p_fmt->i_height / 2 - p_region->fmt.i_height / 2;
            }

            if( p_subpic->i_flags & SUBPICTURE_ALIGN_RIGHT )
            {
                i_x_offset = p_fmt->i_width - p_region->fmt.i_width -
                    i_subpic_x;
            }
            else if ( !(p_subpic->i_flags & SUBPICTURE_ALIGN_LEFT) )
            {
                i_x_offset = p_fmt->i_width / 2 - p_region->fmt.i_width / 2;
            }

            if( p_subpic->b_absolute )
            {
                i_x_offset = p_region->i_x +
                    i_subpic_x * i_scale_width / 1000;
                i_y_offset = p_region->i_y +
                    p_subpic->i_y * i_scale_height / 1000;

            }

            if( m_iMargin != 0 && m_bForceCrop == false )
            {
                int i_diff = 0;
                int i_low = i_y_offset - m_iMargin;
                int i_high = i_y_offset + p_region->fmt.i_height - m_iMargin;

                /* crop extra margin to keep within bounds */
                if( i_low < 0 ) i_diff = i_low;
                if( i_high > (int)p_fmt->i_height ) i_diff = i_high - p_fmt->i_height;
                i_y_offset -= ( m_iMargin + i_diff );
            }

            m_pBlend->fmt_in.video = p_region->fmt;

            /* Force cropping if requested */
            if( m_bForceCrop )
            {
                video_format_t *p_fmt = &m_pBlend->fmt_in.video;
                int i_crop_x = m_iCrop_x * i_scale_width / 1000;
                int i_crop_y = m_iCrop_y * i_scale_height / 1000;
                int i_crop_width = m_iCrop_width * i_scale_width / 1000;
                int i_crop_height = m_iCrop_height * i_scale_height/1000;

                /* Find the intersection */
                if( i_crop_x + i_crop_width <= i_x_offset ||
                    i_x_offset + (int)p_fmt->i_visible_width < i_crop_x ||
                    i_crop_y + i_crop_height <= i_y_offset ||
                    i_y_offset + (int)p_fmt->i_visible_height < i_crop_y )
                {
                    /* No intersection */
                    p_fmt->i_visible_width = p_fmt->i_visible_height = 0;
                }
                else
                {
                    int i_x, i_y, i_x_end, i_y_end;
                    i_x = max( i_crop_x, i_x_offset );
                    i_y = max( i_crop_y, i_y_offset );
                    i_x_end = min( i_crop_x + i_crop_width,
                                   i_x_offset + (int)p_fmt->i_visible_width );
                    i_y_end = min( i_crop_y + i_crop_height,
                                   i_y_offset + (int)p_fmt->i_visible_height );

                    p_fmt->i_x_offset = i_x - i_x_offset;
                    p_fmt->i_y_offset = i_y - i_y_offset;
                    p_fmt->i_visible_width = i_x_end - i_x;
                    p_fmt->i_visible_height = i_y_end - i_y;

                    i_x_offset = i_x;
                    i_y_offset = i_y;
                }
            }

            /* Update the output picture size */
            m_pBlend->fmt_out.video.i_width =
                m_pBlend->fmt_out.video.i_visible_width =
                    p_fmt->i_width;
            m_pBlend->fmt_out.video.i_height =
                m_pBlend->fmt_out.video.i_visible_height =
                    p_fmt->i_height;

            if( p_subpic->b_fade )
            {
                mtime_t i_fade_start = ( p_subpic->i_stop +
                                         p_subpic->i_start ) / 2;
                mtime_t i_now = mdate();
                if( i_now >= i_fade_start && p_subpic->i_stop > i_fade_start )
                {
                    i_fade_alpha = 255 * ( p_subpic->i_stop - i_now ) /
                                   ( p_subpic->i_stop - i_fade_start );
                }
            }

            VideoBlend( m_pBlend, p_pic_dst,
                p_pic_src, &p_region->picture, i_x_offset, i_y_offset,
                i_fade_alpha * p_subpic->i_alpha / 255 );

            p_region = p_region->p_next;
        }

        p_subpic = p_subpic->p_next;
   }

    tme_mutex_unlock( &m_lock );
}

/*****************************************************************************
 * spu_SortSubpictures: find the subpictures to display
 *****************************************************************************
 * This function parses all subpictures and decides which ones need to be
 * displayed. This operation does not need lock, since only READY_SUBPICTURE
 * are handled. If no picture has been selected, display_date will depend on
 * the subpicture.
 * We also check for ephemer DVD subpictures (subpictures that have
 * to be removed if a newer one is available), which makes it a lot
 * more difficult to guess if a subpicture has to be rendered or not.
 *****************************************************************************/
subpicture_t *CSubPicture::SortSubpictures( mtime_t display_date,
                                   bool b_paused )
{
    int i_index, i_channel;
    subpicture_t *p_subpic = NULL;
    subpicture_t *p_ephemer;
    mtime_t      ephemer_date;

    /* We get an easily parsable chained list of subpictures which
     * ends with NULL since p_subpic was initialized to NULL. */
    for( i_channel = 0; i_channel < m_iChannel; i_channel++ )
    {
        p_ephemer = 0;
        ephemer_date = 0;

        for( i_index = 0; i_index < VOUT_MAX_SUBPICTURES; i_index++ )
        {
            if( m_pSubpicture[i_index].i_channel != i_channel ||
                m_pSubpicture[i_index].i_status != READY_SUBPICTURE )
            {
                continue;
            }
            if( display_date &&
                display_date < m_pSubpicture[i_index].i_start )
            {
                /* Too early, come back next monday */
                continue;
            }

            if( m_pSubpicture[i_index].i_start > ephemer_date )
                ephemer_date = m_pSubpicture[i_index].i_start;

            if( display_date > m_pSubpicture[i_index].i_stop &&
                ( !m_pSubpicture[i_index].b_ephemer ||
                  m_pSubpicture[i_index].i_stop >
                  m_pSubpicture[i_index].i_start ) &&
                !( m_pSubpicture[i_index].b_pausable &&
                   b_paused ) )
            {
                /* Too late, destroy the subpic */
                DestroySubpicture( &m_pSubpicture[i_index] );
                continue;
            }

            /* If this is an ephemer subpic, add it to our list */
            if( m_pSubpicture[i_index].b_ephemer )
            {
                m_pSubpicture[i_index].p_next = p_ephemer;
                p_ephemer = &m_pSubpicture[i_index];
                continue;
            }

            m_pSubpicture[i_index].p_next = p_subpic;
            p_subpic = &m_pSubpicture[i_index];
        }

        /* If we found ephemer subpictures, check if they have to be
         * displayed or destroyed */
        while( p_ephemer != NULL )
        {
            subpicture_t *p_tmp = p_ephemer;
            p_ephemer = p_ephemer->p_next;

            if( p_tmp->i_start < ephemer_date )
            {
                /* Ephemer subpicture has lived too long */
                DestroySubpicture( p_tmp );
            }
            else
            {
                /* Ephemer subpicture can still live a bit */
                p_tmp->p_next = p_subpic;
                p_subpic = p_tmp;
            }
        }
    }

    return p_subpic;
}

/*****************************************************************************
 * SpuClearChannel: clear an spu channel
 *****************************************************************************
 * This function destroys the subpictures which belong to the spu channel
 * corresponding to i_channel_id.
 *****************************************************************************/
void CSubPicture::ClearChannel( int i_channel )
{
    int          i_subpic;                               /* subpicture index */
    subpicture_t *p_subpic = NULL;                  /* first free subpicture */

    tme_mutex_lock( &m_lock );

    for( i_subpic = 0; i_subpic < VOUT_MAX_SUBPICTURES; i_subpic++ )
    {
        p_subpic = &m_pSubpicture[i_subpic];
        if( p_subpic->i_status == FREE_SUBPICTURE
            || ( p_subpic->i_status != RESERVED_SUBPICTURE
                 && p_subpic->i_status != READY_SUBPICTURE ) )
        {
            continue;
        }

        if( p_subpic->i_channel == i_channel )
        {
            while( p_subpic->p_region )
            {
                subpicture_region_t *p_region = p_subpic->p_region;
                p_subpic->p_region = p_region->p_next;
                DestroyRegion( p_region );
            }

            //if( p_subpic->pf_destroy ) p_subpic->pf_destroy( p_subpic );
            p_subpic->i_status = FREE_SUBPICTURE;
        }
    }

    tme_mutex_unlock( &m_lock );
}

int CSubPicture::Control(int i_query, int *val)
{
    switch( i_query )
    {
    case SPU_CHANNEL_REGISTER:
        if( val ) *val = m_iChannel++;
        _RPT1(_CRT_WARN, "Registering subpicture channel, ID: %i\n",
                 m_iChannel - 1 );
        break;

    case SPU_CHANNEL_CLEAR:
        ClearChannel( *val );
        break;

    default:
        _RPT0(_CRT_WARN, "control query not supported\n" );
        return EGENERIC;
    }

    return SUCCESS;
}

/**
 * Destroy a subpicture region
 *
 * \param p_region the subpicture region to destroy
 */
void DestroyRegion( subpicture_region_t *p_region )
{
    if( !p_region ) return;
	
    if( p_region->picture.pf_release )
        p_region->picture.pf_release( &p_region->picture );
    if( p_region->fmt.p_palette ) free( p_region->fmt.p_palette );
    if( p_region->p_cache ) DestroyRegion( p_region->p_cache );

	if (p_region->psz_text) {
		free(p_region->psz_text);
		p_region->psz_text = NULL;
	}
	
    free( p_region );
}

/**
 * Create a subpicture region
 *
 * \param p_this vlc_object_t
 * \param p_fmt the format that this subpicture region should have
 */
static void RegionPictureRelease( picture_t *p_pic )
{
    if( p_pic->p_data_orig ) free( p_pic->p_data_orig );
}

subpicture_region_t *spu_CreateRegion( video_format_t *p_fmt )
{
    subpicture_region_t *p_region = (subpicture_region_t *)malloc( sizeof(subpicture_region_t) );
    memset( p_region, 0, sizeof(subpicture_region_t) );
    p_region->p_next = 0;
    p_region->p_cache = 0;
    p_region->fmt = *p_fmt;
    p_region->psz_text = 0;
    p_region->p_style = NULL;

    if( p_fmt->i_chroma == TME_FOURCC('Y','U','V','P') )
        p_fmt->p_palette = p_region->fmt.p_palette =
            (video_palette_t *)malloc( sizeof(video_palette_t) );
    else p_fmt->p_palette = p_region->fmt.p_palette = NULL;

    p_region->picture.p_data_orig = NULL;

    if( p_fmt->i_chroma == TME_FOURCC('T','E','X','T') ) return p_region;

    vout_AllocatePicture( &p_region->picture, p_fmt->i_chroma,
                          p_fmt->i_width, p_fmt->i_height, p_fmt->i_aspect );

    if( !p_region->picture.i_planes )
    {
        free( p_region );
        free( p_fmt->p_palette );
        return NULL;
    }

    p_region->picture.pf_release = RegionPictureRelease;

    return p_region;
}
