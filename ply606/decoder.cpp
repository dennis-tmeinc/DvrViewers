#include "stdafx.h"
#include <crtdbg.h>
#include <process.h>
#include "decoder.h"
#include "audio_output.h"
#include "vout_vector.h"
#include "direct3d.h"
#include "directx.h"
#include "inputstream.h"
#include "trace.h"
#include "vout_mem.h"

static es_format_t null_es_format = {0};
unsigned WINAPI DecoderThreadFunc(PVOID pvoid) {
	CDecoder *p = (CDecoder *)pvoid;
	p->DecoderThread();
	_endthreadex(0);
	return 0;
}

/* creates a fifo (input --> decoder)
*/
CDecoder::CDecoder(tme_object_t *pIpc, HWND hwnd, es_format_t *fmt, CInputStream *pInput)
{
	TRACE(_T("CDecoder(%d)\n"), pInput ? pInput->GetChannelID() : -1);

	m_ch = -1;
	m_pInput = pInput;
	if (m_pInput) m_ch = m_pInput->GetChannelID();
	m_hwnd = hwnd;
	m_pVout = NULL;	
	m_pSpuVout = NULL;
	m_pAout = NULL;
	m_pAoutInput = NULL;
	m_bDie = m_bError = false;
	m_bPause = false;
	m_hThread = NULL;

	es_format_Copy(&m_fmtIn, fmt); 
	es_format_Copy(&m_fmtOut, &null_es_format); 
	m_bNeedPacketized = false;
	m_bPaceControl = true; /* allowed to drop frames? : always false for us */
	m_bOwnThread = true;
	m_iPrerollEnd = -1;
	m_iSpuChannel = 0;
	ZeroMemory(&m_video, sizeof(video_format_t));
	ZeroMemory(&m_audio, sizeof(audio_format_t));

	//tme_thread_init(&m_ipc);
	m_pIpc = pIpc;
	m_pFifo = block_FifoNew(m_pIpc);
}

CDecoder::~CDecoder()
{
	TRACE(_T("~CDecoder()\n"));
	/* Delete decoder configuration */
	DeleteDecoder();

	//tme_mutex_destroy(&(m_ipc.object_lock));
	//tme_cond_destroy(&(m_ipc.object_wait));
}

int CDecoder::StartThread()
{
	if( m_bOwnThread )
	{
		int i_priority;
		if( m_fmtIn.i_cat == AUDIO_ES )
			i_priority = TME_THREAD_PRIORITY_AUDIO;
		else
			i_priority = TME_THREAD_PRIORITY_VIDEO;

		/* Spawn the decoder thread */
		m_hThread = tme_thread_create( m_pIpc,
			(m_fmtIn.i_cat == AUDIO_ES) ? "Audio Decoder Thread" : 
			((m_fmtIn.i_cat == VIDEO_ES) ? "Video Decoder Thread" : "Subtitle Decoder Thread"),
			DecoderThreadFunc, this,				
			i_priority, false );
		if (m_hThread == NULL) {
			_RPT0(_CRT_WARN, "cannot spawn decoder thread\n" );
			DeleteDecoder();
			return 1;
		}
	}

	return 0;
}

void CDecoder::DeleteDecoder()
{
	_RPT2(_CRT_WARN, "killing decoder fourcc `%4.4s', %d PES in FIFO\n",
		(char*)&m_fmtIn.i_codec,
		m_pFifo->i_depth );

	/* Free all packets still in the decoder fifo. */
	block_FifoEmpty( m_pFifo );
	block_FifoRelease( m_pFifo );

	/* Cleanup */
	if( m_pAoutInput )
		m_pAout->DecDelete( m_pAoutInput );
	delete m_pAout;
	m_pAout = NULL;

	if( m_pVout )
	{
		int i_pic;

#define p_pic m_pVout->m_render.pp_picture[i_pic]
		/* Hack to make sure all the the pictures are freed by the decoder */
		for( i_pic = 0; i_pic < m_pVout->m_render.i_pictures; i_pic++ )
		{
			if( p_pic->i_status == RESERVED_PICTURE )
				vout_DestroyPicture( m_pVout, p_pic );
			if( p_pic->i_refcount > 0 )
				vout_UnlinkPicture( m_pVout, p_pic );
		}
#undef p_pic
	}

	es_format_Clean( &m_fmtIn );
	es_format_Clean( &m_fmtOut );
}

