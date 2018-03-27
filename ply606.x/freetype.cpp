#include "stdafx.h"
#include <crtdbg.h>
#include "freetype.h"
#include "subpicture.h"
#include "tmeviewdef.h"

#pragma comment(lib, "freetype.lib")
//#pragma comment(lib, "libfreetype.dll.a")

#define DEFAULT_FONT "" /* Default font found at run-time */
#define EFFECT_BACKGROUND  1 
#define EFFECT_OUTLINE     2
#define EFFECT_OUTLINE_FAT 3

static void FreeLine( line_desc_t *p_line );
static void FreeLines( line_desc_t *p_lines );
static line_desc_t *NewLine( PBYTE psz_string );
static void DrawBlack( line_desc_t *p_line, int i_width, subpicture_region_t *p_region, int xoffset, int yoffset );

CFreetype::CFreetype(filter_t *p_filter)
{
    m_pLibrary			= NULL;
    m_pFace				= NULL;
    m_iUseKerning		= 0;
    m_iFontOpacity		= 0;
    m_iFontColor		= 0;
    m_iFontSize			= 0;
    m_iEffect			= 0;
	m_iDefaultFontSize	= 0;
    m_iDisplayHeight	= 0; 

	es_format_Copy(&m_fmt_in, &p_filter->fmt_in);
	es_format_Copy(&m_fmt_out, &p_filter->fmt_out);
}

CFreetype::~CFreetype()
{
	if (m_pFace) {
	    FT_Done_Face( m_pFace );
		m_pFace = NULL;
	}
	if (m_pLibrary) {
	    FT_Done_FreeType( m_pLibrary );
		m_pLibrary = NULL;
	}
}

int CFreetype::Create()
{
	TCHAR tchrFontfile[32767];
	char  psz_fontfile[32767];
    int i_error;

	m_pLibrary			= NULL;
    m_pFace				= NULL;
    m_iFontSize			= 0;
    m_iDisplayHeight	= 0; 

	/* configuable */
	m_iFontOpacity	= 255; /* 0 to 255 */
	m_iFontColor	= 0xffffff; /* RGB value */
	m_iEffect		= EFFECT_OUTLINE; 
	m_iRelFontSize	= 24; /* small:20, 16, etc*/

	if( !GetWindowsDirectory( tchrFontfile, sizeof(tchrFontfile) ) ) {
        _RPT0( _CRT_WARN, "GetWindowsDirectory Error\n" );
		return EGENERIC;
	}
#ifdef UNICODE
	WideCharToMultiByte(CP_ACP, 0, tchrFontfile, -1, psz_fontfile, sizeof(psz_fontfile), NULL, NULL);
#else
	strcpy(psz_fontfile, tchrFontfile);
#endif
    strcat_s( psz_fontfile, sizeof(psz_fontfile), "\\fonts\\arial.ttf" );

    i_error = FT_Init_FreeType( &m_pLibrary );
    if( i_error ) {
        _RPT0( _CRT_WARN, "couldn't initialize freetype\n" );
        goto error;
    }

	i_error = FT_New_Face( m_pLibrary, psz_fontfile ? psz_fontfile : "",
                           0, &m_pFace );
    if( i_error == FT_Err_Unknown_File_Format )
    {
        _RPT1( _CRT_WARN, "file %s have unknown format\n", psz_fontfile );
        goto error;
    }
    else if( i_error )
    {
        _RPT1( _CRT_WARN, "failed to load font file %s\n", psz_fontfile );
        goto error;
    }

    i_error = FT_Select_Charmap( m_pFace, ft_encoding_unicode );
    if( i_error )
    {
        _RPT0( _CRT_WARN, "font has no unicode translation table\n" );
        goto error;
    }

    m_iUseKerning = FT_HAS_KERNING( m_pFace );

    m_iDefaultFontSize = 0;
    if( SetFontSize( 0 ) != SUCCESS ) goto error;

	return SUCCESS;

 error:
	if( m_pFace ) { FT_Done_Face( m_pFace ); m_pFace = NULL; }
	if( m_pLibrary ) { FT_Done_FreeType( m_pLibrary ); m_pLibrary = NULL; }
	return EGENERIC;
}

