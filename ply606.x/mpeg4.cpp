#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <fcntl.h>
#include <io.h>
#include "mpeg4.h"
#include "player.h"
#include "trace.h"
#include "crypt.h"

static void ParseStreamHeader( BYTE trackid, BYTE flags,
                               unsigned int *pi_track_number, unsigned int *pi_track_type );
static INT64 GetTimeStampAt(mp4_track_t *tk, int idx);
int ps_pkt_size( BYTE *p, int i_peek );

static psblock_t *psblock_New(psblock_t *pBlock, PBYTE p_start, int i_size )
{
    //psblock_t *p_block = (psblock_t *)malloc( sizeof( psblock_t ));

    //if( p_block == NULL ) return NULL;
    psblock_t *p_block = pBlock;

    /* Fill all fields */
    p_block->i_pts          = 0;
    p_block->i_dts          = 0;
    p_block->i_buffer       = p_block->i_buf = i_size;
    p_block->p_buffer       = p_block->p_buf = p_start;

	return p_block;
}

static void psblock_Release(psblock_t *p_block)
{
#if 0
	if (p_block) {
		//if (p_block->p_buf) free(p_block->p_buf);
		free(p_block);
	}
#endif
}

/* Init a set of track */
static void ps_track_init( ps_track_t tk[PS_TK_COUNT] )
{
    int i;
    for( i = 0; i < PS_TK_COUNT; i++ )
    {
        tk[i].b_seen = false;
        tk[i].i_skip = 0;
        tk[i].i_id   = 0;
        tk[i].i_first_pts = -1;
        tk[i].i_last_pts = -1;
    }
}

static int getFrameType(psblock_t *p_pkt, PBYTE *payload)
{
    BYTE *p = p_pkt->p_buf;
	if ((p_pkt->i_buf < 9) ||
		(p[0] != 0x00) ||
		(p[1] != 0x00) ||
		(p[2] != 0x01) ||
		(p[3] < 0xc0)) {
			return 0;
	}
	
	// find where the NAL unit starts
	int packetLen = p[4] << 8 | p[5];
	int headerLen = p[8];

	// sanity check
	if (packetLen + 6 < p_pkt->i_buf)
		return 0;
	if (headerLen + 9 + 5 > p_pkt->i_buf)
		return 0;

	BYTE *nal = p + headerLen + 9;

	if (payload)
		*payload = nal;

	if (p[3] == 0xc0) {
		return FRAME_TYPE_AUDIO;
	}

	if ((nal[0] != 0x00) ||
		(nal[1] != 0x00) ||
		(nal[2] != 0x00) ||
		(nal[3] != 0x01)) {
			return FRAME_TYPE_UNKNOWN;
	}

	if (p[3] == 0xe0) {
		int type = nal[4] & 0x1f;
		if (type == 5)
			return FRAME_TYPE_KEYVIDEO;
		else if (type == 1)
			return FRAME_TYPE_VIDEO;
		return FRAME_TYPE_UNKNOWN;
	}

	return FRAME_TYPE_UNKNOWN;
}

static int getNewFrame(psblock_t *p_pkt, char **framedata, int *framesize)
{
	*framesize = 0;

	if (!p_pkt || !p_pkt->i_buf)
		return 1;

	*framedata = new char [p_pkt->i_buf];
	*framesize = p_pkt->i_buf;
	memcpy(*framedata, p_pkt->p_buf, p_pkt->i_buf);

	return 0;
}

/* parse a PACK PES */
int ps_pkt_parse_pack( psblock_t *p_pkt, INT64 *pi_scr,
                                     int *pi_mux_rate )
{
    BYTE *p = p_pkt->p_buffer;
    if( p_pkt->i_buffer >= 14 && (p[4] >> 6) == 0x01 )
    {
        *pi_scr =((((INT64)p[4]&0x38) << 27 )|
                  (((INT64)p[4]&0x03) << 28 )|
                   ((INT64)p[5] << 20 )|
                  (((INT64)p[6]&0xf8) << 12 )|
                  (((INT64)p[6]&0x03) << 13 )|
                   ((INT64)p[7] << 5 )|
                   ((INT64)p[8] >> 3 )) * 100 / 9;

        *pi_mux_rate = ( p[10] << 14 )|( p[11] << 6 )|( p[12] >> 2);
    }
    else if( p_pkt->i_buffer >= 12 && (p[4] >> 4) == 0x02 )
    {
        *pi_scr =((((INT64)p[4]&0x0e) << 29 )|
                   ((INT64)p[5] << 22 )|
                  (((INT64)p[6]&0xfe) << 14 )|
                   ((INT64)p[7] <<  7 )|
                   ((INT64)p[8] >> 1 )) * 100 / 9;

        *pi_mux_rate = ( ( p[9]&0x7f )<< 15 )|( p[10] << 7 )|( p[11] >> 1);
    }
    else
    {
        return EGENERIC;
    }
    return SUCCESS;
}

/* Parse a SYSTEM PES */
int ps_pkt_parse_system( psblock_t *p_pkt, /*ps_psm_t *p_psm,*/
                                       ps_track_t tk[PS_TK_COUNT] )
{
    BYTE *p = &p_pkt->p_buffer[6 + 3 + 1 + 1 + 1];

    /* System header is not useable if it references private streams (0xBD)
     * or 'all audio streams' (0xB8) or 'all video streams' (0xB9) */
    while( p < &p_pkt->p_buffer[p_pkt->i_buffer] )
    {
        int i_id = p[0];

        if( p[0] >= 0xBC || p[0] == 0xB8 || p[0] == 0xB9 ) p += 2;
        p++;

        if( i_id >= 0xc0 )
        {
            int i_tk = PS_ID_TO_TK( i_id );

            if( !tk[i_tk].b_seen )
            {
                //if( !ps_track_fill( &tk[i_tk], p_psm, i_id ) )
                //{
                    tk[i_tk].b_seen = true;
                //}
            }
        }
    }
    return SUCCESS;
}

