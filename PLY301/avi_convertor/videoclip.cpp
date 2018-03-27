#include "stdafx.h"
#include <process.h>
#include "videoclip.h"
#include "../dvrfilestream.h"
#include "../player.h"
#include "adpcm.h"
#include "xvidenc.h"

#include "cstr.h"

#ifndef TRACE
#define TRACE(...)
#endif

//#define TEST_
#ifdef TEST_
FILE *fpx;
#endif

/*

#define	PLAYM4_BUF_OVER			HIK_PLAYM4_BUF_OVER
#define PlayM4_SetDecCallBack	Hik_PlayM4_SetDecCallBack
#define PlayM4_PlaySound		Hik_PlayM4_PlaySound
#define PlayM4_GetLastError		Hik_PlayM4_GetLastError

*/

/* Things to change.
 * 1. dvrplayer.h
 *    m_playerinfo: change to public 
 *    m_errorstate: change to public 
 *    newstream() : change to public 
 *    m_password  : change to public
 *    m_hVideoKey : change to public
 *    m_channellist:change to public
 * 2. copy inttypedef.h, adpcm.h, adpcm.cpp, xvid.h, xvidenc.h, xvidenc.cpp, videoclip.h, videoclip.cpp,
 *         xvidcore.dll.a to the project folder (ply301)
 * 3. copy xvidcore.dll to the working folder (Debug or Release)
 * 3. Add adpcm.cpp, xvidenc.cpp, videoclip.cpp to the project source file
 * 4. modify dvrplayer.cpp (check for USE_VIDEOCLIP)
 */

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "xvidcore.dll.a")

int write_to_avifile(struct xvid_setup *xs, AVCodecContext *avc, FILE *fp, FILE *fpi, struct avi_info *ai, struct frame_cache *f);

#define MAX_CHANNEL 16
#define CACHE_BUF_SIZE 100

bool g_xvidlib_initialized;
xvid_gbl_init_t xvid_gbl_init;

struct hik_frame {
	DWORD flag;					// 1
	DWORD serial;
	DWORD timestamp;
	DWORD audio;					// 0x1000 for video 0x1001 for audio
	BYTE n_frames, a;           // a: always 0x10 
	WORD b;						// b: alway 0
	WORD width,height;			
	DWORD type;					// 0x1001: I frame, 0x1005: P/B frame, 0x1007: audio
	DWORD res3[5];				// 0: alway 0x100f, 1,2,3: alway 0, 4: increasing number(ts?)
};

struct hik_subframe {
	BYTE type, a; // audio: 1, I frame: 3, P frame: 4, B frame: 5
	WORD b;
	DWORD res1[3];
	DWORD framesize;			// 0x50 for audio
};

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
	int av_start, av_end;
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

int buffer_av_frame(struct hik_state *hs, char *pBuf, long nSize, long nType)
{
	int retry = 0;

	while ((retry++ < 500) && ((hs->av_end + 1) % CACHE_BUF_SIZE == hs->av_start)) {
		Sleep(10);
	}

	if (retry >= 500) {
		return 1;
	}

	struct frame_cache *f = (struct frame_cache *)malloc(sizeof(int) * 2 + nSize);
	if (f) {
		f->type = nType;
		f->len = nSize;
		memcpy((PBYTE)f + sizeof(int) * 2, pBuf, nSize);
		EnterCriticalSection(&hs->cs);
		hs->avf[hs->av_end] = f;
		hs->av_end = (hs->av_end + 1) % CACHE_BUF_SIZE;
		LeaveCriticalSection(&hs->cs);
	}

	//TRACE(_T("buffer_av_frame:(1)%d,%d\n"), hs->av_start,hs->av_end);
	return 0;
}

