#ifndef _DECODER_H
#define _DECODER_H

#include "tmeviewdef.h"
#include "es.h"
#include "block.h"
#include "video.h"
#include "vout_picture.h"
//#include "directx.h"
#include "audio_output.h"
#include "aout_internal.h"

class CInputStream;

class CDecoder
{
public:
	CDecoder(tme_object_t *pIpc, HWND hwnd, es_format_t *fmt, CInputStream *pInput);
	virtual ~CDecoder();
	virtual picture_t *DecodeVideo( block_t **pp_block ) = 0;
	virtual aout_buffer_t *DecodeAudio( block_t **pp_block ) = 0;
	virtual subpicture_t *DecodeSubtitle( block_t **pp_block ) = 0;
	virtual int OpenDecoder() = 0;

	int StartThread(); // should be called at the end of the OpenDecoder in the derived class
	int DecoderData(block_t *p_block);

	// functions used by derived classes
	picture_t *VoutNewBuffer(bool use_vout_mem = false); // also accessed by ffmpeg callback
	void VoutDelBuffer( picture_t *p_pic );
	void VoutLinkPicture( picture_t *p_pic );
	void VoutUninkPicture( picture_t *p_pic );
	aout_buffer_t *AoutNewBuffer( int i_samples );
	void AoutDelBuffer( aout_buffer_t *p_buffer );
	subpicture_t *SpuNewBuffer();
	void SpuDelBuffer( subpicture_t *p_subpic );


    // Input format ie from demuxer (XXX: a lot of field could be invalid)
    es_format_t         m_fmtIn;
    // Output format of decoder/packetizer
    es_format_t         m_fmtOut;

//protected:
	HWND m_hwnd;
	HANDLE m_hThread;
	bool m_bDie, m_bError;
	bool m_bPause;
	int m_ch;

	/* Some decoders only accept packetized data (ie. not truncated) */
    bool          m_bNeedPacketized;
    // Tell the decoder if it is allowed to drop frames
    bool          m_bPaceControl;

    CInputStream  *m_pInput;
	bool      m_bOwnThread;
    INT64         m_iPrerollEnd;

    CAudioOutput *m_pAout;
    aout_input_t    *m_pAoutInput;

	CVideoOutput *m_pVout;

    CVideoOutput *m_pSpuVout; // NULL until SpuNewBuffer() is called
    int              m_iSpuChannel;

    // Current format in use by the output
    video_format_t m_video;
    audio_format_t m_audio;

    // fifo
    block_fifo_t *m_pFifo;


	tme_object_t *m_pIpc;

	int DecoderThread();
	int DecoderDecode(block_t *p_block);
	void DeleteDecoder();
	void DeleteDecoder2();
	void DecoderDiscontinuity();
	bool DecoderEmpty();
	void DecoderPreroll( INT64 i_preroll_end );
};


#endif