/* Parse a PES (and skip i_skip_extra in the payload) */
int ps_pkt_parse_pes( psblock_t *p_pes, int i_skip_extra )
{
    BYTE header[30];
    int     i_skip  = 0;

    memcpy( header, p_pes->p_buffer, min( p_pes->i_buffer, 30 ) );

    switch( header[3] )
    {
        case 0xBC:  /* Program stream map */
        case 0xBE:  /* Padding */
        case 0xBF:  /* Private stream 2 */
        case 0xB0:  /* ECM */
        case 0xB1:  /* EMM */
        case 0xFF:  /* Program stream directory */
        case 0xF2:  /* DSMCC stream */
        case 0xF8:  /* ITU-T H.222.1 type E stream */
            i_skip = 6;
            break;

        default:
            if( ( header[6]&0xC0 ) == 0x80 )
            {
                /* mpeg2 PES */
                i_skip = header[8] + 9;

                if( header[7]&0x80 )    /* has pts */
                {
                    p_pes->i_pts = ((mtime_t)(header[ 9]&0x0e ) << 29)|
                                    (mtime_t)(header[10] << 22)|
                                   ((mtime_t)(header[11]&0xfe) << 14)|
                                    (mtime_t)(header[12] << 7)|
                                    (mtime_t)(header[13] >> 1);

                    if( header[7]&0x40 )    /* has dts */
                    {
                         p_pes->i_dts = ((mtime_t)(header[14]&0x0e ) << 29)|
                                         (mtime_t)(header[15] << 22)|
                                        ((mtime_t)(header[16]&0xfe) << 14)|
                                         (mtime_t)(header[17] << 7)|
                                         (mtime_t)(header[18] >> 1);
                    }
                }
            }
            else
            {
                i_skip = 6;
                while( i_skip < 23 && header[i_skip] == 0xff )
                {
                    i_skip++;
                }
                if( i_skip == 23 )
                {
                    return EGENERIC;
                }
                if( ( header[i_skip] & 0xC0 ) == 0x40 )
                {
                    i_skip += 2;
                }

                if(  header[i_skip]&0x20 )
                {
                     p_pes->i_pts = ((mtime_t)(header[i_skip]&0x0e ) << 29)|
                                     (mtime_t)(header[i_skip+1] << 22)|
                                    ((mtime_t)(header[i_skip+2]&0xfe) << 14)|
                                     (mtime_t)(header[i_skip+3] << 7)|
                                     (mtime_t)(header[i_skip+4] >> 1);

                    if( header[i_skip]&0x10 )    /* has dts */
                    {
                         p_pes->i_dts = ((mtime_t)(header[i_skip+5]&0x0e ) << 29)|
                                         (mtime_t)(header[i_skip+6] << 22)|
                                        ((mtime_t)(header[i_skip+7]&0xfe) << 14)|
                                         (mtime_t)(header[i_skip+8] << 7)|
                                         (mtime_t)(header[i_skip+9] >> 1);
                         i_skip += 10;
                    }
                    else
                    {
                        i_skip += 5;
                    }
                }
                else
                {
                    i_skip += 1;
                }
            }
    }

    i_skip += i_skip_extra;

    if( p_pes->i_buffer <= i_skip )
    {
        return EGENERIC;
    }

    p_pes->p_buffer += i_skip;
    p_pes->i_buffer -= i_skip;

    p_pes->i_dts = 100 * p_pes->i_dts / 9;
    p_pes->i_pts = 100 * p_pes->i_pts / 9;

    return SUCCESS;
}

int stream_peek( struct ps_stream *s, BYTE *pBuf, int size )
{
	__int64 pos;
	size_t n;

	pos = _ftelli64(s->fp);

	n = fread(pBuf, 1, size, s->fp);
	_fseeki64(s->fp, pos, SEEK_SET);

	return n;
}

int stream_peek_from_buffer( struct ps_buffer *s, BYTE *pBuf, int size )
{
	size_t n;

	n = s->pStart + s->size - s->pCurrent; // bytes left

	if (n > size) {
		n = size; // bytes to read
	}

	memcpy(pBuf, s->pCurrent, n);

	return n;
}

__int64 stream_tell( struct ps_stream *s)
{
	return _ftelli64(s->fp);
}

int stream_seek( struct ps_stream *s, __int64 i_pos)
{
	return _fseeki64(s->fp, i_pos, SEEK_SET);
}

__int64 stream_size( struct ps_stream *s)
{
	__int64 pos;

	if (s->end == 0) {
		pos = _ftelli64(s->fp);
		_fseeki64(s->fp, 0LL, SEEK_END);
		s->end = _ftelli64(s->fp);
		_fseeki64(s->fp, pos, SEEK_SET);	
	}
	return s->end;
}


int stream_read( struct ps_stream *s, BYTE *pBuf, int size )
{
	__int64 pos;
	size_t n;

	if (pBuf == NULL) {
		pos = _ftelli64(s->fp);
		if (s->end == 0) {
			_fseeki64(s->fp, 0LL, SEEK_END);
			s->end = _ftelli64(s->fp);
			_fseeki64(s->fp, pos, SEEK_SET);			
		}
		_fseeki64(s->fp, size, SEEK_CUR);
		if ((int)(s->end - pos) < size) {
			return (int)(s->end - pos);
		}
		return size;
	}

	n = fread(pBuf, 1, size, s->fp);

	return n;
}

int stream_read_from_buffer( struct ps_buffer *s, BYTE *pBuf, int size )
{
	__int64 pos;
	size_t n;

	n = s->pStart + s->size - s->pCurrent; // bytes left
	if (n > size) {
		n = size; // bytes to read
	}

	if (pBuf == NULL) {
		s->pCurrent += n;
		return n;
	}

	memcpy(pBuf, s->pCurrent, n);
	s->pCurrent += n;

	return n;
}

/* ps_pkt_resynch: resynch on a system starcode
 *  It doesn't skip more than 512 bytes
 *  -1 -> error, 0 -> not synch, 1 -> ok
 */
int ps_pkt_resynch( struct ps_stream *s, UINT32 *pi_code )
{
    BYTE peek[512], *p_peek;
    int     i_peek;
    int     i_skip;

	if( stream_peek( s, peek, 4 ) < 4 )
    {
        return -1;
    }
    if( peek[0] == 0 && peek[1] == 0 && peek[2] == 1 &&
        peek[3] >= 0xb9 )
    {
        *pi_code = 0x100 | peek[3];
        return 1;
    }

    if( ( i_peek = stream_peek( s, peek, 512 ) ) < 4 )
    {
        return -1;
    }
    i_skip = 0;

	p_peek = peek;
    for( ;; )
    {
        if( i_peek < 4 )
        {
            break;
        }
        if( p_peek[0] == 0 && p_peek[1] == 0 && p_peek[2] == 1 &&
            p_peek[3] >= 0xb9 )
        {
            *pi_code = 0x100 | p_peek[3];
            return stream_read( s, NULL, i_skip ) == i_skip ? 1 : -1;
        }

        p_peek++;
        i_peek--;
        i_skip++;
    }
    return stream_read( s, NULL, i_skip ) == i_skip ? 0 : -1;
}

/* ps_pkt_resynch_from_buffer: resynch on a system starcode
 *  It doesn't skip more than 512 bytes
 *  -1 -> error, 0 -> not synch, 1 -> ok
 */
int ps_pkt_resynch_from_buffer( struct ps_buffer *s, UINT32 *pi_code )
{
    BYTE peek[512], *p_peek;
    int     i_peek;
    int     i_skip;

	if( stream_peek_from_buffer( s, peek, 4 ) < 4 )
    {
        return -1;
    }
    if( peek[0] == 0 && peek[1] == 0 && peek[2] == 1 &&
        peek[3] >= 0xb9 )
    {
        *pi_code = 0x100 | peek[3];
        return 1;
    }

    if( ( i_peek = stream_peek_from_buffer( s, peek, 512 ) ) < 4 )
    {
        return -1;
    }
    i_skip = 0;

	p_peek = peek;
    for( ;; )
    {
        if( i_peek < 4 )
        {
            break;
        }
        if( p_peek[0] == 0 && p_peek[1] == 0 && p_peek[2] == 1 &&
            p_peek[3] >= 0xb9 )
        {
            *pi_code = 0x100 | p_peek[3];
            return stream_read_from_buffer( s, NULL, i_skip ) == i_skip ? 1 : -1;
        }

        p_peek++;
        i_peek--;
        i_skip++;
    }
    return stream_read_from_buffer( s, NULL, i_skip ) == i_skip ? 0 : -1;
}

