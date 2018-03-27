#include "stdafx.h"
#include "mpegps.h"
#include "player.h"

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
		return FRAME_TYPE_AUDIO; /* 1 audio frame: 80 bytes + 16 bytes header, 25 frames/sec = 16kbps */
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
    p_pes->i_pts = 100 * p_pes->i_pts / 9; /* converted to usec */

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
{
	Init();
}

CMpegPs::~CMpegPs()
{
}

void CMpegPs::Init()
{
	m_bLostSync = false;
	m_iLength = -1;
	m_iTimeTrack = -1;
	memset(&m_stream, 0, sizeof(struct ps_stream));
	memset(&m_buffer, 0, sizeof(struct ps_buffer));
	m_iScr = -1;
	m_iMuxRate = 0;
	m_iCurrentPts = 0;
	m_bufSize = 0;
	m_iFlag = 0;
	ps_track_init( m_tk );
}

void CMpegPs::SetStream(FILE *fp)
{
	m_stream.fp = fp;
	m_stream.end = 0;
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

int CMpegPs::Demux2(bool b_end, __int64 *found_ts)
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
        FindLength();

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

__int64 CMpegPs::FindLength()
{
    INT64 i_current_pos = -1, i_size = 0, i_end = 0, len = 0;
    int i;

    if( m_iLength == -1 ) /* First time */
    {
        m_iLength = 0;
        /* Check beginning */
        i = 0;
		i_current_pos = stream_tell( &m_stream );
        while( i < 40 && Demux2( false ) > 0 ) i++;

        /* Check end */
        i_size = stream_size( &m_stream );
        i_end = max( 0, min( 200000, i_size ) );
        stream_seek( &m_stream, i_size - i_end );
    
        while( Demux2( true ) > 0 );
        if( i_current_pos >= 0 ) stream_seek( &m_stream, i_current_pos );
    }

    for( i = 0; i < PS_TK_COUNT; i++ )
    {
        ps_track_t *tk = &m_tk[i];
		if( tk->i_first_pts >= 0 && tk->i_last_pts > 0 ) {
            if( tk->i_last_pts > tk->i_first_pts )
            {
                INT64 i_length = (INT64)tk->i_last_pts - tk->i_first_pts;
                if( i_length > m_iLength )
                {
                    len = m_iLength = i_length;
                    m_iTimeTrack = i;
                    _RPT1(_CRT_WARN, "we found a length of: %lld\n", m_iLength );
                }
            }
		}
	}

	return len;
}

int CMpegPs::GetOneFrame(char **framedata, int *framesize, int *frametype, __int64 *timestamp, int *headerlen)
{
	int ret, retry = 50;

	// reset work buffer
	m_bufSize = 0;
	m_iFlag = 0;

	*timestamp = 0;
	*frametype = FRAME_TYPE_UNKNOWN ;

	while (--retry > 0) {
		ret = Demux(frametype, timestamp, headerlen);
		if (ret < 0) {
			break;
		}
		if (*frametype != FRAME_TYPE_UNKNOWN) {
			//*framedata = new char [m_bufSize];
			*framedata = (char *)m_buf;
			*framesize = m_bufSize;
			//memcpy(*framedata, m_buf, m_bufSize);
			m_bufSize = 0;
			//_RPT2(_CRT_WARN, "type:%d,ts:%I64d\n", *frametype, *timestamp);
			return 1;
		}
	}

	return 0;
}

// Get frame size/type/timestamp from a given data
int CMpegPs::GetFrameInfo(char *framedata, int *framesize, int *frametype, __int64 *timestamp, int *headerlen)
{
	int ret, retry = 50;

	Init();
	// reset work buffer
	m_bufSize = 0;
	m_iFlag = 0;

	SetBuffer((PBYTE)framedata, *framesize);

	*timestamp = 0;
	*frametype = FRAME_TYPE_UNKNOWN ;

	while (--retry > 0) {
		ret = Demux(frametype, timestamp, headerlen);
		if (ret < 0) {
			break;
		}
		if (*frametype != FRAME_TYPE_UNKNOWN) {
			*framesize = m_bufSize;
			m_bufSize = 0;
			//_RPT2(_CRT_WARN, "type:%d,ts:%I64d\n", *frametype, *timestamp);
			return 1;
		}
	}

	return 0;
}

__int64 CMpegPs::FindFirstTimeStamp()
{
    INT64 i_current_pos = -1, i_size = 0, i_end = 0, len = 0;
    int i;
	__int64 found_ts;

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

