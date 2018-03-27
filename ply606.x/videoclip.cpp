#include "stdafx.h"
#include <process.h>
#include "ply606.h"
#include "videoclip.h"
#include "dvrfilestream.h"
#include "player.h"
#include "trace.h"
#include "mp4util.h"
#include "es.h"
#include "ffmpeg.h"
#include "xvidenc.h"
#include "subs.h"
#include "vout_vector.h"

#pragma comment(lib, "xvidcore.dll.a")
bool g_xvidlib_initialized;
xvid_gbl_init_t xvid_gbl_init;

int write_to_avifile(FILE *fp, FILE *fpi, struct avi_info *ai,
										 int frametype, int chunk_size, PBYTE payload);

#define ADD_OSD
#define MAX_CHANNEL 16
#define CACHE_BUF_SIZE 100
#define VID_FCC "DX50" /* or "H264" */
#define AUD_FCC 0 /* 0 for PCM,G.711, 1 for ADPCM */
#define WAV_TAG 7 /* 6 for alaw, 7 for ulaw, 0x11 for ADPCM */
const int audio_channels = 1;
const int audio_sample_rate = 32000;
const int audio_bits_per_sample = 8;

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


static int track_write_wavformat(struct avi_info *ai, unsigned char *hdr)
{
	SetWLE(hdr, WAV_TAG); /* WAVCodecTag */
	SetWLE(hdr + 2, audio_channels); /* audio_channels */
	SetDWLE(hdr + 4, audio_sample_rate); /* sample_rate */
	SetDWLE(hdr + 8, audio_sample_rate * audio_channels * audio_bits_per_sample / 8); /* bytes/second */
	SetWLE(hdr + 12, ai->block_align); /* block_align = channels * bit_per_sample / 8 */;
	SetWLE(hdr + 14, audio_bits_per_sample); /* bits_per_sample */

	int size = 16;
	if (AUD_FCC) {
		SetWLE(hdr + 16, 2); /* WMP needs this for ADPCM */
		SetWLE(hdr + 18, ai->frame_size); /* WMP needs this */
		size += 4;
	}
	return size;
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
		memcpy(hdr + 12 + 12, VID_FCC, 4);
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
		memcpy(hdr + 12 + 64 + 24, VID_FCC, 4);
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
		SetDWLE(hdr + 12 + 12, AUD_FCC); /* fccHandler, 0 for PCM,G.711, 1 for ADPCM */
		SetDWLE(hdr + 12 + 28, AUD_FCC ? ai->frame_size : 1); /* dwScale, 1 for PCM,G.711, frame_size for ADPCM */
		SetDWLE(hdr + 12 + 32, audio_sample_rate); /* dwRate */
		SetDWLE(hdr + 12 + 40, ai->audio_count);
		SetDWLE(hdr + 12 + 44, audio_sample_rate * audio_channels); /* at->audio_rate * at->audio_channels */
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

	int ch;
	FILE *dvrfile ;
	int progress_chmax, progress_chmin ;		// maximum/minimum progress on a channel
	__int64 tdiff;									// total time lenght 
	__int64 chdiff ;								
	struct dvrtime frametime, firsttime, lasttime ;
	int    fsize ;								// frame size
	struct mp5_header mp5h;


	std::vector <index1_entry_t> keyindex ;	// key frame index

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
	fileheaderex.platform_type = DVRFILE_606 ;
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

	tdiff=dvrtime_diffms(&begintime, &endtime);							// total time length

	// goto first channel data position
	_fseeki64(dvrfile, 
		  sizeof(fileheader) + 
			fileheader.totalchannels * sizeof(struct DVRFILECHANNEL) + 
			sizeof(fileheaderex) + 
			fileheaderex.totalchannels * sizeof(struct DVRFILECHANNELEX),
		  SEEK_SET );

	dvrtime_init(&firsttime, 2100);
	dvrtime_init(&lasttime, 1800);

	for( ch=0; ch<fileheader.totalchannels; ch++) {
		// initial channel header
		strncpy( pchannelex[ch].cameraname, player->m_channellist[ch].channelinfo.cameraname, sizeof(pchannelex[ch].cameraname));
		pchannel[ch].end = pchannel[ch].begin = 
			pchannelex[ch].begin = pchannelex[ch].end = _ftelli64(dvrfile) ;
		pchannelex[ch].size = sizeof( pchannelex[ch] ) ;
		pchannelex[ch].keybegin=pchannelex[ch].keyend = 0 ;

		// clean key index list
		keyindex.clear();

		// setup progress informaion
		progress_chmin= 100*ch/fileheader.totalchannels+1 ;
		progress_chmax= 100*(ch+1)/fileheader.totalchannels ;

		dupstate->percent = progress_chmin ;

		/* operator key support */
		struct channel_t * pch ;
		pch = player->m_channellist.at(ch);
		if (!pch->bEnable)
			continue;

		// create a file copy stream
		dvrstream * stream = player->newstream(ch);

		if( stream ) {
			// write stream header
			memset(&mp5h, 0, sizeof(struct mp5_header));
			mp5h.fourcc_main = FOURCC_HKMI;
			if( crypt_table ) {			
				// encrypt 264 header
				RC4_block_crypt( (unsigned char *)&mp5h, sizeof(struct mp5_header), 0, crypt_table, 4096);
			}
			fwrite( &mp5h, 1, sizeof(struct mp5_header), dvrfile);

			// seek to begin position
			if( stream->seek( &begintime )==DVR_ERROR_FILEPASSWORD ) {
				// set stream password
				stream->setpassword(ENC_MODE_RC4FRAME, player->m_password, sizeof(player->m_password) );	// set frameRC4 password
				stream->seek( &begintime );													// seek again
			}

			block_t *pBlock;
			int nodata = 0;
			while( nodata < 100 ) {
				pBlock=stream->getdatablock();
				if (pBlock == NULL) {
					nodata++;
					Sleep(30);
					continue;
				}
				nodata = 0;
				stream->gettime( &frametime ) ;

				// find the begin time
				if( dvrtime_compare( &frametime, &firsttime )<0 ) {
					firsttime = frametime;
				}

				chdiff = dvrtime_diffms(&begintime, &frametime) ;

				if(chdiff>=tdiff) {				// up to total length in time
					block_Release(pBlock);;
					break;
				}

				// find the end time
				if( dvrtime_compare( &frametime, &lasttime )>0 ) {
					lasttime = frametime;
				}

				index1_entry_t entry;
				entry.i_pos = _ftelli64(dvrfile)-pchannelex[ch].begin;
				entry.i_sec = dvrtime_mktime(&frametime);
				entry.i_millisec = frametime.millisecond;
				entry.i_length = pBlock->i_buffer;
				entry.i_flag = 0;
				entry.i_id = 0;
				if (pBlock->i_flags == BLOCK_FLAG_TYPE_I) {
					entry.i_id = '0';
					entry.i_flag = 0x11;
				} else if (pBlock->i_flags == BLOCK_FLAG_TYPE_PB) {
					entry.i_id = '0';
					entry.i_flag = 0x10;
				} else if (pBlock->i_flags == BLOCK_FLAG_TYPE_AUDIO) {
					entry.i_id = '1';
					entry.i_flag = 0x21;
				}
				keyindex.push_back(entry);

				if( crypt_table && pBlock->i_flags != BLOCK_FLAG_TYPE_AUDIO ) {
					// encrypt frame data
					fsize = pBlock->i_buffer;
					if( fsize>1024 ) {
						fsize=1024 ;
					}
					RC4_block_crypt(pBlock->p_buffer, fsize, 0, crypt_table, 4096);
				}
				// write frame header
				frame_header_t frame;
				frame.i_id = entry.i_id;
				frame.i_flag = entry.i_flag;
				frame.i_millisec = entry.i_millisec;
				frame.i_ts = entry.i_sec;
				frame.i_length = entry.i_length;
				fwrite( &frame, 1, sizeof(frame_header_t), dvrfile);
				// write frame data to file
				fwrite( pBlock->p_buffer, 1, pBlock->i_buffer, dvrfile);
				// release frame data
				block_Release(pBlock);;

				if( dupstate->update ) {
					dupstate->update=0;
					dupstate->percent = progress_chmin + (int)(chdiff/1000)*100/tdiff/fileheader.totalchannels ;
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

			std::vector <index1_entry_t>::iterator iter;
			for (iter = keyindex.begin(); iter != keyindex.end(); iter++) {
				fwrite( &(*iter), 1, sizeof( index1_entry_t ), dvrfile );
			}

			pchannelex[ch].keyend = _ftelli64(dvrfile);
		}

		if( dupstate->cancel || dupstate->status<0 ) {		// cancelled or error?
			break;
		}
	}

	fileheaderex.date = firsttime.year * 10000 + firsttime.month*100 + firsttime.day ;
	fileheaderex.time = firsttime.hour * 3600 + firsttime.min * 60 + firsttime.second ;
	fileheaderex.millisec = firsttime.millisecond;
	tdiff = dvrtime_diffms(&firsttime, &lasttime);
	fileheaderex.length = tdiff / 1000;
	fileheaderex.len_millisec = tdiff % 1000;

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

int ExtractOSD(block_t *pBlock, block_t **ppBlock, int size)
{
	PBYTE ptr;
	block_t *p_b;
	int toRead, lineSize;
	int nText = 0;

	pBlock->i_osd = 0;

	if ((pBlock->i_buffer < 16) ||
		(pBlock->p_buffer[0] != 'T') ||
		(pBlock->p_buffer[1] != 'X') ||
		(pBlock->p_buffer[2] != 'T')) {
		return 0;
	}

	ptr = pBlock->p_buffer + 6;
	toRead = *((unsigned short *)ptr);
	pBlock->i_osd = toRead + 8; /* 8:sizeof(struct OSDHeader):let's avoid memmove here, and use this in DecodeVideo */

	ptr += 2;
	while (toRead > 8) {
		if ((ptr[0] != 's') || (ptr[1] != 't')) {
			break;
		}

		ptr += 6;
		lineSize = *((unsigned short *)ptr);
		ptr += 2;
		if (lineSize > 4) {
			p_b = block_New( lineSize );
			if (p_b) {
				p_b->i_pts = pBlock->i_dts;
				p_b->i_dts = pBlock->i_dts;
				p_b->i_buffer = lineSize;
				p_b->i_length = 0;
				memcpy((char *)p_b->p_buffer, ptr, lineSize);
				if (nText < size) {
					ppBlock[nText++] = p_b;
				}
			}
		}
		toRead -= (8 + lineSize);
		ptr += lineSize;
	}

	return nText;
}

PBYTE RemoveOSD(PBYTE in, int in_size)
{
	PBYTE ptr, frame_start;
	int toRead, lineSize;

	frame_start = in;

	if ((in_size < 16) ||
		(in[0] != 'T') ||
		(in[1] != 'X') ||
		(in[2] != 'T')) {
		return frame_start;
	}

	ptr = in + 6;
	toRead = *((unsigned short *)ptr);

	ptr += 2;
	while (toRead > 8) {
		if ((ptr[0] != 's') || (ptr[1] != 't')) {
			break;
		}

		ptr += 6;
		lineSize = *((unsigned short *)ptr);
		ptr += 2;
		if (lineSize > 4) {
			frame_start = ptr + lineSize;
		}
		toRead -= (8 + lineSize);
		ptr += lineSize;
	}

	return frame_start;
}

int parse_mpeg_config(PBYTE frame_start, int frame_size, struct avi_info *ai)
{
	/* simplified only for 606 board */
	if (isVideoObjectStartCode(frame_start)) {
		UINT32 code = GetDWBE(frame_start + 4);
		if (code >= 0x00000120 && code <= 0x0000012f) {
			parse_video_object_layer(frame_start + 4, frame_size - 4,
				&ai->frameperiod.denominator, &ai->frameperiod.numerator, 
				&ai->resolution.width, &ai->resolution.height);
			return 0;
		}
	}

	return 1;
}

void do_osd(CDecoder *pTextDecoder, int nText, block_t **pp_bText, int textBufSize)
{
	int i;

	// check OSD text
	if (pTextDecoder) {
		for (i = 0; i < nText; i++) {
			subpicture_t *p_spu;
			while( (p_spu = pTextDecoder->DecodeSubtitle( &pp_bText[i] ) ) ) {
				if( pTextDecoder->m_pSpuVout ) { 
					pTextDecoder->m_pSpuVout->m_pSpu->DisplaySubpicture( p_spu );
				}
			}
		}
	}
}

//#define TEST_
#ifdef TEST_
FILE *fpt;
int fileext;
#endif

// recording one channel data
// return 
//       >=0 : copied bytes
//       <0  : error
static int avifile_record_channel(class dvrplayer *player, int ch,
						   char *duppath, struct dup_state *dupstate,
						   struct dvrtime begintime,
						   struct dvrtime endtime, int ch_progress_base, int ch_progress_max)
{
	struct dvrtime frametime, firsttime, lasttime ;
	__int64 chdiff, tdiff;								
	FILE *fp, *fpi;
	char filename[MAX_PATH], indexname[MAX_PATH], suffix[16];
	char *ptr;
	struct avi_info ai;
	const int textBufSize = 32;
	block_t *pp_bText[textBufSize];
	int nText;
	CDecoder *pVideoDecoder, *pTextDecoder;
	picture_t *     p_directbuffer;              /* direct buffer to display */
	bool vout_created = false;

	struct xvid_setup xs;
	xvid_setup_default(&xs);
	xvidlib_init(1, &xs);

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
	ai.block_align = 1;

	tdiff=dvrtime_diffms(&begintime, &endtime);							// total time length

	es_format_t fmt;
	es_format_Init(&fmt, VIDEO_ES, TME_FOURCC('D','X','5','0'));
	//testing....fmt.video.i_width = 720;
	//fmt.video.i_height = 480;
	pVideoDecoder = new CFFmpeg(&g_ipc, NULL, &fmt, NULL, false, false);
	if (pVideoDecoder->OpenDecoder()) {
		delete pVideoDecoder;
		pVideoDecoder = NULL;
	}

	es_format_Init(&fmt, SPU_ES, TME_FOURCC('s','u','b','t'));
	pTextDecoder = new CSubs(&g_ipc, NULL, &fmt, NULL, false);
	if (pTextDecoder) {
		if (pTextDecoder->OpenDecoder()) {
			delete pTextDecoder;
			pTextDecoder = NULL;
		}
	}

	// create a file copy stream
	dvrstream * stream = player->newstream(ch);
	if( stream ) {
		// seek to begin position
		if( stream->seek( &begintime )==DVR_ERROR_FILEPASSWORD ) {
			// set stream password
			stream->setpassword(ENC_MODE_RC4FRAME, player->m_password, sizeof(player->m_password) );	// set frameRC4 password
			stream->seek( &begintime );													// seek again
		}

		block_t *pBlock;
		int nodata = 0;
		while( nodata < 100 ) {
			pBlock=stream->getdatablock();
			if (pBlock == NULL) {
				nodata++;
				Sleep(30);
				continue;
			}
			if (pBlock->i_buffer <= 0) {
				block_Release(pBlock);
				continue;
			}
			nodata = 0;
			stream->gettime( &frametime ) ;

			// find the begin time
			if( dvrtime_compare( &frametime, &firsttime )<0 ) {
				firsttime = frametime;
			}

			chdiff = dvrtime_diffms(&begintime, &frametime) ;

			if(chdiff>=tdiff) {				// up to total length in time
				block_Release(pBlock);;
				break;
			}

			// find the end time
			if( dvrtime_compare( &frametime, &lasttime )>0 ) {
				lasttime = frametime;
			}

			PBYTE frame_start = pBlock->p_buffer;
			int frame_size = pBlock->i_buffer;
			int frame_type = FRAME_TYPE_UNKNOWN;
			if (pBlock->i_flags == BLOCK_FLAG_TYPE_I ||
					pBlock->i_flags == BLOCK_FLAG_TYPE_PB) {

				/* skip OSD TXT */
				frame_start = RemoveOSD(pBlock->p_buffer, pBlock->i_buffer);
				frame_size = pBlock->i_buffer - (frame_start - pBlock->p_buffer);

				frame_type = FRAME_TYPE_VIDEO;
				if (pBlock->i_flags == BLOCK_FLAG_TYPE_I) {
					frame_type = FRAME_TYPE_KEYVIDEO;
					if (ai.resolution.width == 0) {
						if (!parse_mpeg_config(frame_start, frame_size, &ai)) {
							xs.width = ai.resolution.width;
							xs.height = ai.resolution.height;
							if (xvid_init(&xs)) {
								xs.enc_handle = NULL;
							}
						}
					}
				}
			} else if (pBlock->i_flags == BLOCK_FLAG_TYPE_AUDIO) {
				frame_type = FRAME_TYPE_AUDIO;
			}

#ifdef ADD_OSD
			nText = ExtractOSD(pBlock, pp_bText, textBufSize);
			if (vout_created) {
				do_osd(pTextDecoder, nText, pp_bText, textBufSize);
				nText = 0; /* avoid doing this again */
			}

			if (pVideoDecoder &&
				(pBlock->i_flags == BLOCK_FLAG_TYPE_I ||
					pBlock->i_flags == BLOCK_FLAG_TYPE_PB)) {
						picture_t *p_pic;
						frame_size = 0;
						while (true) {
							p_pic = pVideoDecoder->DecodeVideo( &pBlock );
							vout_created = true; /* should be here, not below, why 1st frame always not decoded? */
							do_osd(pTextDecoder, nText, pp_bText, textBufSize);
							if (p_pic == NULL)
								break;
							subpicture_t *  p_subpic = NULL; /* subpicture pointer */
							vout_DatePicture( pVideoDecoder->m_pVout, p_pic, p_pic->date );					
							vout_DisplayPicture( pVideoDecoder->m_pVout, p_pic );
							p_subpic = pVideoDecoder->m_pVout->m_pSpu->SortSubpictures( p_pic->date, false);
			        p_directbuffer = vout_RenderPicture( pVideoDecoder->m_pVout, p_pic, p_subpic );
			        if( p_pic != NULL && p_directbuffer != NULL) {
								/* Render the direct buffer returned by vout_RenderPicture */
                        pVideoDecoder->m_pVout->Render( p_directbuffer );
#ifdef TEST_
								char filename[256];
								sprintf(filename, "D:\\test.%03d", fileext++);
	fopen_s(&fpt, filename, "wb");
	if (fpt) {
		fwrite(p_directbuffer->p_data, 1, xs.width * xs.height * 12 / 8, fpt);
					//fwrite(rgb, 1, xs.width * xs.height * 3, fpt);
	fclose(fpt); fpt = NULL;
				}
#endif
								if (xs.enc_handle) {	
									int key;

                        //------------- AVI hook point (dennis)       
                                    player->BlurDecFrame( ch, p_directbuffer );

									frame_type = FRAME_TYPE_VIDEO;
									frame_size = xvid_decode(&xs, p_directbuffer->p_data, &key);
									if (key)
										frame_type = FRAME_TYPE_KEYVIDEO;
									frame_start = xs.mp4_buffer;
								}	
							}
							vout_DestroyPicture( pVideoDecoder->m_pVout, p_pic );
						}
			}
#endif

			if (frame_size > 0) {
				write_to_avifile(fp, fpi, &ai, frame_type, frame_size, frame_start);
			}

#ifdef ADD_OSD
			if (frame_type == FRAME_TYPE_AUDIO) {
				block_Release(pBlock);
			}
#else
			// release frame data
			block_Release(pBlock);
#endif


			if( dupstate->update ) {
				dupstate->update=0;
				dupstate->percent = ch_progress_base + (ch_progress_max - ch_progress_base) * chdiff / tdiff;
				sprintf(dupstate->msg, "Channel %d - %02d:%02d:%02d", 
					ch, 
					frametime.hour, 
					frametime.min, 
					frametime.second);
			}

			if( dupstate->cancel || dupstate->status<0 ) {		// cancelled or error?
					break;
			}
		}

		// delete file copy stream ;
		delete stream ;
	}

#ifdef ADD_OSD
	if (pTextDecoder)
		delete pTextDecoder;
	if (pVideoDecoder)
		delete pVideoDecoder;
	struct vout_set *pvs, vs;
	pvs = getVoutSetForWindowHandle(NULL, &vs);
	if (pvs && vs.vout) {
		CVideoOutput *pVout = vs.vout;
		if (pVout) {
			delete pVout;
		}
		removeVoutForWindowHandle(NULL);
	}
#endif

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

	//DeleteFile(indexname);
	remove(indexname);
	if (ai.body_size == 0) {
		//DeleteFile(filename);
		remove(filename);
	}


	TRACE(_T("avi end ch %d\n"), ch);
#ifdef TEST_
	//fclose(fpt);
#endif
	xvid_close(&xs);

	return 0;
}

static int create_avi_file(class dvrplayer *player,
						   char *duppath, struct dup_state *dupstate,
						   struct dvrtime begintime,
						   struct dvrtime endtime)
{
	int ch, totalch ;
	int progress_chmax, progress_chmin ;		// maximum/minimum progress on a channel

	player->m_errorstate = 2 ;							// pause player

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

	return 0;
}

unsigned WINAPI dvrfile_save_thread(void *p)
{
	int filetype;
	bool enc;
	char   password[PWSIZE] ;
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
	memcpy(password, cpinfo->password, PWSIZE);
	
	SetEvent(cpinfo->hRunEvent);						// let calling function go

	enc = false;
	if( password[0] ) {
		enc = true;
		key_256( password, k256);
		RC4_crypt_table( crypt_table, 4096, k256 );
	}

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

int write_to_avifile(FILE *fp, FILE *fpi, struct avi_info *ai,
										 int frametype, int chunk_size, PBYTE payload)
{
	int bytes;
	BYTE hdr[16];

	if ((frametype==FRAME_TYPE_KEYVIDEO) || (frametype==FRAME_TYPE_VIDEO)) {		// video frames?
		hdr[0] = '0';
		hdr[1] = '0'; // track number
		hdr[2] = 'd';
		hdr[3] = 'c'; // video chunk
		ai->video_count++;
		if (chunk_size > ai->max_video_size)
			ai->max_video_size = chunk_size;
		if (ai->track_count < 1)
			ai->track_count = 1;
	} else if (frametype == FRAME_TYPE_AUDIO) {
		hdr[0] = '0';
		hdr[1] = '1'; // track number
		hdr[2] = 'w';
		hdr[3] = 'b'; // audio chunk
		ai->audio_count++ ;
		if (chunk_size > ai->max_audio_size)
			ai->max_audio_size = chunk_size;
		if (ai->track_count < 2)
			ai->track_count = 2;
	} else {
		return 1;
	}

	if (chunk_size) {
		SetDWLE(hdr +  4, ((frametype==FRAME_TYPE_KEYVIDEO) || (frametype==FRAME_TYPE_AUDIO)) ? 0x10 : 0);
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
