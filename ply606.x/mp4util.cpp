#include "stdafx.h"
#include "mp4util.h"
#include "dvrnet.h"
#include "trace.h"

typedef struct network_frame_header_s
{
	BYTE   i_id;
	BYTE   i_flag;
	UINT16 i_millisec;
    UINT32 i_sec;
} network_frame_header_t;

typedef struct network_playback_frame_header_s
{
	BYTE   i_id;
	BYTE   i_flag;
	UINT16 i_millisec;
    UINT32 i_sec;
    UINT32 i_size;
} network_playback_frame_header_t;

int copyOSD(block_t *pBlock, PBYTE buf, int size)
{
	PBYTE ptr;
	int toRead, lineSize, osdSize;
	int nText = 0;

	osdSize = 0;

	if ((size < 16) ||
		(buf[0] != 'T') ||
		(buf[1] != 'X') ||
		(buf[2] != 'T')) {
		return 0;
	}

	ptr = buf + 6;
	toRead = *((unsigned short *)ptr);
	osdSize = toRead + 8;

	if (osdSize > pBlock->i_buffer)
		return 0;

	memcpy(pBlock->p_buffer, buf, osdSize);

	return osdSize;
}

block_t *getMpegFrameDataFromPacket(PBYTE buf, int *size, char *vosc, int voscSize, int ch)
{
	struct Answer_type ans ;
	bool videoKey = false;
	int newsize, cursize;
	struct Answer_type *pAns;
	pAns = (struct Answer_type *)buf;
	network_frame_header_t *pHeader;
	block_t *pBlock, *pRet = NULL;
	int osd = 0;
	PBYTE dest;
	
	if (*size < sizeof(struct Answer_type)) {
		return NULL;
	}

	if (pAns->anscode != ANSSTREAMDATA) {
		TRACE(_T("-------------- error -----------------\n"));
		*size = 0;
	}

	cursize = pAns->anssize + sizeof(struct Answer_type);
	if (*size < cursize) {
		return NULL; // partial data, wait for more data.
	}

	//int tsize1, tsize2;
	//BYTE last1, last2;
	//TRACE(_T("(%d)size:%d,%02x,%02x,%02x,%02x,%d\n"), ch,*size, buf[0],buf[1],buf[2],buf[3], pAns->anssize);
	pHeader = (network_frame_header_t *)(buf + sizeof(struct Answer_type));
	//TRACE(_T("(%d)size:%d,%02x,%02x,%02x,%02x,%d\n"), ch,*size, buf[0],buf[1],buf[2],buf[3], pAns->anssize);
	if (((pHeader->i_id == '0') || (pHeader->i_id == '1')) &&
		((pHeader->i_flag & 0xf0) == 0x10 || (pHeader->i_flag & 0xf0) == 0x20) &&
		((pHeader->i_flag & 0x0f) == 0x00 || (pHeader->i_flag & 0x0f) == 0x01)) {
		newsize = pAns->anssize - 8;
		//tsize1 = newsize;
		//last1 = buf[pAns->anssize + 12 - 1];
		if (pHeader->i_flag == 0x11) {	// key frame
			newsize += voscSize;
			videoKey = true;
		}
		//tsize2 = newsize;
		pBlock = block_New(newsize);
		if (pBlock) {
			pBlock->i_buffer = newsize;
			if (videoKey) {
				osd = copyOSD(pBlock, (PBYTE)pHeader + 8, pAns->anssize - 8);
				memcpy(pBlock->p_buffer + osd, vosc, voscSize);
				dest = pBlock->p_buffer + osd + voscSize;
				memcpy(dest, (PBYTE)pHeader + 8 + osd, pAns->anssize - 8 - osd);
			} else {
				dest = pBlock->p_buffer;
				memcpy(dest, (PBYTE)pHeader + 8, pAns->anssize - 8);
			}

			__int64 ts = pHeader->i_sec * 1000000LL + pHeader->i_millisec * 1000LL;
			if (pHeader->i_flag == 0x11) {	// key frame
				pBlock->i_pts = 0;
				pBlock->i_dts = ts;
				pBlock->i_flags =  BLOCK_FLAG_TYPE_I ;
			} else if (pHeader->i_flag & 0x20) {
				pBlock->i_pts = ts;
				pBlock->i_dts = ts;
				pBlock->i_flags =  BLOCK_FLAG_TYPE_AUDIO ;
				pBlock->i_rate = 32000;
			} else if( pHeader->i_flag == 0x10 ) {
				pBlock->i_pts = 0;
				pBlock->i_dts = ts;
				pBlock->i_flags =  BLOCK_FLAG_TYPE_PB;
			}
			pRet = pBlock;
			//last2 = pBlock->p_buffer[pBlock->i_buffer - 1];
			//if (last1 != last2)
			//	last1 = last2;
		}
		/* copy remaining data */
		int remaining = *size - cursize;
		if (remaining > 0) {
			memmove(buf, buf + cursize, remaining);
		}
		*size = remaining;
	} else {
		/* TODO: handle this? */
		*size = 0;
	}

	return pRet;
}

