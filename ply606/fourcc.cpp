#include "stdafx.h"
#include <assert.h>
#include "fourcc.h"

typedef struct
{
    char p_class[4];
    char p_fourcc[4];
    const char *psz_description;
} entry_t;


/* XXX You don't want to see the preprocessor generated code ;) */
#ifdef WORDS_BIGENDIAN
#   define FCC2STR(f) { ((f)>>24)&0xff, ((f)>>16)&0xff, ((f)>>8)&0xff, ((f)>>0)&0xff }
#else
#   define FCC2STR(f) { ((f)>>0)&0xff, ((f)>>8)&0xff, ((f)>>16)&0xff, ((f)>>24)&0xff }
#endif

#define USE4B(f) { f[0], f[1], f[2], f[3] }

#define NULL4 {'\0', '\0', '\0', '\0'}

/* Begin a new class */
//#define B(a, c) { .p_class = FCC2STR(a), .p_fourcc = FCC2STR(a), .psz_description = c }
#define B(a, c) { FCC2STR(a), FCC2STR(a), c }
/* Create a sub-class entry with description */
//#define E(b, c) { .p_class = NULL4, .p_fourcc = b, .psz_description = c }
#define E(b, c) { NULL4, USE4B(b), c }
/* Create a sub-class entry without description (alias) */
//#define A(b) E(b, NULL4)
#define A(b) E(b, "")