/* return the id of a PES (should be valid) */
int ps_pkt_id( psblock_t *p_pkt )
{
    if( p_pkt->p_buffer[3] == 0xbd &&
        p_pkt->i_buffer >= 9 &&
        p_pkt->i_buffer >= 9 + p_pkt->p_buffer[8] )
    {
        return 0xbd00 | p_pkt->p_buffer[9+p_pkt->p_buffer[8]];
    }
    return p_pkt->p_buffer[3];
}

/* return the size of the next packet
 * XXX you need to give him at least 14 bytes (and it need to start as a
 * valid packet) */
int ps_pkt_size( BYTE *p, int i_peek )
{
    if( p[3] == 0xb9 && i_peek >= 4 )
    {
        return 4;
    }
    else if( p[3] == 0xba )
    {
        if( (p[4] >> 6) == 0x01 && i_peek >= 14 )
        {
            return 14 + (p[13]&0x07);
        }
        else if( (p[4] >> 4) == 0x02 && i_peek >= 12 )
        {
            return 12;
        }
        return -1;
    }
    else if( i_peek >= 6 )
    {
        return 6 + ((p[4]<<8) | p[5] );
    }
    return -1;
}

CMpegPs::CMpegPs()
: m_idx(NULL), m_track(NULL)
{
	Init();
}

CMpegPs::~CMpegPs()
{
	int i;

	if (m_idx)
		free(m_idx);

	if (m_track) {
		for (i = 0; i < m_iTrack; i++) {
		  mp4_track_t  *tk;
			tk = m_track[i];
			if (tk) {
				if (tk->p_index)
					free (tk->p_index);
				free(tk);
			}
		}
		free(m_track);
	}
}

void CMpegPs::Clear()
{
	int i;

	if (m_idx) {
		free(m_idx);
		m_idx = NULL;
	}

	if (m_track) {
		for (i = 0; i < m_iTrack; i++) {
		  mp4_track_t  *tk;
			tk = m_track[i];
			if (tk) {
				if (tk->p_index) {
					free (tk->p_index);
					tk->p_index = NULL;
				}
				free(tk);
				tk = NULL;
			}
		}
		free(m_track);
		m_track = NULL;
	}
}

void CMpegPs::Init()
{
	Clear();
	//memset(m_filename, 0, sizeof(m_filename));
	m_iIdxCount = 0;
	m_idx = NULL;
	m_iTrack = 0;
	m_track = NULL;
	m_iMoviLastchunkPos = 0;
	m_bLostSync = false;
	m_iLength = -1;
	memset(&m_stream, 0, sizeof(struct ps_stream));
	memset(&m_buffer, 0, sizeof(struct ps_buffer));
	m_iScr = -1;
	m_iMuxRate = 0;
	m_iCurrentPts = 0;
	m_bufSize = 0;
	m_iFlag = 0;
	m_pCryptTable = NULL;
	m_iMpegHeaderSize = 0;
	ps_track_init( m_tk );
    es_format_Init( &m_fmtAudio, AUDIO_ES, 0 );
    es_format_Init( &m_fmtVideo, VIDEO_ES, 0 );
    es_format_Init( &m_fmtText, SPU_ES, 0 );
}

void CMpegPs::SetStream(FILE *fp)
{
	m_stream.fp = fp;
	m_stream.end = 0;
}
#if 0
// used to locate the index(.k) file
void CMpegPs::SetFilename(char *filename)
{
	strcpy_s(m_filename, sizeof(m_filename), filename);
}
#endif
void CMpegPs::SetEncrptTable(unsigned char * pCryptTable, int size)
{
	m_pCryptTable = pCryptTable;
	m_encryptSize = size;
}

/* header is passed already decrypted.
 * SetStream, SetFilename, SetEncrptTable should be called first
 */
int CMpegPs::ParseFileHeader(struct mp5_header *header)
{
	unsigned int i_track = 0, i;

	memcpy(&m_fileHeader, header, sizeof(struct mp5_header));

	if (m_fileHeader.vidh.fourcc == 0x68727473)
		i_track++;
	if (m_fileHeader.audh.fourcc == 0x68727473)
		i_track++;

	if( m_fileHeader.stream_info.streams != i_track )
    {
        _RPT2(_CRT_WARN, 
                  "found %d stream but %d are declared\n",
                  i_track, m_fileHeader.stream_info.streams );

		if (i_track > m_fileHeader.stream_info.streams)
			i_track = m_fileHeader.stream_info.streams;
    }
    if( i_track == 0 )
    {
        _RPT0(_CRT_WARN, "no stream defined!\n" );
        return 1;
    }

    /* now read info on each stream and create ES */
    for( i = 0 ; i < i_track; i++ )
    {
		struct trackHeader *p_strh;
		if (i == 0)
			p_strh = &m_fileHeader.vidh;
		else if (i == 1)
			p_strh = &m_fileHeader.audh;
		else
			continue;

		mp4_track_t      *tk = ( mp4_track_t * )malloc( sizeof( mp4_track_t ) );

        tk->b_activated = false;
        tk->p_index     = 0;
        tk->i_idxnb     = 0;
        tk->i_idxmax    = 0;
        tk->i_idxposc   = 0;
        tk->i_idxposb   = 0;


		tk->i_rate  = p_strh->rate;
        tk->i_scale = p_strh->scale;
		tk->i_samplesize = p_strh->samplesize;
        _RPT4(_CRT_WARN, "stream[%d] rate:%d scale:%d samplesize:%d\n",
                 i, tk->i_rate, tk->i_scale, tk->i_samplesize );

		switch( p_strh->type )
        {
            case( 0x73647561 ):
                tk->i_cat   = AUDIO_ES;
				tk->i_codec = m_fileHeader.sndh.wFormatTag == 7 ?
					TME_FOURCC( 'm', 'l', 'a', 'w' ) : TME_FOURCC( 'a', 'l', 'a', 'w' );
                es_format_Init( &m_fmtAudio, AUDIO_ES, tk->i_codec );

				m_fmtAudio.audio.i_channels        = m_fileHeader.sndh.nChannels;
                m_fmtAudio.audio.i_rate            = m_fileHeader.sndh.nSamplesPerSec;
                m_fmtAudio.i_bitrate               = m_fileHeader.sndh.nAvgBytesPerSec*8;
                m_fmtAudio.audio.i_blockalign      = m_fileHeader.sndh.nBlockAlign;
                m_fmtAudio.audio.i_bitspersample   = m_fileHeader.sndh.wBitsPerSample;
                m_fmtAudio.i_extra = m_fileHeader.sndh.cbSize;
                m_fmtAudio.p_extra = NULL;
				break;

            case( 0x73646976 ):
                tk->i_cat   = VIDEO_ES;
                tk->i_codec = TME_FOURCC('m','p','4','v'); // check bmih.biCompression 
                es_format_Init( &m_fmtVideo, VIDEO_ES, m_fileHeader.imgh.bmih.biCompression );
                
                tk->i_samplesize = 0;
                m_fmtVideo.video.i_width  = m_fileHeader.imgh.bmih.biWidth;
                m_fmtVideo.video.i_height = m_fileHeader.imgh.bmih.biHeight;
                m_fmtVideo.video.i_bits_per_pixel = m_fileHeader.imgh.bmih.biBitCount;
                m_fmtVideo.video.i_frame_rate = tk->i_rate;
                m_fmtVideo.video.i_frame_rate_base = tk->i_scale;
                m_fmtVideo.i_extra = 0;
                m_fmtVideo.p_extra = NULL;
                break;

			case( TME_FOURCC('t','x','t','s')):
                tk->i_cat   = SPU_ES;
                tk->i_codec = TME_FOURCC( 's', 'u', 'b', 't' );
                _RPT1(_CRT_WARN, "stream[%d] subtitles\n", i );
                es_format_Init( &m_fmtText, SPU_ES, tk->i_codec );
                break;

            default:
                _RPT1(_CRT_WARN, "stream[%d] unknown type\n", i );
                free( tk );
                continue;
        }

		if( m_iTrack > 0 ) {	
			m_track = (mp4_track_t **)realloc( m_track, sizeof( void ** ) * ( m_iTrack + 1 ) ); 
		} else {
			m_track = (mp4_track_t **)malloc( sizeof( void ** ) );
		}
		m_track[m_iTrack] = tk;
		m_iTrack++;
    }

    if( m_iTrack <= 0 )
    {
        _RPT0(_CRT_WARN, "no valid track\n" );
        return 1;
    }

	unsigned char *start, *vol, buf[256];
	int size;

	// change for OSD_SUPPORT (1024)
	/* read video object layer */
	stream_seek(&m_stream, 1024);
	size = stream_peek(&m_stream, buf, sizeof(buf));

	if (size <= 10) {
		return 1; // invalid file.
	}
	
	if ((buf[0] == 0x30) && (buf[1] == 0x11) &&
		(buf[8] == 0x00) && (buf[9] == 0x00)) {
		start = buf + 8;
		size -= 12; // skip stream header(8b), and leave 00 00 01 b6(4b) behind
		for (i = 0; i < size; i++) {
			if ((start[i] == 0x00) && (start[i+1] == 0x00) &&
				(start[i+2] == 0x01) && (start[i+3] == 0xb6)) {
					memcpy(m_mpegHeader, start, i);
					m_iMpegHeaderSize = i;
					break;
			}
		}
	}
	

	return 0;
}

