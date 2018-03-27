#ifndef _SUBS_H_
#define _SUBS_H_

#include "decoder.h"

class CSubs : public CDecoder
{
public:
	CSubs(tme_object_t *pIpc, HWND hwnd, es_format_t *fmt, CInputStream *pInput, bool bOwnThread = true);
	virtual ~CSubs();

	virtual int OpenDecoder(); // should be called after the constructor of the derived class
	virtual picture_t *DecodeVideo( block_t **pp_block );
	virtual aout_buffer_t *DecodeAudio( block_t **pp_block );
	virtual subpicture_t *DecodeSubtitle( block_t **pp_block );

protected:
	int m_iAlign;
	subpicture_t *ParseText( block_t *p_block );


};

#endif