/* */
static const entry_t p_list_video[] = {

    B(CODEC_MPGV, "MPEG-1/2 Video"),
        A("mpgv"),
        A("mp1v"),
        A("mpeg"),
        A("mpg1"),
        A("mp2v"),
        A("MPEG"),
        A("mpg2"),
        A("MPG2"),

        E("PIM1", "Pinnacle DC1000 (MPEG-1 Video)"),

        E("hdv1", "HDV 720p30 (MPEG-2 Video)"),
        E("hdv2", "Sony HDV (MPEG-2 Video)"),
        E("hdv3", "FCP HDV (MPEG-2 Video)"),
        E("hdv5", "HDV 720p25 (MPEG-2 Video)"),
        E("hdv6", "HDV 1080p24 (MPEG-2 Video)"),
        E("hdv7", "HDV 1080p25 (MPEG-2 Video)"),
        E("hdv8", "HDV 1080p30 (MPEG-2 Video)"),

        E("mx5n", "MPEG2 IMX NTSC 525/60 50mb/s (FCP)"),
        E("mx5p", "MPEG2 IMX PAL 625/60 50mb/s (FCP)"),
        E("mx4n", "MPEG2 IMX NTSC 525/60 40mb/s (FCP)"),
        E("mx4p", "MPEG2 IMX PAL 625/50 40mb/s (FCP)"),
        E("mx3n", "MPEG2 IMX NTSC 525/60 30mb/s (FCP)"),
        E("mx3p", "MPEG2 IMX NTSC 625/50 30mb/s (FCP)"),

        E("xdv1", "XDCAM HD"),
        E("xdv2", "XDCAM HD 1080i60 35mb/s"),
        E("xdv3", "XDCAM HD 1080i50 35mb/s"),
        E("xdv4", "XDCAM HD"),
        E("xdv5", "XDCAM HD"),
        E("xdv6", "XDCAM HD 1080p24 35mb/s"),
        E("xdv7", "XDCAM HD 1080p25 35mb/s"),
        E("xdv8", "XDCAM HD 1080p30 35mb/s"),
        E("xdv9", "XDCAM HD"),

        E("xdva", "XDCAM"),
        E("xdvb", "XDCAM"),
        E("xdvc", "XDCAM"),
        E("xdvd", "XDCAM"),
        E("xdve", "XDCAM"),
        E("xdvf", "XDCAM"),

        E("xd5a", "XDCAM"),
        E("xd5b", "XDCAM"),
        E("xd5c", "XDCAM"),
        E("xd5d", "XDCAM"),
        E("xd5e", "XDCAM"),
        E("xd5f", "XDCAM"),
        E("xd59", "XDCAM"),

        E("AVmp", "AVID IMX PAL"),
        E("MMES", "Matrox MPEG-2"),
        E("mmes", "Matrox MPEG-2"),
        E("PIM2", "Pinnacle MPEG-2"),
        E("LMP2", "Lead MPEG-2"),

        E("VCR2", "ATI VCR-2"),

    B(CODEC_MP4V, "MPEG-4 Video"),
        A("mp4v"),
        A("DIVX"),
        A("divx"),
        A("MP4S"),
        A("mp4s"),
        A("M4S2"),
        A("m4s2"),
        A("MP4V"),
        A("\x04\x00\x00\x00"),
        A("m4cc"),
        A("M4CC"),
        A("FMP4"),
        A("fmp4"),
        A("DCOD"),
        A("MVXM"),
        A("PM4V"),
        A("M4T3"),
        A("GEOX"),
        A("GEOV"),
        A("DMK2"),
        A("WV1F"),
        A("DIGI"),
        A("INMC"),
        A("SN40"),
        A("EPHV"),
        /* XVID flavours */
        E("xvid", "Xvid MPEG-4 Video"),
        E("XVID", "Xvid MPEG-4 Video"),
        E("XviD", "Xvid MPEG-4 Video"),
        E("XVIX", "Xvid MPEG-4 Video"),
        E("xvix", "Xvid MPEG-4 Video"),
        /* DX50 */
        E("DX50", "DivX MPEG-4 Video"),
        E("dx50", "DivX MPEG-4 Video"),
        E("BLZ0", "Blizzard MPEG-4 Video"),
        E("DXGM", "Electronic Arts Game MPEG-4 Video"),
        /* 3ivx delta 4 */
        E("3IV2", "3ivx MPEG-4 Video"),
        E("3iv2", "3ivx MPEG-4 Video"),
        /* Various */
        E("UMP4", "UB MPEG-4 Video"),
        E("SEDG", "Samsung MPEG-4 Video"),
        E("RMP4", "REALmagic MPEG-4 Video"),
        E("HDX4", "Jomigo HDX4 (MPEG-4 Video)"),
        E("hdx4", "Jomigo HDX4 (MPEG-4 Video)"),
        E("SMP4", "Samsung SMP4 (MPEG-4 Video)"),
        E("smp4", "Samsung SMP4 (MPEG-4 Video)"),
        E("fvfw", "FFmpeg MPEG-4"),
        E("FVFW", "FFmpeg MPEG-4"),
        E("FFDS", "FFDShow MPEG-4"),
        E("VIDM", "vidm 4.01 codec"),
        /* 3ivx delta 3.5 Unsupported
         * putting it here gives extreme distorted images */
        //E("3IV1", "3ivx delta 3.5 MPEG-4 Video"),
        //E("3iv1", "3ivx delta 3.5 MPEG-4 Video"),

    /* MSMPEG4 v1 */
    B(CODEC_DIV1, "MS MPEG-4 Video v1"),
        A("DIV1"),
        A("div1"),
        A("MPG4"),
        A("mpg4"),
        A("mp41"),

    /* MSMPEG4 v2 */
    B(CODEC_DIV2, "MS MPEG-4 Video v2"),
        A("DIV2"),
        A("div2"),
        A("MP42"),
        A("mp42"),

    /* MSMPEG4 v3 / M$ mpeg4 v3 */
    B(CODEC_DIV3, "MS MPEG-4 Video v3"),
        A("DIV3"),
        A("MPG3"),
        A("mpg3"),
        A("div3"),
        A("MP43"),
        A("mp43"),
        /* DivX 3.20 */
        A("DIV4"),
        A("div4"),
        A("DIV5"),
        A("div5"),
        A("DIV6"),
        A("div6"),
        E("divf", "DivX 4.12"),
        E("DIVF", "DivX 4.12"),
        /* Cool Codec */
        A("COL1"),
        A("col1"),
        A("COL0"),
        A("col0"),
        /* AngelPotion stuff */
        A("AP41"),
        /* 3ivx doctered divx files */
        A("3IVD"),
        A("3ivd"),
        /* who knows? */
        A("3VID"),
        A("3vid"),
        A("DVX3"),

    /* Sorenson v1 */
    B(CODEC_SVQ1, "SVQ-1 (Sorenson Video v1)"),
        A("SVQ1"),
        A("svq1"),
        A("svqi"),

    /* Sorenson v3 */
    B(CODEC_SVQ3, "SVQ-3 (Sorenson Video v3)"),
        A("SVQ3"),

    /* h264 */
    B(CODEC_H264, "H264 - MPEG-4 AVC (part 10)"),
        A("H264"),
        A("h264"),
        A("x264"),
        A("X264"),
        /* avc1: special case h264 */
        A("avc1"),
        A("AVC1"),
        E("VSSH", "Vanguard VSS H264"),
        E("VSSW", "Vanguard VSS H264"),
        E("vssh", "Vanguard VSS H264"),
        E("DAVC", "Dicas MPEGable H.264/MPEG-4 AVC"),
        E("davc", "Dicas MPEGable H.264/MPEG-4 AVC"),

    /* H263 and H263i */
    /* H263(+) is also known as Real Video 1.0 */

    /* H263 */
    B(CODEC_H263, "H263"),
        A("H263"),
        A("h263"),
        A("VX1K"),
        A("s263"),
        A("S263"),
        A("U263"),
        A("u263"),
        E("D263", "DEC H263"),
        E("L263", "LEAD H263"),
        E("M263", "Microsoft H263"),
        E("X263", "Xirlink H263"),
        /* Zygo (partial) */
        E("ZyGo", "ITU H263+"),

    /* H263i */
    B(CODEC_H263I, "I263.I"),
        A("I263"),
        A("i263"),

    /* H263P */
    B(CODEC_H263P, "ITU H263+"),
        E("ILVR", "ITU H263+"),
        E("viv1", "H263+"),
        E("vivO", "H263+"),
        E("viv2", "H263+"),
        E("U263", "UB H263+"),

    /* Flash (H263) variant */
    B(CODEC_FLV1, "Flash Video"),
        A("FLV1"),
        A("flv "),

    /* H261 */
    B(CODEC_H261, "H.261"),
        A("H261"),
        A("h261"),

    B(CODEC_FLIC, "Flic Video"),
        A("FLIC"),

    /* MJPEG */
    B(CODEC_MJPG, "Motion JPEG Video"),
        A("MJPG"),
        A("mjpg"),
        A("mjpa"),
        A("jpeg"),
        A("JPEG"),
        A("JFIF"),
        A("JPGL"),
        A("AVDJ"),
        A("MMJP"),
        A("QIVG"),
        /* AVID MJPEG */
        E("AVRn", "Avid Motion JPEG"),
        E("AVDJ", "Avid Motion JPEG"),
        E("ADJV", "Avid Motion JPEG"),
        E("dmb1", "Motion JPEG OpenDML Video"),
        E("ijpg", "Intergraph JPEG Video"),
        E("IJPG", "Intergraph JPEG Video"),
        E("ACDV", "ACD Systems Digital"),
        E("SLMJ", "SL M-JPEG"),

    B(CODEC_MJPGB, "Motion JPEG B Video"),
        A("mjpb"),

    B(CODEC_LJPG, "Lead Motion JPEG Video"),
        A("LJPG"),

    // ? from avcodec/fourcc.c but makes not sense.
    //{ TME_FOURCC( 'L','J','P','G' ), CODEC_ID_MJPEG,       VIDEO_ES, "Lead Motion JPEG Video" },

    /* SP5x */
    B(CODEC_SP5X, "Sunplus Motion JPEG Video"),
        A("SP5X"),
        A("SP53"),
        A("SP54"),
        A("SP55"),
        A("SP56"),
        A("SP57"),
        A("SP58"),

    /* DV */
    B(CODEC_DV, "DV Video"),
        A("dv  "),
        A("dvsl"),
        A("DVSD"),
        A("dvsd"),
        A("DVCS"),
        A("dvcs"),
        A("dvhd"),
        A("dvhp"),
        A("dvhq"),
        A("dvh3"),
        A("dvh5"),
        A("dvh6"),
        A("dv1n"),
        A("dv1p"),
        A("dvc "),
        A("dv25"),
        A("dvh1"),
        A("dvs1"),
        E("dvcp", "DV Video PAL"),
        E("dvp ", "DV Video Pro"),
        E("dvpp", "DV Video Pro PAL"),
        E("dv50", "DV Video C Pro 50"),
        E("dv5p", "DV Video C Pro 50 PAL"),
        E("dv5n", "DV Video C Pro 50 NTSC"),
        E("AVdv", "AVID DV"),
        E("AVd1", "AVID DV"),
        E("CDVC", "Canopus DV Video"),
        E("cdvc", "Canopus DV Video"),
        E("CDVH", "Canopus DV Video"),
        E("cdvh", "Canopus DV Video"),

    /* Windows Media Video */
    B(CODEC_WMV1, "Windows Media Video 7"),
        A("WMV1"),
        A("wmv1"),

    B(CODEC_WMV2, "Windows Media Video 8"),
        A("WMV2"),
        A("wmv2"),

    B(CODEC_WMV3, "Windows Media Video 9"),
        A("WMV3"),
        A("wmv3"),

    /* WMVA is the VC-1 codec before the standardization proces,
     * it is not bitstream compatible and deprecated  */
    B(CODEC_WMVA, "Windows Media Video Advanced Profile"),
        A("WMVA"),
        A("wmva"),
        A("WVP2"),
        A("wvp2"),

    B(CODEC_VC1, "Windows Media Video VC1"),
        A("WVC1"),
        A("wvc1"),
        A("vc-1"),
        A("VC-1"),

    /* Microsoft Video 1 */
    B(CODEC_MSVIDEO1, "Microsoft Video 1"),
        A("MSVC"),
        A("msvc"),
        A("CRAM"),
        A("cram"),
        A("WHAM"),
        A("wham"),

    /* Microsoft RLE */
    B(CODEC_MSRLE, "Microsoft RLE Video"),
        A("mrle"),
        A("WRLE"),
        A("\x01\x00\x00\x00"),
        A("\x02\x00\x00\x00"),

    /* Indeo Video Codecs (Quality of this decoder on ppc is not good) */
    B(CODEC_INDEO3, "Indeo Video v3"),
        A("IV31"),
        A("iv31"),
        A("IV32"),
        A("iv32"),

    /* Huff YUV */
    B(CODEC_HUFFYUV, "Huff YUV Video"),
        A("HFYU"),

    B(CODEC_FFVHUFF, "Huff YUV Video"),
        A("FFVH"),

    /* On2 VP3 Video Codecs */
    B(CODEC_VP3, "On2's VP3 Video"),
        A("VP3 "),
        A("VP30"),
        A("vp30"),
        A("VP31"),
        A("vp31"),

    /* On2  VP5, VP6 codecs */
    B(CODEC_VP5, "On2's VP5 Video"),
        A("VP5 "),
        A("VP50"),

    B(CODEC_VP6, "On2's VP6.2 Video"),
        A("VP62"),
        A("vp62"),
        E("VP60", "On2's VP6.0 Video"),
        E("VP61", "On2's VP6.1 Video"),

    B(CODEC_VP6F, "On2's VP6.2 Video (Flash)"),
        A("VP6F"),
        A("FLV4"),

    B(CODEC_VP6A, "On2's VP6 A Video"),
        A("VP6A"),

    B(CODEC_VP8, "Google/On2's VP8 Video"),
        A("VP80"),


    /* Xiph.org theora */
    B(CODEC_THEORA, "Xiph.org's Theora Video"),
        A("theo"),
        A("Thra"),

    /* Xiph.org tarkin */
    B(CODEC_TARKIN, "Xiph.org's Tarkin Video"),
        A("tark"),

    /* Asus Video (Another thing that doesn't work on PPC) */
    B(CODEC_ASV1, "Asus V1 Video"),
        A("ASV1"),
    B(CODEC_ASV2, "Asus V2 Video"),
        A("ASV2"),

    /* FFMPEG Video 1 (lossless codec) */
    B(CODEC_FFV1, "FFMpeg Video 1"),
        A("FFV1"),

    /* ATI VCR1 */
    B(CODEC_VCR1, "ATI VCR1 Video"),
        A("VCR1"),

    /* Cirrus Logic AccuPak */
    B(CODEC_CLJR, "Creative Logic AccuPak"),
        A("CLJR"),

    /* Real Video */
    B(CODEC_RV10, "Real Video 1.0"),
        A("RV10"),
        A("rv10"),

    B(CODEC_RV13, "Real Video 1.3"),
        A("RV13"),
        A("rv13"),

    B(CODEC_RV20, "Real Video 2.0"),
        A("RV20"),
        A("rv20"),

    B(CODEC_RV30, "Real Video 3.0"),
        A("RV30"),
        A("rv30"),

    B(CODEC_RV40, "Real Video 4.0"),
        A("RV40"),
        A("rv40"),

    /* Apple Video */
    B(CODEC_RPZA, "Apple Video"),
        A("rpza"),
        A("azpr"),
        A("RPZA"),
        A("AZPR"),

    B(CODEC_SMC, "Apple graphics"),
        A("smc "),

    B(CODEC_CINEPAK, "Cinepak Video"),
        A("CVID"),
        A("cvid"),

    /* Screen Capture Video Codecs */
    B(CODEC_TSCC, "TechSmith Camtasia Screen Capture"),
        A("TSCC"),
        A("tscc"),

    B(CODEC_CSCD, "CamStudio Screen Codec"),
        A("CSCD"),
        A("cscd"),

    B(CODEC_ZMBV, "DosBox Capture Codec"),
        A("ZMBV"),

    B(CODEC_VMNC, "VMware Video"),
        A("VMnc"),
    B(CODEC_FRAPS, "FRAPS: Realtime Video Capture"),
        A("FPS1"),
        A("fps1"),

    /* Duck TrueMotion */
    B(CODEC_TRUEMOTION1, "Duck TrueMotion v1 Video"),
        A("DUCK"),
        A("PVEZ"),
    B(CODEC_TRUEMOTION2, "Duck TrueMotion v2.0 Video"),
        A("TM20"),

    /* FFMPEG's SNOW wavelet codec */
    B(CODEC_SNOW, "FFMpeg SNOW wavelet Video"),
        A("SNOW"),
        A("snow"),

    B(CODEC_QTRLE, "Apple QuickTime RLE Video"),
        A("rle "),

    B(CODEC_QDRAW, "Apple QuickDraw Video"),
        A("qdrw"),

    B(CODEC_QPEG, "QPEG Video"),
        A("QPEG"),
        A("Q1.0"),
        A("Q1.1"),

    B(CODEC_ULTI, "IBM Ultimotion Video"),
        A("ULTI"),

    B(CODEC_VIXL, "Miro/Pinnacle VideoXL Video"),
        A("VIXL"),
        A("XIXL"),
        E("PIXL", "Pinnacle VideoXL Video"),

    B(CODEC_LOCO, "LOCO Video"),
        A("LOCO"),

    B(CODEC_WNV1, "Winnov WNV1 Video"),
        A("WNV1"),

    B(CODEC_AASC, "Autodesc RLE Video"),
        A("AASC"),

    B(CODEC_INDEO2, "Indeo Video v2"),
        A("IV20"),
        A("RT21"),

        /* Flash Screen Video */
    B(CODEC_FLASHSV, "Flash Screen Video"),
        A("FSV1"),
    B(CODEC_KMVC, "Karl Morton's Video Codec (Worms)"),
        A("KMVC"),

    B(CODEC_NUV, "Nuppel Video"),
        A("RJPG"),
        A("NUV1"),

    /* CODEC_ID_SMACKVIDEO */
    B(CODEC_SMACKVIDEO, "Smacker Video"),
        A("SMK2"),
        A("SMK4"),

    /* Chinese AVS - Untested */
    B(CODEC_CAVS, "Chinese AVS"),
        A("CAVS"),
        A("AVs2"),
        A("avs2"),

    B(CODEC_AMV, "AMV"),

    /* */
    B(CODEC_DNXHD, "DNxHD"),
        A("AVdn"),
    B(CODEC_8BPS, "8BPS"),
        A("8BPS"),
    B(CODEC_MIMIC, "Mimic"),
        A("ML2O"),

    B(CODEC_CDG, "CD-G Video"),
        A("CDG "),

    B(CODEC_FRWU, "Forward Uncompressed" ),
        A("FRWU"),

    B(CODEC_INDEO5, "Indeo Video v5"),
        A("IV50"),
        A("iv50"),


    /* */
    B(CODEC_YV12, "Planar 4:2:0 YVU"),
        A("YV12"),
        A("yv12"),
    B(CODEC_YV9,  "Planar 4:1:0 YVU"),
        A("YVU9"),
    B(CODEC_I410, "Planar 4:1:0 YUV"),
        A("I410"),
    B(CODEC_I411, "Planar 4:1:1 YUV"),
        A("I411"),
    B(CODEC_I420, "Planar 4:2:0 YUV"),
        A("I420"),
        A("IYUV"),
    B(CODEC_I422, "Planar 4:2:2 YUV"),
        A("I422"),
    B(CODEC_I444, "Planar 4:4:0 YUV"),
        A("I440"),
    B(CODEC_I444, "Planar 4:4:4 YUV"),
        A("I444"),

    B(CODEC_J420, "Planar 4:2:0 YUV full scale"),
        A("J420"),
    B(CODEC_J422, "Planar 4:2:2 YUV full scale"),
        A("J422"),
    B(CODEC_J440, "Planar 4:4:0 YUV full scale"),
        A("J440"),
    B(CODEC_J444, "Planar 4:4:4 YUV full scale"),
        A("J444"),

    B(CODEC_YUVP, "Palettized YUV with palette element Y:U:V:A"),
        A("YUVP"),

    B(CODEC_YUVA, "Planar YUV 4:4:4 Y:U:V:A"),
        A("YUVA"),

    B(CODEC_RGBP, "Palettized RGB with palette element R:G:B"),
        A("RGBP"),

    B(CODEC_RGB8, "8 bits RGB"),
        A("RGB2"),
    B(CODEC_RGB15, "15 bits RGB"),
        A("RV15"),
    B(CODEC_RGB16, "16 bits RGB"),
        A("RV16"),
    B(CODEC_RGB24, "24 bits RGB"),
        A("RV24"),
    B(CODEC_RGB32, "32 bits RGB"),
        A("RV32"),
    B(CODEC_RGBA, "32 bits RGBA"),
        A("RGBA"),

    B(CODEC_GREY, "8 bits greyscale"),
        A("GREY"),
        A("Y800"),
        A("Y8  "),

    B(CODEC_UYVY, "Packed YUV 4:2:2, U:Y:V:Y"),
        A("UYVY"),
        A("UYNV"),
        A("UYNY"),
        A("Y422"),
        A("HDYC"),
        A("AVUI"),
        A("uyv1"),
        A("2vuy"),
        A("2Vuy"),
        A("2Vu1"),
    B(CODEC_VYUY, "Packed YUV 4:2:2, V:Y:U:Y"),
        A("VYUY"),
    B(CODEC_YUYV, "Packed YUV 4:2:2, Y:U:Y:V"),
        A("YUY2"),
        A("YUYV"),
        A("YUNV"),
        A("V422"),
    B(CODEC_YVYU, "Packed YUV 4:2:2, Y:V:Y:U"),
        A("YVYU"),

    B(CODEC_Y211, "Packed YUV 2:1:1, Y:U:Y:V "),
        A("Y211"),
    B(CODEC_CYUV, "Creative Packed YUV 4:2:2, U:Y:V:Y, reverted"),
        A("cyuv"),
        A("CYUV"),

    B(CODEC_V210, "10-bit 4:2:2 Component YCbCr"),
        A("v210"),

    B(CODEC_NV12, "Planar  Y, Packet UV (420)"),
        A("NV12"),

    /* Videogames Codecs */

    /* Interplay MVE */
    B(CODEC_INTERPLAY, "Interplay MVE Video"),
        A("imve"),
        A("INPV"),

    /* Id Quake II CIN */
    B(CODEC_IDCIN, "Id Quake II CIN Video"),
        A("IDCI"),

    /* 4X Technologies */
    B(CODEC_4XM, "4X Technologies Video"),
        A("4XMV"),
        A("4xmv"),

    /* Id RoQ */
    B(CODEC_ROQ, "Id RoQ Video"),
        A("RoQv"),

    /* Sony Playstation MDEC */
    B(CODEC_MDEC, "PSX MDEC Video"),
        A("MDEC"),

    /* Sierra VMD */
    B(CODEC_VMDVIDEO, "Sierra VMD Video"),
        A("VMDV"),
        A("vmdv"),

    B(CODEC_DIRAC, "Dirac" ),
        A("drac"),

    /* Image */
    B(CODEC_PNG, "PNG Image"),
        A("png "),

    B(CODEC_PPM, "PPM Image"),
        A("ppm "),

    B(CODEC_PGM, "PGM Image"),
        A("pgm "),

    B(CODEC_PGMYUV, "PGM YUV Image"),
        A("pgmy"),

    B(CODEC_PAM, "PAM Image"),
        A("pam "),

    B(CODEC_JPEGLS, "Lossless JPEG"),
        A("MJLS"),

    B(CODEC_JPEG, "JPEG"),
        A("jpeg"),
        A("JPEG"),

    B(CODEC_BMP, "BMP Image"),
        A("bmp "),

    B(CODEC_TIFF, "TIFF Image"),
        A("tiff"),

    B(CODEC_GIF, "GIF Image"),
        A("gif "),


    B(CODEC_TARGA, "Truevision Targa Image"),
        A("tga "),
        A("mtga"),
        A("MTGA"),

    B(CODEC_SGI, "SGI Image"),
        A("sgi "),

    B(CODEC_PNM, "Portable Anymap Image"),
        A("pnm "),

    B(CODEC_PCX, "Personal Computer Exchange Image"),
        A("pcx "),

    B(0, "")
};
static const entry_t p_list_audio[] = {

    /* Windows Media Audio 1 */
    B(CODEC_WMA1, "Windows Media Audio 1"),
        A("WMA1"),
        A("wma1"),

    /* Windows Media Audio 2 */
    B(CODEC_WMA2, "Windows Media Audio 2"),
        A("WMA2"),
        A("wma2"),
        A("wma "),

    /* Windows Media Audio Professional */
    B(CODEC_WMAP, "Windows Media Audio Professional"),
        A("WMAP"),
        A("wmap"),

    /* Windows Media Audio Lossless */
    B(CODEC_WMAL, "Windows Media Audio Lossless"),
        A("WMAL"),
        A("wmal"),

    /* Windows Media Audio Speech */
    B(CODEC_WMAS, "Windows Media Audio Voice (Speech)"),
        A("WMAS"),
        A("wmas"),

    /* DV Audio */
    B(CODEC_DVAUDIO, "DV Audio"),
        A("dvau"),
        A("vdva"),
        A("dvca"),
        A("RADV"),

    /* MACE-3 Audio */
    B(CODEC_MACE3, "MACE-3 Audio"),
        A("MAC3"),

    /* MACE-6 Audio */
    B(CODEC_MACE6, "MACE-6 Audio"),
        A("MAC6"),

    /* MUSEPACK7 Audio */
    B(CODEC_MUSEPACK7, "MUSEPACK7 Audio"),
        A("MPC "),

    /* MUSEPACK8 Audio */
    B(CODEC_MUSEPACK8, "MUSEPACK8 Audio"),
        A("MPCK"),
        A("MPC8"),

    /* RealAudio 1.0 */
    B(CODEC_RA_144, "RealAudio 1.0"),
        A("14_4"),
        A("lpcJ"),

    /* RealAudio 2.0 */
    B(CODEC_RA_288, "RealAudio 2.0"),
        A("28_8"),

    B(CODEC_SIPR, "RealAudio Sipr"),
        A("sipr"),

    /* MPEG Audio layer 1/2/3 */
    B(CODEC_MPGA, "MPEG Audio layer 1/2/3"),
        A("mpga"),
        A("mp3 "),
        A(".mp3"),
        A("MP3 "),
        A("LAME"),
        A("ms\x00\x50"),
        A("ms\x00\x55"),

    /* A52 Audio (aka AC3) */
    B(CODEC_A52, "A52 Audio (aka AC3)"),
        A("a52 "),
        A("a52b"),
        A("ac-3"),
        A("ms\x20\x00"),

    B(CODEC_EAC3, "A/52 B Audio (aka E-AC3)"),
        A("eac3"),

    /* DTS Audio */
    B(CODEC_DTS, "DTS Audio"),
        A("dts "),
        A("dtsb"),
        A("ms\x20\x01"),

    /* AAC audio */
    B(CODEC_MP4A, "MPEG AAC Audio"),
        A("mp4a"),
        A("aac "),

    /* ALS audio */
    B(CODEC_ALS, "MPEG-4 Audio Lossless (ALS)"),
        A("als "),

    /* 4X Technologies */
    B(CODEC_ADPCM_4XM, "4X Technologies Audio"),
        A("4xma"),

    /* EA ADPCM */
    B(CODEC_ADPCM_EA, "EA ADPCM Audio"),
        A("ADEA"),

    /* Interplay DPCM */
    B(CODEC_INTERPLAY_DPCM, "Interplay DPCM Audio"),
        A("idpc"),

    /* Id RoQ */
    B(CODEC_ROQ_DPCM, "Id RoQ DPCM Audio"),
        A("RoQa"),

    /* DCIN Audio */
    B(CODEC_DSICINAUDIO, "Delphine CIN Audio"),
        A("DCIA"),

    /* Sony Playstation XA ADPCM */
    B(CODEC_ADPCM_XA, "PSX XA ADPCM Audio"),
        A("xa  "),

    /* ADX ADPCM */
    B(CODEC_ADPCM_ADX, "ADX ADPCM Audio"),
        A("adx "),

    /* Westwood ADPCM */
    B(CODEC_ADPCM_IMA_WS, "Westwood IMA ADPCM audio"),
        A("AIWS"),

    /* MS ADPCM */
    B(CODEC_ADPCM_MS, "MS ADPCM audio"),
        A("ms\x00\x02"),

    /* Sierra VMD */
    B(CODEC_VMDAUDIO, "Sierra VMD Audio"),
        A("vmda"),

    /* G.726 ADPCM */
    B(CODEC_ADPCM_G726, "G.726 ADPCM Audio"),
        A("g726"),

    /* Flash ADPCM */
    B(CODEC_ADPCM_SWF, "Flash ADPCM Audio"),
        A("SWFa"),

    B(CODEC_ADPCM_IMA_WAV, "IMA WAV ADPCM Audio"),
        A("ms\x00\x11"),

    B(CODEC_ADPCM_IMA_AMV, "IMA AMV ADPCM Audio"),
        A("imav"),

    /* AMR */
    B(CODEC_AMR_NB, "AMR narrow band"),
        A("samr"),

    B(CODEC_AMR_WB, "AMR wide band"),
        A("sawb"),

    /* FLAC */
    B(CODEC_FLAC, "FLAC (Free Lossless Audio Codec)"),
        A("flac"),

    /* ALAC */
    B(CODEC_ALAC, "Apple Lossless Audio Codec"),
        A("alac"),

    /* QDM2 */
    B(CODEC_QDM2, "QDM2 Audio"),
        A("QDM2"),

    /* COOK */
    B(CODEC_COOK, "Cook Audio"),
        A("cook"),

    /* TTA: The Lossless True Audio */
    B(CODEC_TTA, "The Lossless True Audio"),
        A("TTA1"),

    /* Shorten */
    B(CODEC_SHORTEN, "Shorten Lossless Audio"),
        A("shn "),
        A("shrn"),

    B(CODEC_WAVPACK, "WavPack"),
        A("WVPK"),
        A("wvpk"),

    B(CODEC_GSM, "GSM Audio"),
        A("gsm "),

    B(CODEC_GSM_MS, "Microsoft GSM Audio"),
        A("agsm"),

    B(CODEC_ATRAC1, "atrac 1"),
        A("atr1"),

    B(CODEC_ATRAC3, "atrac 3"),
        A("atrc"),
        A("\x70\x02\x00\x00"),

    B(CODEC_SONIC, "Sonic"),
        A("SONC"),

    B(CODEC_IMC, "IMC" ),
        A("\x01\x04\x00\x00"),

    B(CODEC_TRUESPEECH,"TrueSpeech"),
        A("\x22\x00\x00\x00"),

    B(CODEC_NELLYMOSER, "NellyMoser ASAO"),
        A("NELL"),

    B(CODEC_APE, "Monkey's Audio"),
        A("APE "),

    B(CODEC_MLP, "MLP/TrueHD Audio"),
        A("mlp "),

    B(CODEC_TRUEHD, "TrueHD Audio"),
        A("trhd"),

    B(CODEC_QCELP, "QCELP Audio"),
        A("Qclp"),

    B(CODEC_SPEEX, "Speex Audio"),
        A("spx "),
        A("spxr"),

    B(CODEC_VORBIS, "Vorbis Audio"),
        A("vorb"),

    B(CODEC_302M, "302M Audio"),
        A("302m"),

    B(CODEC_DVD_LPCM, "DVD LPCM Audio"),
        A("lpcm"),

    B(CODEC_DVDA_LPCM, "DVD-Audio LPCM Audio"),
        A("apcm"),

    B(CODEC_BD_LPCM, "BD LPCM Audio"),
        A("bpcm"),

    B(CODEC_SDDS, "SDDS Audio"),
        A("sdds"),
        A("sddb"),

    B(CODEC_MIDI, "MIDI Audio"),
        A("MIDI"),

    /* PCM */
    B(CODEC_S8, "PCM S8"),
        A("s8  "),

    B(CODEC_U8, "PCM U8"),
        A("u8  "),

    B(CODEC_S16L, "PCM S16 LE"),
        A("s16l"),

    B(CODEC_S16B, "PCM S16 BE"),
        A("s16b"),

    B(CODEC_U16L, "PCM U16 LE"),
        A("u16l"),

    B(CODEC_U16B, "PCM U16 BE"),
        A("u16b"),

    B(CODEC_S24L, "PCM S24 LE"),
        A("s24l"),
        A("42ni"),  /* Quicktime */

    B(CODEC_S24B, "PCM S24 BE"),
        A("s24b"),
        A("in24"),  /* Quicktime */

    B(CODEC_U24L, "PCM U24 LE"),
        A("u24l"),

    B(CODEC_U24B, "PCM U24 BE"),
        A("u24b"),

    B(CODEC_S32L, "PCM S32 LE"),
        A("s32l"),
        A("23ni"),  /* Quicktime */

    B(CODEC_S32B, "PCM S32 BE"),
        A("s32b"),
        A("in32"),  /* Quicktime */

    B(CODEC_U32L, "PCM U32 LE"),
        A("u32l"),

    B(CODEC_U32B, "PCM U32 BE"),
        A("u32b"),

    B(CODEC_ALAW, "PCM ALAW"),
        A("alaw"),

    B(CODEC_MULAW, "PCM MU-LAW"),
        A("mlaw"),
        A("ulaw"),

    B(CODEC_S24DAUD, "PCM DAUD"),
        A("daud"),

    B(CODEC_FI32, "32 bits fixed float"),
        A("fi32"),

    B(CODEC_F32L, "32 bits float LE"),
        A("f32l"),
        A("fl32"),

    B(CODEC_F32B, "32 bits float BE"),
        A("f32b"),

    B(CODEC_F64L, "64 bits float LE"),
        A("f64l"),

    B(CODEC_F64L, "64 bits float BE"),
        A("f64b"),

    B(CODEC_TWINVQ, "TwinVQ"),
        A("TWIN"),

    B(0, "")
};
static const entry_t p_list_spu[] = {

    B(CODEC_SPU, "DVD Subtitles"),
        A("spu "),
        A("spub"),

    B(CODEC_DVBS, "DVB Subtitles"),
        A("dvbs"),

    B(CODEC_SUBT, "Text subtitles with various tags"),
        A("subt"),

    B(CODEC_XSUB, "DivX XSUB subtitles"),
        A("XSUB"),
        A("xsub"),
        A("DXSB"),

    B(CODEC_SSA, "SubStation Alpha subtitles"),
        A("ssa "),

    B(CODEC_TEXT, "Plain text subtitles"),
        A("TEXT"),

    B(CODEC_TELETEXT, "Teletext"),
        A("telx"),

    B(CODEC_KATE, "Kate subtitles"),
        A("kate"),

    B(CODEC_CMML, "CMML annotations/metadata"),
        A("cmml"),

    B(CODEC_ITU_T140, "ITU T.140 subtitles"),
        A("t140"),

    B(CODEC_USF, "USF subtitles"),
        A("usf "),

    B(CODEC_OGT, "OGT subtitles"),
        A("ogt "),

    B(CODEC_CVD, "CVD subtitles"),
        A("cvd "),

    B(CODEC_BD_PG, "BD subtitles"),
        A("bdpg"),

    B(0, "")
};