void CMpegPs::SetBuffer(PBYTE pBuf, int size)
{
	m_buffer.pStart = m_buffer.pCurrent = pBuf;
	m_buffer.size = size;
}

int CMpegPs::PktResynch(UINT32 *i_code)
{
	if (m_stream.fp) {
		return ps_pkt_resynch(&m_stream, i_code);
	} else if (m_buffer.pStart) {
		return ps_pkt_resynch_from_buffer(&m_buffer, i_code);
	}

	return -1;
}

int CMpegPs::Demux2(bool b_end, int *found_ts)
{
    int i_ret, i_id;
    UINT32 i_code;
    psblock_t *p_pkt;

    i_ret = PktResynch(&i_code );
    if( i_ret < 0 )
    {
        return 0;
    }
    else if( i_ret == 0 )
    {
		if( !m_bLostSync )
            _RPT0(_CRT_WARN, "garbage at input, trying to resync...\n" );

        m_bLostSync = true;
        return 1;
    }

    if( m_bLostSync ) _RPT0(_CRT_WARN, "found sync code\n" );
    m_bLostSync = false;

    if( ( p_pkt = PktRead( i_code ) ) == NULL )
    {
        return 0;
    }
    if( (i_id = ps_pkt_id( p_pkt )) >= 0xc0 )
    {
		int tkidx = ((i_id) <= 0xff ? (i_id) - 0xc0 : ((i_id)&0xff) + (256 - 0xc0));
        ps_track_t *tk = &m_tk[tkidx];
        if( !ps_pkt_parse_pes( p_pkt, tk->i_skip ) )
        {
            if( b_end && p_pkt->i_pts > tk->i_last_pts )
            {
                tk->i_last_pts = p_pkt->i_pts;
            }
            else if ( tk->i_first_pts == -1 )
            {
                tk->i_first_pts = p_pkt->i_pts;
				if (found_ts) *found_ts = p_pkt->i_pts;
            }
        }
    }
    psblock_Release( p_pkt );

    return 1;
}

