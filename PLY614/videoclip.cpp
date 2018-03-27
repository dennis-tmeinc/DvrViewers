#include "stdafx.h"
#include <process.h>
#include "ply614.h"
#include "videoclip.h"
#include "dvrfilestream.h"
#include "utils.h"
#include "h264.h"
#include "adpcm.h"
#include "xvidenc.h"

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "xvidcore.dll.a")

int write_to_avifile(struct xvid_setup *xs, AVCodecContext *avc, FILE *fp, FILE *fpi, struct avi_info *ai, struct frame_cache *f);

#define MAX_CHANNEL 16
#define CACHE_BUF_SIZE 500

bool g_xvidlib_initialized;
xvid_gbl_init_t xvid_gbl_init;

struct frameperiod {
	DWORD numerator, denominator;
};

struct resolution {
	DWORD width, height;
};

struct avi_info {
	int video_count, audio_count, track_count;
	int max_video_size, max_audio_size;
	int body_size, index_size;
	struct frameperiod frameperiod;
	struct resolution resolution;
	int frame_size, block_align;
};

struct frame_cache
{
	int type;
	int len;
};

struct hik_state
{
	LONG		port;
	struct frame_cache *avf[CACHE_BUF_SIZE]; // interleaved frame circular buffer (h.264 video frame + decoded audio)
	struct frame_cache *vf[CACHE_BUF_SIZE]; // h.264 video frame circular buffer 
	int av_start, av_end, v_start, v_end;
	int v,a, v_sent, a_sent;
	CRITICAL_SECTION cs;
    dvrplayer * player ;
};
struct hik_state g_hs[MAX_CHANNEL];

static void init_hik_players()
{
	int i;
	for (i = 0; i < MAX_CHANNEL; i++) {
		g_hs[i].port = -1;
	}
}

static struct hik_state *find_hik_state(long nPort)
{
	int i;
	for (i = 0; i < MAX_CHANNEL; i++) {
		if (g_hs[i].port == nPort)
			return &g_hs[i];
	}

	return NULL;
}


// Find channel number from nPort
//    By Dennis, 10/05/2012
static int find_hik_channel(long nPort)
{
	int i;
	for (i = 0; i < MAX_CHANNEL; i++) {
		if (g_hs[i].port == nPort)
            return i ;
	}
    return -1 ;
}

/* must be called with mutex */
static bool is_avbuffer_full(struct hik_state *hs)
{
	return ((hs->av_end + 1) % CACHE_BUF_SIZE == hs->av_start);
}

/* must be called with mutex */
static bool is_vbuffer_full(struct hik_state *hs)
{
	return ((hs->v_end + 1) % CACHE_BUF_SIZE == hs->v_start);
}

/* make sure this is not called with full buffer */
int buffer_av_frame(struct hik_state *hs, char *pBuf, long nSize, long nType)
{
	EnterCriticalSection(&hs->cs);
	if (is_avbuffer_full(hs)) {
		LeaveCriticalSection(&hs->cs);
		return 1;
	}

	struct frame_cache *f = (struct frame_cache *)malloc(sizeof(int) * 2 + nSize);
	if (f) {
		f->type = nType;
		f->len = nSize;
		memcpy((PBYTE)f + sizeof(int) * 2, pBuf, nSize);
		hs->avf[hs->av_end] = f;
		hs->av_end = (hs->av_end + 1) % CACHE_BUF_SIZE;
	}

	LeaveCriticalSection(&hs->cs);
	return 0;
}

/* make sure this is not called with full buffer */
int buffer_av_frame(struct hik_state *hs, struct frame_cache *f)
{
	EnterCriticalSection(&hs->cs);
	if (is_avbuffer_full(hs)) {
		LeaveCriticalSection(&hs->cs);
		return 1;
	}

	hs->avf[hs->av_end] = f;
	hs->av_end = (hs->av_end + 1) % CACHE_BUF_SIZE;
	LeaveCriticalSection(&hs->cs);

	//TRACE(_T("buffer_av_frame:%d,%d\n"), hs->av_start,hs->av_end);
	return 0;
}

struct frame_cache * unbuffer_av_frame(struct hik_state *hs)
{
	EnterCriticalSection(&hs->cs);
	if (hs->av_end != hs->av_start) {
		struct frame_cache *f = hs->avf[hs->av_start];
		hs->av_start = (hs->av_start + 1) % CACHE_BUF_SIZE;
		LeaveCriticalSection(&hs->cs);
		return f;
	}
	LeaveCriticalSection(&hs->cs);

	return NULL;
}

/* make sure this is not called with full buffer */
int buffer_v_frame(struct hik_state *hs, struct frame_cache *f)
{
	EnterCriticalSection(&hs->cs);
	if (is_vbuffer_full(hs)) {
		LeaveCriticalSection(&hs->cs);
		return 1;
	}

	hs->vf[hs->v_end] = f;
	hs->v_end = (hs->v_end + 1) % CACHE_BUF_SIZE;
	LeaveCriticalSection(&hs->cs);

	return 0;
}

struct frame_cache * unbuffer_v_frame(struct hik_state *hs)
{
	EnterCriticalSection(&hs->cs);
	if (hs->v_end != hs->v_start) {
		struct frame_cache *f = hs->vf[hs->v_start];
		hs->v_start = (hs->v_start + 1) % CACHE_BUF_SIZE;
		LeaveCriticalSection(&hs->cs);
		return f;
	}
	LeaveCriticalSection(&hs->cs);

	return NULL;
}

void CALLBACK DecCBFunc(long nPort,char *pBuf,long nSize,FRAME_INFO *pFrameInfo,long nRes,long nRes2)
{
	struct hik_state *hs;

	//TRACE(_T("funcDecCB:%d, %d, %d\n"), pFrameInfo->nType, nSize, GetCurrentThreadId());
	hs = find_hik_state(nPort);
	if (hs == NULL)
		return;

	if (pFrameInfo->nType == T_AUDIO16) {
		hs->a++;
		buffer_av_frame(hs, pBuf, nSize, T_AUDIO16);
	} else {
        // avi blur
        if( pFrameInfo->nType == T_YV12 ) {
            // blur process here,  (by dennis)
            dvrplayer * player = hs->player ;
            int channel = find_hik_channel(nPort) ;
            player->BlurDecFrame( channel, pBuf, nSize, pFrameInfo->nWidth, pFrameInfo->nHeight, pFrameInfo->nType);
        }

		hs->v++;
		buffer_av_frame(hs, pBuf, nSize, pFrameInfo->nType);
	}
}