/* Create a fourcc from a string.
 * XXX it assumes that the string is at least four bytes */
static inline fourcc_t CreateFourcc( const char *psz_fourcc )
{
    return TME_FOURCC( psz_fourcc[0], psz_fourcc[1],
                       psz_fourcc[2], psz_fourcc[3] );
}

/* */
static entry_t Lookup( const entry_t p_list[], fourcc_t i_fourcc )
{
    const char *p_class = NULL;
    const char *psz_description = NULL;

    entry_t e = B(0, "");

    for( int i = 0; ; i++ )
    {
        const entry_t *p = &p_list[i];
        const fourcc_t i_entry_fourcc = CreateFourcc( p->p_fourcc );
        const fourcc_t i_entry_class = CreateFourcc( p->p_class );

        if( i_entry_fourcc == 0 )
            break;

        if( i_entry_class != 0 )
        {
            p_class = p->p_class;
            psz_description = p->psz_description;
        }

        if( i_entry_fourcc == i_fourcc )
        {
            assert( p_class != NULL );

            memcpy( e.p_class, p_class, 4 );
            memcpy( e.p_fourcc, p->p_fourcc, 4 );
            e.psz_description = p->psz_description ?
                                p->psz_description : psz_description;
            break;
        }
    }
    return e;
}

