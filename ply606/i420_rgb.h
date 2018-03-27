#ifndef _I420_RGB_H_
#define _I420_RGB_H_
/*****************************************************************************
 * Constants
 *****************************************************************************/

/* Margins and offsets in conversion tables - Margins are used in case a RGB
 * RGB conversion would give a value outside the 0-255 range. Offsets have been
 * calculated to avoid using the same cache line for 2 tables. conversion tables
 * are 2*MARGIN + 256 long and stores pixels.*/
#define RED_MARGIN      178
#define GREEN_MARGIN    135
#define BLUE_MARGIN     224
#define RED_OFFSET      1501                                 /* 1323 to 1935 */
#define GREEN_OFFSET    135                                      /* 0 to 526 */
#define BLUE_OFFSET     818                                   /* 594 to 1298 */
#define RGB_TABLE_SIZE  1935                             /* total table size */

#define GRAY_MARGIN     384
#define GRAY_TABLE_SIZE 1024                             /* total table size */

#define PALETTE_TABLE_SIZE 2176          /* YUV -> 8bpp palette lookup table */

/* macros used for YUV pixel conversions */
#define SHIFT 20
#define U_GREEN_COEF    ((int)(-0.391 * (1<<SHIFT) / 1.164))
#define U_BLUE_COEF     ((int)(2.018 * (1<<SHIFT) / 1.164))
#define V_RED_COEF      ((int)(1.596 * (1<<SHIFT) / 1.164))
#define V_GREEN_COEF    ((int)(-0.813 * (1<<SHIFT) / 1.164))

int ActivateChromaRGB(CVideoOutput *pVout);

#endif