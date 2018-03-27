#ifndef _VIDEO_H_
#define _VIDEO_H_

#include "es.h"
#include "osd.h"

#define VOUT_MAX_PLANES                 5
#define VOUT_MAX_PICTURES               8
#define VOUT_MAX_SUBPICTURES            16
#define PICTURE_PLANE_MAX (VOUT_MAX_PLANES)

#define memalign(pp_orig,align,size) \
    (( *(pp_orig) = malloc( size + align - 1 )) \
        ? (void *)( (((unsigned long)*(pp_orig)) + (unsigned long)(align-1) ) \
                       & (~(unsigned long)(align-1)) ) \
        : NULL )


typedef struct picture_heap_t picture_heap_t;
typedef struct picture_sys_t picture_sys_t;

/**
 * Description of a planar graphic field
 */
typedef struct plane_t
{
    UINT8 *p_pixels;                        /**< Start of the plane's data */

    /* Variables used for fast memcpy operations */
    int i_lines;           /**< Number of lines, including margins */
    int i_pitch;           /**< Number of bytes in a line, including margins */

    /** Size of a macropixel, defaults to 1 */
    int i_pixel_pitch;

    /* Variables used for pictures with margins */
    int i_visible_lines;            /**< How many visible lines are there ? */
    int i_visible_pitch;            /**< How many visible pixels are there ? */

} plane_t;

/**
 * Video picture
 *
 * Any picture destined to be displayed by a video output thread should be
 * stored in this structure from it's creation to it's effective display.
 * Picture type and flags should only be modified by the output thread. Note
 * that an empty picture MUST have its flags set to 0.
 */
struct picture_t
{
    /**
     * The properties of the picture
     */
    video_format_t format;

    /** Picture data - data can always be freely modified, but p_data may
     * NEVER be modified. A direct buffer can be handled as the plugin
     * wishes, it can even swap p_pixels buffers. */
    UINT8        *p_data;
    void           *p_data_orig;                /**< pointer before memalign */
    plane_t         p[ VOUT_MAX_PLANES ];     /**< description of the planes */
    int             i_planes;                /**< number of allocated planes */

    /** \name Type and flags
     * Should NOT be modified except by the vout thread
     * @{*/
    int             i_status;                             /**< picture flags */
    int             i_type;                /**< is picture a direct buffer ? */
    bool      b_slow;                 /**< is picture in slow memory ? */
    int             i_matrix_coefficients;   /**< in YUV type, encoding type */
    /**@}*/

    /** \name Picture management properties
     * These properties can be modified using the video output thread API,
     * but should never be written directly */
    /**@{*/
    int             i_refcount;                  /**< link reference counter */
    mtime_t         date;                                  /**< display date */
    bool      b_force;
    /**@}*/

    /** \name Picture dynamic properties
     * Those properties can be changed by the decoder
     * @{
     */
    bool      b_progressive;          /**< is it a progressive frame ? */
    unsigned int    i_nb_fields;                  /**< # of displayed fields */
    bool      b_top_field_first;             /**< which field is first */
    /**@}*/

    /** The picture heap we are attached to */
    picture_heap_t* p_heap;

    /* Some vouts require the picture to be locked before it can be modified */
	// changed to use virtual function now.
    //int (*pf_lock) ( picture_t * );
    //int (*pf_unlock) ( picture_t * );

    /** Private data - the video output plugin might want to put stuff here to
     * keep track of the picture */
    picture_sys_t * p_sys;

    /** This way the picture_Release can be overloaded */
    void (*pf_release)( picture_t * );

    /** Next picture in a FIFO a pictures */
    struct picture_t *p_next;
};

/**
 * Video picture heap, either render (to store pictures used
 * by the decoder) or output (to store pictures displayed by the vout plugin)
 */
struct picture_heap_t
{
    int i_pictures;                                   /**< current heap size */

    /* \name Picture static properties
     * Those properties are fixed at initialization and should NOT be modified
     * @{
     */
    unsigned int i_width;                                 /**< picture width */
    unsigned int i_height;                               /**< picture height */
    fourcc_t i_chroma;                               /**< picture chroma */
    unsigned int i_aspect;                                 /**< aspect ratio */
    /**@}*/

    /* Real pictures */
    picture_t*      pp_picture[VOUT_MAX_PICTURES];             /**< pictures */
    int             i_last_used_pic;              /**< last used pic in heap */
    bool      b_allow_modify_pics;

    /* Stuff used for truecolor RGB planes */
    UINT32 i_rmask; int i_rrshift, i_lrshift;
    UINT32 i_gmask; int i_rgshift, i_lgshift;
    UINT32 i_bmask; int i_rbshift, i_lbshift;

    /** Stuff used for palettized RGB planes */
    void (* pf_setpalette) ( UINT16 *, UINT16 *, UINT16 * );
};

