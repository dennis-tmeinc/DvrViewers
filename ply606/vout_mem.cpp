#include "stdafx.h"
#include <stdio.h>
#include <crtdbg.h>
#include <errno.h>
#include <process.h>
#include <shellapi.h>
#include "vout_mem.h"
#include "vout_picture.h"
#include "trace.h"
#include "fourcc.h"

CVoutMem::CVoutMem(tme_object_t *pIpc, video_format_t *p_fmt, int iPtsDelay)
			: CVideoOutput(pIpc, NULL, p_fmt, iPtsDelay, false)
{
	//tme_thread_init(&m_ipc);
}

CVoutMem::~CVoutMem()
{
	if (m_hThread) {
		m_bDie = true;    
		tme_thread_join(m_hThread);
	}

	CloseVideo();
	//tme_mutex_destroy(&(m_ipc.object_lock));
	//tme_cond_destroy(&(m_ipc.object_wait));
}

int CVoutMem::OpenVideo()
{
	InitThread();
	//StartThread();
	return SUCCESS;
}

void CVoutMem::ShowVideoWindow(int nCmdShow)
{
}

void CVoutMem::CloseVideo()
{
	int i;

	for (i = 0; i < I_OUTPUTPICTURES; i++) {
		picture_t *p_pic;
		p_pic = PP_OUTPUTPICTURE[i];
		if (p_pic->p_data_orig)
			free(p_pic->p_data_orig);
	}

	EndThread();
}

int CVoutMem::Init()
{
	int i_index;
	picture_t *p_pic;

	/* Initialize the output structure */
	m_output.i_chroma = m_render.i_chroma;
	m_output.pf_setpalette = NULL;
	m_output.i_width  = m_render.i_width;  
	m_output.i_height = m_render.i_height;
	m_output.i_aspect = m_output.i_width * VOUT_ASPECT_FACTOR / m_output.i_height;

	m_output.i_rmask = 0xff0000;
	m_output.i_gmask = 0x00ff00;
	m_output.i_bmask = 0x0000ff;
  
	/* Try to initialize 1 direct buffer */
	p_pic = NULL;

	/* Find an empty picture slot */
	for( i_index = 0 ; i_index < VOUT_MAX_PICTURES ; i_index++ ) {
		if( m_pPicture[ i_index ].i_status == FREE_PICTURE ) {
			p_pic = m_pPicture + i_index;      
			break;
		}
	}

	/* Allocate the picture */
	if( p_pic == NULL ) {
		return EGENERIC;  
	}

	vout_AllocatePicture( p_pic, m_output.i_chroma,
                          m_output.i_width, m_output.i_height,
                          m_output.i_aspect );

	if( p_pic->i_planes == 0 ) {    
		return EGENERIC;	
	}

	p_pic->i_status = DESTROYED_PICTURE;
	p_pic->i_type   = DIRECT_PICTURE;

	PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;
	I_OUTPUTPICTURES++;
  
	return SUCCESS;
}

/*****************************************************************************
 * End: terminate Sys video thread output method
 *****************************************************************************
 * Terminate an output method created by Create.
 * It is called at the end of the thread.
 *****************************************************************************/
void CVoutMem::End()
{
}

int CVoutMem::Manage() {
    return SUCCESS;
}
void CVoutMem::Render(picture_t *p_pic) 
{
}

void CVoutMem::Display(picture_t *p_pic)
{  
	video_format_t fmt_in = {0}, fmt_out = {0};
}

int CVoutMem::Lock(picture_t *p)
{
	return SUCCESS;
}

int CVoutMem::Unlock(picture_t *p)
{
	return SUCCESS;
}


