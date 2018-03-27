#include "stdafx.h"
#include <crtdbg.h>
#include <errno.h>
#include <process.h>
#include "ffmpeg.h"
#include "mtime.h"
#include "inputstream.h"
#include "trace.h"

#pragma comment(lib, "avformat-51.lib")
#pragma comment(lib, "avcodec-51.lib")
#pragma comment(lib, "avutil-49.lib")

// static members
bool CFFmpeg::m_bFFmpegInit = false;
bool CFFmpeg::m_bFFmpegInit2 = false;
//tme_object_t CFFmpeg::m_ipc;    
tme_mutex_t CFFmpeg::m_lock;


/* (dummy palette for now) */
static AVPaletteControl palette_control;

int GetFfmpegCodec( fourcc_t i_fourcc, int *pi_cat,
									 int *pi_ffmpeg_codec, char **ppsz_name );
static void ffmpeg_CopyPicture( CFFmpeg *p_dec,
															 picture_t *p_pic, AVFrame *p_ff_pic );
static int ffmpeg_GetFrameBuf( struct AVCodecContext *p_context,
															AVFrame *p_ff_pic );
static void ffmpeg_ReleaseFrameBuf( struct AVCodecContext *p_context,
																	 AVFrame *p_ff_pic );

static UINT32 ffmpeg_CodecTag( fourcc_t fcc )
{
	UINT8 *p = (UINT8*)&fcc;
	return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static UINT32 ffmpeg_PixFmtToChroma( int i_ff_chroma )
{
	switch( i_ff_chroma )
	{
	case PIX_FMT_YUV420P:
	case PIX_FMT_YUVJ420P: /* Hacky but better then chroma conversion */
		return TME_FOURCC('I','4','2','0');
	case PIX_FMT_YUV422P:
	case PIX_FMT_YUVJ422P: /* Hacky but better then chroma conversion */
		return TME_FOURCC('I','4','2','2');
	case PIX_FMT_YUV444P:
	case PIX_FMT_YUVJ444P: /* Hacky but better then chroma conversion */
		return TME_FOURCC('I','4','4','4');

	case PIX_FMT_YUV422:
		return TME_FOURCC('Y','U','Y','2');

	case PIX_FMT_RGB555:
		return TME_FOURCC('R','V','1','5');
	case PIX_FMT_RGB565:
		return TME_FOURCC('R','V','1','6');
	case PIX_FMT_RGB24:
		return TME_FOURCC('R','V','2','4');
	case PIX_FMT_RGBA32:
		return TME_FOURCC('R','V','3','2');
	case PIX_FMT_GRAY8:
		return TME_FOURCC('G','R','E','Y');

	case PIX_FMT_YUV410P:
	case PIX_FMT_YUV411P:
	case PIX_FMT_BGR24:
	default:
		return 0;
	}
}

/* Returns a new picture buffer */
static inline picture_t *ffmpeg_NewPictBuf( CFFmpeg *p_dec,
																					 AVCodecContext *p_context )
{
	video_sys_t *p_sys = p_dec->m_pVideoDec;
	picture_t *p_pic;

	p_dec->m_fmtOut.video.i_width = p_context->width;
	p_dec->m_fmtOut.video.i_height = p_context->height;
	p_dec->m_fmtOut.i_codec = ffmpeg_PixFmtToChroma( p_context->pix_fmt );

	if( !p_context->width || !p_context->height )
	{
		return NULL; /* invalid display size */
	}

	if( !p_dec->m_fmtOut.i_codec )
	{
		/* we make conversion if possible*/
		p_dec->m_fmtOut.i_codec = TME_FOURCC('I','4','2','0');
	}

	/* If an aspect-ratio was specified in the input format then force it */
	if( p_dec->m_fmtIn.video.i_aspect )
	{
		p_dec->m_fmtOut.video.i_aspect = p_dec->m_fmtIn.video.i_aspect;
	}
	else
	{
		p_dec->m_fmtOut.video.i_aspect =
			(unsigned int)(VOUT_ASPECT_FACTOR * ( av_q2d(p_context->sample_aspect_ratio) *
			p_context->width / p_context->height ));
		p_dec->m_fmtOut.video.i_sar_num = p_context->sample_aspect_ratio.num;
		p_dec->m_fmtOut.video.i_sar_den = p_context->sample_aspect_ratio.den;
		if( p_dec->m_fmtOut.video.i_aspect == 0 )
		{
			p_dec->m_fmtOut.video.i_aspect =
				VOUT_ASPECT_FACTOR * p_context->width / p_context->height;
		}
	}

	if( p_dec->m_fmtOut.video.i_frame_rate > 0 &&
		p_dec->m_fmtOut.video.i_frame_rate_base > 0 )
	{
		p_dec->m_fmtOut.video.i_frame_rate =
			p_dec->m_fmtIn.video.i_frame_rate;
		p_dec->m_fmtOut.video.i_frame_rate_base =
			p_dec->m_fmtOut.video.i_frame_rate_base; // hyun: using m_fmtIn ?
	}
	else
		if( p_context->time_base.num > 0 && p_context->time_base.den > 0 )
		{
			p_dec->m_fmtOut.video.i_frame_rate = p_context->time_base.den;
			p_dec->m_fmtOut.video.i_frame_rate_base = p_context->time_base.num;
		}

		p_pic = p_dec->VoutNewBuffer(!p_dec->m_bOwnThread);

		return p_pic;
}

CFFmpeg::CFFmpeg(tme_object_t *pIpc, HWND hwnd, es_format_t *fmt, CInputStream *pInput, bool initializeAvcodec, bool bOwnThread)
: CDecoder(pIpc, hwnd, fmt, pInput), m_initializeAvcodec(initializeAvcodec)
{
	_RPT0(_CRT_WARN, "CFFmpeg()\n");
	m_bOwnThread = bOwnThread;
	if (!m_bFFmpegInit2) {
		//tme_thread_init(&m_ipc);
		tme_mutex_init(m_pIpc, &m_lock);
		m_bFFmpegInit2 = true;
		_RPT0(_CRT_WARN, "CFFmpeg mutex initialized\n");
	}
	m_pVideoDec = NULL;
}

CFFmpeg::~CFFmpeg()
{
	_RPT0(_CRT_WARN, "~CFFmpeg()\n");
	DeleteDecoder2();

	if (m_pVideoDec) {
		if (m_pVideoDec->p_ff_pic)
			av_free(m_pVideoDec->p_ff_pic);
		free(m_pVideoDec->p_buffer_orig);

		if( m_pVideoDec->p_context->extradata )
			free( m_pVideoDec->p_context->extradata );
		m_pVideoDec->p_context->extradata = NULL;
		tme_mutex_lock(&m_lock);
		avcodec_close( m_pVideoDec->p_context );
		_RPT1(_CRT_WARN, "ffmpeg codec (%s) stopped\n", m_pVideoDec->psz_namecodec );
		av_free( m_pVideoDec->p_context );
		tme_mutex_unlock(&m_lock);
		free(m_pVideoDec);
	}
}


int CFFmpeg::OpenDecoder()
{
	int i_cat, i_result;
	int i_codec_id;
	char *psz_namecodec;

	AVCodecContext *p_context = NULL;
	AVCodec        *p_codec = NULL;

	/* *** determine codec type *** */
	if( GetFfmpegCodec( m_fmtIn.i_codec, &i_cat, &i_codec_id, &psz_namecodec ) )
	{
		return EGENERIC;
	}

	/* Bail out if buggy decoder */
	if( i_codec_id == CODEC_ID_AAC )
	{
		_RPT1(_CRT_WARN, "refusing to use ffmpeg's (%s) decoder which is buggy",
			psz_namecodec );
		return EGENERIC;
	}

	if (m_initializeAvcodec) // always false in plywisch
		InitLibavcodec();

	tme_mutex_lock(&m_lock);
	p_codec = avcodec_find_decoder( (enum CodecID)i_codec_id );

	if( !p_codec )
	{
		tme_mutex_unlock(&m_lock);
		_RPT1(_CRT_WARN, "codec not found (%s)\n", psz_namecodec );
		return EGENERIC;
	}

	/* *** get a p_context *** */
	p_context = avcodec_alloc_context();
	tme_mutex_unlock(&m_lock);
	if( !p_context )
		return ENOMEM;

	p_context->debug = 0;

	switch( i_cat )
	{
	case VIDEO_ES:
		m_bNeedPacketized = true;
		i_result = InitVideoDec(p_context, p_codec, i_codec_id, psz_namecodec);
		break;
	case AUDIO_ES:
		i_result = EGENERIC;
		break;
	default:
		i_result = EGENERIC;
	}

	if( i_result == SUCCESS ) {
		m_pVideoDec->i_cat = i_cat;

		i_result = StartThread(); // call this at the end of OpenDecoder()
	}
	return i_result;

}

int CFFmpeg::InitVideoDec(AVCodecContext *p_context, AVCodec *p_codec, 
													int i_codec_id, char *psz_namecodec)
{
	video_sys_t *p_sys;

	/* Allocate the memory needed to store the decoder's structure */
	if( ( m_pVideoDec = p_sys =
		(video_sys_t *)malloc(sizeof(video_sys_t)) ) == NULL )
	{
		_RPT0(_CRT_WARN, "out of memory\n" );
		return EGENERIC;
	}

	ZeroMemory(p_sys, sizeof(video_sys_t));

	m_pVideoDec->p_context = p_context;
	m_pVideoDec->p_codec = p_codec;
	m_pVideoDec->i_codec_id = i_codec_id;
	m_pVideoDec->psz_namecodec = psz_namecodec;
	tme_mutex_lock(&m_lock);
	p_sys->p_ff_pic = avcodec_alloc_frame();
	tme_mutex_unlock(&m_lock);

	/* ***** Fill p_context with init values ***** */
	p_sys->p_context->codec_tag = ffmpeg_CodecTag( m_fmtIn.i_codec );
	p_sys->p_context->width  = m_fmtIn.video.i_width;
	p_sys->p_context->height = m_fmtIn.video.i_height;
	p_sys->p_context->bits_per_sample = m_fmtIn.video.i_bits_per_pixel;

	/*  ***** Get configuration of ffmpeg plugin ***** */
	p_sys->p_context->workaround_bugs = 1;
	p_sys->p_context->error_resilience = FF_ER_CAREFUL;

	p_sys->b_hurry_up = 0;

	/* ***** ffmpeg direct rendering ***** */
	p_sys->b_direct_rendering = 0;
	if( (p_sys->p_codec->capabilities & CODEC_CAP_DR1) &&
		/* Apparently direct rendering doesn't work with YUV422P */
		p_sys->p_context->pix_fmt != PIX_FMT_YUV422P &&
		/* H264 uses too many reference frames */
		i_codec_id != CODEC_ID_H264 &&
		!p_sys->p_context->debug_mv )
	{
		/* Some codecs set pix_fmt only after the 1st frame has been decoded,
		* so we need to do another check in ffmpeg_GetFrameBuf() */
		p_sys->b_direct_rendering = 1;
	}

	if( p_sys->b_direct_rendering )
	{
		_RPT0(_CRT_WARN, "using direct rendering (ffmpeg)\n" );
		p_sys->p_context->flags |= CODEC_FLAG_EMU_EDGE;
	}

	/* Always use our get_buffer wrapper so we can calculate the
	* PTS correctly */
	p_sys->p_context->get_buffer = ffmpeg_GetFrameBuf;
	p_sys->p_context->release_buffer = ffmpeg_ReleaseFrameBuf;
	p_sys->p_context->opaque = this;

	/* ***** init this codec with special data ***** */
	InitVideoCodec();

	/* ***** misc init ***** */
	p_sys->input_pts = p_sys->input_dts = 0;
	p_sys->i_pts = 0;
	p_sys->b_has_b_frames = false;
	p_sys->b_first_frame = true;
	p_sys->i_late_frames = 0;
	p_sys->i_buffer = 0;
	p_sys->i_buffer_orig = 1;
	p_sys->p_buffer_orig = p_sys->p_buffer = (char *)malloc( p_sys->i_buffer_orig );

	/* Set output properties */
	m_fmtOut.i_cat = VIDEO_ES;
	m_fmtOut.i_codec = ffmpeg_PixFmtToChroma( p_context->pix_fmt );

	/* Setup palette */
	if( m_fmtIn.video.p_palette )
		m_pVideoDec->p_context->palctrl =
		(AVPaletteControl *)m_fmtIn.video.p_palette;
	else
		m_pVideoDec->p_context->palctrl = &palette_control;

	/* ***** Open the codec ***** */
	tme_mutex_lock(&m_lock);
	//tme_mutex_lock( lockval.p_address );
	if( avcodec_open( m_pVideoDec->p_context, m_pVideoDec->p_codec ) < 0 )
	{
		//tme_mutex_unlock( lockval.p_address );
		tme_mutex_unlock(&m_lock);
		_RPT1(_CRT_WARN, "cannot open codec (%s)\n", p_sys->psz_namecodec );
		free( m_pVideoDec );
		return EGENERIC;
	}
	//tme_mutex_unlock( lockval.p_address );
	tme_mutex_unlock(&m_lock);
	_RPT1(_CRT_WARN, "ffmpeg codec (%s) started\n", p_sys->psz_namecodec );

	return SUCCESS;
}

/*****************************************************************************
* DecodeVideo: Called to decode one or more frames
*              Called from CDecoder's thread until we return NULL
*    1. we call avcodec_decode_video()
*    2. avcodec calls callback "ffmpeg_GetFrameBuf()"
*       in this call back, we set pts
*    3. and we continue after returning from avcodec_decode_video()
*****************************************************************************/
picture_t *CFFmpeg::DecodeVideo( block_t **pp_block )
{
	int b_drawpicture;
	int b_null_size = FALSE;
	block_t *p_block;

	if( !pp_block || !*pp_block ) return NULL;
	//TRACE(_T("1.DecodeVideo-ch:%d,late_frames_start:%I64d,sys->pts:%I64d,size:%d\n"),m_ch,m_pVideoDec->i_late_frames_start,m_pVideoDec->i_pts,(*pp_block)->i_buffer);

	if( !m_pVideoDec->p_context->extradata_size && m_fmtIn.i_extra )
		InitVideoCodec();

	p_block = *pp_block;

	//TRACE(_T("****************ch:%d,p_block->i_rate:%d, dts:%I64d,size:%d(%x)\n"),m_ch,p_block->i_rate, p_block->i_dts,p_block->i_buffer,p_block->i_flags);

	if( p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) )
	{
		TRACE(_T("ch:%d, BLOCK_FLAG_DISCONTINUITY | BLOCK_FLAG_CORRUPTED : returning\n"), m_ch);
		m_pVideoDec->i_buffer = 0;
		m_pVideoDec->i_pts = 0; /* To make sure we recover properly */

		m_pVideoDec->input_pts = m_pVideoDec->input_dts = 0;
		m_pVideoDec->i_late_frames = 0;

		block_Release( p_block );
		return NULL;
	}

	if( p_block->i_flags & BLOCK_FLAG_PREROLL )
	{
		/* Do not care about late frames when prerolling
		* TODO avoid decoding of non reference frame
		* (ie all B except for H264 where it depends only on nal_ref_idc) */
		m_pVideoDec->i_late_frames = 0;
	}

	if( !m_bPaceControl && m_pVideoDec->i_late_frames > 0 &&
		mdate() - m_pVideoDec->i_late_frames_start > 5000000LL )
	{
		//_RPT1(_CRT_WARN, "2.late video > 5:now:%I64d\n",mdate());
		if( m_pVideoDec->i_pts )
		{
			TRACE(_T("ch:%d,more than 5 seconds of late video -> dropping frame (computer too slow ?)\n"), m_ch);
			m_pVideoDec->i_pts = 0; /* To make sure we recover properly */
		}
		block_Release( p_block );
		m_pVideoDec->i_late_frames--;
		return NULL;
	}

	if( p_block->i_pts > 0 || p_block->i_dts > 0 )
	{
		m_pVideoDec->input_pts = p_block->i_pts;
		m_pVideoDec->input_dts = p_block->i_dts;
		//_RPT2(_CRT_WARN, "3.DecodeVideo-input_pts:%I64d,input_dts:%I64d\n",m_pVideoDec->input_pts,m_pVideoDec->input_dts);

		/* Make sure we don't reuse the same timestamps twice */
		p_block->i_pts = p_block->i_dts = 0;
	}

	/* TODO implement it in a better way */
	/* A good idea could be to decode all I pictures and see for the other */
	if( !m_bPaceControl &&
		m_pVideoDec->b_hurry_up && m_pVideoDec->i_late_frames > 4 )
	{
		b_drawpicture = 0;
		if( m_pVideoDec->i_late_frames < 8 )
		{
			m_pVideoDec->p_context->hurry_up = 2;
		}
		else
		{
			/* picture too late, won't decode
			* but break picture until a new I, and for mpeg4 ...*/

			TRACE(_T("ch:%d, picture too late\n"), m_ch);
			m_pVideoDec->i_late_frames--; /* needed else it will never be decrease */
			block_Release( p_block );
			m_pVideoDec->i_buffer = 0;
			return NULL;
		}
	}
	else
	{
		if (!(p_block->i_flags & BLOCK_FLAG_PREROLL))
		{
			b_drawpicture = 1;
			m_pVideoDec->p_context->hurry_up = 0;
		}
		else
		{
			b_drawpicture = 0;
			m_pVideoDec->p_context->hurry_up = 1;
		}
	}

	if( m_pVideoDec->p_context->width <= 0 || m_pVideoDec->p_context->height <= 0 )
	{
		m_pVideoDec->p_context->hurry_up = 5;
		b_null_size = TRUE;
	}

	video_sys_t *p_sys = m_pVideoDec;
	/*
	* Do the actual decoding now
	*/

	/* Check if post-processing was enabled */
	p_sys->b_pp = p_sys->b_pp_async;

	/* when there is no more frames to decode, 
	* we have here p_block->i_buffer == 0, and p_sys->i_buffer == 0
	* so we don't enter decoding loop at all and return 0 at the end of this function
	*/

	/* Don't forget that ffmpeg requires a little more bytes
	* that the real frame size */
	if( p_block->i_buffer > 0 )
	{
		PBYTE p_buf = p_block->p_buffer + p_block->i_osd ;
		int i_buf = p_block->i_buffer - p_block->i_osd;
		p_sys->i_buffer = i_buf;
		if( p_sys->i_buffer + FF_INPUT_BUFFER_PADDING_SIZE >
			p_sys->i_buffer_orig )
		{
			free( p_sys->p_buffer_orig );
			p_sys->i_buffer_orig =
				p_block->i_buffer + FF_INPUT_BUFFER_PADDING_SIZE;
			p_sys->p_buffer_orig = (char *)malloc( p_sys->i_buffer_orig );
		}
		p_sys->p_buffer = p_sys->p_buffer_orig;
		p_sys->i_buffer = i_buf;
		memcpy( p_sys->p_buffer, p_buf, i_buf );
		memset( p_sys->p_buffer + i_buf, 0,
			FF_INPUT_BUFFER_PADDING_SIZE );

		p_block->i_buffer = 0;
		p_block->i_osd = 0;
	}

	while( p_sys->i_buffer > 0 )
	{
		int i_used, b_gotpicture;
		picture_t *p_pic;

		//TRACE(_T("ch:%d,buf:%d\n"),m_ch, p_sys->i_buffer);
		//dump((uint8_t*)p_sys->p_buffer, 32);
		//dump((uint8_t*)p_sys->p_buffer + p_sys->i_buffer - 16, 16);
		tme_mutex_lock(&m_lock);
		i_used = avcodec_decode_video( p_sys->p_context, p_sys->p_ff_pic,
			&b_gotpicture,
			(uint8_t*)p_sys->p_buffer, p_sys->i_buffer );

		if( b_null_size && p_sys->p_context->width > 0 &&
			p_sys->p_context->height > 0 )
		{
			/* Reparse it to not drop the I frame */
			b_null_size = FALSE;
			p_sys->p_context->hurry_up = 0;
			i_used = avcodec_decode_video( p_sys->p_context, p_sys->p_ff_pic,
				&b_gotpicture,
				(uint8_t*)p_sys->p_buffer, p_sys->i_buffer );
		}
		tme_mutex_unlock(&m_lock);

		//TRACE(_T("ch:%d,4.avcodec_decode_video:%d,b_gotpicture:%d\n"), m_ch,i_used,b_gotpicture);

		if( i_used < 0 )
		{
			TRACE(_T("ch:%d,cannot decode one frame (%d bytes)\n"),m_ch,p_sys->i_buffer );
			block_Release( p_block );
			return NULL;
		}
		else if( i_used > p_sys->i_buffer )
		{
			i_used = p_sys->i_buffer;
		}


		/* Consumed bytes */
		p_sys->i_buffer -= i_used;
		p_sys->p_buffer += i_used;

		/* Nothing to display */
		if( !b_gotpicture )
		{
			if( i_used == 0 ) break;
			continue;
		}

		/* Update frame late count (except when doing preroll) */
		if( p_sys->i_pts && p_sys->i_pts <= mdate() &&
			!(p_block->i_flags & BLOCK_FLAG_PREROLL) )
		{
			/* Don't check in slow play */
			if (p_block->i_rate <= INPUT_RATE_DEFAULT) {
				p_sys->i_late_frames++;
				if( p_sys->i_late_frames == 1 )
					p_sys->i_late_frames_start = mdate();
				//_RPT3(_CRT_WARN, "5.late_frame:%d,starts(now):%I64d,pts:%I64d\n",
				//	p_sys->i_late_frames,p_sys->i_late_frames_start,p_sys->i_pts);
			}
		}
		else
		{
			p_sys->i_late_frames = 0;
		}

		if( !b_drawpicture || !p_sys->p_ff_pic->linesize[0] )
		{
			/* Do not display the picture */
			continue;
		}

		if( !p_sys->p_ff_pic->opaque )
		{
			/* Get a new picture */
			p_pic = ffmpeg_NewPictBuf( this, p_sys->p_context );
			if( !p_pic )
			{
				block_Release( p_block );
				return NULL;
			}

			/* Fill p_picture_t from AVVideoFrame and do chroma conversion
			* if needed */
			ffmpeg_CopyPicture( this, p_pic, p_sys->p_ff_pic );
		}
		else
		{
			p_pic = (picture_t *)p_sys->p_ff_pic->opaque;
		}

		/* Set the PTS to the picture PTS from avcodec library*/
		if( p_sys->p_ff_pic->pts ) {
			//_RPT2(_CRT_WARN, "6.set decoder's pts to:%I64d(diff:%I64d)\n", p_sys->p_ff_pic->pts,p_sys->p_ff_pic->pts-p_sys->i_pts);
			p_sys->i_pts = p_sys->p_ff_pic->pts;
		}

		/* Sanity check (seems to be needed for some streams) */
		if( p_sys->p_ff_pic->pict_type == FF_B_TYPE )
		{
			p_sys->b_has_b_frames = true;
		}

		if( !m_fmtIn.video.i_aspect )
		{
			/* Fetch again the aspect ratio in case it changed */
			m_fmtOut.video.i_aspect =
				(unsigned int)(VOUT_ASPECT_FACTOR
				* ( av_q2d(p_sys->p_context->sample_aspect_ratio)
				* p_sys->p_context->width / p_sys->p_context->height ));
			m_fmtOut.video.i_sar_num
				= p_sys->p_context->sample_aspect_ratio.num;
			m_fmtOut.video.i_sar_den
				= p_sys->p_context->sample_aspect_ratio.den;
			if( m_fmtOut.video.i_aspect == 0 )
			{
				m_fmtOut.video.i_aspect = VOUT_ASPECT_FACTOR
					* p_sys->p_context->width / p_sys->p_context->height;
			}
		}

		/* Send decoded frame to vout */
		if( p_sys->i_pts )
		{
			p_pic->date = p_sys->i_pts;

			/* interpolate the next PTS: calculated from frame rate(e.g. 0.03366 interval for 29.7fps) */
			if( m_fmtIn.video.i_frame_rate > 0 &&
				m_fmtIn.video.i_frame_rate_base > 0 )
			{
				p_sys->i_pts += 1000000LL *
					(2 + p_sys->p_ff_pic->repeat_pict) *
					m_fmtIn.video.i_frame_rate_base *
					p_block->i_rate / INPUT_RATE_DEFAULT /
					(2 * m_fmtIn.video.i_frame_rate);
				//_RPT2(_CRT_WARN, "7.decoder's pts changed to:%I64d(rate:%d)\n", p_sys->i_pts,p_block->i_rate);
			}
			else if( p_sys->p_context->time_base.den > 0 )
			{
				p_sys->i_pts += I64C(1000000) *
					(2 + p_sys->p_ff_pic->repeat_pict) *
					p_sys->p_context->time_base.num *
					p_block->i_rate / INPUT_RATE_DEFAULT /
					(2 * p_sys->p_context->time_base.den);
				//_RPT2(_CRT_WARN, "8.decoder's pts changed to:%I64d(rate:%d)\n", p_sys->i_pts,p_block->i_rate);
			}

			if( p_sys->b_first_frame )
			{
				/* Hack to force display of still pictures */
				p_sys->b_first_frame = false;
				p_pic->b_force = true;
			}

			p_pic->i_nb_fields = 2 + p_sys->p_ff_pic->repeat_pict;
			p_pic->b_progressive = !p_sys->p_ff_pic->interlaced_frame;
			p_pic->b_top_field_first = p_sys->p_ff_pic->top_field_first == 0 ? false : true;

			return p_pic;
		}
		else
		{
			vout_DestroyPicture( m_pVout, p_pic );
		}
	}

	/* no more frame to decode */
	block_Release( p_block );
	return NULL;
}