/**
 * This function renders a text subpicture region into another one.
 * It also calculates the size needed for this string, and renders the
 * needed glyphs into memory. 
 */
int CFreetype::RenderText( subpicture_region_t *p_region_out,
                       subpicture_region_t *p_region_in )
{
    line_desc_t  *p_lines = 0, *p_line = 0, *p_next = 0, *p_prev = 0;
    int i, i_pen_y, i_pen_x, i_error, i_glyph_index, i_previous;
    WCHAR *psz_unicode, *psz_unicode_orig = 0, i_char, *psz_line_start;
	int i_unicode_size;
    int i_string_length;
    char *psz_string;
    int i_font_color, i_font_alpha, i_font_size, i_red, i_green, i_blue;

    FT_BBox line;
    FT_BBox glyph_size;
    FT_Vector result;
    FT_Glyph tmp_glyph;

	/* Sanity check */
    if( !p_region_in || !p_region_out ) return EGENERIC;
    psz_string = p_region_in->psz_text;
    if( !psz_string || !*psz_string ) return EGENERIC;

    if( p_region_in->p_style )
    {
        i_font_color = max( min( p_region_in->p_style->i_font_color, 0xFFFFFF ), 0 );
        i_font_alpha = max( min( p_region_in->p_style->i_font_alpha, 255 ), 0 );
        i_font_size  = max( min( p_region_in->p_style->i_font_size, 255 ), 0 );
    }
    else
    {
        i_font_color = m_iFontColor;
        i_font_alpha = 255 - m_iFontOpacity;
        i_font_size  = m_iDefaultFontSize;
    }

    if( i_font_color == 0xFFFFFF ) i_font_color = m_iFontColor;
    if( !i_font_alpha ) i_font_alpha = 255 - m_iFontOpacity;
    SetFontSize( i_font_size );

    i_red   = ( i_font_color & 0x00FF0000 ) >> 16;
    i_green = ( i_font_color & 0x0000FF00 ) >>  8;
    i_blue  =   i_font_color & 0x000000FF;

    result.x =  result.y = 0;
    line.xMin = line.xMax = line.yMin = line.yMax = 0;

	i_unicode_size = ( strlen(psz_string) + 1 ) * sizeof(WCHAR);
    psz_unicode = psz_unicode_orig =
        (LPWSTR)malloc( i_unicode_size );
    if( psz_unicode == NULL )
    {
        _RPT0( _CRT_WARN, "out of memory\n" );
        goto error;
    }

	i_string_length = MultiByteToWideChar(CP_ACP, 0, psz_string, strlen(psz_string), psz_unicode, i_unicode_size);
	if( i_string_length == 0 ) {    
		_RPT1( _CRT_WARN, "failed to convert string to unicode (%d)\n",
                          GetLastError() );
        goto error;      
	}
	psz_unicode[i_string_length] = 0;
        
	/* Calculate relative glyph positions and a bounding box for the
     * entire string */
    if( !(p_line = NewLine( (PBYTE)psz_string )) )
    {
        _RPT0( _CRT_WARN, "out of memory\n" );
        goto error;
    }
    p_lines = p_line;
    i_pen_x = i_pen_y = 0;
    i_previous = i = 0;
    psz_line_start = psz_unicode;

    while( *psz_unicode )
    {
        i_char = *psz_unicode++;
        if( i_char == '\r' ) /* ignore CR chars wherever they may be */
        {
            continue;
        }

        if( i_char == '\n' )
        {
            psz_line_start = psz_unicode;
            if( !(p_next = NewLine( (PBYTE)psz_string )) )
            {
                _RPT0( _CRT_WARN, "out of memory\n" );
                goto error;
            }
            p_line->p_next = p_next;
            p_line->i_width = line.xMax;
			p_line->i_height = m_pFace->size->metrics.height >> 6;
            p_line->pp_glyphs[ i ] = NULL;
            p_line->i_alpha = i_font_alpha;
            p_line->i_red = i_red;
            p_line->i_green = i_green;
            p_line->i_blue = i_blue;
            p_prev = p_line;
            p_line = p_next;
            result.x = max( result.x, line.xMax );
            result.y += m_pFace->size->metrics.height >> 6;
            i_pen_x = 0;
            i_previous = i = 0;
            line.xMin = line.xMax = line.yMin = line.yMax = 0;
            i_pen_y += m_pFace->size->metrics.height >> 6;
            continue;
        }

        i_glyph_index = FT_Get_Char_Index( m_pFace, i_char );
        if( m_iUseKerning && i_glyph_index
            && i_previous )
        {
            FT_Vector delta;
            FT_Get_Kerning( m_pFace, i_previous, i_glyph_index,
                            ft_kerning_default, &delta );
            i_pen_x += delta.x >> 6;

        }
        p_line->p_glyph_pos[ i ].x = i_pen_x;
        p_line->p_glyph_pos[ i ].y = i_pen_y;
        i_error = FT_Load_Glyph( m_pFace, i_glyph_index, FT_LOAD_DEFAULT );
        if( i_error )
        {
            _RPT1( _CRT_WARN, "unable to render text FT_Load_Glyph returned"
                               " %d", i_error );
            goto error;
        }
        i_error = FT_Get_Glyph( m_pFace->glyph, &tmp_glyph );
        if( i_error )
        {
            _RPT1( _CRT_WARN, "unable to render text FT_Get_Glyph returned "
                               "%d", i_error );
            goto error;
        }
        FT_Glyph_Get_CBox( tmp_glyph, ft_glyph_bbox_pixels, &glyph_size );
        i_error = FT_Glyph_To_Bitmap( &tmp_glyph, ft_render_mode_normal, 0, 1);
        if( i_error )
        {
            FT_Done_Glyph( tmp_glyph );
            continue;
        }
        p_line->pp_glyphs[ i ] = (FT_BitmapGlyph)tmp_glyph;

        /* Do rest */
        line.xMax = p_line->p_glyph_pos[i].x + glyph_size.xMax -
            glyph_size.xMin + ((FT_BitmapGlyph)tmp_glyph)->left;
        if( line.xMax > (int)m_fmt_out.video.i_visible_width - 20 )
        {
            p_line->pp_glyphs[ i ] = NULL;
            FreeLine( p_line );
            p_line = NewLine( (PBYTE)psz_string );
            if( p_prev ) p_prev->p_next = p_line;
            else p_lines = p_line;

            while( psz_unicode > psz_line_start && *psz_unicode != ' ' )
            {
                psz_unicode--;
            }
            if( psz_unicode == psz_line_start )
            {
                _RPT0( _CRT_WARN, "unbreakable string\n" );
                goto error;
            }
            else
            {
                *psz_unicode = '\n';
            }
            psz_unicode = psz_line_start;
            i_pen_x = 0;
            i_previous = i = 0;
            line.xMin = line.xMax = line.yMin = line.yMax = 0;
            continue;
        }
        line.yMax = max( line.yMax, glyph_size.yMax );
        line.yMin = min( line.yMin, glyph_size.yMin );

        i_previous = i_glyph_index;
        i_pen_x += m_pFace->glyph->advance.x >> 6;
        i++;
    }

    p_line->i_width = line.xMax;
    p_line->i_height = m_pFace->size->metrics.height >> 6;
    p_line->pp_glyphs[ i ] = NULL;
    p_line->i_alpha = i_font_alpha;
    p_line->i_red = i_red;
    p_line->i_green = i_green;
    p_line->i_blue = i_blue;
    result.x = max( result.x, line.xMax );
    result.y += line.yMax - line.yMin;

    p_region_out->i_x = p_region_in->i_x;
    p_region_out->i_y = p_region_in->i_y;

    RenderYUVA( p_region_out, p_lines, result.x, result.y );

    if( psz_unicode_orig ) free( psz_unicode_orig );
    FreeLines( p_lines );
    return SUCCESS;

 error:
    if( psz_unicode_orig ) free( psz_unicode_orig );
    FreeLines( p_lines );
    return EGENERIC;
}

