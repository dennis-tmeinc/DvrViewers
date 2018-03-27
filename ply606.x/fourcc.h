#ifndef _FOURCC_H_
#define _FOURCC_H_

#include "es.h"

/* Video codec */
#define CODEC_MPGV      TME_FOURCC('m','p','g','v')
#define CODEC_MP4V      TME_FOURCC('m','p','4','v')
#define CODEC_DIV1      TME_FOURCC('D','I','V','1')
#define CODEC_DIV2      TME_FOURCC('D','I','V','2')
#define CODEC_DIV3      TME_FOURCC('D','I','V','3')
#define CODEC_SVQ1      TME_FOURCC('S','V','Q','1')
#define CODEC_SVQ3      TME_FOURCC('S','V','Q','3')
#define CODEC_H264      TME_FOURCC('h','2','6','4')
#define CODEC_H263      TME_FOURCC('h','2','6','3')
#define CODEC_H263I     TME_FOURCC('I','2','6','3')
#define CODEC_H263P     TME_FOURCC('I','L','V','R')
#define CODEC_FLV1      TME_FOURCC('F','L','V','1')
#define CODEC_H261      TME_FOURCC('h','2','6','1')
#define CODEC_MJPG      TME_FOURCC('M','J','P','G')
#define CODEC_MJPGB     TME_FOURCC('m','j','p','b')
#define CODEC_LJPG      TME_FOURCC('L','J','P','G')
#define CODEC_WMV1      TME_FOURCC('W','M','V','1')
#define CODEC_WMV2      TME_FOURCC('W','M','V','2')
#define CODEC_WMV3      TME_FOURCC('W','M','V','3')
#define CODEC_WMVA      TME_FOURCC('W','M','V','A')
#define CODEC_VC1       TME_FOURCC('V','C','-','1')
#define CODEC_THEORA    TME_FOURCC('t','h','e','o')
#define CODEC_TARKIN    TME_FOURCC('t','a','r','k')
#define CODEC_SNOW      TME_FOURCC('S','N','O','W')
#define CODEC_DIRAC     TME_FOURCC('d','r','a','c')
#define CODEC_CAVS      TME_FOURCC('C','A','V','S')
#define CODEC_NUV       TME_FOURCC('N','J','P','G')
#define CODEC_RV10      TME_FOURCC('R','V','1','0')
#define CODEC_RV13      TME_FOURCC('R','V','1','3')
#define CODEC_RV20      TME_FOURCC('R','V','2','0')
#define CODEC_RV30      TME_FOURCC('R','V','3','0')
#define CODEC_RV40      TME_FOURCC('R','V','4','0')
#define CODEC_VP3       TME_FOURCC('V','P','3',' ')
#define CODEC_VP5       TME_FOURCC('V','P','5',' ')
#define CODEC_VP6       TME_FOURCC('V','P','6','2')
#define CODEC_VP6F      TME_FOURCC('V','P','6','F')
#define CODEC_VP6A      TME_FOURCC('V','P','6','A')
#define CODEC_MSVIDEO1  TME_FOURCC('M','S','V','C')
#define CODEC_FLIC      TME_FOURCC('F','L','I','C')
#define CODEC_SP5X      TME_FOURCC('S','P','5','X')
#define CODEC_DV        TME_FOURCC('d','v',' ',' ')
#define CODEC_MSRLE     TME_FOURCC('m','r','l','e')
#define CODEC_INDEO3    TME_FOURCC('I','V','3','1')
#define CODEC_HUFFYUV   TME_FOURCC('H','F','Y','U')
#define CODEC_FFVHUFF   TME_FOURCC('F','F','V','H')
#define CODEC_ASV1      TME_FOURCC('A','S','V','1')
#define CODEC_ASV2      TME_FOURCC('A','S','V','2')
#define CODEC_FFV1      TME_FOURCC('F','F','V','1')
#define CODEC_VCR1      TME_FOURCC('V','C','R','1')
#define CODEC_CLJR      TME_FOURCC('C','L','J','R')
#define CODEC_RPZA      TME_FOURCC('r','p','z','a')
#define CODEC_SMC       TME_FOURCC('s','m','c',' ')
#define CODEC_CINEPAK   TME_FOURCC('C','V','I','D')
#define CODEC_TSCC      TME_FOURCC('T','S','C','C')
#define CODEC_CSCD      TME_FOURCC('C','S','C','D')
#define CODEC_ZMBV      TME_FOURCC('Z','M','B','V')
#define CODEC_VMNC      TME_FOURCC('V','M','n','c')
#define CODEC_FRAPS     TME_FOURCC('F','P','S','1')
#define CODEC_TRUEMOTION1 TME_FOURCC('D','U','C','K')
#define CODEC_TRUEMOTION2 TME_FOURCC('T','M','2','0')
#define CODEC_QTRLE     TME_FOURCC('r','l','e',' ')
#define CODEC_QDRAW     TME_FOURCC('q','d','r','w')
#define CODEC_QPEG      TME_FOURCC('Q','P','E','G')
#define CODEC_ULTI      TME_FOURCC('U','L','T','I')
#define CODEC_VIXL      TME_FOURCC('V','I','X','L')
#define CODEC_LOCO      TME_FOURCC('L','O','C','O')
#define CODEC_WNV1      TME_FOURCC('W','N','V','1')
#define CODEC_AASC      TME_FOURCC('A','A','S','C')
#define CODEC_INDEO2    TME_FOURCC('I','V','2','0')
#define CODEC_FLASHSV   TME_FOURCC('F','S','V','1')
#define CODEC_KMVC      TME_FOURCC('K','M','V','C')
#define CODEC_SMACKVIDEO TME_FOURCC('S','M','K','2')
#define CODEC_DNXHD     TME_FOURCC('A','V','d','n')
#define CODEC_8BPS      TME_FOURCC('8','B','P','S')
#define CODEC_MIMIC     TME_FOURCC('M','L','2','O')
#define CODEC_INTERPLAY TME_FOURCC('i','m','v','e')
#define CODEC_IDCIN     TME_FOURCC('I','D','C','I')
#define CODEC_4XM       TME_FOURCC('4','X','M','V')
#define CODEC_ROQ       TME_FOURCC('R','o','Q','v')
#define CODEC_MDEC      TME_FOURCC('M','D','E','C')
#define CODEC_VMDVIDEO  TME_FOURCC('V','M','D','V')
#define CODEC_CDG       TME_FOURCC('C','D','G',' ')
#define CODEC_FRWU      TME_FOURCC('F','R','W','U')
#define CODEC_AMV       TME_FOURCC('A','M','V',' ')
#define CODEC_INDEO5    TME_FOURCC('I','V','5','0')
#define CODEC_VP8       TME_FOURCC('V','P','8','0')