/* */
static entry_t Find( int i_cat, fourcc_t i_fourcc )
{
    entry_t e;

    switch( i_cat )
    {
    case VIDEO_ES:
        return Lookup( p_list_video, i_fourcc );
    case AUDIO_ES:
        return Lookup( p_list_audio, i_fourcc );
    case SPU_ES:
        return Lookup( p_list_spu, i_fourcc );

    default:
        e = Find( VIDEO_ES, i_fourcc );
        if( CreateFourcc( e.p_class ) == 0 )
            e = Find( AUDIO_ES, i_fourcc );
        if( CreateFourcc( e.p_class ) == 0 )
            e = Find( SPU_ES, i_fourcc );
        return e;
    }
}

/* */
fourcc_t fourcc_GetCodec( int i_cat, fourcc_t i_fourcc )
{
    entry_t e = Find( i_cat, i_fourcc );

    if( CreateFourcc( e.p_class ) == 0 )
        return i_fourcc;
    return CreateFourcc( e.p_class );
}

fourcc_t fourcc_GetCodecFromString( int i_cat, const char *psz_fourcc )
{
    if( !psz_fourcc || strlen(psz_fourcc) != 4 )
        return 0;
    return fourcc_GetCodec( i_cat,
                                TME_FOURCC( psz_fourcc[0], psz_fourcc[1],
                                            psz_fourcc[2], psz_fourcc[3] ) );
}