/*****************************************************************************
 * Render: place string in picture
 *****************************************************************************
 * This function merges the previously rendered freetype glyphs into a picture
 *****************************************************************************/
int CFreetype::RenderYUVA( subpicture_region_t *p_region,
                   line_desc_t *p_line, int i_width, int i_height )
{
    BYTE *p_dst_y,*p_dst_u,*p_dst_v,*p_dst_a;
    video_format_t fmt;
    int i, x, y, i_pitch, i_alpha;
    BYTE i_y, i_u, i_v; /* YUV values, derived from incoming RGB */
    subpicture_region_t *p_region_tmp;

    if( i_width == 0 || i_height == 0 )
        return SUCCESS;

    /* Create a new subpicture region */
    memset( &fmt, 0, sizeof(video_format_t) );
    fmt.i_chroma = TME_FOURCC('Y','U','V','A');
    fmt.i_aspect = 0;
    fmt.i_width = fmt.i_visible_width = i_width + 6;
    fmt.i_height = fmt.i_visible_height = i_height + 6;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    p_region_tmp = spu_CreateRegion( &fmt );
    if( !p_region_tmp )
    {
        _RPT0( _CRT_WARN, "cannot allocate SPU region\n" );
        return EGENERIC;
    }

    p_region->fmt = p_region_tmp->fmt;
    p_region->picture = p_region_tmp->picture;
    free( p_region_tmp );

    /* Calculate text color components */
    i_y = (BYTE)min(abs( 2104 * p_line->i_red  + 4130 * p_line->i_green +
                      802 * p_line->i_blue + 4096 + 131072 ) >> 13, 235);
    i_u = (BYTE)min(abs( -1214 * p_line->i_red  + -2384 * p_line->i_green +
                     3598 * p_line->i_blue + 4096 + 1048576) >> 13, 240);
    i_v = (BYTE)min(abs( 3598 * p_line->i_red + -3013 * p_line->i_green +
                      -585 * p_line->i_blue + 4096 + 1048576) >> 13, 240);
    i_alpha = p_line->i_alpha;

    p_dst_y = p_region->picture.Y_PIXELS;
    p_dst_u = p_region->picture.U_PIXELS;
    p_dst_v = p_region->picture.V_PIXELS;
    p_dst_a = p_region->picture.A_PIXELS;
    i_pitch = p_region->picture.A_PITCH;

    /* Initialize the region pixels */
    if( m_iEffect != EFFECT_BACKGROUND )
    {
        memset( p_dst_y, 0x00, i_pitch * p_region->fmt.i_height );
        memset( p_dst_u, 0x80, i_pitch * p_region->fmt.i_height );
        memset( p_dst_v, 0x80, i_pitch * p_region->fmt.i_height );
        memset( p_dst_a, 0, i_pitch * p_region->fmt.i_height );
    }
    else
    {
        memset( p_dst_y, 0x0, i_pitch * p_region->fmt.i_height );
        memset( p_dst_u, 0x80, i_pitch * p_region->fmt.i_height );
        memset( p_dst_v, 0x80, i_pitch * p_region->fmt.i_height );
        memset( p_dst_a, 0x80, i_pitch * p_region->fmt.i_height );
    }
    if( m_iEffect == EFFECT_OUTLINE ||
        m_iEffect == EFFECT_OUTLINE_FAT )
    {
        DrawBlack( p_line, i_width, p_region,  0,  0);
        DrawBlack( p_line, i_width, p_region, -1,  0);
        DrawBlack( p_line, i_width, p_region,  0, -1);
        DrawBlack( p_line, i_width, p_region,  1,  0);
        DrawBlack( p_line, i_width, p_region,  0,  1);
    }

	if( m_iEffect == EFFECT_OUTLINE_FAT )
    {
        DrawBlack( p_line, i_width, p_region, -1, -1);
        DrawBlack( p_line, i_width, p_region, -1,  1);
        DrawBlack( p_line, i_width, p_region,  1, -1);
        DrawBlack( p_line, i_width, p_region,  1,  1);

        DrawBlack( p_line, i_width, p_region, -2,  0);
        DrawBlack( p_line, i_width, p_region,  0, -2);
        DrawBlack( p_line, i_width, p_region,  2,  0);
        DrawBlack( p_line, i_width, p_region,  0,  2);

        DrawBlack( p_line, i_width, p_region, -2, -2);
        DrawBlack( p_line, i_width, p_region, -2,  2);
        DrawBlack( p_line, i_width, p_region,  2, -2);
        DrawBlack( p_line, i_width, p_region,  2,  2);

        DrawBlack( p_line, i_width, p_region, -3,  0);
        DrawBlack( p_line, i_width, p_region,  0, -3);
        DrawBlack( p_line, i_width, p_region,  3,  0);
        DrawBlack( p_line, i_width, p_region,  0,  3);
    }

    for( ; p_line != NULL; p_line = p_line->p_next )
    {
        int i_glyph_tmax = 0;
        int i_bitmap_offset, i_offset, i_align_offset = 0;
        for( i = 0; p_line->pp_glyphs[i] != NULL; i++ )
        {
            FT_BitmapGlyph p_glyph = p_line->pp_glyphs[ i ];
            i_glyph_tmax = max( i_glyph_tmax, p_glyph->top );
        }

        if( p_line->i_width < i_width )
        {
            if( ( p_region->p_style && p_region->p_style->i_text_align == SUBPICTURE_ALIGN_RIGHT ) ||
                ( !p_region->p_style && (p_region->i_align & 0x3) == SUBPICTURE_ALIGN_RIGHT ) )
            {
                i_align_offset = i_width - p_line->i_width;
            }
            else if( ( p_region->p_style && p_region->p_style->i_text_align != SUBPICTURE_ALIGN_LEFT ) ||
                ( !p_region->p_style && (p_region->i_align & 0x3) != SUBPICTURE_ALIGN_LEFT ) )
            {
                i_align_offset = ( i_width - p_line->i_width ) / 2;
            }
        }

        for( i = 0; p_line->pp_glyphs[i] != NULL; i++ )
        {
            FT_BitmapGlyph p_glyph = p_line->pp_glyphs[ i ];

            i_offset = ( p_line->p_glyph_pos[ i ].y +
                i_glyph_tmax - p_glyph->top + 3 ) *
                i_pitch + p_line->p_glyph_pos[ i ].x + p_glyph->left + 3 +
                i_align_offset;

            for( y = 0, i_bitmap_offset = 0; y < p_glyph->bitmap.rows; y++ )
            {
                for( x = 0; x < p_glyph->bitmap.width; x++, i_bitmap_offset++ )
                {
                    if( p_glyph->bitmap.buffer[i_bitmap_offset] )
                    {
                        p_dst_y[i_offset+x] = ((p_dst_y[i_offset+x] *(255-(int)p_glyph->bitmap.buffer[i_bitmap_offset])) +
                                              i_y * ((int)p_glyph->bitmap.buffer[i_bitmap_offset])) >> 8;

                        p_dst_u[i_offset+x] = i_u;
                        p_dst_v[i_offset+x] = i_v;

                        if( m_iEffect == EFFECT_BACKGROUND )
                            p_dst_a[i_offset+x] = 0xff;
                    }
                }
                i_offset += i_pitch;
            }
        }
    }

    /* Apply the alpha setting */
    for( i = 0; i < (int)fmt.i_height * i_pitch; i++ )
        p_dst_a[i] = p_dst_a[i] * (255 - i_alpha) / 255;

    return SUCCESS;
}