int buffer_av_frame(struct hik_state *hs, struct frame_cache *f)
{
	int retry = 0;

	while ((retry++ < 500) && ((hs->av_end + 1) % CACHE_BUF_SIZE == hs->av_start)) {
		Sleep(10);
	}

	if (retry >= 500) {
		return 1;
	}

	EnterCriticalSection(&hs->cs);
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

	//TRACE(_T("unbuffer_av error:%d,%d\n"), hs->av_start, hs->av_end);
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
#ifdef TEST_
		fwrite(pBuf, 1, nSize, fpx);
#endif
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
	int port;

	for( port=0; port<PLAYM4_MAX_SUPPORTS; port++) {
		if( PlayM4_OpenStream(port, (PBYTE)Head264, 40, SOURCE_BUF_MIN*8 )!=0 ) {
			hs->port = port ;
			PlayM4_SetStreamOpenMode(port,STREAME_REALTIME);	
			PlayM4_SetDecCallBack(port, DecCBFunc);
			PlayM4_ResetSourceBuffer(port);
			PlayM4_Play(port, NULL);
			PlayM4_PlaySound(port);
			return 0;
		}
	}

	return 1;
}

void close_hik_player(struct hik_state *hs)
{
	PlayM4_Stop(hs->port);
	PlayM4_SetDecCallBack(hs->port, NULL);
	PlayM4_CloseStream(hs->port);
	hs->port = -1;
}