// should be called by derived object
void CDecoder::DeleteDecoder2() {

	m_bDie = true;

	// release the fifo waiting.
	block_t *p_block = block_New( 0 );
	block_FifoPut( m_pFifo, p_block );

	if (m_hThread != NULL) {
		TRACE(_T("ch[%d] Joining decoder thread(cat:%d)\n"), m_ch, m_fmtIn.i_cat );
		tme_thread_join(m_hThread);
		TRACE(_T("ch[%d] decoder thread joined(cat:%d)\n"), m_ch, m_fmtIn.i_cat);
	}
	m_hThread = NULL;
}

int CDecoder::DecoderThread()
{
	block_t *p_block;

	TRACE(_T(" ch[%d] Starting %s Decoder thread (id:%u<%08x>,priority:%d)\n"), m_ch,
		m_fmtIn.i_cat == AUDIO_ES ? _T("audio") : 
		(m_fmtIn.i_cat == VIDEO_ES ? _T("video") : _T("subtitle")),
		m_hThread, GetCurrentThreadId(), GetThreadPriority(m_hThread));

	/* The decoder's main loop */
	while( !m_bDie && !m_bError )
	{
		if (!m_bPause) {
			if( ( p_block = block_FifoGet( m_pFifo ) ) == NULL )
			{
				m_bError = true;
				break;
			}
#if 0
			TRACE(_T(" ch[%d] Decoder Got %s data:%d(dts:%I64d)\n"), m_ch,
				m_fmtIn.i_cat == AUDIO_ES ? _T("audio") : 
				(m_fmtIn.i_cat == VIDEO_ES ? _T("video") : _T("subtitle")),
				p_block->i_buffer, p_block->i_dts );
#endif
			if( DecoderDecode( p_block ) != SUCCESS )
			{
				break;
			}
		} else {
			Sleep(25);
		}
	}
	//TRACE(_T(" ch[%d] cat %d loop end\n"), m_ch, m_fmtIn.i_cat);

	while( !m_bDie )
	{
		/* Trash all received PES packets */
		p_block = block_FifoGet( m_pFifo );
		if( p_block ) block_Release( p_block );
	}

	TRACE(_T(" ch[%d] cat %d thread end\n"), m_ch, m_fmtIn.i_cat);
	return 0;
}

int CDecoder::DecoderData(block_t *p_block)
{    
	if( m_pFifo->i_size > 5000000 /* 5 MB */ ) {
		_RPT0(_CRT_WARN, "decoder/packetizer fifo full, resetting fifo!\n" );        
		block_FifoEmpty( m_pFifo );
	}

	/* we use one fifo for each tracks(audio/video) */
	block_FifoPut( m_pFifo, p_block );

	return 0;
}

/* called from decoder thread after retrieving successfully one block from the fifo (input --> decoder)
*/
int CDecoder::DecoderDecode(block_t *p_block)
{
	int ch = -1;
	if (m_pInput) ch =  m_pInput->GetChannelID();
	int i_rate = p_block ? p_block->i_rate : 1000;

	//TRACE(_T("DecoderDecode ch[%d](%d,%I64d:%d)\n"),ch,m_fmtIn.i_cat,p_block->i_dts,p_block?p_block->i_buffer:-1);
	if( p_block && p_block->i_buffer <= 0 )
	{
		block_Release( p_block );
		return SUCCESS;
	}

	if( m_fmtIn.i_cat == AUDIO_ES ) {
		aout_buffer_t *p_aout_buf;
		while( (p_aout_buf = DecodeAudio( &p_block )) )
		{
			if( m_iPrerollEnd > 0 &&
				p_aout_buf->start_date < m_iPrerollEnd )
			{
				m_pAout->DecDeleteBuffer( m_pAoutInput, p_aout_buf );
			}
			else
			{
				m_iPrerollEnd = -1;
				m_pAout->DecPlay( m_pAoutInput, p_aout_buf );
			}
		}
	} else if( m_fmtIn.i_cat == VIDEO_ES )
	{
		picture_t *p_pic;

		while( (p_pic = DecodeVideo( &p_block )) )
		{
			if( m_iPrerollEnd > 0 &&
				p_pic->date < m_iPrerollEnd )
			{
				vout_DestroyPicture( m_pVout, p_pic );
			}
			else
			{
				m_iPrerollEnd = -1;
				//TRACE(_T("Displaying decoded video(%d):%I64d\n"), ch, p_pic->date);
				vout_DatePicture( m_pVout, p_pic, p_pic->date );
				vout_DisplayPicture( m_pVout, p_pic );
				//if (p_block->i_flags == BLOCK_FLAG_TYPE_I) {
				//	m_pInput->m_bGotFirstKeyframe = true;
				//}
			}
		}
	} else if( m_fmtIn.i_cat == SPU_ES ) {
		subpicture_t *p_spu;
		while( (p_spu = DecodeSubtitle( &p_block ) ) )
		{
			if( m_iPrerollEnd > 0 &&
				p_spu->i_start < m_iPrerollEnd &&
				( p_spu->i_stop <= 0 || p_spu->i_stop <= m_iPrerollEnd ) )
			{
				if (m_pSpuVout)
					m_pSpuVout->m_pSpu->DestroySubpicture( p_spu );
				continue;
			}

			m_iPrerollEnd = -1;
			if( m_pSpuVout )
			{
				m_pSpuVout->m_pSpu->DisplaySubpicture( p_spu );
			}
		}
	} else {
		_RPT0(_CRT_WARN, "unknown ES format\n");
		m_bError = true;
	}

	return m_bError ? EGENERIC : SUCCESS;
}