int CFreetype::SetFontSize( int i_size )
{
    if( i_size && i_size == m_iFontSize ) return SUCCESS;

    if( !i_size )
    {
        if( !m_iDefaultFontSize &&
            m_iDisplayHeight == (int)m_fmt_out.video.i_height )
            return SUCCESS;

        if( m_iDefaultFontSize )
        {
            i_size = m_iDefaultFontSize;
        }
        else
        {
            i_size = (int)m_fmt_out.video.i_height / m_iRelFontSize;
            m_iDisplayHeight =
                m_fmt_out.video.i_height;
        }
        if( i_size <= 0 )
        {
            _RPT0( _CRT_WARN, "invalid fontsize, using 12\n" );
            i_size = 12;
        }

        _RPT1( _CRT_WARN, "using fontsize: %i\n", i_size );
    }

    m_iFontSize = i_size;

    if( FT_Set_Pixel_Sizes( m_pFace, 0, i_size ) )
    {
        _RPT1( _CRT_WARN, "couldn't set font size to %d\n", i_size );
        return EGENERIC;
    }

    return SUCCESS;
}

static void FreeLine( line_desc_t *p_line )
{
    unsigned int i;
    for( i = 0; p_line->pp_glyphs[ i ] != NULL; i++ )
    {
        FT_Done_Glyph( (FT_Glyph)p_line->pp_glyphs[ i ] );
    }
    free( p_line->pp_glyphs );
    free( p_line->p_glyph_pos );
    free( p_line );
}

