#ifndef __MPEGPS_H__
#define __MPEGPS_H__

#include "mtime.h"

#define PS_TK_COUNT (512 - 0xc0)
#define PS_ID_TO_TK( id ) ((id) <= 0xff ? (id) - 0xc0 : ((id)&0xff) + (256 - 0xc0))
#define SUCCESS 0
#define EGENERIC 1

#define GOT_PSM 0x01
#define GOT_E1 0x02 // Filler(type 9) with PTS
#define GOT_E2 0x04 // Sequence parameter set
#define GOT_E3 0x04 // picture parameter set


struct psblock_t
{
    mtime_t     i_pts;
    mtime_t     i_dts;

    int         i_buffer;
    BYTE     *p_buffer; // for pointer manipulation
	BYTE	 *p_buf;
    int      i_buf;
};

typedef struct
{
    bool  b_seen;
    int         i_skip;
    int         i_id;
    //es_format_t fmt;
    mtime_t     i_first_pts;
    mtime_t     i_last_pts;
} ps_track_t;

struct ps_stream {
	FILE *fp;
	__int64 end; // size
};

struct ps_buffer {
	PBYTE pStart, pCurrent;
	int size; 
};

typedef struct
{
	bool m_bLostSync, m_bHavePack;
	int m_iMuxRate;
	__int64 m_iLength, m_iTimeTrack, m_iScr, m_iCurrentPts;
	ps_track_t  m_tk[PS_TK_COUNT];
	struct ps_stream m_stream;
	BYTE m_buf[500*1024];
	int m_bufSize;
	int m_iFlag; // for future use
} ps_t;

class CMpegPs
{
public:
	CMpegPs();
	~CMpegPs();
	void Init();
	int GetOneFrame(char **framedata, int *framesize, int *frametype, __int64 *timestamp, int *headerlen);
	int GetFrameInfo(char *framedata, int *framesize, int *frametype, __int64 *timestamp, int *headerlen);
	__int64 FindFirstTimeStamp();
	void SetStream(FILE *fp);
	void SetBuffer(PBYTE pBuf, int size);
	int StreamPeek( BYTE *pBuf, int size );
	__int64 StreamSize();
	__int64 GetFirstTimeStamp();
	__int64 FindLength();

private:
	bool m_bLostSync, m_bHavePack;
	int m_iMuxRate;
	__int64 m_iLength, m_iTimeTrack, m_iScr, m_iCurrentPts;
	ps_track_t  m_tk[PS_TK_COUNT];
	struct ps_stream m_stream;
	struct ps_buffer m_buffer;
	BYTE m_buf[500*1024];
	int m_bufSize; // total bytes read
	int m_iFlag; // for future use
	psblock_t m_block;

	int Demux2(bool b_end, __int64 *found_ts = NULL);
	int Demux(int *frametype, __int64 *timestamp, int *headerlen);
	int copyToBuffer(psblock_t *p_pkt);
	psblock_t *PktRead( UINT32 i_code );
	int PktResynch(UINT32 *i_code);
	psblock_t *StreamBlock( int i_size );

};


#endif