// this will not be called in plywisch, as m_initializeAvcodec == false 
void CFFmpeg::InitLibavcodec()
{
	/* *** init ffmpeg library (libavcodec) *** */
	tme_mutex_lock(&m_lock);
	if( !m_bFFmpegInit )
	{
		avcodec_init();
		avcodec_register_all();
		m_bFFmpegInit = true;

		_RPT0(_CRT_WARN, "libavcodec initialized\n");
	}
	else
	{
		_RPT0(_CRT_WARN, "libavcodec already initialized\n");
	}
	tme_mutex_unlock(&m_lock);
}

void CFFmpeg::InitVideoCodec()
{
	int i_size = m_fmtIn.i_extra;

	if( !i_size ) return;

	m_pVideoDec->p_context->extradata_size = i_size;
	m_pVideoDec->p_context->extradata =
		(PBYTE)malloc( i_size + FF_INPUT_BUFFER_PADDING_SIZE );
	memcpy( m_pVideoDec->p_context->extradata,
		m_fmtIn.p_extra, i_size );
	memset( &((UINT8*)m_pVideoDec->p_context->extradata)[i_size],
		0, FF_INPUT_BUFFER_PADDING_SIZE );

}


/*****************************************************************************
* Codec fourcc -> ffmpeg_id mapping
*****************************************************************************/
static struct
{
	fourcc_t  i_fourcc;
	int  i_codec;
	int  i_cat;
	char *psz_name;

} codecs_table[] =
{
	/* MPEG-1 Video */
	{ TME_FOURCC('m','p','1','v'), CODEC_ID_MPEG1VIDEO,
	VIDEO_ES, "MPEG-1 Video" },

	/* MPEG-2 Video */
	{ TME_FOURCC('m','p','2','v'), CODEC_ID_MPEG2VIDEO,
	VIDEO_ES, "MPEG-2 Video" },
	{ TME_FOURCC('m','p','g','v'), CODEC_ID_MPEG2VIDEO,
	VIDEO_ES, "MPEG-2 Video" },

	/* MPEG-4 Video */
	{ TME_FOURCC('D','I','V','X'), CODEC_ID_MPEG4,
	VIDEO_ES, "MPEG-4 Video" },
	{ TME_FOURCC('d','i','v','x'), CODEC_ID_MPEG4,
	VIDEO_ES, "MPEG-4 Video" },
	{ TME_FOURCC('M','P','4','S'), CODEC_ID_MPEG4,
	VIDEO_ES, "MPEG-4 Video" },
	{ TME_FOURCC('m','p','4','s'), CODEC_ID_MPEG4,
	VIDEO_ES, "MPEG-4 Video" },
	{ TME_FOURCC('M','4','S','2'), CODEC_ID_MPEG4,
	VIDEO_ES, "MPEG-4 Video" },
	{ TME_FOURCC('m','4','s','2'), CODEC_ID_MPEG4,
	VIDEO_ES, "MPEG-4 Video" },
	{ TME_FOURCC('x','v','i','d'), CODEC_ID_MPEG4,
	VIDEO_ES, "MPEG-4 Video" },
	{ TME_FOURCC('X','V','I','D'), CODEC_ID_MPEG4,
	VIDEO_ES, "MPEG-4 Video" },
	{ TME_FOURCC('X','v','i','D'), CODEC_ID_MPEG4,
	VIDEO_ES, "MPEG-4 Video" },
	{ TME_FOURCC('X','V','I','X'), CODEC_ID_MPEG4,
	VIDEO_ES, "MPEG-4 Video" },
	{ TME_FOURCC('x','v','i','x'), CODEC_ID_MPEG4,
	VIDEO_ES, "MPEG-4 Video" },
	{ TME_FOURCC('D','X','5','0'), CODEC_ID_MPEG4,
	VIDEO_ES, "MPEG-4 Video" },
	{ TME_FOURCC('d','x','5','0'), CODEC_ID_MPEG4,
	VIDEO_ES, "MPEG-4 Video" },
	{ TME_FOURCC('m','p','4','v'), CODEC_ID_MPEG4,
	VIDEO_ES, "MPEG-4 Video" },
	{ TME_FOURCC('M','P','4','V'), CODEC_ID_MPEG4,
	VIDEO_ES, "MPEG-4 Video" },
	{ TME_FOURCC( 4,  0,  0,  0 ), CODEC_ID_MPEG4,
	VIDEO_ES, "MPEG-4 Video" },
	{ TME_FOURCC('m','4','c','c'), CODEC_ID_MPEG4,
	VIDEO_ES, "MPEG-4 Video" },
	{ TME_FOURCC('M','4','C','C'), CODEC_ID_MPEG4,
	VIDEO_ES, "MPEG-4 Video" },
	{ TME_FOURCC('F','M','P','4'), CODEC_ID_MPEG4,
	VIDEO_ES, "MPEG-4 Video" },
	/* 3ivx delta 3.5 Unsupported
	* putting it here gives extreme distorted images
	{ TME_FOURCC('3','I','V','1'), CODEC_ID_MPEG4,
	VIDEO_ES, "MPEG-4 Video" },
	{ TME_FOURCC('3','i','v','1'), CODEC_ID_MPEG4,
	VIDEO_ES, "MPEG-4 Video" }, */
	/* 3ivx delta 4 */
	{ TME_FOURCC('3','I','V','2'), CODEC_ID_MPEG4,
	VIDEO_ES, "MPEG-4 Video" },
	{ TME_FOURCC('3','i','v','2'), CODEC_ID_MPEG4,
	VIDEO_ES, "MPEG-4 Video" },

	/* MSMPEG4 v1 */
	{ TME_FOURCC('D','I','V','1'), CODEC_ID_MSMPEG4V1,
	VIDEO_ES, "MS MPEG-4 Video v1" },
	{ TME_FOURCC('d','i','v','1'), CODEC_ID_MSMPEG4V1,
	VIDEO_ES, "MS MPEG-4 Video v1" },
	{ TME_FOURCC('M','P','G','4'), CODEC_ID_MSMPEG4V1,
	VIDEO_ES, "MS MPEG-4 Video v1" },
	{ TME_FOURCC('m','p','g','4'), CODEC_ID_MSMPEG4V1,
	VIDEO_ES, "MS MPEG-4 Video v1" },

	/* MSMPEG4 v2 */
	{ TME_FOURCC('D','I','V','2'), CODEC_ID_MSMPEG4V2,
	VIDEO_ES, "MS MPEG-4 Video v2" },
	{ TME_FOURCC('d','i','v','2'), CODEC_ID_MSMPEG4V2,
	VIDEO_ES, "MS MPEG-4 Video v2" },
	{ TME_FOURCC('M','P','4','2'), CODEC_ID_MSMPEG4V2,
	VIDEO_ES, "MS MPEG-4 Video v2" },
	{ TME_FOURCC('m','p','4','2'), CODEC_ID_MSMPEG4V2,
	VIDEO_ES, "MS MPEG-4 Video v2" },

	/* MSMPEG4 v3 / M$ mpeg4 v3 */
	{ TME_FOURCC('M','P','G','3'), CODEC_ID_MSMPEG4V3,
	VIDEO_ES, "MS MPEG-4 Video v3" },
	{ TME_FOURCC('m','p','g','3'), CODEC_ID_MSMPEG4V3,
	VIDEO_ES, "MS MPEG-4 Video v3" },
	{ TME_FOURCC('d','i','v','3'), CODEC_ID_MSMPEG4V3,
	VIDEO_ES, "MS MPEG-4 Video v3" },
	{ TME_FOURCC('M','P','4','3'), CODEC_ID_MSMPEG4V3,
	VIDEO_ES, "MS MPEG-4 Video v3" },
	{ TME_FOURCC('m','p','4','3'), CODEC_ID_MSMPEG4V3,
	VIDEO_ES, "MS MPEG-4 Video v3" },
	/* DivX 3.20 */
	{ TME_FOURCC('D','I','V','3'), CODEC_ID_MSMPEG4V3,
	VIDEO_ES, "MS MPEG-4 Video v3" },
	{ TME_FOURCC('D','I','V','4'), CODEC_ID_MSMPEG4V3,
	VIDEO_ES, "MS MPEG-4 Video v3" },
	{ TME_FOURCC('d','i','v','4'), CODEC_ID_MSMPEG4V3,
	VIDEO_ES, "MS MPEG-4 Video v3" },
	{ TME_FOURCC('D','I','V','5'), CODEC_ID_MSMPEG4V3,
	VIDEO_ES, "MS MPEG-4 Video v3" },
	{ TME_FOURCC('d','i','v','5'), CODEC_ID_MSMPEG4V3,
	VIDEO_ES, "MS MPEG-4 Video v3" },
	{ TME_FOURCC('D','I','V','6'), CODEC_ID_MSMPEG4V3,
	VIDEO_ES, "MS MPEG-4 Video v3" },
	{ TME_FOURCC('d','i','v','6'), CODEC_ID_MSMPEG4V3,
	VIDEO_ES, "MS MPEG-4 Video v3" },
	/* AngelPotion stuff */
	{ TME_FOURCC('A','P','4','1'), CODEC_ID_MSMPEG4V3,
	VIDEO_ES, "MS MPEG-4 Video v3" },
	/* 3ivx doctered divx files */
	{ TME_FOURCC('3','I','V','D'), CODEC_ID_MSMPEG4V3,
	VIDEO_ES, "MS MPEG-4 Video v3" },
	{ TME_FOURCC('3','i','v','d'), CODEC_ID_MSMPEG4V3,
	VIDEO_ES, "MS MPEG-4 Video v3" },
	/* who knows? */
	{ TME_FOURCC('3','V','I','D'), CODEC_ID_MSMPEG4V3,
	VIDEO_ES, "MS MPEG-4 Video v3" },
	{ TME_FOURCC('3','v','i','d'), CODEC_ID_MSMPEG4V3,
	VIDEO_ES, "MS MPEG-4 Video v3" },

	/* Sorenson v1 */
	{ TME_FOURCC('S','V','Q','1'), CODEC_ID_SVQ1,
	VIDEO_ES, "SVQ-1 (Sorenson Video v1)" },

	/* Sorenson v3 */
	{ TME_FOURCC('S','V','Q','3'), CODEC_ID_SVQ3,
	VIDEO_ES, "SVQ-3 (Sorenson Video v3)" },

	/* h264 */
	{ TME_FOURCC('h','2','6','4'), CODEC_ID_H264,
	VIDEO_ES, "h264" },
	{ TME_FOURCC('H','2','6','4'), CODEC_ID_H264,
	VIDEO_ES, "h264" },
	{ TME_FOURCC('x','2','6','4'), CODEC_ID_H264,
	VIDEO_ES, "h264" },
	{ TME_FOURCC('X','2','6','4'), CODEC_ID_H264,
	VIDEO_ES, "h264" },
	/* avc1: special case h264 */
	{ TME_FOURCC('a','v','c','1'), CODEC_ID_H264,
	VIDEO_ES, "h264" },
	{ TME_FOURCC('V','S','S','H'), CODEC_ID_H264,
	VIDEO_ES, "h264" },
	{ TME_FOURCC('v','s','s','h'), CODEC_ID_H264,
	VIDEO_ES, "h264" },

	/* H263 and H263i */
	/* H263(+) is also known as Real Video 1.0 */

	/* FIXME FOURCC_H263P exist but what fourcc ? */

	/* H263 */
	{ TME_FOURCC('H','2','6','3'), CODEC_ID_H263,
	VIDEO_ES, "H263" },
	{ TME_FOURCC('h','2','6','3'), CODEC_ID_H263,
	VIDEO_ES, "H263" },
	{ TME_FOURCC('U','2','6','3'), CODEC_ID_H263,
	VIDEO_ES, "H263" },
	{ TME_FOURCC('M','2','6','3'), CODEC_ID_H263,
	VIDEO_ES, "H263" },

	/* H263i */
	{ TME_FOURCC('I','2','6','3'), CODEC_ID_H263I,
	VIDEO_ES, "I263.I" },
	{ TME_FOURCC('i','2','6','3'), CODEC_ID_H263I,
	VIDEO_ES, "I263.I" },

	/* Flash (H263) variant */
	{ TME_FOURCC('F','L','V','1'), CODEC_ID_FLV1,
	VIDEO_ES, "Flash Video" },

#if LIBAVCODEC_BUILD > 4716
	{ TME_FOURCC('H','2','6','1'), CODEC_ID_H261,
	VIDEO_ES, "H.261" },
#endif

#if LIBAVCODEC_BUILD > 4680
	{ TME_FOURCC('F','L','I','C'), CODEC_ID_FLIC,
	VIDEO_ES, "Flic Video" },
#endif

	/* MJPEG */
	{ TME_FOURCC( 'M', 'J', 'P', 'G' ), CODEC_ID_MJPEG,
	VIDEO_ES, "Motion JPEG Video" },
	{ TME_FOURCC( 'm', 'j', 'p', 'g' ), CODEC_ID_MJPEG,
	VIDEO_ES, "Motion JPEG Video" },
	{ TME_FOURCC( 'm', 'j', 'p', 'a' ), CODEC_ID_MJPEG, /* for mov file */
	VIDEO_ES, "Motion JPEG Video" },
	{ TME_FOURCC( 'j', 'p', 'e', 'g' ), CODEC_ID_MJPEG,
	VIDEO_ES, "Motion JPEG Video" },
	{ TME_FOURCC( 'J', 'P', 'E', 'G' ), CODEC_ID_MJPEG,
	VIDEO_ES, "Motion JPEG Video" },
	{ TME_FOURCC( 'J', 'F', 'I', 'F' ), CODEC_ID_MJPEG,
	VIDEO_ES, "Motion JPEG Video" },
	{ TME_FOURCC( 'J', 'P', 'G', 'L' ), CODEC_ID_MJPEG,
	VIDEO_ES, "Motion JPEG Video" },

	{ TME_FOURCC( 'm', 'j', 'p', 'b' ), CODEC_ID_MJPEGB, /* for mov file */
	VIDEO_ES, "Motion JPEG B Video" },

#if LIBAVCODEC_BUILD > 4680
	{ TME_FOURCC( 'S', 'P', '5', 'X' ), CODEC_ID_SP5X,
	VIDEO_ES, "Sunplus Motion JPEG Video" },
#endif

	/* DV */
	{ TME_FOURCC('d','v','s','l'), CODEC_ID_DVVIDEO,
	VIDEO_ES, "DV Video" },
	{ TME_FOURCC('d','v','s','d'), CODEC_ID_DVVIDEO,
	VIDEO_ES, "DV Video" },
	{ TME_FOURCC('D','V','S','D'), CODEC_ID_DVVIDEO,
	VIDEO_ES, "DV Video" },
	{ TME_FOURCC('d','v','h','d'), CODEC_ID_DVVIDEO,
	VIDEO_ES, "DV Video" },
	{ TME_FOURCC('d','v','c',' '), CODEC_ID_DVVIDEO,
	VIDEO_ES, "DV Video" },
	{ TME_FOURCC('d','v','c','p'), CODEC_ID_DVVIDEO,
	VIDEO_ES, "DV Video PAL" },
	{ TME_FOURCC('d','v','p',' '), CODEC_ID_DVVIDEO,
	VIDEO_ES, "DV Video Pro" },
	{ TME_FOURCC('d','v','p','p'), CODEC_ID_DVVIDEO,
	VIDEO_ES, "DV Video Pro PAL" },
	{ TME_FOURCC('C','D','V','C'), CODEC_ID_DVVIDEO,
	VIDEO_ES, "DV Video" },

	/* Windows Media Video */
	{ TME_FOURCC('W','M','V','1'), CODEC_ID_WMV1,
	VIDEO_ES, "Windows Media Video 1" },
	{ TME_FOURCC('W','M','V','2'), CODEC_ID_WMV2,
	VIDEO_ES, "Windows Media Video 2" },
#if LIBAVCODEC_BUILD >= ((51<<16)+(10<<8)+1)
	{ TME_FOURCC('W','M','V','3'), CODEC_ID_WMV3,
	VIDEO_ES, "Windows Media Video 3" },
	{ TME_FOURCC('W','V','C','1'), CODEC_ID_VC1,
	VIDEO_ES, "Windows Media Video VC1" },
#endif
#if 0
	/* WMVA is the VC-1 codec before the standardization proces,
	it is not bitstream compatible and deprecated  */
	{ TME_FOURCC('W','M','V','A'), CODEC_ID_VC1,
	VIDEO_ES, "Windows Media Video Advanced Profile" },
#endif

#if LIBAVCODEC_BUILD >= 4683
	/* Microsoft Video 1 */
	{ TME_FOURCC('M','S','V','C'), CODEC_ID_MSVIDEO1,
	VIDEO_ES, "Microsoft Video 1" },
	{ TME_FOURCC('m','s','v','c'), CODEC_ID_MSVIDEO1,
	VIDEO_ES, "Microsoft Video 1" },
	{ TME_FOURCC('C','R','A','M'), CODEC_ID_MSVIDEO1,
	VIDEO_ES, "Microsoft Video 1" },
	{ TME_FOURCC('c','r','a','m'), CODEC_ID_MSVIDEO1,
	VIDEO_ES, "Microsoft Video 1" },
	{ TME_FOURCC('W','H','A','M'), CODEC_ID_MSVIDEO1,
	VIDEO_ES, "Microsoft Video 1" },
	{ TME_FOURCC('w','h','a','m'), CODEC_ID_MSVIDEO1,
	VIDEO_ES, "Microsoft Video 1" },

	/* Microsoft RLE */
	{ TME_FOURCC('m','r','l','e'), CODEC_ID_MSRLE,
	VIDEO_ES, "Microsoft RLE Video" },
	{ TME_FOURCC(0x1,0x0,0x0,0x0), CODEC_ID_MSRLE,
	VIDEO_ES, "Microsoft RLE Video" },
#endif

	/* Indeo Video Codecs (Quality of this decoder on ppc is not good) */
	{ TME_FOURCC('I','V','3','1'), CODEC_ID_INDEO3,
	VIDEO_ES, "Indeo Video v3" },
	{ TME_FOURCC('i','v','3','1'), CODEC_ID_INDEO3,
	VIDEO_ES, "Indeo Video v3" },
	{ TME_FOURCC('I','V','3','2'), CODEC_ID_INDEO3,
	VIDEO_ES, "Indeo Video v3" },
	{ TME_FOURCC('i','v','3','2'), CODEC_ID_INDEO3,
	VIDEO_ES, "Indeo Video v3" },

#if LIBAVCODEC_BUILD >= 4721
	{ TME_FOURCC('t','s','c','c'), CODEC_ID_TSCC,
	VIDEO_ES, "TechSmith Camtasia Screen Capture Video" },
#endif

	/* Huff YUV */
	{ TME_FOURCC('H','F','Y','U'), CODEC_ID_HUFFYUV,
	VIDEO_ES, "Huff YUV Video" },

	/* Creative YUV */
	{ TME_FOURCC('C','Y','U','V'), CODEC_ID_CYUV,
	VIDEO_ES, "Creative YUV Video" },

	/* On2 VP3 Video Codecs */
	{ TME_FOURCC('V','P','3',' '), CODEC_ID_VP3,
	VIDEO_ES, "On2's VP3 Video" },
	{ TME_FOURCC('V','P','3','0'), CODEC_ID_VP3,
	VIDEO_ES, "On2's VP3 Video" },
	{ TME_FOURCC('V','P','3','1'), CODEC_ID_VP3,
	VIDEO_ES, "On2's VP3 Video" },
	{ TME_FOURCC('v','p','3','1'), CODEC_ID_VP3,
	VIDEO_ES, "On2's VP3 Video" },

#if LIBAVCODEC_BUILD >= ((51<<16)+(14<<8)+0)
	{ TME_FOURCC('V','P','5',' '), CODEC_ID_VP5,
	VIDEO_ES, "On2's VP5 Video" },
	{ TME_FOURCC('V','P','5','0'), CODEC_ID_VP5,
	VIDEO_ES, "On2's VP5 Video" },
	{ TME_FOURCC('V','P','6','2'), CODEC_ID_VP6,
	VIDEO_ES, "On2's VP6.2 Video" },
	{ TME_FOURCC('v','p','6','2'), CODEC_ID_VP6,
	VIDEO_ES, "On2's VP6.2 Video" },
	{ TME_FOURCC('V','P','6','F'), CODEC_ID_VP6F,
	VIDEO_ES, "On2's VP6.2 Video (Flash)" },
#endif

#if LIBAVCODEC_BUILD >= 4685
	/* Xiph.org theora */
	{ TME_FOURCC('t','h','e','o'), CODEC_ID_THEORA,
	VIDEO_ES, "Xiph.org's Theora Video" },
#endif

#if ( !defined( WORDS_BIGENDIAN ) )
	/* Asus Video (Another thing that doesn't work on PPC) */
	{ TME_FOURCC('A','S','V','1'), CODEC_ID_ASV1,
	VIDEO_ES, "Asus V1 Video" },
	{ TME_FOURCC('A','S','V','2'), CODEC_ID_ASV2,
	VIDEO_ES, "Asus V2 Video" },
#endif

	/* FFMPEG Video 1 (lossless codec) */
	{ TME_FOURCC('F','F','V','1'), CODEC_ID_FFV1,
	VIDEO_ES, "FFMpeg Video 1" },

	/* ATI VCR1 */
	{ TME_FOURCC('V','C','R','1'), CODEC_ID_VCR1,
	VIDEO_ES, "ATI VCR1 Video" },

	/* Cirrus Logic AccuPak */
	{ TME_FOURCC('C','L','J','R'), CODEC_ID_CLJR,
	VIDEO_ES, "Creative Logic AccuPak" },

	/* Real Video */
	{ TME_FOURCC('R','V','1','0'), CODEC_ID_RV10,
	VIDEO_ES, "Real Video 10" },
	{ TME_FOURCC('R','V','1','3'), CODEC_ID_RV10,
	VIDEO_ES, "Real Video 13" },
#if LIBAVCODEC_BUILD >= ((51<<16)+(15<<8)+1)
	{ TME_FOURCC('R','V','2','0'), CODEC_ID_RV20,
	VIDEO_ES, "Real Video 20" },
#endif

#if LIBAVCODEC_BUILD >= 4684
	/* Apple Video */
	{ TME_FOURCC('r','p','z','a'), CODEC_ID_RPZA,
	VIDEO_ES, "Apple Video" },

	{ TME_FOURCC('s','m','c',' '), CODEC_ID_SMC,
	VIDEO_ES, "Apple graphics" },

	/* Cinepak */
	{ TME_FOURCC('c','v','i','d'), CODEC_ID_CINEPAK,
	VIDEO_ES, "Cinepak Video" },

	/* Id Quake II CIN */
	{ TME_FOURCC('I','D','C','I'), CODEC_ID_IDCIN,
	VIDEO_ES, "Id Quake II CIN Video" },
#endif

	/* 4X Technologies */
	{ TME_FOURCC('4','x','m','v'), CODEC_ID_4XM,
	VIDEO_ES, "4X Technologies Video" },

#if LIBAVCODEC_BUILD >= 4694
	/* Duck TrueMotion */
	{ TME_FOURCC('D','U','C','K'), CODEC_ID_TRUEMOTION1,
	VIDEO_ES, "Duck TrueMotion v1 Video" },
#endif

	/* Interplay MVE */
	{ TME_FOURCC('i','m','v','e'), CODEC_ID_INTERPLAY_VIDEO,
	VIDEO_ES, "Interplay MVE Video" },

	/* Id RoQ */
	{ TME_FOURCC('R','o','Q','v'), CODEC_ID_ROQ,
	VIDEO_ES, "Id RoQ Video" },

	/* Sony Playstation MDEC */
	{ TME_FOURCC('M','D','E','C'), CODEC_ID_MDEC,
	VIDEO_ES, "PSX MDEC Video" },

#if LIBAVCODEC_BUILD >= 4699
	/* Sierra VMD */
	{ TME_FOURCC('v','m','d','v'), CODEC_ID_VMDVIDEO,
	VIDEO_ES, "Sierra VMD Video" },
#endif

#if LIBAVCODEC_BUILD >= 4719
	/* FFMPEG's SNOW wavelet codec */
	{ TME_FOURCC('S','N','O','W'), CODEC_ID_SNOW,
	VIDEO_ES, "FFMpeg SNOW wavelet Video" },
#endif

#if LIBAVCODEC_BUILD >= 4752
	{ TME_FOURCC('r','l','e',' '), CODEC_ID_QTRLE,
	VIDEO_ES, "Apple QuickTime RLE Video" },

	{ TME_FOURCC('q','d','r','w'), CODEC_ID_QDRAW,
	VIDEO_ES, "Apple QuickDraw Video" },

	{ TME_FOURCC('Q','P','E','G'), CODEC_ID_QPEG,
	VIDEO_ES, "QPEG Video" },
	{ TME_FOURCC('Q','1','.','0'), CODEC_ID_QPEG,
	VIDEO_ES, "QPEG Video" },
	{ TME_FOURCC('Q','1','.','1'), CODEC_ID_QPEG,
	VIDEO_ES, "QPEG Video" },

	{ TME_FOURCC('U','L','T','I'), CODEC_ID_ULTI,
	VIDEO_ES, "IBM Ultimotion Video" },

	{ TME_FOURCC('V','I','X','L'), CODEC_ID_VIXL,
	VIDEO_ES, "Miro/Pinnacle VideoXL Video" },

	{ TME_FOURCC('L','O','C','O'), CODEC_ID_LOCO,
	VIDEO_ES, "LOCO Video" },

	{ TME_FOURCC('W','N','V','1'), CODEC_ID_WNV1,
	VIDEO_ES, "Winnov WNV1 Video" },

	{ TME_FOURCC('A','A','S','C'), CODEC_ID_AASC,
	VIDEO_ES, "Autodesc RLE Video" },
#endif
#if LIBAVCODEC_BUILD >= 4753
	{ TME_FOURCC('I','V','2','0'), CODEC_ID_INDEO2,
	VIDEO_ES, "Indeo Video v2" },
	{ TME_FOURCC('R','T','2','1'), CODEC_ID_INDEO2,
	VIDEO_ES, "Indeo Video v2" },
#endif

#if LIBAVCODEC_BUILD >= ((51<<16)+(13<<8)+0)
	{ TME_FOURCC('V','M','n','c'), CODEC_ID_VMNC,
	VIDEO_ES, "VMware Video" },
#endif


	/*
	*  Image codecs
	*/

#if LIBAVCODEC_BUILD >= 4731
	{ TME_FOURCC('p','n','g',' '), CODEC_ID_PNG,
	VIDEO_ES, "PNG Image" },
	{ TME_FOURCC('p','p','m',' '), CODEC_ID_PPM,
	VIDEO_ES, "PPM Image" },
	{ TME_FOURCC('p','g','m',' '), CODEC_ID_PGM,
	VIDEO_ES, "PGM Image" },
	{ TME_FOURCC('p','g','m','y'), CODEC_ID_PGMYUV,
	VIDEO_ES, "PGM YUV Image" },
	{ TME_FOURCC('p','a','m',' '), CODEC_ID_PAM,
	VIDEO_ES, "PAM Image" },
#endif

#if LIBAVCODEC_BUILD >= ((51<<16)+(0<<8)+0)
	{ TME_FOURCC('b','m','p',' '), CODEC_ID_BMP,
	VIDEO_ES, "BMP Image" },
#endif

	/*
	*  Audio Codecs
	*/

	/* Windows Media Audio 1 */
	{ TME_FOURCC('W','M','A','1'), CODEC_ID_WMAV1,
	AUDIO_ES, "Windows Media Audio 1" },
	{ TME_FOURCC('w','m','a','1'), CODEC_ID_WMAV1,
	AUDIO_ES, "Windows Media Audio 1" },

	/* Windows Media Audio 2 */
	{ TME_FOURCC('W','M','A','2'), CODEC_ID_WMAV2,
	AUDIO_ES, "Windows Media Audio 2" },
	{ TME_FOURCC('w','m','a','2'), CODEC_ID_WMAV2,
	AUDIO_ES, "Windows Media Audio 2" },

	/* DV Audio */
	{ TME_FOURCC('d','v','a','u'), CODEC_ID_DVAUDIO,
	AUDIO_ES, "DV Audio" },

	/* MACE-3 Audio */
	{ TME_FOURCC('M','A','C','3'), CODEC_ID_MACE3,
	AUDIO_ES, "MACE-3 Audio" },

	/* MACE-6 Audio */
	{ TME_FOURCC('M','A','C','6'), CODEC_ID_MACE6,
	AUDIO_ES, "MACE-6 Audio" },

	/* RealAudio 1.0 */
	{ TME_FOURCC('1','4','_','4'), CODEC_ID_RA_144,
	AUDIO_ES, "RealAudio 1.0" },

	/* RealAudio 2.0 */
	{ TME_FOURCC('2','8','_','8'), CODEC_ID_RA_288,
	AUDIO_ES, "RealAudio 2.0" },

	/* MPEG Audio layer 1/2/3 */
	{ TME_FOURCC('m','p','g','a'), CODEC_ID_MP2,
	AUDIO_ES, "MPEG Audio layer 1/2" },
	{ TME_FOURCC('m','p','3',' '), CODEC_ID_MP3,
	AUDIO_ES, "MPEG Audio layer 1/2/3" },

	/* A52 Audio (aka AC3) */
	{ TME_FOURCC('a','5','2',' '), CODEC_ID_AC3,
	AUDIO_ES, "A52 Audio (aka AC3)" },
	{ TME_FOURCC('a','5','2','b'), CODEC_ID_AC3, /* VLC specific hack */
	AUDIO_ES, "A52 Audio (aka AC3)" },

#if LIBAVCODEC_BUILD >= 4719
	/* DTS Audio */
	{ TME_FOURCC('d','t','s',' '), CODEC_ID_DTS,
	AUDIO_ES, "DTS Audio" },
#endif

	/* AAC audio */
	{ TME_FOURCC('m','p','4','a'), CODEC_ID_AAC,
	AUDIO_ES, "MPEG AAC Audio" },

	/* 4X Technologies */
	{ TME_FOURCC('4','x','m','a'), CODEC_ID_ADPCM_4XM,
	AUDIO_ES, "4X Technologies Audio" },

	/* Interplay DPCM */
	{ TME_FOURCC('i','d','p','c'), CODEC_ID_INTERPLAY_DPCM,
	AUDIO_ES, "Interplay DPCM Audio" },

	/* Id RoQ */
	{ TME_FOURCC('R','o','Q','a'), CODEC_ID_ROQ_DPCM,
	AUDIO_ES, "Id RoQ DPCM Audio" },

#if LIBAVCODEC_BUILD >= 4685
	/* Sony Playstation XA ADPCM */
	{ TME_FOURCC('x','a',' ',' '), CODEC_ID_ADPCM_XA,
	AUDIO_ES, "PSX XA ADPCM Audio" },

	/* ADX ADPCM */
	{ TME_FOURCC('a','d','x',' '), CODEC_ID_ADPCM_ADX,
	AUDIO_ES, "ADX ADPCM Audio" },
#endif

#if LIBAVCODEC_BUILD >= 4699
	/* Sierra VMD */
	{ TME_FOURCC('v','m','d','a'), CODEC_ID_VMDAUDIO,
	AUDIO_ES, "Sierra VMD Audio" },
#endif

#if LIBAVCODEC_BUILD >= 4706
	/* G.726 ADPCM */
	{ TME_FOURCC('g','7','2','6'), CODEC_ID_ADPCM_G726,
	AUDIO_ES, "G.726 ADPCM Audio" },
#endif

#if LIBAVCODEC_BUILD >= 4683
	/* AMR */
	{ TME_FOURCC('s','a','m','r'), CODEC_ID_AMR_NB,
	AUDIO_ES, "AMR narrow band" },
	{ TME_FOURCC('s','a','w','b'), CODEC_ID_AMR_WB,
	AUDIO_ES, "AMR wide band" },
#endif

#if LIBAVCODEC_BUILD >= 4703
	/* FLAC */
	{ TME_FOURCC('f','l','a','c'), CODEC_ID_FLAC,
	AUDIO_ES, "FLAC (Free Lossless Audio Codec)" },
#endif

#if LIBAVCODEC_BUILD >= 4745
	/* ALAC */
	{ TME_FOURCC('a','l','a','c'), CODEC_ID_ALAC,
	AUDIO_ES, "Apple Lossless Audio Codec" },
#endif

#if LIBAVCODEC_BUILD >= ((50<<16)+(0<<8)+1)
	/* QDM2 */
	{ TME_FOURCC('Q','D','M','2'), CODEC_ID_QDM2,
	AUDIO_ES, "QDM2 Audio" },
#endif

#if LIBAVCODEC_BUILD >= ((51<<16)+(0<<8)+0)
	/* COOK */
	{ TME_FOURCC('c','o','o','k'), CODEC_ID_COOK,
	AUDIO_ES, "Cook Audio" },
#endif

#if LIBAVCODEC_BUILD >= ((51<<16)+(4<<8)+0)
	/* TTA: The Lossless True Audio */
	{ TME_FOURCC('T','T','A','1'), CODEC_ID_TTA,
	AUDIO_ES, "The Lossless True Audio" },
#endif

#if LIBAVCODEC_BUILD >= ((51<<16)+(8<<8)+0)
	/* Shorten */
	{ TME_FOURCC('s','h','n',' '), CODEC_ID_SHORTEN,
	AUDIO_ES, "Shorten Lossless Audio" },
#endif

#if LIBAVCODEC_BUILD >= ((51<<16)+(16<<8)+0)
	/* WavPack */
	{ TME_FOURCC('W','V','P','K'), CODEC_ID_WAVPACK,
	AUDIO_ES, "WavPack Lossless Audio" },
#endif

	/* PCM */
	{ TME_FOURCC('s','8',' ',' '), CODEC_ID_PCM_S8,
	AUDIO_ES, "PCM S8" },
	{ TME_FOURCC('u','8',' ',' '), CODEC_ID_PCM_U8,
	AUDIO_ES, "PCM U8" },
	{ TME_FOURCC('s','1','6','l'), CODEC_ID_PCM_S16LE,
	AUDIO_ES, "PCM S16 LE" },
	{ TME_FOURCC('s','1','6','b'), CODEC_ID_PCM_S16BE,
	AUDIO_ES, "PCM S16 BE" },
	{ TME_FOURCC('u','1','6','l'), CODEC_ID_PCM_U16LE,
	AUDIO_ES, "PCM U16 LE" },
	{ TME_FOURCC('u','1','6','b'), CODEC_ID_PCM_U16BE,
	AUDIO_ES, "PCM U16 BE" },
	{ TME_FOURCC('s','2','4','l'), CODEC_ID_PCM_S24LE,
	AUDIO_ES, "PCM S24 LE" },
	{ TME_FOURCC('s','2','4','b'), CODEC_ID_PCM_S24BE,
	AUDIO_ES, "PCM S24 BE" },
	{ TME_FOURCC('u','2','4','l'), CODEC_ID_PCM_U24LE,
	AUDIO_ES, "PCM U24 LE" },
	{ TME_FOURCC('u','2','4','b'), CODEC_ID_PCM_U24BE,
	AUDIO_ES, "PCM U24 BE" },
	{ TME_FOURCC('s','3','2','l'), CODEC_ID_PCM_S32LE,
	AUDIO_ES, "PCM S32 LE" },
	{ TME_FOURCC('s','3','2','b'), CODEC_ID_PCM_S32BE,
	AUDIO_ES, "PCM S32 BE" },
	{ TME_FOURCC('u','3','2','l'), CODEC_ID_PCM_U32LE,
	AUDIO_ES, "PCM U32 LE" },
	{ TME_FOURCC('u','3','2','b'), CODEC_ID_PCM_U32BE,
	AUDIO_ES, "PCM U32 BE" },
	{ TME_FOURCC('a','l','a','w'), CODEC_ID_PCM_ALAW,
	AUDIO_ES, "PCM ALAW" },
	{ TME_FOURCC('u','l','a','w'), CODEC_ID_PCM_MULAW,
	AUDIO_ES, "PCM ULAW" },

	{0}
};

