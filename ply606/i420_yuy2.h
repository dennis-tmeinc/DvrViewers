#ifndef _I420_YUY2_H_
#define _I420_YUY2_H_

#define MODULE_NAME_IS_i420_yuy2

#ifdef MODULE_NAME_IS_i420_yuy2_mmx

#define MMX_CALL(MMX_INSTRUCTIONS)                                          \
    do {                                                                    \
    __asm__ __volatile__(                                                   \
        ".p2align 3 \n\t"                                                   \
        MMX_INSTRUCTIONS                                                    \
        :                                                                   \
        : "r" (p_line1),  "r" (p_line2),  "r" (p_y1),  "r" (p_y2),          \
          "r" (p_u), "r" (p_v) );                                           \
    p_line1 += 16; p_line2 += 16; p_y1 += 8; p_y2 += 8; p_u += 4; p_v += 4; \
    } while(0);                                                             \

#define MMX_YUV420_YUYV "                                                 \n\
movq       (%2), %%mm0  # Load 8 Y            y7 y6 y5 y4 y3 y2 y1 y0     \n\
movd       (%4), %%mm1  # Load 4 Cb           00 00 00 00 u3 u2 u1 u0     \n\
movd       (%5), %%mm2  # Load 4 Cr           00 00 00 00 v3 v2 v1 v0     \n\
punpcklbw %%mm2, %%mm1  #                     v3 u3 v2 u2 v1 u1 v0 u0     \n\
movq      %%mm0, %%mm2  #                     y7 y6 y5 y4 y3 y2 y1 y0     \n\
punpcklbw %%mm1, %%mm2  #                     v1 y3 u1 y2 v0 y1 u0 y0     \n\
movq      %%mm2, (%0)   # Store low YUYV                                  \n\
punpckhbw %%mm1, %%mm0  #                     v3 y7 u3 y6 v2 y5 u2 y4     \n\
movq      %%mm0, 8(%0)  # Store high YUYV                                 \n\
movq       (%3), %%mm0  # Load 8 Y            Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0     \n\
movq      %%mm0, %%mm2  #                     Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0     \n\
punpcklbw %%mm1, %%mm2  #                     v1 Y3 u1 Y2 v0 Y1 u0 Y0     \n\
movq      %%mm2, (%1)   # Store low YUYV                                  \n\
punpckhbw %%mm1, %%mm0  #                     v3 Y7 u3 Y6 v2 Y5 u2 Y4     \n\
movq      %%mm0, 8(%1)  # Store high YUYV                                 \n\
"

#define MMX_YUV420_YVYU "                                                 \n\
movq       (%2), %%mm0  # Load 8 Y            y7 y6 y5 y4 y3 y2 y1 y0     \n\
movd       (%4), %%mm2  # Load 4 Cb           00 00 00 00 u3 u2 u1 u0     \n\
movd       (%5), %%mm1  # Load 4 Cr           00 00 00 00 v3 v2 v1 v0     \n\
punpcklbw %%mm2, %%mm1  #                     u3 v3 u2 v2 u1 v1 u0 v0     \n\
movq      %%mm0, %%mm2  #                     y7 y6 y5 y4 y3 y2 y1 y0     \n\
punpcklbw %%mm1, %%mm2  #                     u1 y3 v1 y2 u0 y1 v0 y0     \n\
movq      %%mm2, (%0)   # Store low YUYV                                  \n\
punpckhbw %%mm1, %%mm0  #                     u3 y7 v3 y6 u2 y5 v2 y4     \n\
movq      %%mm0, 8(%0)  # Store high YUYV                                 \n\
movq       (%3), %%mm0  # Load 8 Y            Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0     \n\
movq      %%mm0, %%mm2  #                     Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0     \n\
punpcklbw %%mm1, %%mm2  #                     u1 Y3 v1 Y2 u0 Y1 v0 Y0     \n\
movq      %%mm2, (%1)   # Store low YUYV                                  \n\
punpckhbw %%mm1, %%mm0  #                     u3 Y7 v3 Y6 u2 Y5 v2 Y4     \n\
movq      %%mm0, 8(%1)  # Store high YUYV                                 \n\
"

#define MMX_YUV420_UYVY "                                                 \n\
movq       (%2), %%mm0  # Load 8 Y            y7 y6 y5 y4 y3 y2 y1 y0     \n\
movq       (%3), %%mm3  # Load 8 Y            Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0     \n\
movd       (%4), %%mm1  # Load 4 Cb           00 00 00 00 u3 u2 u1 u0     \n\
movd       (%5), %%mm2  # Load 4 Cr           00 00 00 00 v3 v2 v1 v0     \n\
punpcklbw %%mm2, %%mm1  #                     v3 u3 v2 u2 v1 u1 v0 u0     \n\
movq      %%mm1, %%mm2  #                     v3 u3 v2 u2 v1 u1 v0 u0     \n\
punpcklbw %%mm0, %%mm2  #                     y3 v1 y2 u1 y1 v0 y0 u0     \n\
movq      %%mm2, (%0)   # Store low UYVY                                  \n\
movq      %%mm1, %%mm2  #                     u3 v3 u2 v2 u1 v1 u0 v0     \n\
punpckhbw %%mm0, %%mm2  #                     y3 v1 y2 u1 y1 v0 y0 u0     \n\
movq      %%mm2, 8(%0)  # Store high UYVY                                 \n\
movq      %%mm1, %%mm2  #                     u3 v3 u2 v2 u1 v1 u0 v0     \n\
punpcklbw %%mm3, %%mm2  #                     Y3 v1 Y2 u1 Y1 v0 Y0 u0     \n\
movq      %%mm2, (%1)   # Store low UYVY                                  \n\
punpckhbw %%mm3, %%mm1  #                     Y7 v3 Y6 u3 Y5 v2 Y4 u2     \n\
movq      %%mm1, 8(%1)  # Store high UYVY                                 \n\
"