void CDecoder::DecoderDiscontinuity()
{
	block_t *p_null;

	/* Empty the fifo */
	if( m_bOwnThread )
	{
		block_FifoEmpty( m_pFifo );
	}

	/* Send a special block */
	p_null = block_New( 128 );
	p_null->i_flags |= BLOCK_FLAG_DISCONTINUITY;
	memset( p_null->p_buffer, 0, p_null->i_buffer );

	DecoderData(p_null);
}

bool CDecoder::DecoderEmpty()
{
	if( m_bOwnThread && m_pFifo->i_depth > 0 )
	{
		return false;
	}
	return true;
}

void CDecoder::DecoderPreroll( INT64 i_preroll_end )
{
	m_iPrerollEnd = i_preroll_end;
}


/*****************************************************************************
* Buffers allocation callbacks for the decoders
*****************************************************************************/
aout_buffer_t *CDecoder::AoutNewBuffer( int i_samples )
{
	aout_buffer_t *p_buffer;
	int ch = -1;
	if (m_pInput) ch =  m_pInput->GetChannelID();

	if( m_pAoutInput != NULL &&
		( m_fmtOut.audio.i_rate != m_audio.i_rate ||
		m_fmtOut.audio.i_original_channels !=
		m_audio.i_original_channels ||
		m_fmtOut.audio.i_bytes_per_frame !=
		m_audio.i_bytes_per_frame ) )
	{
		/* Parameters changed, restart the aout */
		m_pAout->DecDelete( m_pAoutInput );
		m_pAoutInput = NULL;
	}

	if( m_pAoutInput == NULL )
	{
		audio_sample_format_t format;
		int i_force_dolby = 0;//config_GetInt( p_dec, "force-dolby-surround" );

		m_fmtOut.audio.i_format = m_fmtOut.i_codec;
		m_audio = m_fmtOut.audio;

		memcpy( &format, &m_audio, sizeof( audio_sample_format_t ) );
		if ( i_force_dolby && (format.i_original_channels&AOUT_CHAN_PHYSMASK)
			== (AOUT_CHAN_LEFT|AOUT_CHAN_RIGHT) )
		{
			if ( i_force_dolby == 1 )
			{
				format.i_original_channels = format.i_original_channels |
					AOUT_CHAN_DOLBYSTEREO;
			}
			else /* i_force_dolby == 2 */
			{
				format.i_original_channels = format.i_original_channels &
					~AOUT_CHAN_DOLBYSTEREO;
			}
		}

		m_pAout = new CAudioOutput(m_pIpc, &format, ch);
		if (m_pAout == NULL)
			return NULL;

		/* set audio enable/disable */
		if (m_pInput) {
			m_pAout->VolumeSet(m_pInput->IsAudioOn() ? AOUT_VOLUME_DEFAULT : AOUT_VOLUME_MIN);
		} else {
			m_pAout->VolumeSet(AOUT_VOLUME_DEFAULT);
		}

		m_pAoutInput = m_pAout->DecNew( &format );
		if( m_pAoutInput == NULL )
		{
			_RPT0(_CRT_WARN, "failed to create audio output\n" );
			m_bError = true;
			return NULL;
		}
		m_fmtOut.audio.i_bytes_per_frame = m_audio.i_bytes_per_frame;
	}

	p_buffer = m_pAout->DecNewBuffer( m_pAoutInput, i_samples );

	return p_buffer;
}

void CDecoder::AoutDelBuffer( aout_buffer_t *p_buffer )
{
	m_pAout->DecDeleteBuffer( m_pAoutInput, p_buffer );
}