int mute_audio(int *ndev, DWORD **vol)
{
	int i, ret;
	WAVEOUTCAPS woc;

	*ndev = waveOutGetNumDevs();
	if (ndev == 0)
		return 0;

	*vol = new DWORD[*ndev];
	for (i = 0; i < *ndev; i++) {
		ret = waveOutGetDevCaps(i, &woc, sizeof(woc));
		if (ret != MMSYSERR_NOERROR) {
			TRACE(_T("waveOutGetDevCaps error:%d\n"), ret);
			continue;
		}
		ret = waveOutGetVolume((HWAVEOUT)i, &(*vol)[i]);
		if (woc.dwSupport & WAVECAPS_VOLUME) {
			ret = waveOutSetVolume((HWAVEOUT)i, 0);
		}
	}

	return 0;
}

int restore_audio(int ndev, DWORD *vol)
{
	int i, ret;
	WAVEOUTCAPS woc;

	for (i = 0; i < ndev; i++) {
		ret = waveOutGetDevCaps(i, &woc, sizeof(woc));
		if (ret != MMSYSERR_NOERROR) {
			TRACE(_T("waveOutGetDevCaps error:%d\n"), ret);
			continue;
		}
		if (woc.dwSupport & WAVECAPS_VOLUME) {
			ret = waveOutSetVolume((HWAVEOUT)i, vol[i]);
		}
	}

	return 0;
}

int setup_hik_player(struct hik_state *hs)
{
	DWORD ret;

	HIK_MEDIAINFO hmi;
	SecureZeroMemory(&hmi, sizeof(hmi));
	hmi.media_fourcc = FOURCC_HKMI;
	hmi.media_version = 0x0101;
	hmi.device_id = 1;
	hmi.system_format = SYSTEM_MPEG2_PS;
	hmi.video_format = VIDEO_H264;
	hmi.audio_format = AUDIO_G722_1;
	hmi.audio_channels = 1;
	hmi.audio_bits_per_sample = 14;
	hmi.audio_samplesrate = 16000;
	hmi.audio_bitrate = 16000;

	ret = PlayM4_GetPort(&hs->port);
	if (!ret) return 1;	
	ret =PlayM4_SetStreamOpenMode(hs->port, STREAME_REALTIME);	
	if (!ret) {
		PlayM4_FreePort(hs->port);
		return 1;
	}
	ret = PlayM4_OpenStream(hs->port, (PBYTE)&hmi, sizeof(hmi), SOURCEBUFSIZE );
	if (!ret) {
		PlayM4_FreePort(hs->port);
		return 1;
	}
	PlayM4_SetDecCallBackMend( hs->port, DecCBFunc, 0);
	PlayM4_ResetSourceBuffer(hs->port);
	PlayM4_Play( hs->port, NULL );
	PlayM4_PlaySound(hs->port);

	return 0;
}

void close_hik_player(struct hik_state *hs)
{
	PlayM4_Stop(hs->port);
	PlayM4_SetDecCallBackMend( hs->port, NULL, 0);
	PlayM4_CloseStream(hs->port);
	PlayM4_FreePort(hs->port);
	hs->port = -1;
}

int decode_rbsp_trailing(const uint8_t *src){
    int v= *src;
    int r;

    for(r=1; r<9; r++){
        if(v&1) return r;
        v>>=1;
    }
    return 0;
}