static void FreeLines( line_desc_t *p_lines )
{
    line_desc_t *p_line, *p_next;

    if( !p_lines ) return;

    for( p_line = p_lines; p_line != NULL; p_line = p_next )
    {
        p_next = p_line->p_next;
        FreeLine( p_line );
    }
}
					   
static line_desc_t *NewLine( PBYTE psz_string )
{
    int i_count;
    line_desc_t *p_line = (line_desc_t *)malloc( sizeof(line_desc_t) );

    if( !p_line ) return NULL;
    p_line->i_height = 0;
    p_line->i_width = 0;
    p_line->p_next = NULL;

    /* We don't use CountUtf8Characters() here because we are not acutally
     * sure the string is utf8. Better be safe than sorry. */
    i_count = strlen( (char *)psz_string );

    p_line->pp_glyphs = (FT_BitmapGlyph *)malloc( sizeof(FT_BitmapGlyph) * ( i_count + 1 ) );
    if( p_line->pp_glyphs == NULL )
    {
        free( p_line );
        return NULL;
    }
    p_line->pp_glyphs[0] = NULL;

    p_line->p_glyph_pos = (FT_Vector *)malloc( sizeof( FT_Vector ) * i_count + 1 );
    if( p_line->p_glyph_pos == NULL )
    {
        free( p_line->pp_glyphs );
        free( p_line );
        return NULL;
    }

    return p_line;
}

