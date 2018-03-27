#include "stdafx.h"
#include <crtdbg.h>
#include <process.h>
#include "video_output.h"
#include "vout_picture.h"
#include "decoder.h"
#include "vout_vector.h"
#include "inputstream.h"
#include "trace.h"
#include "i420_rgb.h"
#include "i420_yuy2.h"


static void AspectRatio( int i_aspect, int *i_aspect_x, int *i_aspect_y );
static void MaskToShift( int *pi_left, int *pi_right, UINT32 i_mask );
static void SetOffsetRGB8( int i_width, int i_height, int i_pic_width,
													int i_pic_height, bool *pb_hscale,
													int *pi_vscale, int *p_offset );
static void SetOffsetRGB16( int i_width, int i_height, int i_pic_width,
													 int i_pic_height, bool *pb_hscale,
													 unsigned int *pi_vscale, int *p_offset );

unsigned WINAPI RunThreadFunc(PVOID pvoid) {
	CVideoOutput *p = (CVideoOutput *)pvoid;
	p->RunThread();
	_RPT0(_CRT_WARN, "Video Output Thread Ended\n");
	_endthreadex(0);
	return 0;
}

CVideoOutput::CVideoOutput(tme_object_t *pIpc, HWND hwnd, video_format_t *p_fmt, int iPtsDelay, bool bResizeParent)
{
	int              i_index;                               /* loop variable */

	m_pIpc = pIpc;
	m_hThread = NULL;
	m_bDie = false;
	m_hparent = hwnd;
	m_bSnapshot = false;
	m_pszFilterChain = NULL;
	m_bResizeParent = bResizeParent;
	ZeroMemory(&m_chroma, sizeof(m_chroma));

	m_bDropLate = false;

	unsigned int i_width = p_fmt->i_width;
	unsigned int i_height = p_fmt->i_height;
	fourcc_t i_chroma = p_fmt->i_chroma;
	unsigned int i_aspect = p_fmt->i_aspect;

	ZeroMemory(m_pPicture, sizeof(picture_t) * (2 * VOUT_MAX_PICTURES + 1));
	/* Initialize pictures - translation tables and functions
	* will be initialized later in InitThread */
	for( i_index = 0; i_index < 2 * VOUT_MAX_PICTURES + 1; i_index++)
	{
		//m_pPicture[i_index].pf_lock = NULL;
		//m_pPicture[i_index].pf_unlock = NULL;
		m_pPicture[i_index].i_status = FREE_PICTURE;
		m_pPicture[i_index].i_type   = EMPTY_PICTURE;
		m_pPicture[i_index].b_slow   = 0;
	}

	/* No images in the heap */
	m_iHeapSize = 0;

	/* Initialize the rendering heap */
	I_RENDERPICTURES = 0;

	ureduce( &p_fmt->i_sar_num, &p_fmt->i_sar_den,
		p_fmt->i_sar_num, p_fmt->i_sar_den, 50000 );
	m_fmtRender        = *p_fmt;   /* FIXME palette */
	m_fmtIn            = *p_fmt;   /* FIXME palette */
	SecureZeroMemory(&m_fmtOut, sizeof(video_format_t));

	m_render.i_width	= i_width;
	m_render.i_height   = i_height;
	m_render.i_chroma   = i_chroma;
	m_render.i_aspect   = i_aspect;

	m_render.i_rmask    = 0;
	m_render.i_gmask    = 0;
	m_render.i_bmask    = 0;

	m_render.i_last_used_pic = -1;
	m_render.b_allow_modify_pics = 1;

	/* Zero the output heap */
	I_OUTPUTPICTURES = 0;
	m_output.i_width    = 0;
	m_output.i_height   = 0;
	m_output.i_chroma   = 0;
	m_output.i_aspect   = 0;

	m_output.i_rmask    = 0;
	m_output.i_gmask    = 0;
	m_output.i_bmask    = 0;

	/* Initialize misc stuff */
	m_iChanges    = 0;
	m_fGamma      = 0;
	m_bGrayscale  = false;
	m_bInfo       = false;
	m_bInterface  = false;
	m_bScale      = true;
	m_bFullscreen = false;
	m_iAlignment  = 0;
	m_renderTime  = 10;
	m_cFpsSamples = 0;
	m_bFilterChange = 0;
	//m_pf_control = 0;
	//m_pParentIntf = 0;
	m_iParNum = m_iParDen = 1;
	m_iChromaType = 0;

	m_pSnapshotpic = NULL;

	/* Initialize locks */
	//tme_thread_init(&m_ipc);
	tme_mutex_init( m_pIpc, &m_pictureLock );
	tme_mutex_init( m_pIpc, &m_changeLock );

	m_pSpu = new CSubPicture(m_pIpc);

	InitWindowSize();

	m_iPtsDelay = iPtsDelay;
	if (m_iPtsDelay < 0) {
		m_iPtsDelay = DEFAULT_PTS_DELAY;
	}

    // init blur area
    m_ba_number=0 ;
}