int decode_seq_parameter_set(SPS *sps, GetBitContext *gb){
    int profile_idc, level_idc;
    unsigned int sps_id;
    int i;

    profile_idc= get_bits(gb, 8);
    get_bits1(gb);   //constraint_set0_flag
    get_bits1(gb);   //constraint_set1_flag
    get_bits1(gb);   //constraint_set2_flag
    get_bits1(gb);   //constraint_set3_flag
    get_bits(gb, 4); // reserved
    level_idc= get_bits(gb, 8);
    sps_id= get_ue_golomb_31(gb);

    if(sps_id >= MAX_SPS_COUNT) {
        TRACE(_T("sps_id (%d) out of range\n"), sps_id);
        return -1;
    }

	sps->time_offset_length = 24;
    sps->profile_idc= profile_idc;
    sps->level_idc= level_idc;

    memset(sps->scaling_matrix4, 16, sizeof(sps->scaling_matrix4));
    memset(sps->scaling_matrix8, 16, sizeof(sps->scaling_matrix8));
    sps->scaling_matrix_present = 0;

    if(sps->profile_idc >= 100){ //high profile
        sps->chroma_format_idc= get_ue_golomb_31(gb);
        if(sps->chroma_format_idc == 3)
            sps->residual_color_transform_flag = get_bits1(gb);
        sps->bit_depth_luma   = get_ue_golomb(gb) + 8;
        sps->bit_depth_chroma = get_ue_golomb(gb) + 8;
        sps->transform_bypass = get_bits1(gb);
        //decode_scaling_matrices(h, sps, NULL, 1, sps->scaling_matrix4, sps->scaling_matrix8);
    }else{
        sps->chroma_format_idc= 1;
        sps->bit_depth_luma   = 8;
        sps->bit_depth_chroma = 8;
    }

    sps->log2_max_frame_num= get_ue_golomb(gb) + 4;
    sps->poc_type= get_ue_golomb_31(gb);

    if(sps->poc_type == 0){ //FIXME #define
        sps->log2_max_poc_lsb= get_ue_golomb(gb) + 4;
    } else if(sps->poc_type == 1){//FIXME #define
        sps->delta_pic_order_always_zero_flag= get_bits1(gb);
        sps->offset_for_non_ref_pic= get_se_golomb(gb);
        sps->offset_for_top_to_bottom_field= get_se_golomb(gb);
        sps->poc_cycle_length                = get_ue_golomb(gb);

        for(i=0; i<sps->poc_cycle_length; i++)
            sps->offset_for_ref_frame[i]= get_se_golomb(gb);
    }else if(sps->poc_type != 2){
        goto fail;
    }

    sps->ref_frame_count= get_ue_golomb_31(gb);
    if(sps->ref_frame_count > MAX_PICTURE_COUNT-2 || sps->ref_frame_count >= 32U){
        goto fail;
    }
    sps->gaps_in_frame_num_allowed_flag= get_bits1(gb);
    sps->mb_width = get_ue_golomb(gb) + 1;
    sps->mb_height= get_ue_golomb(gb) + 1;

    sps->frame_mbs_only_flag= get_bits1(gb);
    if(!sps->frame_mbs_only_flag)
        sps->mb_aff= get_bits1(gb);
    else
        sps->mb_aff= 0;

    sps->direct_8x8_inference_flag= get_bits1(gb);
    if(!sps->frame_mbs_only_flag && !sps->direct_8x8_inference_flag){
        TRACE(_T("This stream was generated by a broken encoder, invalid 8x8 inference\n"));
        goto fail;
    }

#ifndef ALLOW_INTERLACE
    if(sps->mb_aff)
        TRACE(_T("MBAFF support not included; enable it at compile-time.\n"));
#endif
    sps->crop= get_bits1(gb);
    if(sps->crop){
        sps->crop_left  = get_ue_golomb(gb);
        sps->crop_right = get_ue_golomb(gb);
        sps->crop_top   = get_ue_golomb(gb);
        sps->crop_bottom= get_ue_golomb(gb);
        if(sps->crop_left || sps->crop_top){
            TRACE(_T("insane cropping not completely supported, this could look slightly wrong ...\n"));
        }
        if(sps->crop_right >= 8 || sps->crop_bottom >= (8>> !sps->frame_mbs_only_flag)){
            TRACE(_T("brainfart cropping not supported, this could look slightly wrong ...\n"));
        }
    }else{
        sps->crop_left  =
        sps->crop_right =
        sps->crop_top   =
        sps->crop_bottom= 0;
    }

    sps->vui_parameters_present_flag= get_bits1(gb);
    if( sps->vui_parameters_present_flag )
        if (decode_vui_parameters(sps, gb) < 0)
            goto fail;

    if(!sps->sar.den)
        sps->sar.den= 1;
#if 0
    TRACE(_T("sps:%u profile:%d/%d poc:%d ref:%d %dx%d %s %s crop:%d/%d/%d/%d %s %s %d/%d\n"),
               sps_id, sps->profile_idc, sps->level_idc,
               sps->poc_type,
               sps->ref_frame_count,
               sps->mb_width, sps->mb_height,
               sps->frame_mbs_only_flag ? _T("FRM") : (sps->mb_aff ? _T("MB-AFF") : _T("PIC-AFF")),
               sps->direct_8x8_inference_flag ? _T("8B8") : _T(""),
               sps->crop_left, sps->crop_right,
               sps->crop_top, sps->crop_bottom,
               sps->vui_parameters_present_flag ? _T("VUI") : _T(""),
               ((const char*[]){"Gray","420","422","444"})[sps->chroma_format_idc],
               sps->timing_info_present_flag ? sps->num_units_in_tick : 0,
               sps->timing_info_present_flag ? sps->time_scale : 0
               );
#endif
    return 0;
fail:
    return -1;
}

int parse_sps(PBYTE sps_data, int sps_length, struct frameperiod *fp, struct resolution *res)
{
	GetBitContext gb;
	int bit_length;
    SPS sps;

	while (sps_data[sps_length - 1] == 0 && sps_length > 1)
		sps_length--;

	bit_length = !sps_length ? 0 : (8 * sps_length - decode_rbsp_trailing(sps_data + sps_length - 1));

	init_get_bits(&gb, sps_data, bit_length);

	if (!decode_seq_parameter_set(&sps, &gb)) {
		fp->numerator = sps.num_units_in_tick * 2;
		fp->denominator = sps.time_scale;
		res->width = sps.mb_width * 16;
		res->height = sps.mb_height * 16;
	}

	return 0;
}

static int remove_ps_header(PBYTE pes, int pes_size, PBYTE payload, struct frameperiod *fp, struct resolution *res)
{
	PBYTE ptr = pes;
	int payload_size = 0;
	int to_read = pes_size;

	if (to_read < 4)
		return 0;

	/* PS Pack header: skip it */
	if ((ptr[0] == 0) && (ptr[1] == 0) && (ptr[2] == 1) && (ptr[3] == 0xba)) {
		if (to_read < 14)
			return 0;
		int stuffing_bytes = ptr[13] & 0x07;
		if (to_read < 14 + stuffing_bytes)
			return 0;
		ptr += (14 + stuffing_bytes);
		to_read -= (14 + stuffing_bytes);
	}

	if (to_read < 4)
		return 0;

	/* PS Stream map header: skip it */
	if ((ptr[0] == 0) && (ptr[1] == 0) && (ptr[2] == 1) && (ptr[3] == 0xbc)) {
		if (to_read < 6)
			return 0;
		int len = ptr[4] << 8 | ptr[5];
		if (to_read < 6 + len)
			return 0;
		ptr += (6 + len);
		to_read -= (6 + len);
	}

	while (true) {
		if (to_read < 4)
			break;

		if ((ptr[0] == 0) && (ptr[1] == 0) && (ptr[2] == 1) && /* start code */
			(((ptr[3] & 0xe0) == 0xc0) || ((ptr[3] & 0xe0) == 0xe0))) { /* 110xxxxx : audio,  111xxxxx : video,(xxxxx being track no). */
			if (to_read < 9)
				break;
			int len = ptr[4] << 8 | ptr[5];
			int pes_header_len = ptr[8];
			if (to_read <= 9 + pes_header_len)
				break;
			if (((ptr[3] & 0xe0) == 0xe0) && /* video */
				(6 + len - 9 - pes_header_len > 5) && /* have NAL type */
				((*(ptr + 9 + pes_header_len + 4) & 0x1f) == 7)){
				if (!fp->numerator || !fp->denominator || !res->width || !res->height)
					parse_sps(ptr + 9 + pes_header_len + 5, 6 + len - 9 - pes_header_len - 5, fp, res);
			}
			if (payload) {
				memcpy(payload + payload_size, ptr + 9 + pes_header_len, 6 + len - 9 - pes_header_len);
			}
			payload_size += (6 + len - 9 - pes_header_len);
			ptr += (6 + len);
			to_read -= (6 + len);
		} else {
			/* bug in DVR firmware, more than 1 frame */
			break;
		}
	}

	return payload_size;
}