int getCombinedAudioFrames(PBYTE dest, PBYTE buf, int size)
{
	int toread = size;
	int framesize = 0;
	network_playback_frame_header_t *pHeader;
	pHeader = (network_playback_frame_header_t *)buf;

	if (dest == NULL)
		return 0;

	if (pHeader->i_flag & 0x20 == 0)
		return 0;

	int cursize = pHeader->i_size + sizeof(network_playback_frame_header_t);
	while (toread >= cursize) {
		memcpy(dest + framesize, (PBYTE)pHeader + sizeof(network_playback_frame_header_t), pHeader->i_size);
		framesize += pHeader->i_size;
		toread -= cursize;
		if (toread < sizeof(network_playback_frame_header_t)) {
			break;
		}
		pHeader = (network_playback_frame_header_t *)((PBYTE)pHeader + cursize);
		cursize = pHeader->i_size + sizeof(network_playback_frame_header_t);
	}

	return framesize;
}

block_t *getMpegFrameDataFromPacket2(PBYTE buf, int size)
{
	int newsize;
	network_playback_frame_header_t *pHeader;
	pHeader = (network_playback_frame_header_t *)buf;
	block_t *pBlock = NULL;
	
	if (size < 12) {
		return NULL;
	}

	if (((pHeader->i_id == '0') || (pHeader->i_id == '1')) &&
		((pHeader->i_flag & 0xf0) == 0x10 || (pHeader->i_flag & 0xf0) == 0x20) &&
		((pHeader->i_flag & 0x0f) == 0x00 || (pHeader->i_flag & 0x0f) == 0x01)) {
		int real_size = pHeader->i_size;
		newsize = size - sizeof(network_playback_frame_header_t);
		pBlock = block_New(newsize);
		if (pBlock) {
			if (pHeader->i_flag & 0x20) {
				newsize = getCombinedAudioFrames(pBlock->p_buffer, (PBYTE)pHeader, size);
			} else {
				memcpy(pBlock->p_buffer, buf + sizeof(network_playback_frame_header_t), newsize);
				if (real_size != newsize)
					newsize = real_size;
			}
			pBlock->i_buffer = newsize;
			__int64 ts = pHeader->i_sec * 1000000LL + pHeader->i_millisec * 1000LL;
			if( (pHeader->i_flag == 0x11) ) {	// key frame
				pBlock->i_pts = 0;
				pBlock->i_dts = ts;
				pBlock->i_flags =  BLOCK_FLAG_TYPE_I ;
			} else if (pHeader->i_flag & 0x20) {
				pBlock->i_pts = ts;
				pBlock->i_dts = ts;
				pBlock->i_flags =  BLOCK_FLAG_TYPE_AUDIO ;
				pBlock->i_rate = 32000;
			} else if( pHeader->i_flag == 0x10 ) {
				pBlock->i_pts = 0;
				pBlock->i_dts = ts;
				pBlock->i_flags =  BLOCK_FLAG_TYPE_PB;
			}
			return pBlock;
		}
	}

	return pBlock;
}