CVideoOutput::~CVideoOutput()
{
	_RPT0(_CRT_WARN, "~CVideoOutput()\n" );

	/* Destroy the locks */
	tme_mutex_destroy( &m_pictureLock );
	tme_mutex_destroy( &m_changeLock );

	//tme_mutex_destroy(&(m_ipc.object_lock));
	//tme_cond_destroy(&(m_ipc.object_wait));
}

int CVideoOutput::StartThread()
{
	m_hThread = tme_thread_create( m_pIpc, "Vout RunThread",
		RunThreadFunc, this,
		TME_THREAD_PRIORITY_OUTPUT, true );
	if (m_hThread == NULL) {
		_RPT0(_CRT_WARN, "out of memory\n" );
		return 1;
	}

	return 0;
}

void CVideoOutput::RunThread()
{
	_RPT1(_CRT_WARN, "Starting video output thread (%08x)\n", GetCurrentThreadId());

	int             i_index;                                /* index in heap */
	mtime_t         current_date;                            /* current date */
	mtime_t         display_date;                            /* display date */

	picture_t *     p_picture;                   /* picture pointer */
	picture_t *     p_last_picture = NULL ;      /* last picture */
	picture_t *     p_directbuffer;              /* direct buffer to display */

	subpicture_t *  p_subpic = NULL;                   /* subpicture pointer */

	//input_thread_t *p_input = NULL ;           /* Parent input, if it exists */
	struct vout_set *pvs, vs;

   	i_idle_loops = 0;                            /* loops without displaying a picture */

	m_bError = InitThread() == 0 ? false : true;

	tme_thread_ready(m_pIpc);

	if( m_bError )
	{
		return;
	}

	/*
	* Main loop - it is not executed if an error occurred during
	* initialization
	*/
	while( (!m_bDie) && (!m_bError) )
	{
		/* Initialize loop variables */
		p_picture = NULL;
		display_date = 0;
		current_date = mdate();
		/*
		* Find the picture to display (the one with the earliest date).
		* This operation does not need lock, since only READY_PICTUREs
		* are handled. */
		for( i_index = 0; i_index < I_RENDERPICTURES; i_index++ )
		{
			//TRACE(_T("index:%d, status(ready=4):%d\n"), i_index, PP_RENDERPICTURE[i_index]->i_status);
			//TRACE(_T("p_picture:%p, picturedate:%I64u, displaydate:%I64u\n"), p_picture, PP_RENDERPICTURE[i_index]->date, display_date);
			if( (PP_RENDERPICTURE[i_index]->i_status == READY_PICTURE)
				&& ( (p_picture == NULL) ||
				(PP_RENDERPICTURE[i_index]->date < display_date) ) )
			{
				p_picture = PP_RENDERPICTURE[i_index];
				//TRACE(_T("updating displaydate:%I64u with picure:%I64u(now:%I64d)\n"), display_date, p_picture->date, current_date);
				display_date = p_picture->date;
			}
		}

		if( p_picture )
		{
			//_RPT2(_CRT_WARN, "Video Output: found picture:%p, last:%p\n", p_picture, p_last_picture);

			/* If we met the last picture, parse again to see whether there is
			* a more appropriate one. */
			if( p_picture == p_last_picture )
			{
				//_RPT0(_CRT_WARN, "p_picture == p_last_picture\n");
				for( i_index = 0; i_index < I_RENDERPICTURES; i_index++ )
				{
					if( (PP_RENDERPICTURE[i_index]->i_status == READY_PICTURE)
						&& (PP_RENDERPICTURE[i_index] != p_last_picture)
						&& ((p_picture == p_last_picture) ||
						(PP_RENDERPICTURE[i_index]->date < display_date)) )
					{
						p_picture = PP_RENDERPICTURE[i_index];
						display_date = p_picture->date;
					}
				}
			}

			//_RPT1(_CRT_WARN, "found better one?:%I64u\n", display_date);
			/* If we found better than the last picture, destroy it */
			if( p_last_picture && p_picture != p_last_picture )
			{
				//_RPT0(_CRT_WARN, "If we found better than the last picture, destroy it\n");
				tme_mutex_lock( &m_pictureLock );
				if( p_last_picture->i_refcount )
				{
					p_last_picture->i_status = DISPLAYED_PICTURE;
				}
				else
				{
					p_last_picture->i_status = DESTROYED_PICTURE;
					m_iHeapSize--;
				}
				tme_mutex_unlock( &m_pictureLock );
				p_last_picture = NULL;
			}

			/* Compute FPS rate */
			m_pFpsSample[ m_cFpsSamples++ % VOUT_FPS_SAMPLES ]
			= display_date;

			pvs = getVoutSetForWindowHandle(m_hparent, &vs);
			if (!pvs) {
				vs.b_input_stepplay = false;
				vs.input_rate = INPUT_RATE_DEFAULT;
				vs.input_pts_delay = DEFAULT_PTS_DELAY;
				vs.input_state = ERROR_S;
			}

			if (m_iPtsDelay != vs.input_pts_delay) {
				m_iPtsDelay = vs.input_pts_delay;
			}

			if( !p_picture->b_force &&
				p_picture != p_last_picture &&
				display_date < current_date + m_renderTime &&
				m_bDropLate && !vs.b_input_stepplay && (vs.input_rate <= INPUT_RATE_DEFAULT) )
			{
				/* Picture is late: it will be destroyed and the thread
				* will directly choose the next picture */
				tme_mutex_lock( &m_pictureLock );
				if( p_picture->i_refcount )
				{
					/* Pretend we displayed the picture, but don't destroy
					* it since the decoder might still need it. */
					p_picture->i_status = DISPLAYED_PICTURE;
				}
				else
				{
					/* Destroy the picture without displaying it */
					p_picture->i_status = DESTROYED_PICTURE;
					m_iHeapSize--;
				}
				_RPT1(_CRT_WARN, "late picture skipped (%I64u)\n",
					current_date - display_date );
				//stats_UpdateInteger( p_vout, STATS_LOST_PICTURES, 1 , NULL);
				tme_mutex_unlock( &m_pictureLock );

				continue;
			}

			if( display_date >
				current_date + m_iPtsDelay + VOUT_BOGUS_DELAY )
			{
				//_RPT4(_CRT_WARN, "current:%I64u, disp:%I64u, ptsDelay:%I64u, bogus:%I64u\n",
				//                  current_date, display_date,  m_iPtsDelay,  VOUT_BOGUS_DELAY);
				/* Picture is waaay too early: it will be destroyed */
				tme_mutex_lock( &m_pictureLock );
				if( p_picture->i_refcount )
				{
					/* Pretend we displayed the picture, but don't destroy
					* it since the decoder might still need it. */
					p_picture->i_status = DISPLAYED_PICTURE;
				}
				else
				{
					/* Destroy the picture without displaying it */
					p_picture->i_status = DESTROYED_PICTURE;
					m_iHeapSize--;
				}
				//stats_UpdateInteger( p_vout, STATS_LOST_PICTURES, 1, NULL );
				_RPT4(_CRT_WARN, "vout warning: early picture skipped (%I64d),%I64d,%I64d,%I64d\n", 
					display_date - current_date - m_iPtsDelay,display_date,current_date,m_iPtsDelay );
				tme_mutex_unlock( &m_pictureLock );

				continue;
			}

			if( display_date > current_date + VOUT_DISPLAY_DELAY )
			{
				/* A picture is ready to be rendered, but its rendering date
				* is far from the current one so the thread will perform an
				* empty loop as if no picture were found. The picture state
				* is unchanged */
				//_RPT0(_CRT_WARN, "display > current + VOUT_DISPLAY_DELAY\n"); 
				//_RPT4(_CRT_WARN, "display (%I64d), current(%I64d), diff(%I64d),DELAY:%d\n", display_date, current_date, display_date - current_date,VOUT_DISPLAY_DELAY); 
				p_picture    = NULL;
				display_date = 0;
			}
			else if( p_picture == p_last_picture )
			{
				/* We are asked to repeat the previous picture, but we first
				* wait for a couple of idle loops */
				if( i_idle_loops < 4 )
				{
					p_picture    = NULL;
					display_date = 0;
				}
				else
				{
					/* We set the display date to something high, otherwise
					* we'll have lots of problems with late pictures */
					display_date = current_date + m_renderTime;
				}
			}
		}

		if( p_picture == NULL )
		{
			i_idle_loops++;
		}

		if( p_picture && m_bSnapshot )
		{
			m_bSnapshot = false;
			//vout_Snapshot( p_vout, p_picture );
		}

		/*
		* Check for subpictures to display
		*/
		if( display_date > 0 )
		{
			pvs = getVoutSetForWindowHandle(m_hparent, &vs);
			if (!pvs) {
				vs.input_state = ERROR_S;
			}

			p_subpic = m_pSpu->SortSubpictures( display_date,
				vs.input_state == PAUSE_S);
		}

		/*
		* Perform rendering
		*/
		//stats_UpdateInteger( p_vout, STATS_DISPLAYED_PICTURES, 1, NULL );
		p_directbuffer = vout_RenderPicture( this, p_picture, p_subpic );

		/*
		* Call the plugin-specific rendering method if there is one
		*/
		if( p_picture != NULL && p_directbuffer != NULL)
		{
			/* Render the direct buffer returned by vout_RenderPicture */
			Render( p_directbuffer );

            // Display blur hook, (dennis)
            UINT8 * pixel = p_directbuffer->p[0].p_pixels ;
            if( pixel!= NULL && m_ba_number>0 ) {
                blurPicture(p_directbuffer);
            }
		}

		/*
		* Sleep, wake up
		*/
		if( display_date != 0 && p_directbuffer != NULL )
		{
			mtime_t current_render_time = mdate() - current_date;
			/* if render time is very large we don't include it in the mean */
			if( current_render_time < m_renderTime +
				VOUT_DISPLAY_DELAY )
			{
				/* Store render time using a sliding mean weighting to
				* current value in a 3 to 1 ratio*/
				m_renderTime *= 3;
				m_renderTime += current_render_time;
				m_renderTime >>= 2;
			}
		}

		// take snap shot pointer before goto sleep. (dennisc)
		if( p_directbuffer )
			m_pSnapshotpic = p_directbuffer ;

		/* Give back change lock */
		tme_mutex_unlock( &m_changeLock );

		/* Sleep a while or until a given date */
		if( display_date != 0 )
		{
			/* If there are filters in the chain, better give them the picture
			* in advance */
			if( !m_pszFilterChain || !*m_pszFilterChain )
			{
				mwait( display_date - VOUT_MWAIT_TOLERANCE );
			}
		}
		else
		{
			msleep( VOUT_IDLE_SLEEP );
		}

		/* On awakening, take back lock and send immediately picture
		* to display. */
		tme_mutex_lock( &m_changeLock );

		/*
		* Display the previously rendered picture
		*/
		if( p_picture != NULL && p_directbuffer != NULL )
		{
			/* Display the direct buffer returned by vout_RenderPicture */
			Display( p_directbuffer );

			/* Tell the vout this was the last picture and that it does not
			* need to be forced anymore. */
			p_last_picture = p_picture;
			p_last_picture->b_force = 0;
		}

		if( p_picture != NULL )
		{
			/* Reinitialize idle loop count */
			i_idle_loops = 0;
		}

		/*
		* Check events and manage thread
		*/
		if( Manage() )
		{
			/* A fatal error occurred, and the thread must terminate
			* immediately, without displaying anything - setting b_error to 1
			* causes the immediate end of the main while() loop. */
			m_bError = true;
		}

		if( m_iChanges & VOUT_SIZE_CHANGE )
		{
			/* this must only happen when the vout plugin is incapable of
			* rescaling the picture itself. In this case we need to destroy
			* the current picture buffers and recreate new ones with the right
			* dimensions */
			int i;

			m_iChanges &= ~VOUT_SIZE_CHANGE;

			End();
			for( i = 0; i < I_OUTPUTPICTURES; i++ )
				m_pPicture[ i ].i_status = FREE_PICTURE;

			I_OUTPUTPICTURES = 0;
			if( Init() )
			{
				_RPT0(_CRT_WARN, "cannot resize display\n" );
				/* FIXME: End will be called again in EndThread() */
				m_bError = true;
			}

			/* Need to reinitialise the chroma plugin */
			if( m_iChromaType )
			{
				DeactivateChroma();
				ActivateChroma();
			}
		}

		if( m_iChanges & VOUT_PICTURE_BUFFERS_CHANGE )
		{
			/* This happens when the picture buffers need to be recreated.
			* This is useful on multimonitor displays for instance.
			*
			* Warning: This only works when the vout creates only 1 picture
			* buffer!! */
			m_iChanges &= ~VOUT_PICTURE_BUFFERS_CHANGE;

			if( !m_bDirect )
			{
				DeactivateChroma();
			}

			tme_mutex_lock( &m_pictureLock );

			End();

			I_OUTPUTPICTURES = I_RENDERPICTURES = 0;

			m_bError = InitThread() == 0 ? false : true;

			tme_mutex_unlock( &m_pictureLock );
		}
	}
	/*
	* Error loop - wait until the thread destruction is requested
	*/
	if( m_bError )
	{
		ErrorThread();
	}

	/* End of thread */
	EndThread();

	m_hThread = NULL;
}