static int get_filetype(char *filepath)
{
	char *ptr;

	if (filepath == NULL)
		return FILETYPE_UNKNOWN;

	ptr = strrchr(filepath, '.'); 
	if (ptr == NULL)
		return FILETYPE_UNKNOWN;

	ptr++;

	if (0 == _stricmp(ptr, "dvr"))
		return FILETYPE_DVR;

	if (0 == _stricmp(ptr, "avi"))
		return FILETYPE_AVI;

	return FILETYPE_UNKNOWN;
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
	char * framedata ;							// frame data
	int    framesize, frametype ;
	struct dvrtime frametime, lasttime ;
	int    fsize ;								// frame size

	list <struct DVRFILEKEYENTRY> keyindex ;	// key frame index
	list <struct DVRFILECLIPENTRY> clipindex ;	// list of clips 

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
	fileheaderex.platform_type = 0 ;
    fileheaderex.date = begintime.year * 10000 + begintime.month*100 + begintime.day ;
	fileheaderex.time = begintime.hour * 3600 + begintime.min * 60 + begintime.second ;
	fileheaderex.length = dvrtime_diff(&begintime, &endtime);
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
    lasttime = begintime ;

	// goto first channel data position
	file_seek(dvrfile, 
		  sizeof(fileheader) + 
			fileheader.totalchannels * sizeof(struct DVRFILECHANNEL) + 
			sizeof(fileheaderex) + 
			fileheaderex.totalchannels * sizeof(struct DVRFILECHANNELEX),
		  SEEK_SET );

	for( ch=0; ch<fileheader.totalchannels; ch++) {
		// initial channel header
		strncpy( pchannelex[ch].cameraname, player->m_channellist[ch].channelinfo.cameraname, sizeof(pchannelex[ch].cameraname));
		pchannel[ch].end = pchannel[ch].begin = 
			pchannelex[ch].begin = pchannelex[ch].end = (DWORD)file_tell(dvrfile) ;
		pchannelex[ch].size = sizeof( pchannelex[ch] ) ;
		pchannelex[ch].keybegin=pchannelex[ch].keyend = 0 ;

   		// clean clip list
		clipindex.clean();
        struct DVRFILECLIPENTRY clipentry ;
        clipentry.cliptime = 0 ;
        clipentry.length = 0 ;

		// clean key index list
		keyindex.clean();

        // setup progress informaion
		progress_chmin= 100*ch/fileheader.totalchannels+1 ;
		progress_chmax= 100*(ch+1)/fileheader.totalchannels ;
        if( progress_chmax>99 ) progress_chmax = 99 ;

		dupstate->percent = progress_chmin ;

		// create a file copy stream
		dvrstream * stream = player->newstream(ch);

		if( stream ) {
			// write stream header
			unsigned char encrypted_header[256];
			memcpy( encrypted_header, Head264, 40);
			if( crypt_table ) {			
				// encrypt 264 header
				RC4_block_crypt( encrypted_header, 40, 0, crypt_table, 4096);
			}
			fwrite( encrypted_header, 1, 40, dvrfile);

			// seek to begin position
			if( stream->seek( &begintime )==DVR_ERROR_FILEPASSWORD ) {
				// set stream password
				stream->setpassword(ENC_MODE_RC4FRAME, player->m_password, sizeof(player->m_password) );	// set frameRC4 password
				stream->setpassword(ENC_MODE_WIN, &player->m_hVideoKey, sizeof(player->m_hVideoKey) ) ;		// set winRC4 password
				stream->seek( &begintime );													// seek again
			}

			framedata=NULL ;
			while( stream->getdata( &framedata, &framesize, &frametype ) ) {
				stream->gettime( &frametime ) ;

				int chdiff = dvrtime_diff(&begintime, &frametime) ;

				// check key frame (harry)
				if( frametype==FRAME_TYPE_KEYVIDEO ) {
					pkeyentry=keyindex.append();
					pkeyentry->offset = file_tell(dvrfile)-pchannelex[ch].begin;
					pkeyentry->time = chdiff ;
				}

                // clip time check
                if( chdiff - (clipentry.cliptime+clipentry.length) > 3 ) {
                    if( clipentry.length>0 ) {
                        clipindex.add(clipentry);
                    }
                    // make a new clip entry
                    clipentry.cliptime = chdiff ;
                    clipentry.length = 1 ;
                }
                else {
                    // merge clip entry
                    if( chdiff > (clipentry.cliptime+clipentry.length) ) {
                        clipentry.length = chdiff-clipentry.cliptime + 1 ;
                    }
                    else if( chdiff < clipentry.cliptime ) {
                        clipentry.length += clipentry.cliptime-chdiff ;
                        clipentry.cliptime = chdiff ;
                    }
                }

				if( crypt_table ) {
					// encrypt frame data
					struct hd_frame *pframe =(struct hd_frame *)framedata ;
					fsize = pframe->framesize ;
					if( fsize>1024 ) {
						fsize=1024 ;
					}
					RC4_block_crypt( (unsigned char *)framedata+sizeof(struct hd_frame), 
									 fsize,
									 0, crypt_table, 4096);
				}
				// write frame data to file
				fwrite( framedata, 1, framesize, dvrfile);
				// release frame data
				delete framedata ;
				framedata=NULL ;

				if( dupstate->update ) {
					dupstate->update=0;
					dupstate->percent = progress_chmin + chdiff*100/tdiff/fileheader.totalchannels ;
					if( dupstate->percent > progress_chmax ) 
                        dupstate->percent = progress_chmax ;
					sprintf(dupstate->msg, "Channel %d - %02d:%02d:%02d", 
						ch, 
						frametime.hour, 
						frametime.min, 
						frametime.second );
				}

                // find the end time
				if( dvrtime_compare( &frametime, &lasttime )>0 ) {
					lasttime = frametime;
				}

                // reach end time
				if( dvrtime_compare( &frametime, &endtime )>=0 ) {
                    break ;
				}

				if( dupstate->cancel || dupstate->status<0 ) {		// cancelled or error?
                    break;
				}
			}

			// delete file copy stream ;
			delete stream ;
		}
		
		// channel end position
		pchannel[ch].end = pchannelex[ch].end = file_tell(dvrfile) ;

		// write key index
		if( keyindex.size()>0 ) {
			pchannelex[ch].keybegin = file_tell(dvrfile) ;
			for( int k=0; k<keyindex.size(); k++ ) {
				fwrite( keyindex.at(k), 1, sizeof( struct DVRFILEKEYENTRY ), dvrfile );
			}
			pchannelex[ch].keyend = file_tell(dvrfile);
		}

        // write clip index
        if( clipentry.length>0 ) {
            clipindex.add(clipentry);
        }
		if( clipindex.size()>0 ) {
            pchannelex[ch].clipbegin = file_tell(dvrfile) ;
			for( int k=0; k<clipindex.size(); k++ ) {
				fwrite( clipindex.at(k), 1, sizeof( struct DVRFILECLIPENTRY ), dvrfile );
			}
            pchannelex[ch].clipend = file_tell(dvrfile);
		}

		if( dupstate->cancel || dupstate->status<0 ) {		// cancelled or error?
			break;
		}
	}

    // fix file length (in seconds)
	fileheaderex.length = dvrtime_diff(&begintime, &lasttime);

	// write completed file header
	file_seek( dvrfile, 0, SEEK_SET );
	fwrite( &fileheader, sizeof(fileheader), 1, dvrfile);			// write file header
	fwrite( pchannel, sizeof( struct DVRFILECHANNEL ), fileheader.totalchannels, dvrfile );		// old channel header
	fwrite( &fileheaderex, sizeof(fileheaderex), 1, dvrfile);		// extra file header
	fwrite( pchannelex, sizeof( struct DVRFILECHANNELEX), fileheader.totalchannels, dvrfile );	// extra channel header
	fclose(dvrfile);

	// release memory for channel header
	delete pchannel ;
	delete pchannelex ;

    dupstate->percent = 100 ;
	if( dupstate->cancel || dupstate->status<0 ) {		// cancelled or error?
		remove(duppath);								// remove the file
	}
	player->m_errorstate = 0 ;						

	return 0;
}

