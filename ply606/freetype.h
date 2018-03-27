#ifndef _FREETYPE_H_
#define _FREETYPE_H_

#include "filter.h"
#include "video.h"

#include "ft2build.h"
#include FT_FREETYPE_H
#include FT_GLYPH_H

struct line_desc_t
{
    /** NULL-terminated list of glyphs making the string */
    FT_BitmapGlyph *pp_glyphs;
    /** list of relative positions for the glyphs */
    FT_Vector      *p_glyph_pos;

    int             i_height;
    int             i_width;
    int             i_red, i_green, i_blue;
    int             i_alpha;

    line_desc_t    *p_next;
};
typedef struct line_desc_t line_desc_t;

class CFreetype
{
public:
	CFreetype(filter_t *p_filter);
	~CFreetype();
	int Create();
	int RenderText( subpicture_region_t *p_region_out,
                       subpicture_region_t *p_region_in );
    /* Input format */
    es_format_t         m_fmt_in;
    /* Output format of filter */
    es_format_t         m_fmt_out;

private:
	FT_Library     m_pLibrary;   /* handle to library     */
    FT_Face        m_pFace;      /* handle to face object */
    bool		m_iUseKerning;
    UINT8        m_iFontOpacity;
    int            m_iFontColor;
    int            m_iFontSize;
    int            m_iEffect;

    int            m_iDefaultFontSize;
    int            m_iDisplayHeight;
	int				m_iRelFontSize;

	int SetFontSize( int i_size );
	int RenderYUVA( subpicture_region_t *p_region,
                   line_desc_t *p_line, int i_width, int i_height );
};

#endif