int CMpegPs::copyToBuffer(psblock_t *p_pkt)
{
	if (!p_pkt || !p_pkt->i_buf)
		return 1;
	if (sizeof (m_buf) - m_bufSize < p_pkt->i_buf)
		return 1;

	//memcpy(m_buf + m_bufSize, p_pkt->p_buf, p_pkt->i_buf);
	m_bufSize += p_pkt->i_buf;

	return 0;
}
#if 0
int CMpegPs::Demux(int *frametype, __int64 *timestamp, int *headerlen)
{
    int i_ret, i_id, i_mux_rate;
    UINT32 i_code;
    psblock_t *p_pkt;

	i_ret = PktResynch( &i_code );
    if( i_ret < 0 )
    {
        return -1;
    }
    else if( i_ret == 0 )
    {
        if( !m_bLostSync )
            _RPT0(_CRT_WARN, "garbage at input, trying to resync...\n" );

        m_bLostSync = true;
        return 1;
    }

    if( m_bLostSync ) _RPT0(_CRT_WARN, "found sync code\n" );
    m_bLostSync = false;

    if( (m_iLength < 0) && (m_stream.fp) )
        FindLength(NULL);

	if( ( p_pkt = PktRead( i_code ) ) == NULL )
    {
        return 0;
    }

    switch( i_code )
    {
    case 0x1b9:
		copyToBuffer(p_pkt);
		psblock_Release( p_pkt );
        break;

    case 0x1ba:
		copyToBuffer(p_pkt);
        if( !ps_pkt_parse_pack( p_pkt, &m_iScr, &i_mux_rate ) )
        {
            if( !m_bHavePack ) m_bHavePack = true;
            if( i_mux_rate > 0 ) m_iMuxRate = i_mux_rate;
        }
        psblock_Release( p_pkt );
        break;

    case 0x1bb:
		/* we don't get this */
		copyToBuffer(p_pkt);
        psblock_Release( p_pkt );
        break;

    case 0x1bc:
		m_iFlag |= GOT_PSM;
		copyToBuffer(p_pkt);
        psblock_Release( p_pkt );
        break;

    default:
        if( (i_id = ps_pkt_id( p_pkt )) >= 0xc0 )
        {
            bool b_new = false;
            ps_track_t *tk = &m_tk[PS_ID_TO_TK(i_id)];

            if( !tk->b_seen )
            {
                tk->b_seen = true;
            }

            m_iScr = -1;

            if( tk->b_seen &&
                !ps_pkt_parse_pes( p_pkt, tk->i_skip ) )
            {
				if( (INT64)p_pkt->i_pts > m_iCurrentPts )
                {
                    m_iCurrentPts = (INT64)p_pkt->i_pts;
                }

				copyToBuffer(p_pkt);
				PBYTE payload;
				*frametype = getFrameType(p_pkt, &payload);
				if (headerlen) {
					if ((*frametype == FRAME_TYPE_KEYVIDEO) ||
						(*frametype == FRAME_TYPE_VIDEO)) {
							// header includes NAL header + type
						*headerlen = payload - m_buf + 5;
					} else if (*frametype == FRAME_TYPE_AUDIO) {
						*headerlen = payload - m_buf;
					}
				}
				if (p_pkt->i_pts)
					*timestamp = p_pkt->i_pts;
                psblock_Release( p_pkt );
            }
            else
            {
                psblock_Release( p_pkt );
            }
        }
        else
        {
            psblock_Release( p_pkt );
        }
        break;
    }

    return 1;
}
#endif
void CMpegPs::IndexAddEntry( int i_stream, mp4_entry_t *p_index)
{
    mp4_track_t *tk = m_track[i_stream];

    /* Update i_movi_lastchunk_pos */
    if( m_iMoviLastchunkPos < p_index->i_pos )
    {
        m_iMoviLastchunkPos = p_index->i_pos;
    }

    /* add the entry */
    if( tk->i_idxnb >= tk->i_idxmax )
    {
        tk->i_idxmax += 16384;
        tk->p_index = ( mp4_entry_t *)realloc( tk->p_index,
                               tk->i_idxmax * sizeof( mp4_entry_t ) );
        if( tk->p_index == NULL )
        {
            return;
        }
    }
    /* calculate cumulate length */
    if( tk->i_idxnb > 0 )
    {
        p_index->i_lengthtotal =
            tk->p_index[tk->i_idxnb - 1].i_length +
                tk->p_index[tk->i_idxnb - 1].i_lengthtotal;
    }
    else
    {
        p_index->i_lengthtotal = 0;
    }

    tk->p_index[tk->i_idxnb++] = *p_index;
}

void CMpegPs::IndexLoad()
{
    unsigned int i_stream;

    for( i_stream = 0; i_stream < m_iTrack; i_stream++ )
    {
        m_track[i_stream]->i_idxnb  = 0;
        m_track[i_stream]->i_idxmax = 0;
        m_track[i_stream]->p_index  = NULL;
    }

	IndexLoad_idx1();

	for( i_stream = 0; i_stream < m_iTrack; i_stream++ )
    {
        _RPT2(_CRT_WARN, "stream[%d] created %d index entries\n",
                i_stream, m_track[i_stream]->i_idxnb );
    }
}

int CMpegPs::IndexLoad_idx1()
{
    unsigned int i_stream;
    unsigned int i_index;
    unsigned int i;

    bool b_keyset[100];

    if( !m_iIdxCount )
    {
        _RPT0(_CRT_WARN, "cannot find idx data, no index defined\n" );
        return EGENERIC;
    }

    /* Reset b_keyset */
    for( i_stream = 0; i_stream < m_iTrack; i_stream++ )
        b_keyset[i_stream] = false;
	
	unsigned int vidCount = 0, audCount = 0;
	unsigned int vidIdxFromRef = 0, audTotalFromRef = 0;
	INT64 lastVidTs, lastAudTs;
	INT64 refVidTs = 0, refAudTs = 0;
	/* we get all video/audio entries here in the order they were read from .K file */ 
    for( i_index = 0; i_index < m_iIdxCount; i_index++ )
    {
        unsigned int i_cat;

		ParseStreamHeader( m_idx[i_index].i_id, m_idx[i_index].i_flag,
                               &i_stream,
                               &i_cat );

        if( i_stream < m_iTrack &&
            i_cat == m_track[i_stream]->i_cat )
        {
			INT64 enc, ts, diff;
            mp4_entry_t index;

			index.i_id      = m_idx[i_index].i_id;
            index.i_flags   = (m_idx[i_index].i_flag & 0x01) ? IF_KEYFRAME : 0;
            index.i_pos     = m_idx[i_index].i_pos;
            index.i_length  = m_idx[i_index].i_length;
			index.i_millisec    = m_idx[i_index].i_millisec;
			index.i_sec         = m_idx[i_index].i_sec;

            IndexAddEntry( i_stream, &index );

            if( index.i_flags&IF_KEYFRAME )
                b_keyset[i_stream] = true;
        }
    }

    for( i_stream = 0; i_stream < m_iTrack; i_stream++ )
    {
        if( !b_keyset[i_stream] )
        {
            mp4_track_t *tk = m_track[i_stream];

            _RPT1(_CRT_WARN, "no key frame set for track %d\n", i_stream );
            for( i_index = 0; i_index < tk->i_idxnb; i_index++ )
                tk->p_index[i_index].i_flags |= IF_KEYFRAME;
        }
    }
    return SUCCESS;
}

/*
 * Reads index from given filename.k or from .DVR file if filename==NULL
 * returns length in millisec
 */
