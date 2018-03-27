#ifndef _FFMPEG_H_
#define _FFMPEG_H_

#include "tme_thread.h"
#include "block.h"
#include "es.h"
#include "vout_picture.h"
#include "decoder.h"

#define EMULATE_INTTYPES
#include <avcodec.h>
#include <avformat.h>

#define VOUT_ASPECT_FACTOR              432000

struct video_sys_t
{
    /* Common part between video and audio decoder */
    int i_cat;
    int i_codec_id;
    char *psz_namecodec;

    AVCodecContext      *p_context;
    AVCodec             *p_codec;

    /* Video decoder specific part */
    mtime_t input_pts; /* pts to pass to avcodec library */
    mtime_t input_dts; /* dts to pass to avcodec library */
    mtime_t i_pts;     

    AVFrame          *p_ff_pic;
    BITMAPINFOHEADER *p_format;

    /* for frame skipping algo */
    int b_hurry_up;
    int i_frame_skip;

    /* how many decoded frames are late */
    int     i_late_frames;
    mtime_t i_late_frames_start;

    /* for direct rendering */
    int b_direct_rendering;

    bool b_has_b_frames;

    /* Hack to force display of still pictures */
    bool b_first_frame;

    int i_buffer_orig, i_buffer;
    char *p_buffer_orig, *p_buffer;

    /* Postprocessing handle */
    void *p_pp;
    bool b_pp;
    bool b_pp_async;
    bool b_pp_init;
};

class CInput;

class CFFmpeg : public CDecoder
{
public:
	CFFmpeg(tme_object_t *pIpc, HWND hwnd, es_format_t *fmt, CInputStream *pInput, bool initializeAvcodec = true, bool bOwnThread = true);
	~CFFmpeg();

	virtual int OpenDecoder(); // should be called by the user after instantiation
	virtual picture_t *DecodeVideo( block_t **pp_block );
	virtual aout_buffer_t *DecodeAudio( block_t **pp_block );
	virtual subpicture_t *DecodeSubtitle( block_t **pp_block );

	video_sys_t * m_pVideoDec;
	static tme_mutex_t m_lock;

	int DecoderThread();

private:
	bool m_initializeAvcodec;
	static bool m_bFFmpegInit, m_bFFmpegInit2;
	//static tme_object_t m_ipc; 

	void InitLibavcodec();
	int InitVideoDec(AVCodecContext *p_context, AVCodec *p_codec, 
						  int i_codec_id, char *psz_namecodec);
	void InitVideoCodec();
	void InitCodec();
};

#endif