static int track_write_wavformat(struct avi_info *ai, unsigned char *hdr)
{
	SetWLE(hdr, 0x11); /* WAVCodecTag */
	SetWLE(hdr + 2, 1); /* audio_channels */
	SetDWLE(hdr + 4, 16000); /* sample_rate */
	SetDWLE(hdr + 8, 8000); /* sample_rate * audio_channels(1) * bits_per_sample / 8 */
	SetWLE(hdr + 12, ai->block_align); /* block_align */;
	SetWLE(hdr + 14, 4); /* bits_per_sample */
	SetWLE(hdr + 16, 2); /* WMP needs this */
	SetWLE(hdr + 18, ai->frame_size); /* WMP needs this */

	return 20;
}

static int track_write_header(struct avi_info *ai, int track,
		PBYTE hdr)
{
	unsigned int fourcc = 0;

	if (!track) { /* video */
		memcpy(hdr, "LIST", 4);
		SetDWLE(hdr + 4, 12 + 64 + 48 - 8);
		memcpy(hdr + 8, "strl", 4);
		memcpy(hdr + 12, "strh", 4);
		SetDWLE(hdr + 12 + 4, 64 - 8);
		memcpy(hdr + 12 + 8, "vids", 4);
		memcpy(hdr + 12 + 12, "xvid", 4);
		SetDWLE(hdr + 12 + 28, ai->frameperiod.numerator);
		SetDWLE(hdr + 12 + 32, ai->frameperiod.denominator);
		SetDWLE(hdr + 12 + 40, ai->video_count);
		SetDWLE(hdr + 12 + 44, ai->max_video_size);
		SetWLE(hdr + 12 + 60, ai->resolution.width);
		SetWLE(hdr + 12 + 62, ai->resolution.height);
		memcpy(hdr + 12 + 64, "strf", 4);
		SetDWLE(hdr + 12 + 64 + 4, 48 - 8);
		SetDWLE(hdr + 12 + 64 + 8, 48 - 8);
		SetDWLE(hdr + 12 + 64 + 12, ai->resolution.width);
		SetDWLE(hdr + 12 + 64 + 16, ai->resolution.height);
		SetDWLE(hdr + 12 + 64 + 20, 1);
		SetDWLE(hdr + 12 + 64 + 22, 24);
		memcpy(hdr + 12 + 64 + 24, "XVID", 4);
		SetDWLE(hdr + 12 + 64 + 28, ai->resolution.width *
				ai->resolution.height * 3);
		return 12 + 64 + 48;
	} else {
		int wavformat_len;

		wavformat_len = track_write_wavformat(ai, hdr + 12 + 64 + 8);
		memcpy(hdr, "LIST", 4);
		SetDWLE(hdr + 4, 4 + 64 + 8 + wavformat_len);
		memcpy(hdr + 8, "strl", 4);
		memcpy(hdr + 12, "strh", 4);
		SetDWLE(hdr + 12 + 4, 64 - 8);
		memcpy(hdr + 12 + 8, "auds", 4);
		SetDWLE(hdr + 12 + 12, 1); /* fccHandler, 0 for PCM,G.711, 1 for ADPCM */
		SetDWLE(hdr + 12 + 28, ai->frame_size); /* dwScale, 1 for PCM,G.711, frame_size for ADPCM */
		SetDWLE(hdr + 12 + 32, 16000); /* dwRate */
		SetDWLE(hdr + 12 + 40, ai->audio_count);
		SetDWLE(hdr + 12 + 44, 16000); /* at->audio_rate * at->audio_channels */
		SetDWLE(hdr + 12 + 52, ai->block_align); /* dwSampleSize, 1 for PCM,G.711, block_align for ADPCM */
		memcpy(hdr + 12 + 64, "strf", 4);
		SetDWLE(hdr + 12 + 64 + 4, wavformat_len);
		return 12 + 64 + 8 + wavformat_len;
	}
}

static void write_avi_headers(FILE *fp, struct avi_info *ai)
{
	unsigned char hdr[1024 + 12];
	unsigned int frame_usec;
	int off, i;

	memset(hdr, 0, sizeof(hdr));
	memcpy(hdr, "RIFF", 4);
	SetDWLE(hdr + 4, 1024 + 12 + ai->body_size + ai->index_size);
	memcpy(hdr + 8, "AVI ", 4);
	memcpy(hdr + 12, "LIST", 4);
	memcpy(hdr + 12 + 8, "hdrl", 4);
	memcpy(hdr + 12 + 12, "avih", 4);
	SetDWLE(hdr + 12 + 12 + 4, 64 - 8);
	frame_usec = 1000000ULL *
			(unsigned long long)ai->frameperiod.numerator /
			(unsigned long long)ai->frameperiod.denominator;
	SetDWLE(hdr + 12 + 12 + 8, frame_usec);
	SetDWLE(hdr + 12 + 12 + 20, 2320);
	SetDWLE(hdr + 12 + 12 + 24, ai->video_count);
	SetDWLE(hdr + 12 + 12 + 32, ai->track_count);
	SetDWLE(hdr + 12 + 12 + 36, 128*1024);
	SetDWLE(hdr + 12 + 12 + 40, ai->resolution.width);
	SetDWLE(hdr + 12 + 12 + 44, ai->resolution.height);

	off = 64;
	for (i = 0; i < ai->track_count; ++i)
		off += track_write_header(ai, i, hdr + 12 + 12 + off);
	SetDWLE(hdr + 12 + 4, 12 - 8 + off);

	memcpy(hdr + 12 + 12 + off, "JUNK", 4);
	SetDWLE(hdr + 12 + 12 + off + 4, 1024 - 12 - 12 - off - 8);
	memcpy(hdr + 1024, "LIST", 4);
	SetDWLE(hdr + 1024 + 4, ai->body_size + 4);
	memcpy(hdr + 1024 + 8, "movi", 4);
	fwrite(hdr, sizeof(char), 1024 + 12, fp);
}

