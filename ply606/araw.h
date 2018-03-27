#ifndef _ARAW_H_
#define _ARAW_H_

#include "decoder.h"
#include "audio_output.h"

// this test will save audio data in PCM (S16LE,32000Hz)
//#define AUDIO_DECODE_TEST

class CARaw : public CDecoder
{
public:
	CARaw(tme_object_t *pIpc, HWND hwnd, es_format_t *fmt, CInputStream *pInput);
	virtual ~CARaw();

	virtual int OpenDecoder(); // should be called after the constructor of the derived class
	virtual picture_t *DecodeVideo( block_t **pp_block );
	virtual aout_buffer_t *DecodeAudio( block_t **pp_block );
	virtual subpicture_t *DecodeSubtitle( block_t **pp_block );

protected:
    INT16 *m_pLogtos16;  /* used with m/alaw to int16 */
    audio_date_t m_endDate;
#ifdef AUDIO_DECODE_TEST
	FILE *m_fp;
#endif


};

#endif