__int64 CMpegPs::ReadIndexFromFile(char *filename, _int64 *firstTimestamp)
{
    INT64 len = 0;
    unsigned int i_count, i_index;
	unsigned char *p_read, *p_buff;
	const int baseSize = 10 * 4096;
	int bufSize = baseSize; // initial buffer size

	p_buff = (unsigned char *)malloc(bufSize);
	if (p_buff == NULL)
		return 0;

	if (filename == NULL)
		return 0;

	char *psz_name = _strdup(filename);
	psz_name[strlen(psz_name) - 3] = '\0';
	strcat(psz_name, "k");

	int fd = _open( psz_name, _O_RDONLY | _O_BINARY );
	if ( fd == -1 )
	{
		_RPT2(_CRT_WARN, "cannot open file %s (%s)\n", psz_name, strerror(errno) );
		goto check_filename;
	}

	unsigned int i_read = 0, bytes = 0, toRead;

	toRead = baseSize;
	while (1) {
		bytes = _read(fd, p_buff + i_read, toRead);
		if (bytes <= 0)
			break;
		i_read += bytes;
		toRead -= bytes;
		if (bytes == baseSize) { // need to read more data
			bufSize += baseSize;
			p_buff = (unsigned char *)realloc(p_buff, bufSize);
			toRead = baseSize;
		} else {
			break;
		}
	}

	_close( fd );

	i_count = i_read / 16;
    m_iIdxCount = i_count;

	p_read = p_buff;
    if( m_iIdxCount > 0 )
    {
        m_idx = (index1_entry_t *)calloc( i_count, sizeof( index1_entry_t ) );

        for( i_index = 0; i_index < i_count ; i_index++ )
        {
			/* new index entry */
			m_idx[i_index].i_id = *p_read;
			p_read += 1;
			i_read -= 1;
			m_idx[i_index].i_flag = *p_read;
			p_read += 1;
			i_read -= 1;
			m_idx[i_index].i_millisec = GetWLE( p_read );
			p_read += 2;
			i_read -= 2;
			m_idx[i_index].i_sec = GetDWLE( p_read );
			p_read += 4;
			i_read -= 4;
			m_idx[i_index].i_pos = GetDWLE( p_read );
			p_read += 4;
			i_read -= 4;
			m_idx[i_index].i_length = GetDWLE( p_read );
			p_read += 4;
			i_read -= 4;
        }

		__int64 last, first;
		last = m_idx[i_count - 1].i_sec * 1000LL + m_idx[i_count - 1].i_millisec;
		first = m_idx[0].i_sec * 1000LL + m_idx[0].i_millisec;
		len = last - first;
		if (firstTimestamp != NULL) {
			*firstTimestamp = 1;//first * 1000LL;
		}
    }

	IndexLoad();

check_filename:
#if 0
	char *fname = strrchr(m_filename, '\\');
	if (fname == NULL) {
		fname = m_filename;
	}
#endif

	if (psz_name) free(psz_name);
	if (p_buff) free(p_buff);

	m_iLength = len;
    m_iMoviBegin = 1024;

	stream_seek(&m_stream, 1024);

	return len;
}
#if 0
__int64 CMpegPs::ReadIndexFromMemory(std::vector <index1_entry_t> *index, _int64 *firstTimestamp)
{
    INT64 len = 0;
    unsigned int i_count, i_index;
	bool bVideo = false, bAudio = false, bText = false;
	mp4_track_t      *tk;

	i_count = index->size();
	if (i_count <= 0)
		return 0;

	m_iIdxCount = i_count;

    if( m_iIdxCount > 0 )
    {
        m_idx = (index1_entry_t *)calloc( i_count, sizeof( index1_entry_t ) );

        for( i_index = 0; i_index < i_count ; i_index++ )
        {
			tk = NULL;

			/* I'm just lazy, could have used vector here */
			m_idx[i_index] = index->at(i_index);
			if ((m_idx[i_index].i_flag & 0x10) && !bVideo) {
				bVideo = true;
				tk = ( mp4_track_t * )malloc( sizeof( mp4_track_t ) );
				if (tk) {
					memset(tk, 0, sizeof(mp4_track_t));
					tk->i_cat   = VIDEO_ES;
					tk->i_codec = TME_FOURCC('m','p','4','v'); // check bmih.biCompression 
					es_format_Init( &m_fmtVideo, VIDEO_ES, TME_FOURCC('D','X','5','0') );                
					tk->i_samplesize = 0;
					m_fmtVideo.video.i_width  = 0;
					m_fmtVideo.video.i_height = 0;
					m_fmtVideo.video.i_bits_per_pixel = 0;
					m_fmtVideo.video.i_frame_rate = 0;
					m_fmtVideo.video.i_frame_rate_base = 0;
					m_fmtVideo.i_extra = 0;
					m_fmtVideo.p_extra = NULL;
				}
			} else if ((m_idx[i_index].i_flag & 0x20) && !bAudio) {
				bAudio = true;
				tk = ( mp4_track_t * )malloc( sizeof( mp4_track_t ) );
				if (tk) {
					memset(tk, 0, sizeof(mp4_track_t));
					tk->i_cat   = AUDIO_ES;
					tk->i_codec = TME_FOURCC( 'm', 'l', 'a', 'w' );
					es_format_Init( &m_fmtAudio, AUDIO_ES, tk->i_codec );

					m_fmtAudio.audio.i_channels        = 1;
					m_fmtAudio.audio.i_rate            = 32000;
					m_fmtAudio.i_bitrate               = 32000*8;
					m_fmtAudio.audio.i_blockalign      = 1;
					m_fmtAudio.audio.i_bitspersample   = 8;
					m_fmtAudio.i_extra = 0;
					m_fmtAudio.p_extra = NULL;
				}
			}

			if (tk) {
				if( m_iTrack > 0 ) {	
					m_track = (mp4_track_t **)realloc( m_track, sizeof( void ** ) * ( m_iTrack + 1 ) ); 
				} else {
					m_track = (mp4_track_t **)malloc( sizeof( void ** ) );
				}
				m_track[m_iTrack] = tk;
				m_iTrack++;
			}
        }

		__int64 last, first;
		last = m_idx[i_count - 1].i_sec * 1000LL + m_idx[i_count - 1].i_millisec;
		first = m_idx[0].i_sec * 1000LL + m_idx[0].i_millisec;
		len = last - first;
		if (firstTimestamp != NULL) {
			*firstTimestamp = 1;//first * 1000LL;
		}
    }

	IndexLoad();

	m_iLength = len;
    m_iMoviBegin = 1024;

	stream_seek(&m_stream, 1024);

	return len;
}
#else