static int create_dvr_file(class dvrplayer *player,
						   char *duppath, struct dup_state *dupstate,
						   struct dvrtime begintime,
						   struct dvrtime endtime,
						   unsigned char *crypt_table)
{
	struct DVRFILEHEADER fileheader ;
	struct DVRFILECHANNEL * pchannel ;
	struct DVRFILEHEADEREX fileheaderex ;
	struct DVRFILECHANNELEX * pchannelex ;
	struct DVRFILEKEYENTRY * pkeyentry ;

	int ch;
	FILE *dvrfile ;
	int progress_chmax, progress_chmin ;		// maximum/minimum progress on a channel
	int tdiff;									// total time lenght 
	int chdiff ;								
	char * framedata ;							// frame data
	int    framesize, frametype ;
	struct dvrtime frametime, firsttime, lasttime ;
	int    fsize ;								// frame size

	int headerlen;


	list <struct DVRFILEKEYENTRY> keyindex ;	// key frame index

	dvrfile = fopen(duppath, "wb");
	if( dvrfile==NULL ) {
		dupstate->status=-1 ;		// error on file creation
		return 1;
	}	

	player->m_errorstate = 2 ;							// pause player

	// set file header
	fileheader.flag = DVRFILEFLAG ;
	fileheader.totalchannels = player->m_playerinfo.total_channel;
	
	fileheader.begintime = (DWORD) dvrtime_mktime( &begintime);
	fileheader.endtime = (DWORD) dvrtime_mktime( &endtime ) ;

	// set file extra header
	fileheaderex.size = sizeof( fileheaderex );
	fileheaderex.flagex = DVRFILEFLAGEX ;
	fileheaderex.platform_type = DVRFILE_614 ;
	fileheaderex.version[0] = player->m_playerinfo.serverversion[0] ;
	fileheaderex.version[1] = player->m_playerinfo.serverversion[1] ;
	fileheaderex.version[2] = player->m_playerinfo.serverversion[2] ;
	fileheaderex.version[3] = player->m_playerinfo.serverversion[3] ;
	fileheaderex.encryption_type = 0 ;
	if( crypt_table ) {
		fileheaderex.encryption_type = ENC_MODE_RC4FRAME ;
	}
	fileheaderex.totalchannels = fileheader.totalchannels ;
	strncpy( fileheaderex.servername, player->m_playerinfo.servername, sizeof(fileheaderex.servername) );

	pchannel = new struct DVRFILECHANNEL [fileheader.totalchannels] ;
	pchannelex = new struct DVRFILECHANNELEX [fileheaderex.totalchannels] ;

	memset(pchannel, 0, fileheader.totalchannels * sizeof(struct DVRFILECHANNEL) );
	memset(pchannelex, 0, fileheaderex.totalchannels * sizeof(struct DVRFILECHANNELEX) );

	dupstate->percent=1 ;										// preset to 1%

	tdiff=dvrtime_diff(&begintime, &endtime);							// total time length

	// goto first channel data position
	_fseeki64(dvrfile, 
		  sizeof(fileheader) + 
			fileheader.totalchannels * sizeof(struct DVRFILECHANNEL) + 
			sizeof(fileheaderex) + 
			fileheaderex.totalchannels * sizeof(struct DVRFILECHANNELEX),
		  SEEK_SET );

	dvrtime_init(&firsttime, 2100);
	dvrtime_init(&lasttime, 1800);

	HIK_MEDIAINFO hmi;
	SecureZeroMemory(&hmi, sizeof(hmi));
	hmi.media_fourcc = FOURCC_HKMI;
	hmi.media_version = 0x0101;
	hmi.device_id = 1;
	hmi.system_format = SYSTEM_MPEG2_PS;
	hmi.video_format = VIDEO_H264;
	hmi.audio_format = AUDIO_G722_1;
	hmi.audio_channels = 1;
	hmi.audio_bits_per_sample = 14;
	hmi.audio_samplesrate = 16000;
	hmi.audio_bitrate = 16000;

	for( ch=0; ch<fileheader.totalchannels; ch++) {
		// initial channel header
		strncpy( pchannelex[ch].cameraname, player->m_channellist[ch].channelinfo.cameraname, sizeof(pchannelex[ch].cameraname));
		pchannel[ch].end = pchannel[ch].begin = 
			pchannelex[ch].begin = pchannelex[ch].end = _ftelli64(dvrfile) ;
		pchannelex[ch].size = sizeof( pchannelex[ch] ) ;
		pchannelex[ch].keybegin=pchannelex[ch].keyend = 0 ;

		// clean key index list
		keyindex.clean();

		// setup progress informaion
		progress_chmin= 100*ch/fileheader.totalchannels+1 ;
		progress_chmax= 100*(ch+1)/fileheader.totalchannels ;

		dupstate->percent = progress_chmin ;

		// create a file copy stream
		dvrstream * stream = player->newstream(ch);

		if( stream ) {
			// write stream header
			unsigned char encrypted_header[256];
			memcpy( encrypted_header, &hmi, sizeof(hmi));
			if( crypt_table ) {			
				// encrypt 264 header
				RC4_block_crypt( encrypted_header, sizeof(hmi), 0, crypt_table, 4096);
			}
			fwrite( encrypted_header, 1, sizeof(hmi), dvrfile);

			// seek to begin position
			if( stream->seek( &begintime )==DVR_ERROR_FILEPASSWORD ) {
				// set stream password
				stream->setpassword(ENC_MODE_RC4FRAME, player->m_password, sizeof(player->m_password) );	// set frameRC4 password
				stream->seek( &begintime );													// seek again
			}

			framedata=NULL ;
			int error = 0;
			while( 1 ) {
				int dataok = stream->getdata( &framedata, &framesize, &frametype, &headerlen );
				if (!dataok) {
					error++;
					if (error > 100) {
						break;
					} else {
						Sleep(10);
						continue;
					}
				}
				error = 0;
				//TRACE(_T("ch:%d, %d, %d\n"), ch, frametype, framesize);
				stream->gettime( &frametime ) ;

				// find the begin time
				if( dvrtime_compare( &frametime, &firsttime )<0 ) {
					firsttime = frametime;
				}

				chdiff = dvrtime_diff(&begintime, &frametime) ;
				//TRACE(_T("time:%d/%d/%d,%d:%d:%d.%d\n"),frametime.year,frametime.month,frametime.day,frametime.hour, frametime.min, frametime.second,frametime.millisecond);

				if(chdiff>=tdiff) {				// up to totol length in time
					free(framedata);
					break;
				}

				// find the end time
				if( dvrtime_compare( &frametime, &lasttime )>0 ) {
					lasttime = frametime;
				}

				// check key frame (harry)
				if( frametype==FRAME_TYPE_KEYVIDEO ) {
					pkeyentry=keyindex.append();
					pkeyentry->offset = _ftelli64(dvrfile)-pchannelex[ch].begin;
					pkeyentry->time = chdiff ;
				}

				if( crypt_table ) {
					// encrypt frame data
					fsize = framesize - headerlen ;
					if( fsize>1024 ) {
						fsize=1024 ;
					}
					if (headerlen <= framesize) {
						RC4_block_crypt( (unsigned char *)framedata+headerlen, 
										 fsize,
										 0, crypt_table, 4096);
					}
				}
				// write frame data to file
				fwrite( framedata, 1, framesize, dvrfile);
				// release frame data
				free(framedata) ;
				framedata=NULL ;

				if( dupstate->update ) {
					dupstate->update=0;
					dupstate->percent = progress_chmin + chdiff*100/tdiff/fileheader.totalchannels ;
					sprintf(dupstate->msg, "Channel %d - %02d:%02d:%02d", 
						ch, 
						frametime.hour, 
						frametime.min, 
						frametime.second );
				}

				if( dupstate->cancel || dupstate->status<0 ) {		// cancelled or error?
						break;
				}
			}

			// delete file copy stream ;
			delete stream ;
		}
		
		// channel end position
		pchannel[ch].end = pchannelex[ch].end = _ftelli64(dvrfile) ;

		// write key index
		if( keyindex.size()>0 ) {
			pchannelex[ch].keybegin = _ftelli64(dvrfile) ;
			fwrite( keyindex.at(0), sizeof( struct DVRFILEKEYENTRY ), keyindex.size(), dvrfile );
			pchannelex[ch].keyend = _ftelli64(dvrfile);
		}

		if( dupstate->cancel || dupstate->status<0 ) {		// cancelled or error?
			break;
		}
	}

	fileheaderex.date = firsttime.year * 10000 + firsttime.month*100 + firsttime.day ;
	fileheaderex.time = firsttime.hour * 3600 + firsttime.min * 60 + firsttime.second ;
	//fileheaderex.millisec = firsttime.millisecond;
	tdiff = dvrtime_diffms(&firsttime, &lasttime);
	fileheaderex.length = tdiff / 1000;
	//fileheaderex.len_millisec = tdiff % 1000;

	// write completed file header
	_fseeki64( dvrfile, 0, SEEK_SET );
	fwrite( &fileheader, sizeof(fileheader), 1, dvrfile);			// write file header
	fwrite( pchannel, sizeof( struct DVRFILECHANNEL ), fileheader.totalchannels, dvrfile );		// old channel header
	fwrite( &fileheaderex, sizeof(fileheaderex), 1, dvrfile);		// extra file header
	fwrite( pchannelex, sizeof( struct DVRFILECHANNELEX), fileheader.totalchannels, dvrfile );	// extra channel header
	fclose(dvrfile);

	// release memory for channel header
	delete pchannel ;
	delete pchannelex ;

	if( dupstate->cancel || dupstate->status<0 ) {		// cancelled or error?
		remove(duppath);								// remove the file
	}
	player->m_errorstate = 0 ;						

	return 0;
}
//#define TEST_
//FILE *fpt, *fpx;
// recording one channel data
// return 
//       >=0 : copied bytes
//       <0  : error
static int avifile_record_channel(class dvrplayer *player, int ch,
						   char *duppath, struct dup_state *dupstate,
						   struct dvrtime begintime,
						   struct dvrtime endtime, int ch_progress_base, int ch_progress_max)
{
	int    framesize, frametype ;
	struct dvrtime frametime, firsttime, lasttime ;
	int headerlen;
	int tdiff;									// total time lenght 
	int chdiff ;								
	char * framedata ;							// frame data
	FILE *fp, *fpi;
	char filename[MAX_PATH], indexname[MAX_PATH], suffix[16];
	char *ptr;
	struct avi_info ai;
	int bytes;
	struct frame_cache *f;
	AVCodecContext avc;
	mtime_t lt = 0LL;