/*****************************************************************************
* ErrorThread: RunThread() error loop
*****************************************************************************
* This function is called when an error occurred during thread main's loop.
* The thread can still receive feed, but must be ready to terminate as soon
* as possible.
*****************************************************************************/
void CVideoOutput::ErrorThread()
{
	/* Wait until a `die' order */
	while( !m_bDie )
	{
		/* Sleep a while */
		msleep( VOUT_IDLE_SLEEP );
	}
}

/*****************************************************************************
* EndThread: thread destruction
*****************************************************************************
* This function is called when the thread ends after a sucessful
* initialization. It frees all resources allocated by InitThread.
*****************************************************************************/
void CVideoOutput::EndThread()
{
	int     i_index;                                        /* index in heap */

	if( !m_bDirect) {
		DeactivateChroma();
	}

	/* Destroy all remaining pictures */
	for( i_index = 0; i_index < 2 * VOUT_MAX_PICTURES + 1; i_index++ )
	{
		if ( m_pPicture[i_index].i_type == MEMORY_PICTURE )
		{
			free( m_pPicture[i_index].p_data_orig );
		}
	}

	/* Destroy subpicture unit */
	delete m_pSpu;

	/* Destroy translation tables */
	End();

	/* Release the change lock */
	tme_mutex_unlock( &m_changeLock );
}