picture_t *CDecoder::VoutNewBuffer(bool use_vout_mem)
{
	picture_t *p_pic;

	if( m_pVout == NULL ||
		m_fmtOut.video.i_width != m_video.i_width ||
		m_fmtOut.video.i_height != m_video.i_height ||
		m_fmtOut.video.i_chroma != m_video.i_chroma ||
		m_fmtOut.video.i_aspect != m_video.i_aspect )
	{
		if( !m_fmtOut.video.i_width ||
			!m_fmtOut.video.i_height )
		{
			/* Can't create a new vout without display size */
			return NULL;
		}

		if( !m_fmtOut.video.i_visible_width ||
			!m_fmtOut.video.i_visible_height )
		{
			if( m_fmtIn.video.i_visible_width &&
				m_fmtIn.video.i_visible_height )
			{
				m_fmtOut.video.i_visible_width =
					m_fmtIn.video.i_visible_width;
				m_fmtOut.video.i_visible_height =
					m_fmtIn.video.i_visible_height;
			}
			else
			{
				m_fmtOut.video.i_visible_width =
					m_fmtOut.video.i_width;
				m_fmtOut.video.i_visible_height =
					m_fmtOut.video.i_height;
			}
		}

		if( m_fmtOut.video.i_visible_height == 1088 )
		{
			m_fmtOut.video.i_visible_height = 1080;
			m_fmtOut.video.i_sar_num *= 135;
			m_fmtOut.video.i_sar_den *= 136;
			_RPT0(_CRT_WARN, "Fixing broken HDTV stream (display_height=1088)\n");
		}

		if( !m_fmtOut.video.i_sar_num ||
			!m_fmtOut.video.i_sar_den )
		{
			m_fmtOut.video.i_sar_num = m_fmtOut.video.i_aspect *
				m_fmtOut.video.i_visible_height;

			m_fmtOut.video.i_sar_den = VOUT_ASPECT_FACTOR *
				m_fmtOut.video.i_visible_width;
		}

		ureduce( &m_fmtOut.video.i_sar_num,
			&m_fmtOut.video.i_sar_den,
			m_fmtOut.video.i_sar_num,
			m_fmtOut.video.i_sar_den, 50000 );

		m_fmtOut.video.i_chroma = m_fmtOut.i_codec;
		m_video = m_fmtOut.video;

		//if (m_pInput == NULL)
		//	return NULL;
		struct vout_set *pvs, vs;
		pvs = getVoutSetForWindowHandle(m_hwnd, &vs);

		/* if resolution has been changed, delete/create video output */
		if (pvs && vs.vout) {
			m_pVout = vs.vout;
			vs.input_pts_delay = DEFAULT_PTS_DELAY;//m_pInput->m_iPtsDelay; /* for CVideouOutpu */
			//vs.input_rate = m_pInput->m_iRate; /* for CVideouOutpu */
			//vs.input_state = m_pInput->m_iState;
			//vs.b_input_stepplay = m_pInput->m_bStepPlay; /* for CVideouOutpu */
			//vs.input_state = m_pInput->m_iState;
			if ((vs.width != m_video.i_width) ||
				(vs.height != m_video.i_height)) {
					if (m_pVout) {
						_RPT4(_CRT_WARN, "++++++++++++DELETING video output+++++++++++++:%d,%d,%d,%d\n",pvs->width,pvs->height,m_video.i_width,m_video.i_height );
						delete vs.vout;
						m_pVout = NULL;
						vs.vout = NULL;
					}
			}
			setVoutSetForWindowHandle(vs);
		}

		/* vout is created only once per channel */
		if ((pvs == NULL) || (pvs && vs.vout == NULL)) {
			TRACE(_T("ch[%d] creating video output(hwnd:%x)\n"), m_ch, m_hwnd);

			if (m_pInput) m_pInput->m_bCreatingVideoOutput = true;
			if (m_hwnd) {
				m_pVout = new CDirect3D(m_pIpc, m_hwnd, &m_fmtOut.video, DEFAULT_PTS_DELAY, false);
				// should be false for plywisch, and can be true/false for tmeview
				//m_pVout = new CDirectX(m_hwnd, &m_fmtOut.video, DEFAULT_PTS_DELAY, false, false);
			} else {
				if (use_vout_mem) {
					m_pVout = new CVoutMem(m_pIpc, &m_fmtOut.video, DEFAULT_PTS_DELAY);
				}
			}
			TRACE(_T("ch[%d] after video output:%p\n"), m_ch, m_pVout);
			if ((m_pVout == NULL) || m_pVout->OpenVideo())
			{
				TRACE(_T("ch[%d] failed to create video output\n"), m_ch);
				m_bError = true;
				if (m_pInput) m_pInput->m_bCreatingVideoOutput = false;
				return NULL;
			}
			if (m_pInput) m_pInput->m_bCreatingVideoOutput = false;
			TRACE(_T("ch[%d] created video output:%p\n"), m_ch, m_pVout);

			vs.hwnd = m_hwnd;
			vs.width = m_video.i_width;
			vs.height = m_video.i_height;
			vs.vout = m_pVout;
			vs.input_pts_delay = DEFAULT_PTS_DELAY;//m_pInput->m_iPtsDelay; /* for CVideouOutpu */
			//vs.input_rate = m_pInput->m_iRate; /* for CVideouOutpu */
			//vs.input_state = m_pInput->m_iState;
			//vs.b_input_stepplay = m_pInput->m_bStepPlay; /* for CVideouOutpu */
			setVoutSetForWindowHandle(vs);
		}

		if( m_video.i_rmask )
			m_pVout->m_render.i_rmask = m_video.i_rmask;
		if( m_video.i_gmask )
			m_pVout->m_render.i_gmask = m_video.i_gmask;
		if( m_video.i_bmask )
			m_pVout->m_render.i_bmask = m_video.i_bmask;
	}

	/* Get a new picture */
	while( !(p_pic = vout_CreatePicture( m_pVout, 0, 0, 0 ) ) )
	{
		int i_pic, i_ready_pic = 0;

		if( m_bDie || m_bError )
		{
			return NULL;
		}

#define p_pic m_pVout->m_render.pp_picture[i_pic]
		/* Check the decoder doesn't leak pictures */
		for( i_pic = 0; i_pic < m_pVout->m_render.i_pictures;
			i_pic++ )
		{
			if( p_pic->i_status == READY_PICTURE )
			{
				if( i_ready_pic++ > 0 ) break;
				else continue;
			}

			if( p_pic->i_status != DISPLAYED_PICTURE &&
				p_pic->i_status != RESERVED_PICTURE &&
				p_pic->i_status != READY_PICTURE ) break;

			if( !p_pic->i_refcount && p_pic->i_status != RESERVED_PICTURE )
				break;
		}
		if( i_pic == m_pVout->m_render.i_pictures )
		{
			_RPT0(_CRT_WARN, "decoder is leaking pictures, resetting the heap\n" );

			/* Just free all the pictures */
			for( i_pic = 0; i_pic < m_pVout->m_render.i_pictures;
				i_pic++ )
			{
				if( p_pic->i_status == RESERVED_PICTURE )
					vout_DestroyPicture( m_pVout, p_pic );
				if( p_pic->i_refcount > 0 )
					vout_UnlinkPicture( m_pVout, p_pic );
			}
		}
#undef p_pic

		msleep( VOUT_OUTMEM_SLEEP );
	}

	return p_pic;
}

