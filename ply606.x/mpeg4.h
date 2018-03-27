#ifndef __MPEGPS_H__
#define __MPEGPS_H__

#include "mtime.h"
#include "es.h"
#include "block.h"

#define PS_TK_COUNT (512 - 0xc0)
#define PS_ID_TO_TK( id ) ((id) <= 0xff ? (id) - 0xc0 : ((id)&0xff) + (256 - 0xc0))

#define GOT_PSM 0x01
#define GOT_E1 0x02 // Filler(type 9) with PTS
#define GOT_E2 0x04 // Sequence parameter set
#define GOT_E3 0x04 // picture parameter set

#define IF_KEYFRAME    0x00000010L /* this frame is a key frame.*/


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

typedef struct frame_header_s
{
	BYTE   i_id;
	BYTE   i_flag;
	UINT16 i_millisec;
    UINT32 i_ts;
    UINT32 i_length;
} frame_header_t;

typedef struct index1_entry_s
{
	BYTE   i_id;
	BYTE   i_flag;
	UINT16 i_millisec;
	UINT32 i_sec;
    UINT32 i_pos;
    UINT32 i_length;
} index1_entry_t;

struct streamInfo {
	DWORD fourcc;
	DWORD size;
	DWORD usecPerFrame;
	DWORD res1;
	DWORD encryption;
	DWORD res2[3];
	DWORD streams;
	DWORD bufSize;
	DWORD width, height;
	DWORD res3[4];
};
struct trackHeader {
	DWORD fourcc;
	DWORD size, type, handler;
	DWORD res1[3];
	DWORD scale, rate;
	DWORD res2;
	DWORD totalFrames;
	DWORD bufSize;
	DWORD res3;
	DWORD samplesize;
	DWORD res4;
	WORD width, height;
};

struct imgHeader {
	DWORD fourcc;
	DWORD size;
	BITMAPINFOHEADER bmih;
};

struct sndHeader {
	DWORD fourcc;
	DWORD size;
	WORD wFormatTag, nChannels;
	DWORD nSamplesPerSec;
	DWORD nAvgBytesPerSec;
	WORD nBlockAlign;
	WORD wBitsPerSample;
	WORD cbSize;
	BYTE res[12];
};

struct mp5_header {
	DWORD fourcc_main;
	DWORD res1[5];
	struct streamInfo stream_info;
	DWORD list_vid[3];
	struct trackHeader vidh;
	struct imgHeader imgh;
	DWORD list_aud[3];
	struct trackHeader audh;
	struct sndHeader sndh;
	DWORD reserved[174];
};

typedef struct
{
	fourcc_t i_id;
    DWORD     i_flags;
	DWORD	i_millisec;
	DWORD   i_sec;
    DWORD     i_length;
    DWORD     i_lengthtotal; /* sum of length of all entries BEFORE THIS ENTRY for the track */
    INT64        i_pos;
} mp4_entry_t;

typedef struct
{
    bool      b_activated; /* flag set after TrackSeek(m_iTime) is called */

    unsigned int    i_cat; /* AUDIO_ES, VIDEO_ES */
    fourcc_t    i_codec;

    int             i_rate;
    int             i_scale;
    int             i_samplesize;

    mp4_entry_t     *p_index;
    unsigned int        i_idxnb; /* each track's index entry count */
    unsigned int        i_idxmax;

    unsigned int        i_idxposc;  /* numero of chunk (current chunk #)*/
    unsigned int        i_idxposb;  /* byte position in the current chunk in case we should read in multiple read */
} mp4_track_t;

class CMpegPs
{
public:
	CMpegPs();
	~CMpegPs();
	void Init();
	block_t *GetFrame(__int64 offset = 0, __int64 end = 0);
	__int64 FindFirstTimeStamp();
	void SetStream(FILE *fp);
	//void SetFilename(char *filename);
	void SetBuffer(PBYTE pBuf, int size);
	int StreamPeek( BYTE *pBuf, int size );
	__int64 StreamSize();
	__int64 GetFirstTimeStamp();
	__int64 ReadIndexFromFile(char *filename, _int64 *firstTimestamp);
	//__int64 ReadIndexFromMemory(std::vector <index1_entry_t> *index, _int64 *firstTimestamp);
	__int64 ReadIndexFromMemory(index1_entry_t *index, int idxSize, _int64 *firstTimestamp);
	int ParseFileHeader(struct mp5_header *header);
	int Seek(mtime_t i_date, __int64 offset = 0);
	void SetEncrptTable(unsigned char * pCryptTable, int size = 1024);

private:
	bool m_bLostSync, m_bHavePack;
	int m_iMuxRate;
	__int64 m_iLength, m_iScr, m_iCurrentPts;
	//char m_filename[256];
	struct mp5_header m_fileHeader;
	index1_entry_t *m_idx;
	int m_iIdxCount;
	int m_iTrack;
    mp4_track_t  **m_track;
	__int64 m_iMoviLastchunkPos, m_iMoviBegin;
	unsigned char *m_pCryptTable;
	int m_encryptSize;
	BYTE m_mpegHeader[256];
	int m_iMpegHeaderSize;

	ps_track_t  m_tk[PS_TK_COUNT];
	struct ps_stream m_stream;
	struct ps_buffer m_buffer;
	BYTE m_buf[500*1024];
	int m_bufSize; // total bytes read
	int m_iFlag; // for future use
	psblock_t m_block;
    es_format_t m_fmtVideo, m_fmtAudio, m_fmtText;


	int Demux2(bool b_end, int *found_ts = NULL);
	int copyToBuffer(psblock_t *p_pkt);
	psblock_t *PktRead( UINT32 i_code );
	int PktResynch(UINT32 *i_code);
	psblock_t *StreamBlock( int i_size );
	void IndexLoad();
	int IndexLoad_idx1();
	void IndexAddEntry( int i_stream, mp4_entry_t *p_index);
	__int64 FindTimestamp(int track_id, __int64 pos);
	int TrackSeek(int i_stream, mtime_t i_date);
	void Clear();
};


#endif