fourcc_t fourcc_GetCodecAudio( fourcc_t i_fourcc, int i_bits )
{
    const int i_bytes = ( i_bits + 7 ) / 8;

    if( i_fourcc == TME_FOURCC( 'a', 'f', 'l', 't' ) )
    {
        switch( i_bytes )
        {
        case 4:
            return CODEC_FL32;
        case 8:
            return CODEC_FL64;
        default:
            return 0;
        }
    }
    else if( i_fourcc == TME_FOURCC( 'a', 'r', 'a', 'w' ) ||
             i_fourcc == TME_FOURCC( 'p', 'c', 'm', ' ' ) )
    {
        switch( i_bytes )
        {
        case 1:
            return CODEC_U8;
        case 2:
            return CODEC_S16L;
        case 3:
            return CODEC_S24L;
            break;
        case 4:
            return CODEC_S32L;
        default:
            return 0;
        }
    }
    else if( i_fourcc == TME_FOURCC( 't', 'w', 'o', 's' ) )
    {
        switch( i_bytes )
        {
        case 1:
            return CODEC_S8;
        case 2:
            return CODEC_S16B;
        case 3:
            return CODEC_S24B;
        case 4:
            return CODEC_S32B;
        default:
            return 0;
        }
    }
    else if( i_fourcc == TME_FOURCC( 's', 'o', 'w', 't' ) )
    {
        switch( i_bytes )
        {
        case 1:
            return CODEC_S8;
        case 2:
            return CODEC_S16L;
        case 3:
            return CODEC_S24L;
        case 4:
            return CODEC_S32L;
        default:
            return 0;
        }
    }
    else
    {
        return fourcc_GetCodec( AUDIO_ES, i_fourcc );
    }
}