int CVideoOutput::InitThread()
{
	int i, i_aspect_x, i_aspect_y;

	tme_mutex_lock( &m_changeLock );

	/* Initialize output method, it allocates direct buffers for us */
	if( Init() )
	{
		tme_mutex_unlock( &m_changeLock );
		return EGENERIC;
	}

	if( !I_OUTPUTPICTURES )
	{
		_RPT0(_CRT_WARN, "plugin was unable to allocate at least one direct buffer\n" );
		End();
		tme_mutex_unlock( &m_changeLock );
		return EGENERIC;
	}

	if( I_OUTPUTPICTURES > VOUT_MAX_PICTURES )
	{
		_RPT0(_CRT_WARN, "plugin allocated too many direct buffers, our internal buffers must have overflown.\n" );
		End();
		tme_mutex_unlock( &m_changeLock );
		return EGENERIC;
	}

	_RPT1(_CRT_WARN, "got %i direct buffer(s)\n", I_OUTPUTPICTURES );

	AspectRatio( m_fmtRender.i_aspect, &i_aspect_x, &i_aspect_y );

	TRACE(_T("picture in %ix%i (%i,%i,%ix%i), ")
		_T("chroma %x, ar %i:%i, sar %i:%i\n"),
		m_fmtRender.i_width, m_fmtRender.i_height,
		m_fmtRender.i_x_offset, m_fmtRender.i_y_offset,
		m_fmtRender.i_visible_width,
		m_fmtRender.i_visible_height,
		m_fmtRender.i_chroma,
		i_aspect_x, i_aspect_y,
		m_fmtRender.i_sar_num, m_fmtRender.i_sar_den );

	AspectRatio( m_fmtIn.i_aspect, &i_aspect_x, &i_aspect_y );

	TRACE(_T("picture user %ix%i (%i,%i,%ix%i), ")
		_T("chroma %x, ar %i:%i, sar %i:%i\n"),
		m_fmtIn.i_width, m_fmtIn.i_height,
		m_fmtIn.i_x_offset, m_fmtIn.i_y_offset,
		m_fmtIn.i_visible_width,
		m_fmtIn.i_visible_height,
		m_fmtIn.i_chroma,
		i_aspect_x, i_aspect_y,
		m_fmtIn.i_sar_num, m_fmtIn.i_sar_den );

	if( !m_fmtOut.i_width || !m_fmtOut.i_height )
	{
		m_fmtOut.i_width = m_fmtOut.i_visible_width =
			m_output.i_width;
		m_fmtOut.i_height = m_fmtOut.i_visible_height =
			m_output.i_height;
		m_fmtOut.i_x_offset =  m_fmtOut.i_y_offset = 0;

		m_fmtOut.i_aspect = m_output.i_aspect;
		m_fmtOut.i_chroma = m_output.i_chroma;
	}
	if( !m_fmtOut.i_sar_num || !m_fmtOut.i_sar_num )
	{
		m_fmtOut.i_sar_num = m_fmtOut.i_aspect *
			m_fmtOut.i_height;
		m_fmtOut.i_sar_den = VOUT_ASPECT_FACTOR *
			m_fmtOut.i_width;
	}

	ureduce( &m_fmtOut.i_sar_num, &m_fmtOut.i_sar_den,
		m_fmtOut.i_sar_num, m_fmtOut.i_sar_den, 0 );

	AspectRatio( m_fmtOut.i_aspect, &i_aspect_x, &i_aspect_y );

	TRACE(_T("picture out %ix%i (%i,%i,%ix%i), ")
		_T("chroma %x, ar %i:%i, sar %i:%i\n"),
		m_fmtOut.i_width, m_fmtOut.i_height,
		m_fmtOut.i_x_offset, m_fmtOut.i_y_offset,
		m_fmtOut.i_visible_width,
		m_fmtOut.i_visible_height,
		m_fmtOut.i_chroma,
		i_aspect_x, i_aspect_y,
		m_fmtOut.i_sar_num, m_fmtOut.i_sar_den );

	/* Calculate shifts from system-updated masks */
	MaskToShift( &m_output.i_lrshift, &m_output.i_rrshift,
		m_output.i_rmask );
	MaskToShift( &m_output.i_lgshift, &m_output.i_rgshift,
		m_output.i_gmask );
	MaskToShift( &m_output.i_lbshift, &m_output.i_rbshift,
		m_output.i_bmask );

	/* Check whether we managed to create direct buffers similar to
	* the render buffers, ie same size and chroma */
	if( ( m_output.i_width == m_render.i_width )
		&& ( m_output.i_height == m_render.i_height )
		&& ( vout_ChromaCmp( m_output.i_chroma, m_render.i_chroma ) ) )
	{
		/* Cool ! We have direct buffers, we can ask the decoder to
		* directly decode into them ! Map the first render buffers to
		* the first direct buffers, but keep the first direct buffer
		* for memcpy operations */
		m_bDirect = true;

		for( i = 1; i < VOUT_MAX_PICTURES; i++ )
		{
			if( m_pPicture[ i ].i_type != DIRECT_PICTURE &&
				I_RENDERPICTURES >= VOUT_MIN_DIRECT_PICTURES - 1 &&
				m_pPicture[ i - 1 ].i_type == DIRECT_PICTURE )
			{
				/* We have enough direct buffers so there's no need to
				* try to use system memory buffers. */
				break;
			}
			//_RPT1(_CRT_WARN, "direct render, mapping %d\n", I_RENDERPICTURES);
			PP_RENDERPICTURE[ I_RENDERPICTURES ] = &m_pPicture[ i ];
			I_RENDERPICTURES++;
		}

		_RPT2(_CRT_WARN, "direct render, mapping render pictures 0-%i to system pictures 1-%i\n",
			VOUT_MAX_PICTURES - 2, VOUT_MAX_PICTURES - 1 );
	}
	else
	{
		/* Rats... Something is wrong here, we could not find an output
		* plugin able to directly render what we decode. See if we can
		* find a chroma plugin to do the conversion */
		m_bDirect = false;
		ActivateChroma();

		_RPT3(_CRT_WARN, "indirect render, mapping "
			"render pictures 0-%i to system pictures %i-%i\n",
			VOUT_MAX_PICTURES - 1, I_OUTPUTPICTURES,
			I_OUTPUTPICTURES + VOUT_MAX_PICTURES - 1 );

		/* Append render buffers after the direct buffers */
		for( i = I_OUTPUTPICTURES; i < 2 * VOUT_MAX_PICTURES; i++ )
		{
			PP_RENDERPICTURE[ I_RENDERPICTURES ] = &m_pPicture[ i ];
			I_RENDERPICTURES++;

			/* Check if we have enough render pictures */
			if( I_RENDERPICTURES == VOUT_MAX_PICTURES )
				break;
		}
	}

	return 0;
}

