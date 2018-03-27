#ifndef _SUBPICTURE_H_
#define _SUBPICTURE_H_

#include "tme_thread.h"
#include "video.h"
#include "filter.h"

subpicture_region_t *spu_CreateRegion( video_format_t *p_fmt );
void DestroyRegion( subpicture_region_t *p_region );

class CFreetype;
class CSubPicture
{
public:
	CSubPicture(tme_object_t *p_this);
	~CSubPicture();
	int Init();
	void DisplaySubpicture( subpicture_t *p_subpic );
	subpicture_t *CreateSubpicture();
	void DestroySubpicture( subpicture_t *p_subpic );
	void ClearChannel( int i_channel );
	int Control(int i_query, int *val);
	void RenderSubpictures( video_format_t *p_fmt,
                            picture_t *p_pic_dst, picture_t *p_pic_src,
                            subpicture_t *p_subpic,
                            int i_scale_width_orig, int i_scale_height_orig );
	subpicture_t *SortSubpictures( mtime_t display_date,
                                   bool b_paused );

private:
    tme_mutex_t  m_lock;                  /**< subpicture heap lock */
    subpicture_t m_pSubpicture[VOUT_MAX_SUBPICTURES];        /**< subpictures */
    int m_iChannel;             /**< number of subpicture channels registered */
    int m_iMargin;                        /**< force position of a subpicture */
    filter_t *m_pBlend;                            /**< alpha blending module */
    CFreetype *m_pText;                              /**< text renderer module */
    filter_t *m_pScale;                                   /**< scaling module */
    bool m_bForcePalette;             /**< force palette of subpicture */
    UINT8 m_palette[4][4];                               /**< forced palette */
    bool m_bForceCrop;               /**< force cropping of subpicture */
    int m_iCrop_x, m_iCrop_y, m_iCrop_width, m_iCrop_height;       /**< cropping */

};

#endif