/* */
const char *fourcc_GetDescription( int i_cat, fourcc_t i_fourcc )
{
    entry_t e = Find( i_cat, i_fourcc );

    return e.psz_description;
}


/* */
#define CODEC_YUV_PLANAR_410 \
    CODEC_I410, CODEC_YV9

#define CODEC_YUV_PLANAR_420 \
    CODEC_I420, CODEC_YV12, CODEC_J420

#define CODEC_YUV_PLANAR_422 \
    CODEC_I422, CODEC_J422

#define CODEC_YUV_PLANAR_440 \
    CODEC_I440, CODEC_J440

#define CODEC_YUV_PLANAR_444 \
    CODEC_I444, CODEC_J444

#define CODEC_YUV_PACKED \
    CODEC_YUYV, CODEC_YVYU, \
    CODEC_UYVY, CODEC_VYUY

#define CODEC_FALLBACK_420 \
    CODEC_YUV_PLANAR_422, CODEC_YUV_PACKED, \
    CODEC_YUV_PLANAR_444, CODEC_YUV_PLANAR_440, \
    CODEC_I411, CODEC_YUV_PLANAR_410, CODEC_Y211

static const fourcc_t p_I420_fallback[] = {
    CODEC_I420, CODEC_YV12, CODEC_J420, CODEC_FALLBACK_420, 0
};
static const fourcc_t p_J420_fallback[] = {
    CODEC_J420, CODEC_I420, CODEC_YV12, CODEC_FALLBACK_420, 0
};
static const fourcc_t p_YV12_fallback[] = {
    CODEC_YV12, CODEC_I420, CODEC_J420, CODEC_FALLBACK_420, 0
};