/* FIXME: this code does not work ! Chroma seems to be wrong. */
#define MMX_YUV420_Y211 "                                                 \n\
movq       (%2), %%mm0  # Load 8 Y            y7 y6 y5 y4 y3 y2 y1 y0     \n\
movq       (%3), %%mm1  # Load 8 Y            Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0     \n\
movd       (%4), %%mm2  # Load 4 Cb           00 00 00 00 u3 u2 u1 u0     \n\
movd       (%5), %%mm3  # Load 4 Cr           00 00 00 00 v3 v2 v1 v0     \n\
pand    i_00ffw, %%mm0  # get Y even          00 Y6 00 Y4 00 Y2 00 Y0     \n\
packuswb  %%mm0, %%mm0  # pack Y              y6 y4 y2 y0 y6 y4 y2 y0     \n\
pand    i_00ffw, %%mm2  # get U even          00 u6 00 u4 00 u2 00 u0     \n\
packuswb  %%mm2, %%mm2  # pack U              00 00 u2 u0 00 00 u2 u0     \n\
pand    i_00ffw, %%mm3  # get V even          00 v6 00 v4 00 v2 00 v0     \n\
packuswb  %%mm3, %%mm3  # pack V              00 00 v2 v0 00 00 v2 v0     \n\
punpcklbw %%mm3, %%mm2  #                     00 00 00 00 v2 u2 v0 u0     \n\
psubsw    i_80w, %%mm2  # U,V -= 128                                      \n\
punpcklbw %%mm2, %%mm0  #                     v2 y6 u2 y4 v0 y2 u0 y0     \n\
movq      %%mm0, (%0)   # Store YUYV                                      \n\
pand    i_00ffw, %%mm1  # get Y even          00 Y6 00 Y4 00 Y2 00 Y0     \n\
packuswb  %%mm1, %%mm1  # pack Y              Y6 Y4 Y2 Y0 Y6 Y4 Y2 Y0     \n\
punpcklbw %%mm2, %%mm1  #                     v2 Y6 u2 Y4 v0 Y2 u0 Y0     \n\
movq      %%mm1, (%1)   # Store YUYV                                      \n\
"

#else

#define C_YUV420_YVYU( )                                                    \
    *(p_line1)++ = *(p_y1)++; *(p_line2)++ = *(p_y2)++;                     \
    *(p_line1)++ =            *(p_line2)++ = *(p_v)++;                      \
    *(p_line1)++ = *(p_y1)++; *(p_line2)++ = *(p_y2)++;                     \
    *(p_line1)++ =            *(p_line2)++ = *(p_u)++;                      \

#define C_YUV420_Y211( )                                                    \
    *(p_line1)++ = *(p_y1); p_y1 += 2;                                      \
    *(p_line2)++ = *(p_y2); p_y2 += 2;                                      \
    *(p_line1)++ = *(p_line2)++ = *(p_u) - 0x80; p_u += 2;                  \
    *(p_line1)++ = *(p_y1); p_y1 += 2;                                      \
    *(p_line2)++ = *(p_y2); p_y2 += 2;                                      \
    *(p_line1)++ = *(p_line2)++ = *(p_v) - 0x80; p_v += 2;                  \

#endif

/* Used in both MMX and C modules */
#define C_YUV420_YUYV( )                                                    \
    *(p_line1)++ = *(p_y1)++; *(p_line2)++ = *(p_y2)++;                     \
    *(p_line1)++ =            *(p_line2)++ = *(p_u)++;                      \
    *(p_line1)++ = *(p_y1)++; *(p_line2)++ = *(p_y2)++;                     \
    *(p_line1)++ =            *(p_line2)++ = *(p_v)++;                      \

#define C_YUV420_UYVY( )                                                    \
    *(p_line1)++ =            *(p_line2)++ = *(p_u)++;                      \
    *(p_line1)++ = *(p_y1)++; *(p_line2)++ = *(p_y2)++;                     \
    *(p_line1)++ =            *(p_line2)++ = *(p_v)++;                      \
    *(p_line1)++ = *(p_y1)++; *(p_line2)++ = *(p_y2)++;                     \

class CVideoOutput;
int ActivateChromaYUY2( CVideoOutput *p_vout );
#endif