/**
 * Resource for a picture.
 */
typedef struct
{
    picture_sys_t *p_sys;

    /* Plane resources
     * XXX all fields MUST be set to the right value.
     */
    struct
    {
        UINT8 *p_pixels;  /**< Start of the plane's data */
        int i_lines;        /**< Number of lines, including margins */
        int i_pitch;        /**< Number of bytes in a line, including margins */
    } p[PICTURE_PLANE_MAX];

} picture_resource_t;

/*****************************************************************************
 * Flags used to describe the status of a picture
 *****************************************************************************/

/* Picture type */
#define EMPTY_PICTURE           0                            /* empty buffer */
#define MEMORY_PICTURE          100                 /* heap-allocated buffer */
#define DIRECT_PICTURE          200                         /* direct buffer */

/* Picture status */
#define FREE_PICTURE            0                  /* free and not allocated */
#define RESERVED_PICTURE        1                  /* allocated and reserved */
#define RESERVED_DATED_PICTURE  2              /* waiting for DisplayPicture */
#define RESERVED_DISP_PICTURE   3               /* waiting for a DatePicture */
#define READY_PICTURE           4                       /* ready for display */
#define DISPLAYED_PICTURE       5            /* been displayed but is linked */
#define DESTROYED_PICTURE       6              /* allocated but no more used */

/*****************************************************************************
 * Shortcuts to access image components
 *****************************************************************************/

/* Plane indices */
#define Y_PLANE      0
#define U_PLANE      1
#define V_PLANE      2
#define A_PLANE      3

/* Shortcuts */
#define Y_PIXELS     p[Y_PLANE].p_pixels
#define Y_PITCH      p[Y_PLANE].i_pitch
#define U_PIXELS     p[U_PLANE].p_pixels
#define U_PITCH      p[U_PLANE].i_pitch
#define V_PIXELS     p[V_PLANE].p_pixels
#define V_PITCH      p[V_PLANE].i_pitch
#define A_PIXELS     p[A_PLANE].p_pixels
#define A_PITCH      p[A_PLANE].i_pitch

/**
 * Video subtitle region
 *
 * A subtitle region is defined by a picture (graphic) and its rendering
 * coordinates.
 * Subtitles contain a list of regions.
 */
struct subpicture_region_t
{
    video_format_t  fmt;                          /**< format of the picture */
    picture_t       picture;             /**< picture comprising this region */

    int             i_x;                             /**< position of region */
    int             i_y;                             /**< position of region */
    int             i_align;                  /**< alignment within a region */

    char            *psz_text;       /**< text string comprising this region */
    text_style_t    *p_style;  /* a description of the text style formatting */

    subpicture_region_t *p_next;                /**< next region in the list */
    subpicture_region_t *p_cache;       /**< modified version of this region */
};

struct subpicture_t
{
    int             i_channel;                    /**< subpicture channel ID */

    int             i_type;                                        /**< type */
    int             i_status;                                     /**< flags */
    subpicture_t *  p_next;               /**< next subtitle to be displayed */

    mtime_t         i_start;                  /**< beginning of display date */
    mtime_t         i_stop;                         /**< end of display date */
    bool      b_ephemer;    /**< If this flag is set to true the subtitle
                                will be displayed untill the next one appear */
    bool      b_fade;                               /**< enable fading */
    bool      b_pausable;               /**< subpicture will be paused if
                                                            stream is paused */

    subpicture_region_t *p_region;  /**< region list composing this subtitle */

    int          i_x;                    /**< offset from alignment position */
    int          i_y;                    /**< offset from alignment position */
    int          i_width;                                 /**< picture width */
    int          i_height;                               /**< picture height */
    int          i_alpha;                                  /**< transparency */
    int          i_original_picture_width;  /**< original width of the movie */
    int          i_original_picture_height;/**< original height of the movie */
    bool   b_absolute;                       /**< position is absolute */
    int          i_flags;                                /**< position flags */
};

/* Subpicture type */
#define EMPTY_SUBPICTURE       0     /* subtitle slot is empty and available */
#define MEMORY_SUBPICTURE      100            /* subpicture stored in memory */

/* Default subpicture channel ID */
#define DEFAULT_CHAN           1

/* Subpicture status */
#define FREE_SUBPICTURE        0                   /* free and not allocated */
#define RESERVED_SUBPICTURE    1                   /* allocated and reserved */
#define READY_SUBPICTURE       2                        /* ready for display */

/* Subpicture position flags */
#define SUBPICTURE_ALIGN_LEFT 0x1
#define SUBPICTURE_ALIGN_RIGHT 0x2
#define SUBPICTURE_ALIGN_TOP 0x4
#define SUBPICTURE_ALIGN_BOTTOM 0x8

enum spu_query_e
{
    SPU_CHANNEL_REGISTER, 
    SPU_CHANNEL_CLEAR 
};

#endif