#define DVRSIZE32	(0x7f000000LL)

// check if ch enabled (in channels)
static int check_channels(int ch, char * channels) 
{
	if (channels == NULL || *channels == 0 ) {
		return 1;
	}
	int more = 1;
	char * chch = channels;
	while (more) {
		int r, n, c;
		char comma[4];
		comma[0] = 0;
		c = -1;
		n = 0;
		r = sscanf(chch, "%d %c%n", &c, comma, &n );
		if (r > 0 && c == ch) 
			return 1;
		if (r > 1 && comma[0] == ',' && n>0 ) {
			more = 1;
			chch += n;
		}
		else {
			more = 0;
		}
	}
	return 0;
}

static int create_dvr_file_64(class dvrplayer *player,
						   char *duppath, struct dup_state *dupstate,
						   struct dvrtime begintime,
						   struct dvrtime endtime,
						   unsigned char *crypt_table,
							char * channels)
{
	struct DVRFILEHEADER fileheader ;
	struct DVRFILECHANNEL * pchannel ;
	struct DVRFILEHEADEREX fileheaderex ;
	struct DVRFILECHANNELEX64 * pchannelex64 ;
	struct DVRFILEKEYENTRY64 * pkeyentry64 ;
	int ch;
	FILE *dvrfile ;
	int progress_chmax, progress_chmin ;		// maximum/minimum progress on a channel
	int tdiff;									// total time lenght 
	char * framedata ;							// frame data
	int		framesize ;
	int		frametype ;
	struct dvrtime frametime, lasttime ;
	OFF_T  fsize ;								// frame size
	OFF_T pos ;

	list <struct DVRFILEKEYENTRY64> keyindex64 ;	// key frame index
	list <struct DVRFILECLIPENTRY> clipindex ;	// list of clips 

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
	fileheaderex.platform_type = 0 ;
    fileheaderex.date = begintime.year * 10000 + begintime.month*100 + begintime.day ;
	fileheaderex.time = begintime.hour * 3600 + begintime.min * 60 + begintime.second ;
	fileheaderex.length = dvrtime_diff(&begintime, &endtime);
	fileheaderex.version[0] = g_playerversion[0] ;
	fileheaderex.version[1] = g_playerversion[1] ;
	fileheaderex.version[2] = g_playerversion[2] ;
	fileheaderex.version[3] = g_playerversion[3] ;
	fileheaderex.encryption_type = 0 ;
	if( crypt_table ) {
		fileheaderex.encryption_type = ENC_MODE_RC4FRAME ;
	}
	fileheaderex.totalchannels = fileheader.totalchannels ;
	strncpy( fileheaderex.servername, player->m_playerinfo.servername, sizeof(fileheaderex.servername) );

	pchannel = new struct DVRFILECHANNEL [fileheader.totalchannels] ;
	pchannelex64 = new struct DVRFILECHANNELEX64 [fileheaderex.totalchannels] ;

	memset(pchannel, 0, fileheader.totalchannels * sizeof(struct DVRFILECHANNEL) );
	memset(pchannelex64, 0, fileheaderex.totalchannels * sizeof(struct DVRFILECHANNELEX) );

	dupstate->percent=1 ;										// preset to 1%

	tdiff=dvrtime_diff(&begintime, &endtime);							// total time length
    lasttime = begintime ;

	// goto first channel data position
	file_seek(dvrfile, 
		  sizeof(fileheader) + 
			fileheader.totalchannels * sizeof(struct DVRFILECHANNEL) + 
			sizeof(fileheaderex) + 
			fileheaderex.totalchannels * sizeof(struct DVRFILECHANNELEX64),
		  SEEK_SET );

	for( ch=0; ch<fileheader.totalchannels; ch++) {

		// initial channel header
		strncpy( pchannelex64[ch].cameraname, player->m_channellist[ch].channelinfo.cameraname, sizeof(pchannelex64[ch].cameraname));

		pos = file_tell(dvrfile);
		pchannelex64[ch].begin64 = pos ;
		pchannelex64[ch].end64 = pos ;
		pchannelex64[ch].size = sizeof( struct DVRFILECHANNELEX64 ) ;
		pchannelex64[ch].ex64 = DVRFILECHANNELEX64_FLAG ;

		// clean key index list
		keyindex64.clean();

		// clean clip list
		clipindex.clean();
        struct DVRFILECLIPENTRY clipentry ;
        clipentry.cliptime = 0 ;
        clipentry.length = 0 ;

        // setup progress informaion
		progress_chmin= 100*ch/fileheader.totalchannels+1 ;
		progress_chmax= 100*(ch+1)/fileheader.totalchannels ;
        if( progress_chmax>99 ) progress_chmax = 99 ;

		dupstate->percent = progress_chmin ;

		if (check_channels(ch, channels)) {
			// create a file copy stream
			dvrstream * stream = player->newstream(ch);
			if (stream) {
				// write stream header
				unsigned char encrypted_header[256];
				memcpy(encrypted_header, Head264, 40);
				if (crypt_table) {
					// encrypt 264 header
					RC4_block_crypt(encrypted_header, 40, 0, crypt_table, 4096);
				}
				fwrite(encrypted_header, 1, 40, dvrfile);

				// seek to begin position
				if (stream->seek(&begintime) == DVR_ERROR_FILEPASSWORD) {
					// set stream password
					stream->setpassword(ENC_MODE_RC4FRAME, player->m_password, sizeof(player->m_password));	// set frameRC4 password
					stream->setpassword(ENC_MODE_WIN, &player->m_hVideoKey, sizeof(player->m_hVideoKey));		// set winRC4 password
					stream->seek(&begintime);													// seek again
				}

				framedata = NULL;
				while (stream->getdata(&framedata, &framesize, &frametype)) {
					stream->gettime(&frametime);

					// frame time in seconds
					int framediff = dvrtime_diff(&begintime, &frametime);

					// check key frame
					if (frametype == FRAME_TYPE_KEYVIDEO) {
						pkeyentry64 = keyindex64.append();
						pkeyentry64->offset = file_tell(dvrfile) - pchannelex64[ch].begin64;
						pkeyentry64->time = framediff;
					}

					// clip time check
					if (framediff - (clipentry.cliptime + clipentry.length) > 3) {
						if (clipentry.length>0) {
							clipindex.add(clipentry);
						}
						// make a new clip entry
						clipentry.cliptime = framediff;
						clipentry.length = 1;
					}
					else {
						// merge clip entry
						if (framediff > (clipentry.cliptime + clipentry.length)) {
							clipentry.length = framediff - clipentry.cliptime + 1;
						}
						else if (framediff < clipentry.cliptime) {
							clipentry.length += clipentry.cliptime - framediff;
							clipentry.cliptime = framediff;
						}
					}

					if (crypt_table) {
						// encrypt frame data
						struct hd_frame *pframe = (struct hd_frame *)framedata;
						fsize = pframe->framesize;
						if (fsize>1024) {
							fsize = 1024;
						}
						RC4_block_crypt((unsigned char *)framedata + sizeof(struct hd_frame),
							fsize,
							0, crypt_table, 4096);
					}
					// write frame data to file
					fwrite(framedata, 1, framesize, dvrfile);
					// release frame data
					delete framedata;
					framedata = NULL;

					if (dupstate->update) {
						dupstate->update = 0;
						dupstate->percent = progress_chmin + framediff * 100 / tdiff / fileheader.totalchannels;
						if (dupstate->percent > progress_chmax)
							dupstate->percent = progress_chmax;
						sprintf(dupstate->msg, "Channel %d - %02d:%02d:%02d",
							ch,
							frametime.hour,
							frametime.min,
							frametime.second);
					}

					// find the end time
					if (dvrtime_compare(&frametime, &lasttime)>0) {
						lasttime = frametime;
					}

					// reach end time
					if (dvrtime_compare(&frametime, &endtime) >= 0) {
						break;
					}

					if (dupstate->cancel || dupstate->status<0) {		// cancelled or error?
						break;
					}
				}

				// delete file copy stream ;
				delete stream;
			}
		}

		// write last clip index
        if( clipentry.length>0 ) {
            clipindex.add(clipentry);
        }
		
		// channel end position
		pos = file_tell(dvrfile) ;
		pchannelex64[ch].end64 = pos ;

		if( pos<DVRSIZE32 ) {
			// 32 bits support
			pchannel[ch].begin = pchannelex64[ch].begin = (DWORD) pchannelex64[ch].begin64 ;
			pchannel[ch].end = pchannelex64[ch].end = (DWORD) pchannelex64[ch].end64 ;
		}

		// write key index
		if( keyindex64.size()>0 ) {
			if( pos + keyindex64.size() * sizeof( struct DVRFILEKEYENTRY ) < DVRSIZE32 ) {		// fit into 32 bit offset ?
				pchannelex64[ch].keybegin = (DWORD)file_tell(dvrfile) ;
				for( int k=0; k<keyindex64.size(); k++ ) {
					struct DVRFILEKEYENTRY key32 ;
					key32.offset = (DWORD) keyindex64.at(k)->offset ;
					key32.time = keyindex64.at(k)->time ;
					fwrite( &key32, 1, sizeof( struct DVRFILEKEYENTRY ), dvrfile );
				}
				pchannelex64[ch].keyend = (DWORD)file_tell(dvrfile);
				pchannelex64[ch].keybegin64 = pchannelex64[ch].keyend64 = 0;
			}
			else {
				pchannelex64[ch].keybegin64 = file_tell(dvrfile) ;
				for( int k=0; k<keyindex64.size(); k++ ) {
					fwrite( keyindex64.at(k), 1, sizeof( struct DVRFILEKEYENTRY64 ), dvrfile );
				}
				pchannelex64[ch].keyend64 = file_tell(dvrfile);
				pchannelex64[ch].keybegin = pchannelex64[ch].keyend = 0;
			}
		}


		if( clipindex.size()>0 ) {
            pchannelex64[ch].clipbegin64 = file_tell(dvrfile) ;
			for( int k=0; k<clipindex.size(); k++ ) {
				fwrite( clipindex.at(k), 1, sizeof( struct DVRFILECLIPENTRY ), dvrfile );
			}
            pchannelex64[ch].clipend64 = file_tell(dvrfile);

			if( pchannelex64[ch].clipend64 < DVRSIZE32 ) {
				// 32 bits supports
				pchannelex64[ch].clipbegin = (int) pchannelex64[ch].clipbegin64;
				pchannelex64[ch].clipend = (int) pchannelex64[ch].clipend64;
			}
		}

		if( dupstate->cancel || dupstate->status<0 ) {		// cancelled or error?
			break;
		}
	}

    // fix file length (in seconds)
	fileheaderex.length = dvrtime_diff(&begintime, &lasttime);

	// write completed file header
	file_seek( dvrfile, 0, SEEK_SET );
	fwrite( &fileheader, sizeof(fileheader), 1, dvrfile);			// write file header
	fwrite( pchannel, sizeof( struct DVRFILECHANNEL ), fileheader.totalchannels, dvrfile );		// old channel header
	fwrite( &fileheaderex, sizeof(fileheaderex), 1, dvrfile);		// extra file header
	fwrite( pchannelex64, sizeof( struct DVRFILECHANNELEX64 ), fileheaderex.totalchannels, dvrfile );	// extra channel header
	fclose(dvrfile);

	// release memory for channel header
	delete pchannel ;
	delete pchannelex64 ;

    dupstate->percent = 100 ;
	if( dupstate->cancel || dupstate->status<0 ) {		// cancelled or error?
		remove(duppath);								// remove the file
	}
	player->m_errorstate = 0 ;						

	return 0;
}