	struct xvid_setup xs;
	xvid_setup_default(&xs);
	xvidlib_init(1, &xs);

#ifdef TEST_
	BYTE tbuf[60];
	fopen_s(&fpt, "D:\\test.wav", "rb");
	fopen_s(&fpx, "D:\\test1.wav", "wb");
	fread(tbuf, 1, 60, fpt);
	fwrite(tbuf, 1, 60, fpx);
#endif

	// filename for avi file (with _CH suffix)
	strcpy_s(filename, sizeof(filename), duppath);
	ptr = strrchr(filename, '.'); 
	if (ptr == NULL)
		return -1;

	*ptr = '\0';
	sprintf_s(suffix, sizeof(suffix), "_CH%d.avi", ch + 1);
	strcat_s(filename, sizeof(filename), suffix);

	// filename for temporary index file (with _CH suffix)
	strcpy_s(indexname, sizeof(indexname), duppath);
	ptr = strrchr(indexname, '.'); 
	*ptr = '\0';
	sprintf_s(suffix, sizeof(suffix), "_CH%d.k", ch + 1);
	strcat_s(indexname, sizeof(indexname), suffix);

	// open avi & index file
	if (fopen_s(&fp, filename, "wb"))
		return -1;

	_fseeki64(fp, 1024 + 12, SEEK_SET );

	if (fopen_s(&fpi, indexname, "w+b")) {
		fclose(fp);
		return -1;
	}

	memset(&ai, 0, sizeof(struct avi_info));
	memset(&g_hs[ch], 0, sizeof(struct hik_state));
	setup_hik_player(&g_hs[ch]);

    // set player
    g_hs[ch].player = player ;     // by Dennis

	InitializeCriticalSection(&g_hs[ch].cs);
	adpcm_encode_init(&avc);
	ai.frame_size = avc.frame_size;
	ai.block_align = avc.block_align;

	tdiff=dvrtime_diff(&begintime, &endtime);							// total time length

	// create a file copy stream
	dvrstream * stream = player->newstream(ch);