/* Planar YUV 4:1:0 Y:V:U */
#define CODEC_YV9       TME_FOURCC('Y','V','U','9')
/* Planar YUV 4:2:0 Y:V:U */
#define CODEC_YV12      TME_FOURCC('Y','V','1','2')
/* Planar YUV 4:1:0 Y:U:V */
#define CODEC_I410      TME_FOURCC('I','4','1','0')
/* Planar YUV 4:1:1 Y:U:V */
#define CODEC_I411      TME_FOURCC('I','4','1','1')
/* Planar YUV 4:2:0 Y:U:V */
#define CODEC_I420      TME_FOURCC('I','4','2','0')
/* Planar YUV 4:2:2 Y:U:V */
#define CODEC_I422      TME_FOURCC('I','4','2','2')
/* Planar YUV 4:4:0 Y:U:V */
#define CODEC_I440      TME_FOURCC('I','4','4','0')
/* Planar YUV 4:4:4 Y:U:V */
#define CODEC_I444      TME_FOURCC('I','4','4','4')
/* Planar YUV 4:2:0 Y:U:V full scale */
#define CODEC_J420      TME_FOURCC('J','4','2','0')
/* Planar YUV 4:2:2 Y:U:V full scale */
#define CODEC_J422      TME_FOURCC('J','4','2','2')
/* Planar YUV 4:4:0 Y:U:V full scale */
#define CODEC_J440      TME_FOURCC('J','4','4','0')
/* Planar YUV 4:4:4 Y:U:V full scale */
#define CODEC_J444      TME_FOURCC('J','4','4','4')
/* Palettized YUV with palette element Y:U:V:A */
#define CODEC_YUVP      TME_FOURCC('Y','U','V','P')
/* Planar YUV 4:4:4 Y:U:V:A */
#define CODEC_YUVA      TME_FOURCC('Y','U','V','A')
/* Palettized RGB with palette element R:G:B */
#define CODEC_RGBP      TME_FOURCC('R','G','B','P')
/* 8 bits RGB */
#define CODEC_RGB8      TME_FOURCC('R','G','B','2')
/* 15 bits RGB stored on 16 bits */
#define CODEC_RGB15     TME_FOURCC('R','V','1','5')
/* 16 bits RGB store on a 16 bits */
#define CODEC_RGB16     TME_FOURCC('R','V','1','6')
/* 24 bits RGB */
#define CODEC_RGB24     TME_FOURCC('R','V','2','4')
/* 32 bits RGB */
#define CODEC_RGB32     TME_FOURCC('R','V','3','2')
/* 32 bits VLC RGBA */
#define CODEC_RGBA      TME_FOURCC('R','G','B','A')
/* 8 bits grey */
#define CODEC_GREY      TME_FOURCC('G','R','E','Y')
/* Packed YUV 4:2:2, U:Y:V:Y */
#define CODEC_UYVY      TME_FOURCC('U','Y','V','Y')
/* Packed YUV 4:2:2, V:Y:U:Y */
#define CODEC_VYUY      TME_FOURCC('V','Y','U','Y')
/* Packed YUV 4:2:2, Y:U:Y:V */
#define CODEC_YUYV      TME_FOURCC('Y','U','Y','2')
/* Packed YUV 4:2:2, Y:V:Y:U */
#define CODEC_YVYU      TME_FOURCC('Y','V','Y','U')
/* Packed YUV 2:1:1, Y:U:Y:V */
#define CODEC_Y211      TME_FOURCC('Y','2','1','1')
/* Packed YUV 4:2:2, U:Y:V:Y, reverted */
#define CODEC_CYUV      TME_FOURCC('c','y','u','v')
/* 10-bit 4:2:2 Component YCbCr */
#define CODEC_V210      TME_FOURCC('v','2','1','0')
/* Planar Y Packet UV (420) */
#define CODEC_NV12      TME_FOURCC('N','V','1','2')