int GetFfmpegCodec( fourcc_t i_fourcc, int *pi_cat,
									 int *pi_ffmpeg_codec, char **ppsz_name )
{
	int i;

	for( i = 0; codecs_table[i].i_fourcc != 0; i++ )
	{
		if( codecs_table[i].i_fourcc == i_fourcc )
		{
			if( pi_cat ) *pi_cat = codecs_table[i].i_cat;
			if( pi_ffmpeg_codec ) *pi_ffmpeg_codec = codecs_table[i].i_codec;
			if( ppsz_name ) *ppsz_name = codecs_table[i].psz_name;

			return SUCCESS;
		}
	}
	return EGENERIC;
}

/*****************************************************************************
* ffmpeg_CopyPicture: copy a picture from ffmpeg internal buffers to a
*                     picture_t structure (when not in direct rendering mode).
*****************************************************************************/
static void ffmpeg_CopyPicture( CFFmpeg *p_dec,
															 picture_t *p_pic, AVFrame *p_ff_pic )
{
	video_sys_t *p_sys = p_dec->m_pVideoDec;

	if( ffmpeg_PixFmtToChroma( p_sys->p_context->pix_fmt ) )
	{
		int i_plane, i_size, i_line;
		UINT8 *p_dst, *p_src;
		int i_src_stride, i_dst_stride;

#ifdef LIBAVCODEC_PP
		if( p_sys->p_pp && p_sys->b_pp )
			E_(PostprocPict)( p_dec, p_sys->p_pp, p_pic, p_ff_pic );
		else
#endif
			for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
			{
				p_src  = p_ff_pic->data[i_plane];
				p_dst = p_pic->p[i_plane].p_pixels;
				i_src_stride = p_ff_pic->linesize[i_plane];
				i_dst_stride = p_pic->p[i_plane].i_pitch;

				i_size = min( i_src_stride, i_dst_stride );
				for( i_line = 0; i_line < p_pic->p[i_plane].i_visible_lines;
					i_line++ )
				{
					memcpy( p_dst, p_src, i_size );
					p_src += i_src_stride;
					p_dst += i_dst_stride;
				}
			}
	}
	else
	{
		AVPicture dest_pic;
		int i;

		/* we need to convert to I420 */
		switch( p_sys->p_context->pix_fmt )
		{
		case PIX_FMT_YUV410P:
		case PIX_FMT_YUV411P:
		case PIX_FMT_BGR24:
		case PIX_FMT_PAL8:
			for( i = 0; i < p_pic->i_planes; i++ )
			{
				dest_pic.data[i] = p_pic->p[i].p_pixels;
				dest_pic.linesize[i] = p_pic->p[i].i_pitch;
			}
			img_convert( &dest_pic, PIX_FMT_YUV420P,
				(AVPicture *)p_ff_pic,
				p_sys->p_context->pix_fmt,
				p_sys->p_context->width,
				p_sys->p_context->height );
			break;
		default:
			_RPT1(_CRT_WARN, "don't know how to convert chroma %i",
				p_sys->p_context->pix_fmt );
			p_dec->m_bError = 1;
			break;
		}
	}
}

