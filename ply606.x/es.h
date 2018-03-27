#ifndef _ES_H
#define _ES_H

#include <stdlib.h>
#include <malloc.h>
#include "mtime.h"

#define TME_FOURCC( a, b, c, d ) \
        ( ((UINT32)a) | ( ((UINT32)b) << 8 ) \
           | ( ((UINT32)c) << 16 ) | ( ((UINT32)d) << 24 ) )
#define TWOCC( a, b ) \
        ( (UINT16)(a) | ( (UINT16)(b) << 8 ) )

typedef UINT32 fourcc_t;
//typedef __int64 off_t;

struct video_palette_t
{
    int i_entries;      /**< to keep the compatibility with ffmpeg's palette */
    BYTE palette[256][4];                   /**< 4-byte RGBA/YUVA palette */
};

struct audio_format_t {
	fourcc_t i_format; /* audio format fourcc */
    unsigned int i_rate;                              /**< audio sample-rate */

    /* Describes the channels configuration of the samples (ie. number of
     * channels which are available in the buffer, and positions). */
    UINT32     i_physical_channels;

    /* Describes from which original channels, before downmixing, the
     * buffer is derived. */
    UINT32     i_original_channels;

    /* Optional - for A/52, SPDIF and DTS types : */
    /* Bytes used by one compressed frame, depends on bitrate. */
    unsigned int i_bytes_per_frame;

    /* Number of sampleframes contained in one compressed frame. */
    unsigned int i_frame_length;
    /* Please note that it may be completely arbitrary - buffers are not
     * obliged to contain a integral number of so-called "frames". It's
     * just here for the division :
     * buffer_size = i_nb_samples * i_bytes_per_frame / i_frame_length
     */

    /* FIXME ? (used by the codecs) */
    int i_channels;
    int i_blockalign;
    int i_bitspersample;
};

struct video_format_t
{
    fourcc_t i_chroma;                               /**< picture chroma */
    unsigned int i_aspect;                                 /**< aspect ratio */

    unsigned int i_width;                                 /**< picture width */
    unsigned int i_height;                               /**< picture height */
    unsigned int i_x_offset;               /**< start offset of visible area */
    unsigned int i_y_offset;               /**< start offset of visible area */
    unsigned int i_visible_width;                 /**< width of visible area */
    unsigned int i_visible_height;               /**< height of visible area */

    unsigned int i_bits_per_pixel;             /**< number of bits per pixel */

    unsigned int i_sar_num;                   /**< sample/pixel aspect ratio */
    unsigned int i_sar_den;

    unsigned int i_frame_rate;                     /**< frame rate numerator */
    unsigned int i_frame_rate_base;              /**< frame rate denominator */

    int i_rmask, i_gmask, i_bmask;          /**< color masks for RGB chroma */
    video_palette_t *p_palette;              /**< video palette from demuxer */
};

struct subs_format_t
{
    /* the character encoding of the text of the subtitle.
     * all gettext recognized shorts can be used */
    char *psz_encoding;


    int  i_x_origin; /**< x coordinate of the subtitle. 0 = left */
    int  i_y_origin; /**< y coordinate of the subtitle. 0 = top */

    struct
    {
        /*  */
        UINT32 palette[16+1];

        /* the width of the original movie the spu was extracted from */
        int i_original_frame_width;
        /* the height of the original movie the spu was extracted from */
        int i_original_frame_height;
    } spu;

    struct
    {
        int i_id;
    } dvb;
};

typedef struct extra_languages_t
{
        char *psz_language;
        char *psz_description;
} extra_languages_t;

struct es_format_t
{
    int             i_cat;
    fourcc_t    i_codec;

    int             i_id;       /* -1: let the core mark the right id
                                   >=0: valid id */
    int             i_group;    /* -1 : standalone
                                   >= 0 then a "group" (program) is created
                                        for each value */
    int             i_priority; /*  -2 : mean not selectable by the users
                                    -1 : mean not selected by default even
                                        when no other stream
                                    >=0: priority */

    char            *psz_language;
    char            *psz_description;
    int             i_extra_languages;
    extra_languages_t *p_extra_languages;

    audio_format_t audio;
    video_format_t video;
    subs_format_t  subs;

