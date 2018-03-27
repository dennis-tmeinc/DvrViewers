#include "stdafx.h"
#include <crtdbg.h>
#include "subs.h"
#include "inputstream.h"

CSubs::CSubs(tme_object_t *pIpc, HWND hwnd, es_format_t *fmt, CInputStream *pInput, bool bOwnThread)
	: CDecoder(pIpc, hwnd, fmt, pInput)
{
	m_bOwnThread = bOwnThread;
	m_iAlign = 0;
}

CSubs::~CSubs()
{
	_RPT0(_CRT_WARN, "~CSubs\n");
	DeleteDecoder2();
}


int CSubs::OpenDecoder()
{
    if( m_fmtIn.i_codec != TME_FOURCC('s','u','b','t') )
    {
        return EGENERIC;
    }

	m_iAlign = 1; /* 0: center, 1: left, 2: right */

	return StartThread(); // call this at the end of OpenDecoder()
}

subpicture_t *CSubs::DecodeSubtitle( block_t **pp_block ) {
    subpicture_t *p_spu = NULL;

    if( !pp_block || *pp_block == NULL ) return NULL;

    if( (*pp_block)->i_flags & BLOCK_FLAG_DISCONTINUITY ) {
        block_Release( *pp_block );
        return NULL;
    }

		p_spu = ParseText( *pp_block );

    block_Release( *pp_block );
    *pp_block = NULL;

    return p_spu;
}

picture_t *CSubs::DecodeVideo( block_t **pp_block ) {
	return NULL;
}

aout_buffer_t *CSubs::DecodeAudio( block_t **pp_block )
{
	return NULL;
}

subpicture_t *CSubs::ParseText( block_t *p_block )
{
    subpicture_t *p_spu = NULL;
    char *psz_subtitle = NULL;
    video_format_t fmt;

    /* We cannot display a subpicture with no date */
    if( p_block->i_pts == 0 )
    {
        _RPT0(_CRT_WARN, "subtitle without a date\n" );
        return NULL;
    }

    /* Check validity of packet data */
    /* An "empty" line containing only \0 can be used to force
       and ephemer picture from the screen */
    if( p_block->i_buffer < 4 )
    {
        _RPT0(_CRT_WARN, "no subtitle data\n" );
        return NULL;
    }

    /* Should be resiliant against bad subtitles */
    psz_subtitle = (char *)malloc(p_block->i_buffer - 4 + 1);
	if (psz_subtitle) {
		memcpy(psz_subtitle, (const char *)p_block->p_buffer + 4,                
				p_block->i_buffer - 4);
		psz_subtitle[p_block->i_buffer - 4] = 0;
	}
    if( psz_subtitle == NULL )
        return NULL;
    /* Create the subpicture unit */
    p_spu = SpuNewBuffer();
    if( !p_spu )
    {
        _RPT0(_CRT_WARN, "can't get spu buffer\n" );
        if( psz_subtitle ) free( psz_subtitle );
        return NULL;
    }

    p_spu->b_pausable = true;

    /* Create a new subpicture region */
    memset( &fmt, 0, sizeof(video_format_t) );
    fmt.i_chroma = TME_FOURCC('T','E','X','T');
    fmt.i_aspect = 0;
    fmt.i_width = fmt.i_height = 0;
    fmt.i_x_offset = fmt.i_y_offset = 0;

	p_spu->p_region = spu_CreateRegion(&fmt);
    if( !p_spu->p_region )
    {
        _RPT0(_CRT_WARN, "cannot allocate SPU region\n" );
        if( psz_subtitle ) free( psz_subtitle );
        SpuDelBuffer( p_spu );
        return NULL;
    }

    /* Normal text subs, easy markup */
    //p_spu->i_flags = SUBPICTURE_ALIGN_BOTTOM | m_iAlign;
	int aline_horizontal = p_block->p_buffer[0] & 0x03;
	int aline_vertical = p_block->p_buffer[0] & 0x0c;
	if (aline_vertical == 0) aline_vertical = SUBPICTURE_ALIGN_TOP;
	p_spu->i_flags = aline_vertical | aline_horizontal;
	p_spu->i_x = p_block->p_buffer[1];
    p_spu->i_y = p_block->p_buffer[2];

    p_spu->p_region->psz_text = psz_subtitle;
    p_spu->i_start = p_block->i_pts;
    p_spu->i_stop = p_block->i_pts + p_block->i_length;
    p_spu->b_ephemer = (p_block->i_length == 0);
    p_spu->b_absolute = false;

	return p_spu;
}