void CDecoder::VoutDelBuffer( picture_t *p_pic )
{
	vout_DestroyPicture( m_pVout, p_pic );
}

void CDecoder::VoutLinkPicture( picture_t *p_pic )
{
	vout_LinkPicture( m_pVout, p_pic );
}

void CDecoder::VoutUninkPicture( picture_t *p_pic )
{
	vout_UnlinkPicture( m_pVout, p_pic );
}

subpicture_t *CDecoder::SpuNewBuffer()
{
	CVideoOutput *pVout = NULL;
	subpicture_t *pSubpic = NULL;
	int i_attempts = 30;

	while( i_attempts-- )
	{
		// don't do this: if( m_bDie || m_bError ) break;

		struct vout_set *pvs, vs;
		pvs = getVoutSetForWindowHandle(m_hwnd, &vs);
		if (pvs) {
			pVout = vs.vout;
		}
		if( pVout ) break;

		msleep( VOUT_DISPLAY_DELAY );
	}

	if( !pVout )
	{
		TRACE(_T("ch[%d] no vout found, dropping subpicture\n"), m_ch);
		return NULL;
	}

	if( m_pSpuVout != pVout ) // same as if (!m_pSpuVout)
	{
		pVout->m_pSpu->Control( SPU_CHANNEL_REGISTER,
			&m_iSpuChannel );
		m_pSpuVout = pVout;
	}

	pSubpic = pVout->m_pSpu->CreateSubpicture();
	if( pSubpic )
	{
		pSubpic->i_channel = m_iSpuChannel;
	}

	return pSubpic;
}

void CDecoder::SpuDelBuffer( subpicture_t *p_subpic )
{
	if( !m_pSpuVout)
	{
		_RPT0(_CRT_WARN, "no vout found, leaking subpicture\n" );
		return;
	}

	m_pSpuVout->m_pSpu->DestroySubpicture( p_subpic );
}