__int64 CMpegPs::ReadIndexFromMemory(index1_entry_t *index, int idxSize, _int64 *firstTimestamp)
{
    INT64 len = 0;
    unsigned int i_count, i_index;
	bool bVideo = false, bAudio = false, bText = false;
	mp4_track_t      *tk;

	i_count = idxSize;
	if (i_count <= 0)
		return 0;

	m_iIdxCount = i_count;

    if( m_iIdxCount > 0 )
    {
        m_idx = (index1_entry_t *)calloc( i_count, sizeof( index1_entry_t ) );

        for( i_index = 0; i_index < i_count ; i_index++ )
        {
			tk = NULL;

			/* I'm just lazy, could have used vector here */
			m_idx[i_index] = index[i_index];
			if ((m_idx[i_index].i_flag & 0x10) && !bVideo) {
				bVideo = true;
				tk = ( mp4_track_t * )malloc( sizeof( mp4_track_t ) );
				if (tk) {
					memset(tk, 0, sizeof(mp4_track_t));
					tk->i_cat   = VIDEO_ES;
					tk->i_codec = TME_FOURCC('m','p','4','v'); // check bmih.biCompression 
					es_format_Init( &m_fmtVideo, VIDEO_ES, TME_FOURCC('D','X','5','0') );                
					tk->i_samplesize = 0;
					m_fmtVideo.video.i_width  = 0;
					m_fmtVideo.video.i_height = 0;
					m_fmtVideo.video.i_bits_per_pixel = 0;
					m_fmtVideo.video.i_frame_rate = 0;
					m_fmtVideo.video.i_frame_rate_base = 0;
					m_fmtVideo.i_extra = 0;
					m_fmtVideo.p_extra = NULL;
				}
			} else if ((m_idx[i_index].i_flag & 0x20) && !bAudio) {
				bAudio = true;
				tk = ( mp4_track_t * )malloc( sizeof( mp4_track_t ) );
				if (tk) {
					memset(tk, 0, sizeof(mp4_track_t));
					tk->i_cat   = AUDIO_ES;
					tk->i_codec = TME_FOURCC( 'm', 'l', 'a', 'w' );
					es_format_Init( &m_fmtAudio, AUDIO_ES, tk->i_codec );

					m_fmtAudio.audio.i_channels        = 1;
					m_fmtAudio.audio.i_rate            = 32000;
					m_fmtAudio.i_bitrate               = 32000*8;
					m_fmtAudio.audio.i_blockalign      = 1;
					m_fmtAudio.audio.i_bitspersample   = 8;
					m_fmtAudio.i_extra = 0;
					m_fmtAudio.p_extra = NULL;
				}
			}

			if (tk) {
				if( m_iTrack > 0 ) {	
					m_track = (mp4_track_t **)realloc( m_track, sizeof( void ** ) * ( m_iTrack + 1 ) ); 
				} else {
					m_track = (mp4_track_t **)malloc( sizeof( void ** ) );
				}
				m_track[m_iTrack] = tk;
				m_iTrack++;
			}
        }

		__int64 last, first;
		last = m_idx[i_count - 1].i_sec * 1000LL + m_idx[i_count - 1].i_millisec;
		first = m_idx[0].i_sec * 1000LL + m_idx[0].i_millisec;
		len = last - first;
		if (firstTimestamp != NULL) {
			*firstTimestamp = 1;//first * 1000LL;
		}
    }

	IndexLoad();

	m_iLength = len;
    m_iMoviBegin = 1024;

	stream_seek(&m_stream, 1024);

	return len;
}
#endif
__int64 CMpegPs::FindTimestamp(int track_id, __int64 pos)
{
	int low, mid, high;

	if (track_id >= m_iTrack)
		return 0;

	mp4_track_t *tk = m_track[track_id];
#if 0
	int i;
	for (i = 0; i < tk->i_idxnb; i++) {
		if (tk->p_index[i].i_pos == pos)
			return tk->p_index[i].i_sec * 1000000LL + tk->p_index[i].i_millisec * 1000LL;
	}
#endif
	// binary search
	low = 0;
	high = tk->i_idxnb; // N for [0...N-1]
	while (low < high) {
		mid = (low + high) / 2;
		if (tk->p_index[mid].i_pos < pos) {
			low = mid + 1;
		} else {
			high = mid;
		}
	}
	/* high == low here */
	if (low < tk->i_idxnb) {
		return GetTimeStampAt(tk, low) - GetTimeStampAt(tk, 0) + 1;
	}

	return 0;
}

// offset: offset of the begining of current file (0 for nomal file, begining of the channel for .DVR file)
block_t *CMpegPs::GetFrame(__int64 offset, __int64 end)
{
	int ret, type;
	block_t *pBlock;

	// reset work buffer
	m_bufSize = 0;
	m_iFlag = 0;

	__int64 cur_pos = stream_tell(&m_stream);
	if (offset && (cur_pos >= end))
		return NULL;

	frame_header_t frame;
	if( stream_read(&m_stream, (PBYTE)&frame, sizeof(frame))==sizeof(frame) ) {
		if (((frame.i_id == '0') || (frame.i_id == '1')/* || (frame.i_id == '2')*/) &&
			((frame.i_flag & 0xf0) == 0x10 || (frame.i_flag & 0xf0) == 0x20/* || (frame.i_flag & 0xf0) == 0x40*/) &&
		    ((frame.i_flag & 0x0f) == 0x00 || (frame.i_flag & 0x0f) == 0x01)) {
					//TRACE(_T("track:%d, ts:%d.%03d, len:%d\n"), frame.i_id-'0', frame.i_ts, frame.i_millisec, frame.i_length);
			pBlock = block_New(frame.i_length);
			if (pBlock) {
				ret = stream_read(&m_stream, pBlock->p_buffer, frame.i_length);
				if (ret == frame.i_length) {
					/* decrypt only video frames */
					if (m_pCryptTable && (frame.i_flag & 0x10)) {
						int size = frame.i_length;
						if (size > m_encryptSize) {
							size = m_encryptSize;
						}
						RC4_block_crypt(pBlock->p_buffer, size, 0, m_pCryptTable, CRYPT_TABLE_SIZE);
					}
					pBlock->i_buffer = frame.i_length;
					__int64 ts = FindTimestamp(frame.i_id - '0', cur_pos - offset);
					if( (frame.i_flag == 0x11) ) {	// key frame
						/* see if we need to add mpeg header */
						PBYTE ptr = pBlock->p_buffer;
						if ((frame.i_length > 4) &&
							(ptr[0] == 0x00) && (ptr[1] == 0x00) && 
							(ptr[2] == 0x01) && (ptr[3] == 0xb6)) {
							block_t *pBlock2 = block_New(frame.i_length + m_iMpegHeaderSize);
							if (pBlock2) {
								memcpy(pBlock2->p_buffer, m_mpegHeader, m_iMpegHeaderSize);
								memcpy(pBlock2->p_buffer + m_iMpegHeaderSize, pBlock->p_buffer, frame.i_length);
								pBlock2->i_buffer = frame.i_length + m_iMpegHeaderSize;
								block_Release(pBlock);
								pBlock = pBlock2;
							}
						}
						pBlock->i_pts = 0;
						pBlock->i_dts = ts;
						pBlock->i_flags =  BLOCK_FLAG_TYPE_I ;
					} else if (frame.i_flag & 0x20) {
						pBlock->i_pts = ts;
						pBlock->i_dts = ts;
						pBlock->i_flags =  BLOCK_FLAG_TYPE_AUDIO ;
						pBlock->i_rate = m_fmtAudio.audio.i_rate;
					} else if( frame.i_flag == 0x10 ) {
						pBlock->i_pts = 0;
						pBlock->i_dts = ts;
						pBlock->i_flags =  BLOCK_FLAG_TYPE_PB;
#if 0
					} else if( frame.i_flag == 0x40 ) {
						pBlock->i_pts = 0;
						pBlock->i_dts = ts;
						pBlock->i_flags =  BLOCK_FLAG_TYPE_TEXT;
					} else if( frame.i_flag == 0x41 ) {
						pBlock->i_pts = 0;
						pBlock->i_dts = ts;
						pBlock->i_flags =  BLOCK_FLAG_TYPE_KEYTEXT;
#endif
					}
					return pBlock;
				}
				block_Release(pBlock);
			}
		}
	}

	return NULL;
}

__int64 CMpegPs::FindFirstTimeStamp()
{
    INT64 i_current_pos = -1, i_size = 0, i_end = 0, len = 0;
    int i, found_ts;

	if( m_iLength != -1 ) { /* should be called first */
		return 0;
	}
 
	m_iLength = 0;
	found_ts = 0;

	/* Check beginning */
	i = 0;
	i_current_pos = stream_tell( &m_stream );    
	while (i < 40) {
		if (Demux2(false, &found_ts) <= 0) {
			break;
		}
		if (found_ts)
			break;
		i++;
	}

	if( i_current_pos >= 0 ) stream_seek( &m_stream, i_current_pos );

	return found_ts;
}

int CMpegPs::StreamPeek( BYTE *pBuf, int size )
{
	if (m_stream.fp) {
		return stream_peek( &m_stream, pBuf, size );
	} else if (m_buffer.pStart) {
		return stream_peek_from_buffer( &m_buffer, pBuf, size );
	}
	return 0;
}