	if( stream ) {
		// seek to begin position
		if( stream->seek( &begintime )==DVR_ERROR_FILEPASSWORD ) {
			// set stream password
			stream->setpassword(ENC_MODE_RC4FRAME, player->m_password, sizeof(player->m_password) );	// set frameRC4 password
			stream->seek( &begintime );													// seek again
		}

		framedata=NULL ;
		int error = 0;
		int chunk_size;
		while( 1 ) {
			int dataok = stream->getdata( &framedata, &framesize, &frametype, &headerlen );
			if (!dataok) {
				error++;
				if (error > 100) {
					break;
				} else {
					Sleep(10);
					continue;
				}
			}
			error = 0;
			//TRACE(_T("ch:%d, %d, %d\n"), ch, frametype, framesize);
			stream->gettime( &frametime ) ;

			// find the begin time
			if( dvrtime_compare( &frametime, &firsttime )<0 ) {
				firsttime = frametime;
			}

			chdiff = dvrtime_diff(&begintime, &frametime) ;
			//TRACE(_T("time:%d/%d/%d,%d:%d:%d.%d\n"),frametime.year,frametime.month,frametime.day,frametime.hour, frametime.min, frametime.second,frametime.millisecond);

			if(chdiff>=tdiff) {				// up to totol length in time
				free(framedata);
				break;
			}

			// find the end time
			if( dvrtime_compare( &frametime, &lasttime )>0 ) {
				lasttime = frametime;
			}

			if ((frametype == FRAME_TYPE_KEYVIDEO) || (frametype == FRAME_TYPE_VIDEO)) {
				if (ai.resolution.width == 0) {
					struct frameperiod fp;
					struct resolution res;
					remove_ps_header((PBYTE)framedata, framesize, NULL, &ai.frameperiod, &ai.resolution);
					xs.width = ai.resolution.width;
					xs.height = ai.resolution.height;
					if (xvid_init(&xs)) {
						xs.enc_handle = NULL;
					}
				}
			}

			while(true) {
				if (PlayM4_InputData(g_hs[ch].port, (PBYTE)framedata, (DWORD)framesize))
					break;
				DWORD dwErr = PlayM4_GetLastError(g_hs[ch].port);
				TRACE(_T("Inputdata error:%d\n"),dwErr);
				if (dwErr != PLAYM4_BUF_OVER)
					break;

				// fix hang issue , by Dennis
				if( PlayM4_GetSourceBufferRemain(g_hs[ch].port)<10 )
					break ;

				Sleep(10);
			}
			if ((frametype == FRAME_TYPE_KEYVIDEO) || (frametype == FRAME_TYPE_VIDEO)) {
				g_hs[ch].v_sent++;
			} else if (frametype == FRAME_TYPE_AUDIO) {
				g_hs[ch].a_sent++;
			}
			free(framedata) ;
			framedata=NULL ;

			while ((f = unbuffer_av_frame(&g_hs[ch]))) {
				write_to_avifile(&xs, &avc, fp, fpi, &ai, f);
				//TRACE(_T("unbuffer:%d,%d\n"),f->type, f->len);
				free(f);
				lt = mdate(); // mark successful unbuffer time
			}

			if( dupstate->update ) {
				dupstate->update=0;
				dupstate->percent = ch_progress_base + (ch_progress_max - ch_progress_base) * chdiff / tdiff;
				sprintf(dupstate->msg, "Channel %d - %02d:%02d:%02d", 
					ch, 
					frametime.hour, 
					frametime.min, 
					frametime.second );
			}

			if( dupstate->cancel || dupstate->status<0 ) {		// cancelled or error?
					break;
			}
		}

		// delete file copy stream ;
		delete stream ;
	}
		
	while (true) {
		f = unbuffer_av_frame(&g_hs[ch]);
		if (f) {
			//TRACE(_T("unbuffer2:%d,%d\n"),f->type, f->len);
			write_to_avifile(&xs, &avc, fp, fpi, &ai, f);
			free(f);
			lt = mdate(); // mark successful unbuffer time
		} else {
			//if ((g_hs[ch].a == g_hs[ch].a_sent) && (g_hs[ch].v == g_hs[ch].v_sent))
			if ((g_hs[ch].a == ai.audio_count) && (g_hs[ch].v == ai.video_count))
				break;
			mtime_t now = mdate();
			mtime_t diff = now - lt;
			if ((lt > 0LL) && (diff > 1000000LL))
				break; // sometime shit happens.
			Sleep(0);
		}
	}

	TRACE(_T("a:%d,v:%d,a_sent:%d,v_sent:%d\n"),
		g_hs[ch].a, g_hs[ch].v, g_hs[ch].a_sent, g_hs[ch].v_sent);

	/* copy index data to the end of the avi file */
	if (ai.body_size > 0) {
		BYTE buf[2048];
		int bytes;

		memcpy(buf, "idx1", 4);
		SetDWLE(buf + 4, ai.index_size);
		fwrite(buf, sizeof(char), 8, fp);
		_fseeki64(fpi, 0, SEEK_SET );
		while ((bytes = fread(buf, sizeof(char), sizeof(buf), fpi)) > 0) {
			//_RPT1(_CRT_WARN, "%d bytes read\n", bytes);
			fwrite(buf, sizeof(char), bytes, fp);
		//_RPT1(_CRT_WARN, "%d bytes written\n", bytes);
		}
	}
	fclose(fpi);

	if (ai.body_size > 0) {
		_fseeki64(fp, 0, SEEK_SET );
		write_avi_headers(fp, &ai);
	}
	fclose(fp);

	remove(indexname);
	if (ai.body_size == 0) {
		remove(filename);
	}

	adpcm_encode_close(&avc);
	xvid_close(&xs);
	close_hik_player(&g_hs[ch]);
	DeleteCriticalSection(&g_hs[ch].cs);

	TRACE(_T("avi end ch %d\n"), ch);
#ifdef TEST_
	fclose(fpx);
	fclose(fpt);
#endif
	return 0;
}

static int create_avi_file(class dvrplayer *player,
						   char *duppath, struct dup_state *dupstate,
						   struct dvrtime begintime,
						   struct dvrtime endtime)
{
	int ch, totalch ;
	int progress_chmax, progress_chmin ;		// maximum/minimum progress on a channel
	DWORD *vol;
	int nWaveOutDev;

	mute_audio(&nWaveOutDev, &vol);

	player->m_errorstate = 2 ;							// pause player

	init_hik_players();