int parse_hik_frame(char *buf, int size, DWORD *n_frames, DWORD *width, DWORD *height, DWORD *ts)
{
	int consumed = 0, i;
	struct hik_frame *f = (struct hik_frame *)buf;
	struct hik_subframe *sf;

	*n_frames = 0;

	if (size < sizeof(struct hik_frame))
		return 1;

	consumed += sizeof(struct hik_frame);
	if (f->flag != 1)
		return 1;

	//TRACE(_T("n_frame:%d(type:%x, ts:%d)\n"), f->n_frames, f->type, f->timestamp);
	*n_frames = f->n_frames;
	*ts = f->timestamp;
	*width = f->width;
	*height = f->height;

	for (i = 0; i < f->n_frames; i++) {
		sf = (struct hik_subframe *)(buf + consumed);
		consumed += sizeof(struct hik_subframe);
		if (consumed > size)
			break;
		//TRACE(_T("subframe%d:(type:%d, size:%d)\n"), i, sf->type, sf->framesize);
		consumed += sf->framesize;
	}
	return 0;
}

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
	DWORD lt = 0;
	struct dvrtime frametime;
	DWORD first_ts = 0, last_ts = 0;

	struct xvid_setup xs;
	xvid_setup_default(&xs);
	xvidlib_init(1, &xs);