static unsigned int get_field(unsigned char *d, int bits, int *offset)
{
	unsigned int v = 0;
	int i;

	for (i = 0; i < bits;)
		if (bits - i >= 8 && *offset % 8 == 0) {
			v <<= 8;
			v |= d[*offset/8];
			i += 8;
			*offset += 8;
		} else {
			v <<= 1;
			v |= ( d[*offset/8] >> ( 7 - *offset % 8 ) ) & 1;
			++i;
			++(*offset);
		}
	return v;
}

void parse_video_object_layer(unsigned char *d, int len,
															DWORD *vop_time_increment_resolution, DWORD *vop_time_increment,
															DWORD *width, DWORD *height)
{
	int off = 41, i;
	int verid = 1;

	if (get_field(d, 1, &off)) {
		verid = get_field(d, 4, &off);
		off += 3;
	}
	if (get_field(d, 4, &off) == 15) // aspect ratio info
		off += 16; // extended par
	if (get_field(d, 1, &off)) { // vol control parameters
		off += 3; // chroma format, low delay
		if (get_field(d, 1, &off))
			off += 79; // vbw parameters
	}
	off += 2; // video object layer shape
	if (!get_field(d, 1, &off)) {
		TRACE(_T("rtp-mpeg4: missing marker\n"));
		return;
	}
	*vop_time_increment_resolution = get_field(d, 16, &off);
	i = *vop_time_increment_resolution;
	if (!get_field(d, 1, &off)) {
		TRACE(_T("rtp-mpeg4: missing marker\n"));
		return;
	}
	int vtir_bitlen;
	for (vtir_bitlen = 0; i != 0; i >>= 1)
		++vtir_bitlen;
	if (get_field(d, 1, &off)) {
		*vop_time_increment = get_field(d, vtir_bitlen, &off);
		if (*vop_time_increment == 0) {
			TRACE(_T("rtp-mpeg4: fixed VOP time increment is 0!\n"));
			return;
		}
	} else
		*vop_time_increment = 1;
	if (!get_field(d, 1, &off)) {
		TRACE(_T("rtp-mpeg4: missing marker\n"));
		return;
	}
	*width = get_field(d, 13, &off);
	if (!get_field(d, 1, &off))
	{
		TRACE(_T("rtp-mpeg4: missing marker\n"));
		return;
	}
	*height = get_field(d, 13, &off);
	if (!get_field(d, 1, &off)) {
		TRACE(_T("rtp-mpeg4: missing marker\n"));
		return;
	}
	int interaced = get_field(d, 1, &off);

	off += verid == 1 ? 2 : 3;
	if (get_field(d, 1, &off))
		off += 8;
	if (get_field(d, 1, &off)) {
		TRACE(_T("rtp-mpeg4: can't handle custom quant table\n"));
		return;
	}
	if (verid != 1)
		++off;
	if (!get_field(d, 1, &off)) {
		int estimation_method = get_field(d, 2, &off);

		if (estimation_method == 0 || estimation_method == 1) {
			if (!get_field(d, 1, &off))
				off += 6;
			if (!get_field(d, 1, &off))
				off += 4;
			if (!get_field(d, 1, &off)) {
				TRACE(_T("rtp-mpeg4: missing marker\n"));
				return;
			}
			if (!get_field(d, 1, &off))
				off += 4;
			if (!get_field(d, 1, &off))
				off += 6;
			if (!get_field(d, 1, &off)) {
				TRACE(_T("rtp-mpeg4: missing marker\n"));
				return;
			}
			if (estimation_method == 1 && !get_field(d, 1, &off))
				off += 2;
		}
	}
	int resync_marker_off = get_field(d, 1, &off);
}