/* must be called after PsFindLength() */
__int64 CMpegPs::GetFirstTimeStamp()
{
	int i;
	__int64 ts = 0;
	
	for( i = 0; i < PS_TK_COUNT; i++ ) {
		ps_track_t *tk = &m_tk[i];
		if( tk->i_first_pts >= 0 && tk->i_last_pts > 0 ) {	
			if( tk->i_last_pts > tk->i_first_pts ) {
				if (ts == 0) {
					ts = tk->i_first_pts;	
				} else if (tk->i_first_pts < ts) {
					ts = tk->i_first_pts;	
				}
			}
		}
	}

	return ts;
}

__int64 CMpegPs::StreamSize()
{
	return stream_size(&m_stream);
}

psblock_t *CMpegPs::PktRead( UINT32 i_code )
{
    PBYTE p_peek = NULL;
	int pbuf_size = 14;
	p_peek = (PBYTE)malloc(pbuf_size);
	if (!p_peek)
		return NULL;

    int      i_peek = StreamPeek( p_peek, 14 );
    int      i_size = ps_pkt_size( p_peek, i_peek );

    if( i_size <= 6 && p_peek[3] > 0xba )
    {
        /* Special case, search the next start code */
        i_size = 6;
        for( ;; )
        {
			if (i_size + 1024 > pbuf_size) {
				p_peek = (PBYTE)realloc(p_peek, i_size + 1024);
				pbuf_size = i_size + 1024;
			}
            i_peek = StreamPeek( p_peek, i_size + 1024 );
            if( i_peek <= i_size + 4 )
            {
				free(p_peek);
                return NULL;
            }
            while( i_size <= i_peek - 4 )
            {
                if( p_peek[i_size] == 0x00 && p_peek[i_size+1] == 0x00 &&
                    p_peek[i_size+2] == 0x01 && p_peek[i_size+3] >= 0xb9 )
                {
					free(p_peek);
                    return StreamBlock( i_size );
                }
                i_size++;
            }
        }
    }
    else
    {
        /* Normal case */
		free(p_peek);
        return StreamBlock( i_size );
    }

	free(p_peek);
    return NULL;
}

psblock_t *CMpegPs::StreamBlock( int i_size )
{
	if ((m_stream.fp == NULL) && (m_buffer.pStart == NULL))
		return NULL;

	if( i_size <= 0 )
		return NULL;

	if (sizeof(m_buf) - m_bufSize < i_size) { 
		_RPT2(_CRT_WARN, "buffer too small:%d,%d\n", m_bufSize, i_size);
		return NULL;
	}


	psblock_t *p_bk = psblock_New( &m_block, m_buf + m_bufSize, i_size );
	if( p_bk ) {
		if (m_stream.fp) {
			p_bk->i_buffer = p_bk->i_buf = stream_read( &m_stream, m_buf + m_bufSize, i_size ); 
		} else {
			p_bk->i_buffer = p_bk->i_buf = stream_read_from_buffer( &m_buffer, m_buf + m_bufSize, i_size ); 
		}
		p_bk->p_buffer = p_bk->p_buf = m_buf + m_bufSize;
		if( p_bk->i_buffer > 0 ) {    
			return p_bk;
		}
	}

	if( p_bk ) psblock_Release( p_bk );
	return NULL;
}

int CMpegPs::TrackSeek(int i_stream, mtime_t i_date)
{
	mtime_t timeToSeek, timeFound = 0;
	unsigned int found = -1;
	int low, mid, high;
	int i;

    mp4_track_t *tk = m_track[i_stream];

	/* get start time */
	if (tk->i_idxnb <= 0) {
		return EGENERIC;
	}

	timeToSeek = GetTimeStampAt(tk, 0) + i_date;

	// binary search
	low = 0;
	high = tk->i_idxnb; // N for [0...N-1]
	while (low < high) {
		mid = (low + high) / 2;
		if (GetTimeStampAt(tk, mid) < timeToSeek) {
			low = mid + 1;
		} else {
			high = mid;
		}
	}
	/* high == low here */
	if (low < tk->i_idxnb) {
		timeFound = GetTimeStampAt(tk, low);
		if (timeFound >= timeToSeek) {
			found = low;
		}
	}

	if (found == -1) {
		return EGENERIC;
	}

	int found2 = -1;
	/* find key frame from there */
	for (i = found; i < tk->i_idxnb; i++) {
		if (tk->p_index[i].i_flags & IF_KEYFRAME) {
			found2 = i;
			break;
		}
	}

	if (found2 == -1) {
		return EGENERIC;
	}

	tk->i_idxposc = found2;
	tk->i_idxposb = 0;
#if 0
	/* more precision for audio */
	if (tk->i_samplesize) {
		if ((timeFound > timeToSeek) && (found > 0)) {
			/* get time of one chunk before */
			mtime_t previous = GetTimeStampAt(tk, found - 1);
			if (previous < timeToSeek) {
				unsigned int bytes;
				bytes = AVI_PTSToByte(tk, timeToSeek - previous);	
				if (tk->p_index[found - 1].i_length > bytes) {
					tk->i_idxposc = found - 1;
					tk->i_idxposb = bytes;
				}
			}
		}
	}
#endif
	return 0;
}

// offset: offset of the begining of current file (0 for nomal file, begining of the channel for .DVR file)
int CMpegPs::Seek(mtime_t i_date, __int64 offset)
{
    int i_stream;	
	int idx;
    mp4_track_t *tk = m_track[0];

	if (!TrackSeek(0, i_date)) {
		idx = tk->i_idxposc;
		stream_seek( &m_stream, offset + tk->p_index[idx].i_pos );
		return 0;
	}

	return 1;
}

static void ParseStreamHeader( BYTE trackid, BYTE flags,
                               unsigned int *pi_track_number, unsigned int *pi_track_type )
{
#define SET_PTR( p, v ) if( p ) *(p) = (v);
    {
        switch( flags & 0xf0 )
        {
            case 0x10:
		        SET_PTR( pi_track_number, 0); /* video is alway track 0 */
                SET_PTR( pi_track_type, VIDEO_ES );
                break;
            case 0x20:
		        SET_PTR( pi_track_number, 1); /* audio is alway track 1 */
                SET_PTR( pi_track_type, AUDIO_ES );
                break;
            case 0x40:
		        SET_PTR( pi_track_number, 2); /* txt is alway track 2 */
                SET_PTR( pi_track_type, SPU_ES );
                break;
            default:
		        SET_PTR( pi_track_number, 100); /* error */
                SET_PTR( pi_track_type, UNKNOWN_ES );
                break;
        }
    }
#undef SET_PTR
}

static INT64 GetTimeStampAt(mp4_track_t *tk, int idx)
{
	INT64 ts;

	if ((tk == NULL) || (idx >= tk->i_idxnb))
		return 0LL;

	ts = tk->p_index[idx].i_sec * 1000000LL + tk->p_index[idx].i_millisec * 1000LL;

	return ts;
}