void CVideoOutput::InitWindowSize()
{
	int i_width = -1;
	int i_height = -1;
	UINT64 ll_zoom = 1000;

	/* harry 20110502: let's make sure the video's initial size fits the window set by Viewer */
	if (m_hparent != NULL && !m_bResizeParent) {
		RECT rcWin, rc;
		GetClientRect(m_hparent, &rc);
		i_width = rc.right;
		i_height = rc.bottom;
	}

#define FP_FACTOR 1000                             /* our fixed point factor */

	if( i_width > 0 && i_height > 0)
	{
		m_iWindowWidth = (int)( i_width * ll_zoom / FP_FACTOR );
		m_iWindowHeight = (int)( i_height * ll_zoom / FP_FACTOR );
		goto initwsize_end;
	}
	else if( i_width > 0 )
	{
		m_iWindowWidth = (int)( i_width * ll_zoom / FP_FACTOR );
		m_iWindowHeight = (int)( m_fmtIn.i_visible_height * ll_zoom *
			m_fmtIn.i_sar_den * i_width / m_fmtIn.i_sar_num /
			FP_FACTOR / m_fmtIn.i_visible_width );
		goto initwsize_end;
	}
	else if( i_height > 0 )
	{
		m_iWindowHeight = (int)( i_height * ll_zoom / FP_FACTOR );
		m_iWindowWidth = (int)( m_fmtIn.i_visible_width * ll_zoom *
			m_fmtIn.i_sar_num * i_height / m_fmtIn.i_sar_den /
			FP_FACTOR / m_fmtIn.i_visible_height );
		goto initwsize_end;
	}

	if( m_fmtIn.i_sar_num >= m_fmtIn.i_sar_den )
	{
		m_iWindowWidth = (int)( m_fmtIn.i_visible_width * ll_zoom *
			m_fmtIn.i_sar_num / m_fmtIn.i_sar_den / FP_FACTOR );
		m_iWindowHeight = (int)( m_fmtIn.i_visible_height * ll_zoom 
			/ FP_FACTOR );
	}
	else
	{
		m_iWindowWidth = (int)( m_fmtIn.i_visible_width * ll_zoom 
			/ FP_FACTOR );
		m_iWindowHeight = (int)( m_fmtIn.i_visible_height * ll_zoom *
			m_fmtIn.i_sar_den / m_fmtIn.i_sar_num / FP_FACTOR );
	}

initwsize_end:
	_RPT2(_CRT_WARN, "window size: %dx%d\n", m_iWindowWidth, m_iWindowHeight );

	if (m_hparent != NULL && m_bResizeParent) {
		RECT rcWin, rc;
		GetWindowRect(m_hparent, &rcWin);
		GetClientRect(m_hparent, &rc);
		int accWidth = rcWin.right - rcWin.left - rc.right;
		int accHeight = rcWin.bottom - rcWin.top - rc.bottom;
		SetWindowPos(m_hparent, 0, rcWin.left, rcWin.top, m_iWindowWidth + accWidth, m_iWindowHeight + accHeight, 0);
	}
#undef FP_FACTOR
}