#define CODEC_FALLBACK_422 \
    CODEC_YUV_PACKED, CODEC_YUV_PLANAR_420, \
    CODEC_YUV_PLANAR_444, CODEC_YUV_PLANAR_440, \
    CODEC_I411, CODEC_YUV_PLANAR_410, CODEC_Y211

static const fourcc_t p_I422_fallback[] = {
    CODEC_I422, CODEC_J422, CODEC_FALLBACK_422, 0
};
static const fourcc_t p_J422_fallback[] = {
    CODEC_J422, CODEC_I422, CODEC_FALLBACK_422, 0
};

#define CODEC_FALLBACK_444 \
    CODEC_YUV_PLANAR_422, CODEC_YUV_PACKED, \
    CODEC_YUV_PLANAR_420, CODEC_YUV_PLANAR_440, \
    CODEC_I411, CODEC_YUV_PLANAR_410, CODEC_Y211

static const fourcc_t p_I444_fallback[] = {
    CODEC_I444, CODEC_J444, CODEC_FALLBACK_444, 0
};
static const fourcc_t p_J444_fallback[] = {
    CODEC_J444, CODEC_I444, CODEC_FALLBACK_444, 0
};

static const fourcc_t p_I440_fallback[] = {
    CODEC_I440,
    CODEC_YUV_PLANAR_420,
    CODEC_YUV_PLANAR_422,
    CODEC_YUV_PLANAR_444,
    CODEC_YUV_PACKED,
    CODEC_I411, CODEC_YUV_PLANAR_410, CODEC_Y211, 0
};