/* Image codec (video) */
#define CODEC_PNG       TME_FOURCC('p','n','g',' ')
#define CODEC_PPM       TME_FOURCC('p','p','m',' ')
#define CODEC_PGM       TME_FOURCC('p','g','m',' ')
#define CODEC_PGMYUV    TME_FOURCC('p','g','m','y')
#define CODEC_PAM       TME_FOURCC('p','a','m',' ')
#define CODEC_JPEG      TME_FOURCC('j','p','e','g')
#define CODEC_JPEGLS    TME_FOURCC('M','J','L','S')
#define CODEC_BMP       TME_FOURCC('b','m','p',' ')
#define CODEC_TIFF      TME_FOURCC('t','i','f','f')
#define CODEC_GIF       TME_FOURCC('g','i','f',' ')
#define CODEC_TARGA     TME_FOURCC('t','g','a',' ')
#define CODEC_SGI       TME_FOURCC('s','g','i',' ')
#define CODEC_PNM       TME_FOURCC('p','n','m',' ')
#define CODEC_PCX       TME_FOURCC('p','c','x',' ')


/* Audio codec */
#define CODEC_MPGA      TME_FOURCC('m','p','g','a')
#define CODEC_MP4A      TME_FOURCC('m','p','4','a')
#define CODEC_ALS       TME_FOURCC('a','l','s',' ')
#define CODEC_A52       TME_FOURCC('a','5','2',' ')
#define CODEC_EAC3      TME_FOURCC('e','a','c','3')
#define CODEC_DTS       TME_FOURCC('d','t','s',' ')
#define CODEC_WMA1      TME_FOURCC('W','M','A','1')
#define CODEC_WMA2      TME_FOURCC('W','M','A','2')
#define CODEC_WMAP      TME_FOURCC('W','M','A','P')
#define CODEC_WMAL      TME_FOURCC('W','M','A','L')
#define CODEC_WMAS      TME_FOURCC('W','M','A','S')
#define CODEC_FLAC      TME_FOURCC('f','l','a','c')
#define CODEC_MLP       TME_FOURCC('m','l','p',' ')
#define CODEC_TRUEHD    TME_FOURCC('t','r','h','d')
#define CODEC_DVAUDIO   TME_FOURCC('d','v','a','u')
#define CODEC_SPEEX     TME_FOURCC('s','p','x',' ')
#define CODEC_VORBIS    TME_FOURCC('v','o','r','b')
#define CODEC_MACE3     TME_FOURCC('M','A','C','3')
#define CODEC_MACE6     TME_FOURCC('M','A','C','6')
#define CODEC_MUSEPACK7 TME_FOURCC('M','P','C',' ')
#define CODEC_MUSEPACK8 TME_FOURCC('M','P','C','K')
#define CODEC_RA_144    TME_FOURCC('1','4','_','4')
#define CODEC_RA_288    TME_FOURCC('2','8','_','8')
#define CODEC_ADPCM_4XM TME_FOURCC('4','x','m','a')
#define CODEC_ADPCM_EA  TME_FOURCC('A','D','E','A')
#define CODEC_INTERPLAY_DPCM TME_FOURCC('i','d','p','c')
#define CODEC_ROQ_DPCM  TME_FOURCC('R','o','Q','a')
#define CODEC_DSICINAUDIO TME_FOURCC('D','C','I','A')
#define CODEC_ADPCM_XA  TME_FOURCC('x','a',' ',' ')
#define CODEC_ADPCM_ADX TME_FOURCC('a','d','x',' ')
#define CODEC_ADPCM_IMA_WS TME_FOURCC('A','I','W','S')
#define CODEC_ADPCM_G726 TME_FOURCC('g','7','2','6')
#define CODEC_ADPCM_SWF TME_FOURCC('S','W','F','a')
#define CODEC_ADPCM_MS  TME_FOURCC('m','s',0x00,0x02)
#define CODEC_ADPCM_IMA_WAV TME_FOURCC('m','s',0x00,0x11)
#define CODEC_VMDAUDIO  TME_FOURCC('v','m','d','a')
#define CODEC_AMR_NB    TME_FOURCC('s','a','m','r')
#define CODEC_AMR_WB    TME_FOURCC('s','a','w','b')
#define CODEC_ALAC      TME_FOURCC('a','l','a','c')
#define CODEC_QDM2      TME_FOURCC('Q','D','M','2')
#define CODEC_COOK      TME_FOURCC('c','o','o','k')
#define CODEC_SIPR      TME_FOURCC('s','i','p','r')
#define CODEC_TTA       TME_FOURCC('T','T','A','1')
#define CODEC_SHORTEN   TME_FOURCC('s','h','n',' ')
#define CODEC_WAVPACK   TME_FOURCC('W','V','P','K')
#define CODEC_GSM       TME_FOURCC('g','s','m',' ')
#define CODEC_GSM_MS    TME_FOURCC('a','g','s','m')
#define CODEC_ATRAC1    TME_FOURCC('a','t','r','1')
#define CODEC_ATRAC3    TME_FOURCC('a','t','r','c')
#define CODEC_SONIC     TME_FOURCC('S','O','N','C')
#define CODEC_IMC       TME_FOURCC(0x1,0x4,0x0,0x0)
#define CODEC_TRUESPEECH TME_FOURCC(0x22,0x0,0x0,0x0)
#define CODEC_NELLYMOSER TME_FOURCC('N','E','L','L')
#define CODEC_APE       TME_FOURCC('A','P','E',' ')
#define CODEC_QCELP     TME_FOURCC('Q','c','l','p')
#define CODEC_302M      TME_FOURCC('3','0','2','m')
#define CODEC_DVD_LPCM  TME_FOURCC('l','p','c','m')
#define CODEC_DVDA_LPCM TME_FOURCC('a','p','c','m')
#define CODEC_BD_LPCM   TME_FOURCC('b','p','c','m')
#define CODEC_SDDS      TME_FOURCC('s','d','d','s')
#define CODEC_MIDI      TME_FOURCC('M','I','D','I')
#define CODEC_S8        TME_FOURCC('s','8',' ',' ')
#define CODEC_U8        TME_FOURCC('u','8',' ',' ')
#define CODEC_S16L      TME_FOURCC('s','1','6','l')
#define CODEC_S16B      TME_FOURCC('s','1','6','b')
#define CODEC_U16L      TME_FOURCC('u','1','6','l')
#define CODEC_U16B      TME_FOURCC('u','1','6','b')
#define CODEC_S24L      TME_FOURCC('s','2','4','l')
#define CODEC_S24B      TME_FOURCC('s','2','4','b')
#define CODEC_U24L      TME_FOURCC('u','2','4','l')
#define CODEC_U24B      TME_FOURCC('u','2','4','b')
#define CODEC_S32L      TME_FOURCC('s','3','2','l')
#define CODEC_S32B      TME_FOURCC('s','3','2','b')
#define CODEC_U32L      TME_FOURCC('u','3','2','l')
#define CODEC_U32B      TME_FOURCC('u','3','2','b')
#define CODEC_F32L      TME_FOURCC('f','3','2','l')
#define CODEC_F32B      TME_FOURCC('f','3','2','b')
#define CODEC_F64L      TME_FOURCC('f','6','4','l')
#define CODEC_F64B      TME_FOURCC('f','6','4','b')