/*****************************************************************************
* ffmpeg_GetFrameBuf: callback used by ffmpeg to get a frame buffer.
*****************************************************************************
* It is used for direct rendering as well as to get the right PTS for each
* decoded picture (even in indirect rendering mode).
*****************************************************************************/
static int ffmpeg_GetFrameBuf( struct AVCodecContext *p_context,
															AVFrame *p_ff_pic )
{
	CFFmpeg *p_dec = (CFFmpeg *)p_context->opaque;
	video_sys_t *p_sys = p_dec->m_pVideoDec;

	picture_t *p_pic;

	//_RPT0(_CRT_WARN, "ffmpeg_GetFrameBuf\n");

	/* Set picture PTS */
	if( p_sys->input_pts )
	{
		p_ff_pic->pts = p_sys->input_pts;
	}
	else if( p_sys->input_dts )
	{
		/* Some demuxers only set the dts so let's try to find a useful
		* timestamp from this */
		if( !p_context->has_b_frames || !p_sys->b_has_b_frames ||
			!p_ff_pic->reference || !p_sys->i_pts )
		{
			p_ff_pic->pts = p_sys->input_dts;
		}
		else p_ff_pic->pts = 0;
	}
	else p_ff_pic->pts = 0;

	if( p_sys->i_pts ) /* make sure 1st frame has a pts > 0 */
	{
		p_sys->input_pts = p_sys->input_dts = 0;
	}

	p_ff_pic->opaque = 0;

	int ret;
	tme_mutex_lock(&CFFmpeg::m_lock);
	/* Not much to do in indirect rendering mode */
	if( !p_sys->b_direct_rendering || p_sys->b_pp )
	{
		ret = avcodec_default_get_buffer( p_context, p_ff_pic );
		tme_mutex_unlock(&CFFmpeg::m_lock);
		return ret;
	}

	/* Some codecs set pix_fmt only after the 1st frame has been decoded,
	* so this check is necessary. */
	if( !ffmpeg_PixFmtToChroma( p_context->pix_fmt ) ||
		p_sys->p_context->width % 16 || p_sys->p_context->height % 16 )
	{
		_RPT0(_CRT_WARN, "disabling direct rendering\n" );
		p_sys->b_direct_rendering = 0;
		ret = avcodec_default_get_buffer( p_context, p_ff_pic );
		tme_mutex_unlock(&CFFmpeg::m_lock);
		return ret;
	}

	/* Get a new picture */
	p_pic = ffmpeg_NewPictBuf( p_dec, p_sys->p_context );
	if( !p_pic )
	{
		p_sys->b_direct_rendering = 0;
		ret = avcodec_default_get_buffer( p_context, p_ff_pic );
		tme_mutex_unlock(&CFFmpeg::m_lock);
		return ret;
	}
	tme_mutex_unlock(&CFFmpeg::m_lock);
	p_sys->p_context->draw_horiz_band = NULL;

	p_ff_pic->opaque = (void*)p_pic;
	p_ff_pic->type = FF_BUFFER_TYPE_USER;
	p_ff_pic->data[0] = p_pic->p[0].p_pixels;
	p_ff_pic->data[1] = p_pic->p[1].p_pixels;
	p_ff_pic->data[2] = p_pic->p[2].p_pixels;
	p_ff_pic->data[3] = NULL; /* alpha channel but I'm not sure */

	p_ff_pic->linesize[0] = p_pic->p[0].i_pitch;
	p_ff_pic->linesize[1] = p_pic->p[1].i_pitch;
	p_ff_pic->linesize[2] = p_pic->p[2].i_pitch;
	p_ff_pic->linesize[3] = 0;

	if( p_ff_pic->reference != 0 ||
		p_sys->i_codec_id == CODEC_ID_H264 /* Bug in libavcodec */ )
	{
		vout_LinkPicture( p_dec->m_pVout, p_pic );
	}

	/* FIXME what is that, should give good value */
	p_ff_pic->age = 256*256*256*64; // FIXME FIXME from ffmpeg

	return 0;
}

static void ffmpeg_ReleaseFrameBuf( struct AVCodecContext *p_context,
																	 AVFrame *p_ff_pic )
{
	CFFmpeg *p_dec = (CFFmpeg *)p_context->opaque;
	picture_t *p_pic;

	if( !p_ff_pic->opaque )
	{
		avcodec_default_release_buffer( p_context, p_ff_pic );
		return;
	}

	p_pic = (picture_t*)p_ff_pic->opaque;

	p_ff_pic->data[0] = NULL;
	p_ff_pic->data[1] = NULL;
	p_ff_pic->data[2] = NULL;
	p_ff_pic->data[3] = NULL;

	if( p_ff_pic->reference != 0 ||
		p_dec->m_pVideoDec->i_codec_id == CODEC_ID_H264 /* Bug in libavcodec */ )
	{
		vout_UnlinkPicture( p_dec->m_pVout, p_pic );
	}
}

// just fill the blank
aout_buffer_t *CFFmpeg::DecodeAudio( block_t **pp_block ) {
	return NULL;
}

subpicture_t *CFFmpeg::DecodeSubtitle( block_t **pp_block ) {
	return NULL;
}