static void DrawBlack( line_desc_t *p_line, int i_width, subpicture_region_t *p_region, int xoffset, int yoffset )
{
    BYTE *p_dst = p_region->picture.A_PIXELS;
    int i_pitch = p_region->picture.A_PITCH;
    int x,y;

    for( ; p_line != NULL; p_line = p_line->p_next )
    {
        int i_glyph_tmax=0, i = 0;
        int i_bitmap_offset, i_offset, i_align_offset = 0;
        for( i = 0; p_line->pp_glyphs[i] != NULL; i++ )
        {
            FT_BitmapGlyph p_glyph = p_line->pp_glyphs[ i ];
            i_glyph_tmax = max( i_glyph_tmax, p_glyph->top );
        }

        if( p_line->i_width < i_width )
        {
            if( ( p_region->p_style && p_region->p_style->i_text_align == SUBPICTURE_ALIGN_RIGHT ) ||
                ( !p_region->p_style && (p_region->i_align & 0x3) == SUBPICTURE_ALIGN_RIGHT ) )
            {
                i_align_offset = i_width - p_line->i_width;
            }
            else if( ( p_region->p_style && p_region->p_style->i_text_align != SUBPICTURE_ALIGN_LEFT ) ||
                ( !p_region->p_style && (p_region->i_align & 0x3) != SUBPICTURE_ALIGN_LEFT ) )
            {
                i_align_offset = ( i_width - p_line->i_width ) / 2;
            }
        }

        for( i = 0; p_line->pp_glyphs[i] != NULL; i++ )
        {
            FT_BitmapGlyph p_glyph = p_line->pp_glyphs[ i ];

            i_offset = ( p_line->p_glyph_pos[ i ].y +
                i_glyph_tmax - p_glyph->top + 3 + yoffset ) *
                i_pitch + p_line->p_glyph_pos[ i ].x + p_glyph->left + 3 +
                i_align_offset +xoffset;

            for( y = 0, i_bitmap_offset = 0; y < p_glyph->bitmap.rows; y++ )
            {
                for( x = 0; x < p_glyph->bitmap.width; x++, i_bitmap_offset++ )
                {
                    if( p_glyph->bitmap.buffer[i_bitmap_offset] )
                        if( p_dst[i_offset+x] <
                            ((int)p_glyph->bitmap.buffer[i_bitmap_offset]) )
                            p_dst[i_offset+x] =
                                ((int)p_glyph->bitmap.buffer[i_bitmap_offset]);
                }
                i_offset += i_pitch;
            }
        }
    }
    
}