static int ReduceHeight( int i_ratio )
{
	int i_dummy = VOUT_ASPECT_FACTOR;
	int i_pgcd  = 1;

	if( !i_ratio )
	{
		return i_pgcd;
	}

	/* VOUT_ASPECT_FACTOR is (2^7 * 3^3 * 5^3), we just check for 2, 3 and 5 */
	while( !(i_ratio & 1) && !(i_dummy & 1) )
	{
		i_ratio >>= 1;
		i_dummy >>= 1;
		i_pgcd  <<= 1;
	}

	while( !(i_ratio % 3) && !(i_dummy % 3) )
	{
		i_ratio /= 3;
		i_dummy /= 3;
		i_pgcd  *= 3;
	}

	while( !(i_ratio % 5) && !(i_dummy % 5) )
	{
		i_ratio /= 5;
		i_dummy /= 5;
		i_pgcd  *= 5;
	}

	return i_pgcd;
}

static void AspectRatio( int i_aspect, int *i_aspect_x, int *i_aspect_y )
{
	unsigned int i_pgcd = ReduceHeight( i_aspect );
	*i_aspect_x = i_aspect / i_pgcd;
	*i_aspect_y = VOUT_ASPECT_FACTOR / i_pgcd;
}

/*****************************************************************************
* BinaryLog: computes the base 2 log of a binary value
*****************************************************************************
* This functions is used by MaskToShift, to get a bit index from a binary
* value.
*****************************************************************************/
static int BinaryLog( UINT32 i )
{
	int i_log = 0;

	if( i == 0 ) return -31337;

	if( i & 0xffff0000 ) i_log += 16;
	if( i & 0xff00ff00 ) i_log += 8;
	if( i & 0xf0f0f0f0 ) i_log += 4;
	if( i & 0xcccccccc ) i_log += 2;
	if( i & 0xaaaaaaaa ) i_log += 1;

	return i_log;
}

