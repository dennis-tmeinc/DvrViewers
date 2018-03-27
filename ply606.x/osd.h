#ifndef _OSD_H_
#define _OSD_H_

struct text_style_t
{
    char *     psz_fontname;      /**< The name of the font */
    int        i_font_size;       /**< The font size in pixels */
    int        i_font_color;      /**< The color of the text 0xRRGGBB
                                       (native endianness) */
    int        i_font_alpha;      /**< The transparency of the text.
                                       0x00 is fully opaque,
                                       0xFF fully transparent */
    int        i_style_flags;     /**< Formatting style flags */
    int        i_outline_color;   /**< The color of the outline 0xRRGGBB */
    int        i_outline_alpha;   /**< The transparency of the outline.
                                       0x00 is fully opaque,
                                       0xFF fully transparent */
    int        i_shadow_color;    /**< The color of the shadow 0xRRGGBB */
    int        i_shadow_alpha;    /**< The transparency of the shadow.
                                        0x00 is fully opaque,
                                        0xFF fully transparent */
    int        i_background_color;/**< The color of the background 0xRRGGBB */
    int        i_background_alpha;/**< The transparency of the background.
                                       0x00 is fully opaque,
                                       0xFF fully transparent */
    int        i_outline_width;   /**< The width of the outline in pixels */
    int        i_shadow_width;    /**< The width of the shadow in pixels */
    int        i_spacing;         /**< The spaceing between glyphs in pixels */
    int        i_text_align;      /**< An alignment hint for the text */
};

#endif