#ifdef TEST_
    fpx = fopen("D:\\test1.yuv", "wb");
#endif
	// filename for avi file (with _CH suffix)
	strcpy(filename, duppath);
	ptr = strrchr(filename, '.'); 
	if (ptr == NULL)
		return -1;

	*ptr = '\0';
	sprintf(suffix, "_CH%d.avi", ch + 1);
	strcat(filename, suffix);

	// filename for temporary index file (with _CH suffix)
	strcpy(indexname, duppath);
	ptr = strrchr(indexname, '.'); 
	*ptr = '\0';
	sprintf(suffix, "_CH%d.k", ch + 1);
	strcat(indexname, suffix);

	// open avi & index file
    fp = fopen( filename, "wb") ;
    if( fp==NULL ) 
		return -1;

	fseek(fp, 1024 + 12, SEEK_SET );

    fpi = fopen( indexname, "w+b" ) ;
	if (fpi==NULL) {
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
			stream->setpassword(ENC_MODE_WIN, &player->m_hVideoKey, sizeof(player->m_hVideoKey) ) ;		// set winRC4 password
			stream->seek( &begintime );													// seek again
		}

		framedata=NULL ;
		int error = 0;
		int chunk_size;
		while( stream->getdata( &framedata, &framesize, &frametype ) ) {
			stream->gettime( &frametime ) ;

			chdiff = dvrtime_diff(&begintime, &frametime) ;
			//TRACE(_T("time:%d/%d/%d,%d:%d:%d.%d\n"),frametime.year,frametime.month,frametime.day,frametime.hour, frametime.min, frametime.second,frametime.millisecond);

			if(chdiff>=tdiff) {				// up to totol length in time
				delete framedata ;
				break;
			}

			DWORD n_frames, width, height, ts;
			parse_hik_frame(framedata, framesize, &n_frames, &width, &height, &ts);
			if (ai.resolution.width == 0) {
				ai.resolution.width = width;
				ai.resolution.height = height;
				
				xs.width = width;
				xs.height = height;
				if (xvid_init(&xs)) {
					xs.enc_handle = NULL;
				}
			}
			if (first_ts == 0)
				first_ts = ts;
			if (ts > last_ts)
				last_ts = ts;

			//TRACE(_T("Inputdata:%d,%d\n"),frametype, framesize);
			while(true) {
				int r;
				if (PlayM4_InputData(g_hs[ch].port, (PBYTE)framedata, (DWORD)framesize))
					break;
				r = PlayM4_GetLastError(g_hs[ch].port);
				if (r != PLAYM4_BUF_OVER)
					break;
			}
			if ((frametype == FRAME_TYPE_KEYVIDEO) || (frametype == FRAME_TYPE_VIDEO)) {
				g_hs[ch].v_sent += n_frames;
			} else if (frametype == FRAME_TYPE_AUDIO) {
				g_hs[ch].a_sent += n_frames;
			}
			delete framedata ;
			framedata=NULL ;

			//TRACE(_T("a:%d,v:%d,a_sent:%d,v_sent:%d\n"),g_hs[ch].a, g_hs[ch].v, g_hs[ch].a_sent, g_hs[ch].v_sent);
			while ((f = unbuffer_av_frame(&g_hs[ch]))) {
				write_to_avifile(&xs, &avc, fp, fpi, &ai, f);
				//TRACE(_T("unbuffer:%d,%d\n"),f->type, f->len);
				free(f);
				lt = GetTickCount(); // mark successful unbuffer time
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
			lt = GetTickCount(); // mark successful unbuffer time
		} else {
			if ((g_hs[ch].a == g_hs[ch].a_sent) && (g_hs[ch].v == g_hs[ch].v_sent))
				break;
			DWORD now = GetTickCount();
			DWORD diff = now - lt;
			if ((lt > 0) && (diff > 1000))
				break; // sometime shit happens.
			Sleep(0);
		}
	}
	//TRACE(_T("a:%d,v:%d,a_sent:%d,v_sent:%d\n"),g_hs[ch].a, g_hs[ch].v, g_hs[ch].a_sent, g_hs[ch].v_sent);

	// calculate frame rate
	float rate;
	float elapsed_time_in_sec = tstamp2ms(last_ts - first_ts) / 1000.0;
	rate = g_hs[ch].v_sent / elapsed_time_in_sec;
	int frame_rate = (int)(rate + 0.2f);
	ai.frameperiod.numerator = 1001;
	ai.frameperiod.denominator = frame_rate * 1000;

	/* copy index data to the end of the avi file */
	if (ai.body_size > 0) {
		BYTE buf[2048];
		int bytes;

		memcpy(buf, "idx1", 4);
		SetDWLE(buf + 4, ai.index_size);
		fwrite(buf, sizeof(char), 8, fp);
		fseek(fpi, 0, SEEK_SET );
		while ((bytes = fread(buf, sizeof(char), sizeof(buf), fpi)) > 0) {
			//_RPT1(_CRT_WARN, "%d bytes read\n", bytes);
			fwrite(buf, sizeof(char), bytes, fp);
		//_RPT1(_CRT_WARN, "%d bytes written\n", bytes);
		}
	}
	fclose(fpi);

	if (ai.body_size > 0) {
		fseek(fp, 0, SEEK_SET );
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

#ifdef TEST_
	fclose(fpx);
#endif

	return 0;
}