#define CODEC_FALLBACK_PACKED \
    CODEC_YUV_PLANAR_422, CODEC_YUV_PLANAR_420, \
    CODEC_YUV_PLANAR_444, CODEC_YUV_PLANAR_440, \
    CODEC_I411, CODEC_YUV_PLANAR_410, CODEC_Y211

static const fourcc_t p_YUYV_fallback[] = {
    CODEC_YUYV,
    CODEC_YVYU,
    CODEC_UYVY,
    CODEC_VYUY,
    CODEC_FALLBACK_PACKED, 0
};
static const fourcc_t p_YVYU_fallback[] = {
    CODEC_YVYU,
    CODEC_YUYV,
    CODEC_UYVY,
    CODEC_VYUY,
    CODEC_FALLBACK_PACKED, 0
};
static const fourcc_t p_UYVY_fallback[] = {
    CODEC_UYVY,
    CODEC_VYUY,
    CODEC_YUYV,
    CODEC_YVYU,
    CODEC_FALLBACK_PACKED, 0
};
static const fourcc_t p_VYUY_fallback[] = {
    CODEC_VYUY,
    CODEC_UYVY,
    CODEC_YUYV,
    CODEC_YVYU,
    CODEC_FALLBACK_PACKED, 0
};

static const fourcc_t *pp_YUV_fallback[] = {
    p_YV12_fallback,
    p_I420_fallback,
    p_J420_fallback,
    p_I422_fallback,
    p_J422_fallback,
    p_I444_fallback,
    p_J444_fallback,
    p_I440_fallback,
    p_YUYV_fallback,
    p_YVYU_fallback,
    p_UYVY_fallback,
    p_VYUY_fallback,
    NULL,
};

static const fourcc_t p_list_YUV[] = {
    CODEC_YUV_PLANAR_420,
    CODEC_YUV_PLANAR_422,
    CODEC_YUV_PLANAR_440,
    CODEC_YUV_PLANAR_444,
    CODEC_YUV_PACKED,
    CODEC_I411, CODEC_YUV_PLANAR_410, CODEC_Y211,
    0,
};

/* */
static const fourcc_t p_RGB32_fallback[] = {
    CODEC_RGB32,
    CODEC_RGB24,
    CODEC_RGB16,
    CODEC_RGB15,
    CODEC_RGB8,
    0,
};
static const fourcc_t p_RGB24_fallback[] = {
    CODEC_RGB24,
    CODEC_RGB32,
    CODEC_RGB16,
    CODEC_RGB15,
    CODEC_RGB8,
    0,
};
static const fourcc_t p_RGB16_fallback[] = {
    CODEC_RGB16,
    CODEC_RGB24,
    CODEC_RGB32,
    CODEC_RGB15,
    CODEC_RGB8,
    0,
};
static const fourcc_t p_RGB15_fallback[] = {
    CODEC_RGB15,
    CODEC_RGB16,
    CODEC_RGB24,
    CODEC_RGB32,
    CODEC_RGB8,
    0,
};
static const fourcc_t p_RGB8_fallback[] = {
    CODEC_RGB8,
    CODEC_RGB15,
    CODEC_RGB16,
    CODEC_RGB24,
    CODEC_RGB32,
    0,
};
static const fourcc_t *pp_RGB_fallback[] = {
    p_RGB32_fallback,
    p_RGB24_fallback,
    p_RGB16_fallback,
    p_RGB15_fallback,
    p_RGB8_fallback,
    NULL,
};


/* */
static const fourcc_t *GetFallback( fourcc_t i_fourcc,
                                        const fourcc_t *pp_fallback[],
                                        const fourcc_t p_list[] )
{
    for( unsigned i = 0; pp_fallback[i]; i++ )
    {
        if( pp_fallback[i][0] == i_fourcc )
            return pp_fallback[i];
    }
    return p_list;
}

const fourcc_t *fourcc_GetYUVFallback( fourcc_t i_fourcc )
{
    return GetFallback( i_fourcc, pp_YUV_fallback, p_list_YUV );
}
const fourcc_t *fourcc_GetRGBFallback( fourcc_t i_fourcc )
{
    return GetFallback( i_fourcc, pp_RGB_fallback, p_RGB32_fallback );
}

bool fourcc_AreUVPlanesSwapped( fourcc_t a, fourcc_t b )
{
    static const fourcc_t pp_swapped[][4] = {
        { CODEC_YV12, CODEC_I420, CODEC_J420, 0 },
        { CODEC_YV9,  CODEC_I410, 0 },
        { 0 }
    };

    for( int i = 0; pp_swapped[i][0]; i++ )
    {
        if( pp_swapped[i][0] == b )
        {
            fourcc_t t = a;
            a = b;
            b = t;
        }
        if( pp_swapped[i][0] != a )
            continue;
        for( int j = 1; pp_swapped[i][j]; j++ )
        {
            if( pp_swapped[i][j] == b )
                return true;
        }
    }
    return false;
}

bool fourcc_IsYUV(fourcc_t fcc)
{
    for( unsigned i = 0; p_list_YUV[i]; i++ )
    {
        if( p_list_YUV[i] == fcc )
            return true;
    }
    return false;
}
