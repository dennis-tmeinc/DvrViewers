#ifndef _VFILTER_BLEND_H_
#define _VFILTER_BLEND_H_

#include "filter.h"
#include "video.h"

int OpenFilterBlender( filter_t *p_filter );
void VideoBlend( filter_t *p_filter, picture_t *p_dst,
                   picture_t *p_dst_orig, picture_t *p_src,
                   int i_x_offset, int i_y_offset, int i_alpha );

#endif