static int create_avi_file(class dvrplayer *player,
						   char *duppath, struct dup_state *dupstate,
						   struct dvrtime begintime,
						   struct dvrtime endtime,
							char * channels)
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

		if (check_channels(ch, channels)) {
			// start recording channel data
			if (avifile_record_channel(player, ch, duppath, dupstate,
				begintime, endtime, progress_chmin, progress_chmax) < 0)
				break;
		}

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
	char duppath[256] ;
    strcpy( duppath, cpinfo->duppath );
	int flags = cpinfo->flags ;
	struct dup_state *dupstate= cpinfo->dupstate;
	enc = false;
	if( cpinfo->password && cpinfo->password[0] != 0 ) {
		enc = true;
		key_256( cpinfo->password, k256);
		RC4_crypt_table( crypt_table, 4096, k256 );
	}

	string channels;
	channels = cpinfo->channels;

	SetEvent(cpinfo->hRunEvent);						// let calling function go

	if (player->m_playerinfo.total_channel == 0) {
		dupstate->status = -1;
		return 1;
	}

	filetype = get_filetype(duppath);

	if (filetype == FILETYPE_UNKNOWN) {
		dupstate->status = -1;
	} else if (filetype == FILETYPE_DVR) {
		create_dvr_file_64(player, duppath, dupstate, begintime, endtime,
			enc ? crypt_table : NULL, (char *)channels);
	} else {
		create_avi_file(player, duppath, dupstate, begintime, endtime, (char *)channels);
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