/*****************************************************************************
* MaskToShift: transform a color mask into right and left shifts
*****************************************************************************
* This function is used for obtaining color shifts from masks.
*****************************************************************************/
static void MaskToShift( int *pi_left, int *pi_right, UINT32 i_mask )
{
	UINT32 i_low, i_high;            /* lower hand higher bits of the mask */

	if( !i_mask )
	{
		*pi_left = *pi_right = 0;
		return;
	}

	/* Get bits */
	i_low = i_high = i_mask;

	i_low &= - (INT32)i_low;          /* lower bit of the mask */
	i_high += i_low;                    /* higher bit of the mask */

	/* Transform bits into an index. Also deal with i_high overflow, which
	* is faster than changing the BinaryLog code to handle 64 bit integers. */
	i_low =  BinaryLog (i_low);
	i_high = i_high ? BinaryLog (i_high) : 32;

	/* Update pointers and return */
	*pi_left =   i_low;
	*pi_right = (8 - i_high + i_low);
}

int convert_yuv_to_rgb_pixel(int y, int u, int v)
{
	unsigned int pixel32 = 0;
	unsigned char *pixel = (unsigned char *)&pixel32;
	int r, g, b;

	r = y + (1.370705 * (v-128));
	g = y - (0.698001 * (v-128)) - (0.337633 * (u-128));
	b = y + (1.732446 * (u-128));

	if(r > 255) r = 255;
	if(g > 255) g = 255;
	if(b > 255) b = 255;
	if(r < 0) r = 0;
	if(g < 0) g = 0;
	if(b < 0) b = 0;

	pixel[0] = r * 220 / 256;
	pixel[1] = g * 220 / 256;
	pixel[2] = b * 220 / 256;

	return pixel32;
}


int convert_yuv_to_rgb_buffer(unsigned char *yuv, unsigned char *rgb,
															unsigned int width, unsigned int height, unsigned int pitch)
{
	unsigned int in, out = 0;
	unsigned int pixel_16;
	unsigned char pixel_24[3];
	unsigned int pixel32;
	int y0, u, y1, v;
	int mb, line; /* mb: macroblock or 4 bytes (YUYV), pitch: line size */

	for(line = 0; line < height; line++) {
		for (mb = 0; mb < width / 2; mb++) {
			in = (pitch * line) + (mb * 4);
			pixel_16 =
				yuv[in + 3] << 24 |
				yuv[in + 2] << 16 |
				yuv[in + 1] <<  8 |
				yuv[in + 0];

			y0 = (pixel_16 & 0x000000ff);
			u  = (pixel_16 & 0x0000ff00) >>  8;
			y1 = (pixel_16 & 0x00ff0000) >> 16;
			v  = (pixel_16 & 0xff000000) >> 24;

			pixel32 = convert_yuv_to_rgb_pixel(y0, u, v);
			pixel_24[0] = (pixel32 & 0x000000ff);
			pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
			pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;

			rgb[out++] = pixel_24[2];
			rgb[out++] = pixel_24[1];
			rgb[out++] = pixel_24[0];

			pixel32 = convert_yuv_to_rgb_pixel(y1, u, v);
			pixel_24[0] = (pixel32 & 0x000000ff);
			pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
			pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;

			rgb[out++] = pixel_24[2];
			rgb[out++] = pixel_24[1];
			rgb[out++] = pixel_24[0];
		}
	}

	return 0;
}