    unsigned int   i_bitrate;

    bool     b_packetized; /* wether the data is packetized
                                    (ie. not truncated) */
    int     i_extra;
    void    *p_extra;

};

/* ES Categories */
#define UNKNOWN_ES      0x00
#define VIDEO_ES        0x01
#define AUDIO_ES        0x02
#define SPU_ES          0x03
#define NAV_ES          0x04

static inline void es_format_Init( es_format_t *fmt,
                                   int i_cat, fourcc_t i_codec )
{
    fmt->i_cat                  = i_cat;
    fmt->i_codec                = i_codec;
    fmt->i_id                   = -1;
    fmt->i_group                = 0;
    fmt->i_priority             = 0;
    fmt->psz_language           = NULL;
    fmt->psz_description        = NULL;

    fmt->i_extra_languages      = 0;
    fmt->p_extra_languages      = NULL;

    memset( &fmt->audio, 0, sizeof(audio_format_t) );
    memset( &fmt->video, 0, sizeof(video_format_t) );
    memset( &fmt->subs, 0, sizeof(subs_format_t) );

    fmt->b_packetized           = true;
    fmt->i_bitrate              = 0;
    fmt->i_extra                = 0;
    fmt->p_extra                = NULL;
}

static inline void es_format_Copy( es_format_t *dst, es_format_t *src )
{
    int i;
    memcpy( dst, src, sizeof( es_format_t ) );
    if( src->psz_language )
         dst->psz_language = _strdup( src->psz_language );
    if( src->psz_description )
        dst->psz_description = _strdup( src->psz_description );
    if( src->i_extra > 0 )
    {
        dst->i_extra = src->i_extra;
        dst->p_extra = malloc( src->i_extra );
        memcpy( dst->p_extra, src->p_extra, src->i_extra );
    }
    else
    {
        dst->i_extra = 0;
        dst->p_extra = NULL;
    }

    if( src->subs.psz_encoding )
        dst->subs.psz_encoding = _strdup( src->subs.psz_encoding );

    if( src->video.p_palette )
    {
        dst->video.p_palette =
            (video_palette_t*)malloc( sizeof( video_palette_t ) );
        memcpy( dst->video.p_palette, src->video.p_palette,
                sizeof( video_palette_t ) );
    }

    dst->i_extra_languages = src->i_extra_languages;
    if( dst->i_extra_languages )
        dst->p_extra_languages = (extra_languages_t*)
            malloc(dst->i_extra_languages * sizeof(*dst->p_extra_languages ));
    for( i = 0; i < dst->i_extra_languages; i++ ) {
        if( src->p_extra_languages[i].psz_language )
            dst->p_extra_languages[i].psz_language = _strdup(src->p_extra_languages[i].psz_language);
        else
            dst->p_extra_languages[i].psz_language = NULL;
        if( src->p_extra_languages[i].psz_description )
            dst->p_extra_languages[i].psz_description = _strdup(src->p_extra_languages[i].psz_description);
        else
            dst->p_extra_languages[i].psz_description = NULL;
    }
}

static inline void es_format_Clean( es_format_t *fmt )
{
    if( fmt->psz_language ) free( fmt->psz_language );
    fmt->psz_language = NULL;

    if( fmt->psz_description ) free( fmt->psz_description );
    fmt->psz_description = NULL;

    if( fmt->i_extra > 0 ) free( fmt->p_extra );
    fmt->i_extra = 0; fmt->p_extra = NULL;

    if( fmt->video.p_palette )
        free( fmt->video.p_palette );
    fmt->video.p_palette = NULL;

    if( fmt->subs.psz_encoding ) free( fmt->subs.psz_encoding );
    fmt->subs.psz_encoding = NULL;

    if( fmt->i_extra_languages && fmt->p_extra_languages ) {
        int i = 0;
        while( i < fmt->i_extra_languages ) {
            if( fmt->p_extra_languages[i].psz_language )
                free( fmt->p_extra_languages[i].psz_language );
            if( fmt->p_extra_languages[i].psz_description )
                free( fmt->p_extra_languages[i].psz_description );
            i++;
        }
        free(fmt->p_extra_languages);
    }
}


#endif