	totalch = player->m_playerinfo.total_channel;
	dupstate->percent=1 ;					// assume 1% task done

	for (ch = 0; ch < totalch; ch++) {
		// setup progress informaion
		progress_chmin = 100 * ch / totalch + 1;
		progress_chmax = 100 * (ch + 1) / totalch ;
		dupstate->percent = progress_chmin ;

		// start recording channel data
		if (avifile_record_channel(player, ch, duppath, dupstate,
				begintime, endtime, progress_chmin, progress_chmax) < 0)
			break;

		if( dupstate->cancel || dupstate->status < 0 ) {		// cancelled or error?
			break;
		}
	}

	player->m_errorstate = 0 ;	

	if (nWaveOutDev) {
		restore_audio(nWaveOutDev, vol);
		delete [] vol;
	}

	return 0;
}

unsigned WINAPI dvrfile_save_thread(void *p)
{
	int filetype;
	bool enc;
	unsigned char k256[256];
	unsigned char crypt_table[4096];
	struct dvrfile_save_info * cpinfo=(struct dvrfile_save_info *)p ;
	class dvrplayer *player = cpinfo->player;
	struct dvrtime begintime = *(cpinfo->begintime) ;
	struct dvrtime endtime = *(cpinfo->endtime) ;
	char *duppath = cpinfo->duppath ;
	int flags = cpinfo->flags ;
	struct dup_state *dupstate= cpinfo->dupstate;
	filetype = cpinfo->filetype;
	enc = false;
	if( cpinfo->enc_password ) {
		enc = true;
		key_256( cpinfo->enc_password, k256);
		RC4_crypt_table( crypt_table, 4096, k256 );
	}

	SetEvent(cpinfo->hRunEvent);						// let calling function go

	if (player->m_playerinfo.total_channel == 0) {
		dupstate->status = -1;
		return 1;
	}

	if (filetype == FILETYPE_UNKNOWN) {
		dupstate->status = -1;
	} else if (filetype == FILETYPE_DVR) {
		create_dvr_file(player, duppath, dupstate, begintime, endtime,
			enc ? crypt_table : NULL);
	} else {
		create_avi_file(player, duppath, dupstate, begintime, endtime);
	}

	dupstate->msg[0]='\0';								// empty progress message

	if( dupstate->cancel || dupstate->status<0 ) {		// cancelled or error?
		if( dupstate->status==0 )
			dupstate->status=-1 ;						// Error!
	} else {
		dupstate->percent=100 ;		// 100% finish
		dupstate->status=1 ;		// success
	}

	_endthreadex(0);
	return 0;
}

int write_to_avifile(struct xvid_setup *xs, AVCodecContext *avc,
					 FILE *fp, FILE *fpi, struct avi_info *ai, struct frame_cache *f)
{
	int bytes;
	BYTE hdr[16];

	int frametype = f->type;
	int chunk_size = f->len;
	PBYTE payload = (PBYTE)f + sizeof(int) * 2;
	if (frametype == T_YV12 && xs->enc_handle) {		// video frames?
		int key;
		bytes = xvid_decode(xs, payload, &key);
		if (bytes > 0) {
			chunk_size = bytes;
			payload = xs->mp4_buffer;
			frametype = FRAME_TYPE_VIDEO;
			if (key)
				frametype = FRAME_TYPE_KEYVIDEO;
			hdr[0] = '0';
			hdr[1] = '0'; // track number
			hdr[2] = 'd';
			hdr[3] = 'c'; // video chunk
			ai->video_count++;
			if (chunk_size > ai->max_video_size)
				ai->max_video_size = chunk_size;
			if (ai->track_count < 1)
				ai->track_count = 1;
		} else {
			chunk_size = 0;
		}
	} else if (frametype == T_AUDIO16) {
		//int c = 0;
		int audio_size = chunk_size;
		chunk_size = 0;
		//TRACE(_T("T_AUDIO16\n"));
		while (audio_size > 0) { /* not necessary */
			/* copy raw data to work buffer */
			int to_copy = avc->work_buf_size - avc->data_size;
			if (to_copy > audio_size) {
				to_copy = audio_size;
			}
			if (to_copy > 0) {
				//TRACE(_T("copying %d (%d)\n"), to_copy, avc->data_size);
				memcpy(avc->work_buf + avc->data_size, payload, to_copy);
				avc->data_size += to_copy;
				audio_size -= to_copy;
				payload += to_copy;
			}
			/* check if there is enough raw data for one encoded audio frame */
			if (avc->data_size >= avc->work_buf_size) {
				bytes = adpcm_encode_frame(avc, avc->frame_buf, avc->work_buf_size, avc->work_buf);	
				avc->data_size = 0;
				chunk_size = bytes;
				//TRACE(_T("audio encoding done\n"));
				//c++;
				//if (c > 1) { // will not happen
				//	TRACE(_T("XXXXX\n"));
				//}
				/* write to avi */
				hdr[0] = '0';
				hdr[1] = '1'; // track number
				hdr[2] = 'w';
				hdr[3] = 'b'; // audio chunk
				ai->audio_count++ ;
				if (bytes > ai->max_audio_size)
					ai->max_audio_size = bytes;
				if (ai->track_count < 2)
					ai->track_count = 2;
			}
		}
		payload = avc->frame_buf;
#ifdef TEST_
		fwrite(payload, 1, chunk_size, fpx);
#endif
	} else {
		return 1;
	}

	if (chunk_size) {
		SetDWLE(hdr +  4, ((frametype==FRAME_TYPE_KEYVIDEO) || (frametype==T_AUDIO16)) ? 0x10 : 0);
		SetDWLE(hdr +  8, ai->body_size + 4); // offset relative to the 1st byte of "movi" 
		SetDWLE(hdr + 12, chunk_size);
		bytes = fwrite(hdr, sizeof(char), 16, fpi);
		ai->index_size += 16;

		// write avi chunk header
		SetDWLE(hdr + 4, chunk_size);
		bytes = fwrite(hdr, sizeof(char), 8, fp);		
		bytes = fwrite(payload, sizeof(char), chunk_size, fp);

		if (chunk_size & 0x1) {
			BYTE pad = 0;
			bytes = fwrite(&pad, sizeof(char), 1, fp);
			chunk_size++;
		}
		ai->body_size += (chunk_size + 8);
	}

	return 0;
}