#define CODEC_ALAW      TME_FOURCC('a','l','a','w')
#define CODEC_MULAW     TME_FOURCC('m','l','a','w')
#define CODEC_S24DAUD   TME_FOURCC('d','a','u','d')
#define CODEC_FI32      TME_FOURCC('f','i','3','2')
#define CODEC_TWINVQ    TME_FOURCC('T','W','I','N')
#define CODEC_ADPCM_IMA_AMV TME_FOURCC('i','m','a','v')

/* Subtitle */
#define CODEC_SPU       TME_FOURCC('s','p','u',' ')
#define CODEC_DVBS      TME_FOURCC('d','v','b','s')
#define CODEC_SUBT      TME_FOURCC('s','u','b','t')
#define CODEC_XSUB      TME_FOURCC('X','S','U','B')
#define CODEC_SSA       TME_FOURCC('s','s','a',' ')
#define CODEC_TEXT      TME_FOURCC('T','E','X','T')
#define CODEC_TELETEXT  TME_FOURCC('t','e','l','x')
#define CODEC_KATE      TME_FOURCC('k','a','t','e')
#define CODEC_CMML      TME_FOURCC('c','m','m','l')
#define CODEC_ITU_T140  TME_FOURCC('t','1','4','0')
#define CODEC_USF       TME_FOURCC('u','s','f',' ')
#define CODEC_OGT       TME_FOURCC('o','g','t',' ')
#define CODEC_CVD       TME_FOURCC('c','v','d',' ')
/* Blu-ray Presentation Graphics */
#define CODEC_BD_PG     TME_FOURCC('b','d','p','g')


