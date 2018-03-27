#ifndef _VOUT_PICTURE_H_
#define _VOUT_PICTURE_H_

#include "tme_thread.h"
#include "video.h"
#include "video_output.h"

void vout_DisplayPicture( CVideoOutput *p_vout, picture_t *p_pic );
void vout_DatePicture( CVideoOutput *p_vout,
                       picture_t *p_pic, mtime_t date );
picture_t *vout_CreatePicture( CVideoOutput *p_vout,
                               bool b_progressive,
                               bool b_top_field_first,
                               unsigned int i_nb_fields );
void vout_DestroyPicture( CVideoOutput *p_vout, picture_t *p_pic );
void vout_LinkPicture( CVideoOutput *p_vout, picture_t *p_pic );
void vout_UnlinkPicture( CVideoOutput *p_vout, picture_t *p_pic );
int vout_InitPicture( picture_t *p_pic,
                        fourcc_t i_chroma,
                        int i_width, int i_height, int i_aspect );
int vout_AllocatePicture( picture_t *p_pic,
                            fourcc_t i_chroma,
                            int i_width, int i_height, int i_aspect );
void vout_PlacePicture( CVideoOutput *p_vout,
                        unsigned int i_width, unsigned int i_height,
                        unsigned int *pi_x, unsigned int *pi_y,
                        unsigned int *pi_width, unsigned int *pi_height );
int vout_ChromaCmp( fourcc_t i_chroma, fourcc_t i_amorhc );
picture_t * vout_RenderPicture( CVideoOutput *p_vout, picture_t *p_pic,
                                                       subpicture_t *p_subpic );
void vout_CopyPicture( CVideoOutput *p_this,
                         picture_t *p_dest, picture_t *p_src );

bool ureduce( unsigned *pi_dst_nom, unsigned *pi_dst_den,
                        UINT64 i_nom, UINT64 i_den, UINT64 i_max );
void video_format_Setup( video_format_t *p_fmt, fourcc_t i_chroma,
                         int i_width, int i_height,
                         int i_sar_num, int i_sar_den );
int picture_Setup( picture_t *p_picture, fourcc_t i_chroma,
                   int i_width, int i_height, int i_sar_num, int i_sar_den );


/*****************************************************************************
 * Fourcc definitions that we can handle internally
 *****************************************************************************/

/* Packed RGB for 8bpp */
#define FOURCC_BI_RGB       0x00000000
#define FOURCC_RGB2         TME_FOURCC('R','G','B','2')

/* Packed RGB for 16, 24, 32bpp */
#define FOURCC_BI_BITFIELDS 0x00000003

/* Packed RGB 15bpp, usually 0x7c00, 0x03e0, 0x001f */
#define FOURCC_RV15         TME_FOURCC('R','V','1','5')

/* Packed RGB 16bpp, usually 0xf800, 0x07e0, 0x001f */
#define FOURCC_RV16         TME_FOURCC('R','V','1','6')

/* Packed RGB 24bpp, usually 0x00ff0000, 0x0000ff00, 0x000000ff */
#define FOURCC_RV24         TME_FOURCC('R','V','2','4')

/* Packed RGB 32bpp, usually 0x00ff0000, 0x0000ff00, 0x000000ff */
#define FOURCC_RV32         TME_FOURCC('R','V','3','2')

/* Planar YUV 4:2:0, Y:U:V */
#define FOURCC_I420         TME_FOURCC('I','4','2','0')
#define FOURCC_IYUV         TME_FOURCC('I','Y','U','V')
#define FOURCC_J420         TME_FOURCC('J','4','2','0')

/* Planar YUV 4:2:0, Y:V:U */
#define FOURCC_YV12         TME_FOURCC('Y','V','1','2')

/* Packed YUV 4:2:2, U:Y:V:Y, interlaced */
#define FOURCC_IUYV         TME_FOURCC('I','U','Y','V')

/* Packed YUV 4:2:2, U:Y:V:Y */
#define FOURCC_UYVY         TME_FOURCC('U','Y','V','Y')
#define FOURCC_UYNV         TME_FOURCC('U','Y','N','V')
#define FOURCC_Y422         TME_FOURCC('Y','4','2','2')

/* Packed YUV 4:2:2, U:Y:V:Y, reverted */
#define FOURCC_cyuv         TME_FOURCC('c','y','u','v')

/* Packed YUV 4:2:2, Y:U:Y:V */
#define FOURCC_YUY2         TME_FOURCC('Y','U','Y','2')
#define FOURCC_YUNV         TME_FOURCC('Y','U','N','V')

/* Packed YUV 4:2:2, Y:V:Y:U */
#define FOURCC_YVYU         TME_FOURCC('Y','V','Y','U')

/* Packed YUV 2:1:1, Y:U:Y:V */
#define FOURCC_Y211         TME_FOURCC('Y','2','1','1')

/* Planar YUV 4:1:1, Y:U:V */
#define FOURCC_I411         TME_FOURCC('I','4','1','1')

/* Planar YUV 4:1:0, Y:U:V */
#define FOURCC_I410         TME_FOURCC('I','4','1','0')
#define FOURCC_YVU9         TME_FOURCC('Y','V','U','9')

/* Planar Y, packed UV, from Matrox */
#define FOURCC_YMGA         TME_FOURCC('Y','M','G','A')

/* Planar 4:2:2, Y:U:V */
#define FOURCC_I422         TME_FOURCC('I','4','2','2')
#define FOURCC_J422         TME_FOURCC('J','4','2','2')

/* Planar 4:4:4, Y:U:V */
#define FOURCC_I444         TME_FOURCC('I','4','4','4')
#define FOURCC_J444         TME_FOURCC('J','4','4','4')

/* Planar 4:4:4:4 Y:U:V:A */
#define FOURCC_YUVA         TME_FOURCC('Y','U','V','A')

/* Palettized YUV with palette element Y:U:V:A */
#define FOURCC_YUVP         TME_FOURCC('Y','U','V','P')


#endif