int CVideoOutput::GetSnapShotPic(PBYTE rgb, int width, int height)
{
	int res=-1 ;
	tme_mutex_lock( &m_changeLock );		// wait output thread sleep
	if( m_pSnapshotpic ) {
		res=0 ;
		convert_yuv_to_rgb_buffer(m_pSnapshotpic->p[0].p_pixels, rgb,
			width, height, m_pSnapshotpic->p[0].i_pitch);
	}
	tme_mutex_unlock( &m_changeLock );
	return res ;
}

void CVideoOutput::ActivateChroma()
{    
	m_iChromaType = 0;
	if (!ActivateChromaRGB(this)) {
		m_iChromaType = CHROMA_I420_RGB;
	} else if (!ActivateChromaYUY2(this)) {
		m_iChromaType = CHROMA__I420_YUY2;
	}
}

void CVideoOutput::DeactivateChroma()
{
	if (m_chroma.p_base) free( m_chroma.p_base );
	if (m_chroma.p_offset) free( m_chroma.p_offset );
	if (m_chroma.p_buffer) free( m_chroma.p_buffer );
	//free( p_vout->chroma.p_sys );
}

// quick blur
static void blur( unsigned char *pix, int pitch, int pixwidth, int bx, int by, int bw, int bh, int radius, int shape ) 
{
    if (radius<1) return;
    int wm=bw-1;
    int hm=bh-1;
    int csum,x,y,i,p,yp,yi;
    int div=radius+radius+1;
    int vmin, vmax ;
    unsigned char * c = new unsigned char [bw*bh] ;

    int a, b, aa, bb ;
    a = bw/2 ;
    b = bh/2 ;
    aa = a*a ;
    bb = b*b ;

    yi=0;

    unsigned char * pixline = pix+by*pitch+(bx*pixwidth) ;
    for (y=0;y<bh;y++){
        csum=0;
        for(i=-radius;i<=radius;i++){
            p = min(wm, max(i,0));
            csum += pixline[p*pixwidth];
        }
        for (x=0;x<bw;x++){
            c[yi]=csum/div ;
            vmin = min(x+radius+1,wm) ;
            vmax = max(x-radius, 0 );
            csum += pixline[vmin*pixwidth] - pixline[vmax*pixwidth];
            yi++;
        }
        pixline += pitch ;
    }

    pixline = pix+by*pitch+(bx*pixwidth) ;

    for (x=0;x<bw;x++){
        csum=0;
        yp=-radius*bw;
        for(i=-radius;i<=radius;i++){
            yi=max(0,yp)+x;
            csum+=c[yi];
            yp+=bw ;
        }

        yi = 0 ;
        for (y=0;y<bh;y++){

            if( shape == 0 ){
                pixline[ yi ] = csum/div ;
            }
            else {
                int dx = x-a ;
                int dy = y-b ;
                if( 512*dx*dx/aa + 512*dy*dy/bb < 512 ) {
                    pixline[ yi ] = csum/div ;
                }
            }
            vmin = min(y+radius+1,hm)*bw;
            vmax = max(y-radius,0)*bw;
            csum+=c[x+vmin]-c[x+vmax];
            yi+=pitch;
        }
        pixline += pixwidth ;
    }

    delete c ;
}


void CVideoOutput::blurPicture(picture_t * pic)
{
    int i;
    int width, height, pitch;

    unsigned char * picbuf = pic->p[0].p_pixels ;
    if( picbuf == NULL ) {
        return ;
    }
    width = pic->p[0].i_visible_pitch / 2 ;
    height = pic->p[0].i_visible_lines ;
    pitch = pic->p[0].i_pitch ;
    for( i=0;i<m_ba_number;i++) {
        int bx, by, bw, bh, radius ;
        bx = m_blur_area[i].x*width / 256 ;
        by = m_blur_area[i].y*height / 256 ;
        bw = m_blur_area[i].w*width / 256 ;
        bh = m_blur_area[i].h*height / 256 ;
        radius = m_blur_area[i].radius*(width+height)/512 ;

        if(bx<0) bx=0 ;
        if(by<0) by=0 ;
        if(bw+bx>=width) {
            bw = width - bx ;
        }
        if(bh+by>=height) {
            bh = height - by ;
        }
        if( bw>3 && bh>3 && radius>3 && radius<bw && radius<bh ) {
            blur( (unsigned char *)picbuf,   pitch, 2, bx,   by, bw,   bh, radius/2, m_blur_area[i].shape );
            blur( (unsigned char *)picbuf,   pitch, 2, bx,   by, bw,   bh, radius/2, m_blur_area[i].shape );
            blur( (unsigned char *)picbuf+1, pitch, 4, bx/2, by, bw/2, bh, radius/2, m_blur_area[i].shape );
            blur( (unsigned char *)picbuf+3, pitch, 4, bx/2, by, bw/2, bh, radius/2, m_blur_area[i].shape );
        }
    }
}