/* Special endian dependant values
 * The suffic N means Native
 * The suffix I means Inverted (ie non native) */
#ifdef WORDS_BIGENDIAN
#   define CODEC_S16N CODEC_S16B
#   define CODEC_U16N CODEC_U16B
#   define CODEC_S24N CODEC_S24B
#   define CODEC_S32N CODEC_S32B
#   define CODEC_FL32 CODEC_F32B
#   define CODEC_FL64 CODEC_F64B

#   define CODEC_S16I CODEC_S16L
#   define CODEC_U16I CODEC_U16L
#   define CODEC_S24I CODEC_S24L
#   define CODEC_S32I CODEC_S32L
#else
#   define CODEC_S16N CODEC_S16L
#   define CODEC_U16N CODEC_U16L
#   define CODEC_S24N CODEC_S24L
#   define CODEC_S32N CODEC_S32L
#   define CODEC_FL32 CODEC_F32L
#   define CODEC_FL64 CODEC_F64L

#   define CODEC_S16I CODEC_S16B
#   define CODEC_U16I CODEC_U16B
#   define CODEC_S24I CODEC_S24B
#   define CODEC_S32I CODEC_S32B
#endif

/* Non official codecs, used to force a profile in an encoder */
/* MPEG-1 video */
#define CODEC_MP1V      TME_FOURCC('m','p','1','v')
/* MPEG-2 video */
#define CODEC_MP2V      TME_FOURCC('m','p','2','v')
/* MPEG-I/II layer 3 audio */
#define CODEC_MP3       TME_FOURCC('m','p','3',' ')

const fourcc_t *fourcc_GetYUVFallback( fourcc_t i_fourcc );
const fourcc_t *fourcc_GetRGBFallback( fourcc_t i_fourcc );
bool fourcc_AreUVPlanesSwapped( fourcc_t a, fourcc_t b );
bool fourcc_IsYUV(fourcc_t fcc);
fourcc_t fourcc_GetCodec( int i_cat, fourcc_t i_fourcc );
#endif