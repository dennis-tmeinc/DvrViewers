
// hard drive stream in module
//

#include "stdafx.h"
#include <stdio.h>
#include <io.h>
#include <process.h>
#include "player.h"
#include "ply606.h"
#include "dvrnet.h"
#include "dvrfilestream.h"
#include "utils.h"


// open a .DVR file stream for playback
dvrfilestream::dvrfilestream(char * filename, int channel)
{
	struct DVRFILEHEADER fileheader ;
	struct DVRFILECHANNEL channelhd ;
	struct DVRFILEHEADEREX  fileheaderex ;
	struct DVRFILECHANNELEX channelhdex ;
	int i, idxSize = 0;				
	index1_entry_t *readBuf = NULL;

	__int64 fpos ;							// store file position

	m_ps.Init();

	m_maxframesize = 50*1024 ;

	m_channel=channel ;
	m_file = fopen( filename, "rb" );
	if( m_file ) {
		fread( &fileheader, sizeof( fileheader ), 1, m_file );

		if( fileheader.flag != DVRFILEFLAG ) {			// not a right file
			fclose( m_file );
			m_file=NULL ;
			return ;
		}

		m_totalchannel = fileheader.totalchannels ;

		if( channel>=m_totalchannel ) {
			fclose( m_file );
			m_file=NULL ;
		}

		dvrtime_localtime( &m_begintime, (time_t)fileheader.begintime );
		dvrtime_localtime( &m_endtime, (time_t) fileheader.endtime );
		m_curtime = m_begintime ;
	
		// seek to channel header
		_fseeki64( m_file, sizeof( fileheader )+m_channel*sizeof(struct DVRFILECHANNEL), SEEK_SET );
		fread( &channelhd, sizeof(struct DVRFILECHANNEL), 1, m_file);

		m_beginpos = channelhd.begin ;
		m_endpos = channelhd.end ;

		// initialize extra information
		m_version[0]=m_version[1]=m_version[2]=m_version[3]=0;
		strcpy(m_servername, "DVR");	// default servername
		sprintf(m_cameraname, "camera%d", m_channel+1 );	// default camera name

		m_encryptmode = ENC_MODE_NONE ;

		// seek to extra file header
		_fseeki64( m_file, sizeof( fileheader ) + m_totalchannel * sizeof(struct DVRFILECHANNEL), SEEK_SET );
		fpos = _ftelli64(m_file);								// fpos is extre file header position
		fread( &fileheaderex, sizeof( fileheaderex ), 1, m_file );
		if( fileheaderex.flagex == DVRFILEFLAGEX ) {		// found file extra header
			strncpy( m_servername, fileheaderex.servername, sizeof(m_servername) );
			m_version[0] = fileheaderex.version[0] ;
			m_version[1] = fileheaderex.version[1] ;
			m_version[2] = fileheaderex.version[2] ;
			m_version[3] = fileheaderex.version[3] ;

			m_encryptmode = fileheaderex.encryption_type ;

			m_begintime.year  = fileheaderex.date / 10000 ;
			m_begintime.month = fileheaderex.date % 10000 / 100 ;
			m_begintime.day   = fileheaderex.date % 100 ;
			m_begintime.hour  = fileheaderex.time / 3600 ;
			m_begintime.min   = fileheaderex.time % 3600 / 60 ;
			m_begintime.second= fileheaderex.time % 60 ;
			m_begintime.millisecond  = fileheaderex.millisec ;
			m_begintime.tz = 0 ;
			m_curtime = m_begintime ;
			m_endtime = m_begintime ;
			dvrtime_addmilisecond(&m_endtime, fileheaderex.length * 1000 + fileheaderex.len_millisec) ;

			// go to extra channel info header
			fpos+=fileheaderex.size;										// fpos is channel extra header position
			_fseeki64( m_file, fpos, SEEK_SET );
			fread( &channelhdex, sizeof(channelhdex), 1, m_file );			// read first channel ex header
			_fseeki64( m_file, fpos+m_channel*channelhdex.size, SEEK_SET );		
			fread( &channelhdex, sizeof(channelhdex), 1, m_file );			// read this channel ex header
			strncpy( m_cameraname, channelhdex.cameraname, sizeof(m_cameraname) );
			m_beginpos = channelhdex.begin ;
			m_endpos = channelhdex.end ;
			if( channelhdex.keybegin>0 && 
				channelhdex.keyend>0 && 
				channelhdex.keyend>channelhdex.keybegin ) {		// key frame index available ?
				idxSize = (channelhdex.keyend-channelhdex.keybegin)/sizeof(index1_entry_t);
				_fseeki64(m_file, channelhdex.keybegin, SEEK_SET );
#if 0
				index1_entry_t entry;

				m_keyindex.clear();
				readBuf = (index1_entry_t *)malloc(idxSize * sizeof(index1_entry_t));
				if (readBuf) {
					int read = fread(readBuf, sizeof(index1_entry_t), idxSize, m_file);
					if (read == idxSize) {
						for (i = 0; i < idxSize; i++) {
							entry = readBuf[i];
							m_keyindex.push_back(entry);
						}
					}
					free(readBuf);
					readBuf = NULL;
				}
#else
				readBuf = (index1_entry_t *)malloc(idxSize * sizeof(index1_entry_t));
				if (readBuf) {
					int read = fread(readBuf, sizeof(index1_entry_t), idxSize, m_file);
					if (read < idxSize) {
						free(readBuf);
						readBuf = NULL;
					}
				}
#endif
			}
		}

		if (idxSize <= 0) {
			fclose( m_file );
			m_file=NULL ;
			goto dvrfile_end;
		}

		struct mp5_header fileHeader;
		int ret;
		// seek to the beginning of channel data
		_fseeki64( m_file, m_beginpos, SEEK_SET );
		ret = fread( &fileHeader, 1, sizeof(fileHeader), m_file );
		if (ret < sizeof(fileHeader)) {
			fclose( m_file );
			m_file=NULL ;
			goto dvrfile_end;
		}

		m_ps.SetStream(m_file);

		__int64 timestamp, len;
		//len = m_ps.ReadIndexFromMemory(&m_keyindex, &timestamp);
		len = m_ps.ReadIndexFromMemory(readBuf, idxSize, &timestamp);
		if (len == 0LL) {
			fclose( m_file );
			m_file=NULL;
			m_ps.SetStream(NULL);
			goto dvrfile_end;
		}

dvrfile_end:
		if (readBuf) free(readBuf);
		//m_sync.m_insync=0 ;							// not in sync yet
		//m_sync.m_synctime = m_begintime ;
		//m_sync.m_synctimestamp = timestamp ;
	}
}

dvrfilestream::~dvrfilestream()
{
	if( m_file ) {
		fclose( m_file );
	}
}

// seek to specified time.
// return 1: success
//        0: error
//        DVR_ERROR_FILEPASSWORD: need password/password error
int dvrfilestream::seek( struct dvrtime * dvrt ) 		
{
	struct mp5_header fileHeader;

	m_curtime = *dvrt ;
	//m_curtime.millisecond=0 ;		// make sure we don't have insane milisecond
	//m_sync.m_insync = 0 ;			// make synctimer invalid

	if( m_file==NULL ) {
		return -1;
	}

	_fseeki64( m_file, m_beginpos, SEEK_SET );
	fread( &fileHeader, 1, sizeof( fileHeader ), m_file );
	if( m_encryptmode == ENC_MODE_RC4FRAME ) {
		RC4_crypt((unsigned char *)&fileHeader, min(sizeof( fileHeader ), 1024));
		if( fileHeader.fourcc_main != FOURCC_HKMI ) {
			return DVR_ERROR_FILEPASSWORD ; 
		}
		m_ps.SetEncrptTable(m_crypt_table);
	}

	if( fileHeader.fourcc_main != FOURCC_HKMI ) {
		return 0;
	}

	_fseeki64( m_file, m_beginpos+1024, SEEK_SET );
	if (compareDvrTime(dvrt, &m_begintime) > 0) {
		__int64 beginTime, seekTime, diff;
		beginTime = GetMicroTimeFromDvrTime(&m_begintime);
		seekTime = GetMicroTimeFromDvrTime(dvrt);
		m_ps.Seek(seekTime - beginTime, m_beginpos);							
	}

	return 1 ;
}

block_t *dvrfilestream::getdatablock()
{
	__int64 framestart;
	block_t *pBlock;

	if( m_file==NULL ) {
		return NULL;
	}

	pBlock = m_ps.GetFrame(m_beginpos, m_endpos);
	if (pBlock) {
		if ((pBlock->i_pts > 0) || (pBlock->i_dts > 0)) {
			__int64 ts = max(pBlock->i_pts, pBlock->i_dts);
			m_curtime = m_begintime ;
			__int64 diff = ts-1;
			__int64 diff_in_milsec = diff / 1000;
			dvrtime_addmilisecond(&m_curtime, (int)diff_in_milsec) ;
		}
		return pBlock;
	}

	return NULL;
}

// get current time and timestamp
// return 1: in sync,
//        0: not in sync
int dvrfilestream::gettime( struct dvrtime * dvrt )
{
	*dvrt = m_curtime ;
	return  1;//m_sync.m_insync ;
}

// provide and internal structure to sync other stream
void * dvrfilestream::getsynctimer()
{
	m_sync.m_insync=1 ;
	return (void *)&m_sync ;
}

// use synctimer to setup internal sync timer
void dvrfilestream::setsynctimer(void * synctimer)
{
	m_sync = * (struct sync_timer *)synctimer ;
}


// get stream information
// set ppi->total_channel, ppi->servername 
// return 0: failed
//        1: success ;
int dvrfilestream::getserverinfo(struct player_info *ppi)
{
	if( ppi->size<sizeof(struct player_info) ){
		return 0 ;
	}
	memset( ppi, 0, sizeof( struct player_info ) );
	ppi->size = sizeof(struct player_info) ;
	ppi->type = 0 ;
	ppi->serverversion[0] = m_version[0] ;
	ppi->serverversion[1] = m_version[1] ;
	ppi->serverversion[2] = m_version[2] ;
	ppi->serverversion[3] = m_version[3] ;
	ppi->total_channel = m_totalchannel ;
	strncpy( ppi->servername, m_servername, sizeof(ppi->servername) );
	return 1 ;
}

// get stream channel information
int dvrfilestream::getchannelinfo(struct channel_info * pci)
{
	if( pci->size<sizeof(struct channel_info) ) {
		return 0 ;
	}
	memset( pci, 0, sizeof( struct channel_info ));
	pci->size = sizeof( struct channel_info ) ;
	pci->features = 1 ;
	strncpy( pci->cameraname, m_cameraname, sizeof( pci->cameraname ) );
	return 1 ;
}

// get stream day data availability information
int dvrfilestream::getdayinfo( dvrtime * daytime )			
{
	if( daytime->day == m_begintime.day &&
		daytime->month == m_begintime.month && 
		daytime->year == m_begintime.year ) {
			return 1 ;
	}
	else {
		return 0;
	}
}

// get data availability information
// get video clip time information in specified time range
// parameter:
//    begintime -  begining time to get clip time information
//    endtime   -  ending time to get clip time information
//    tinfo     -  array of cliptimeinfo to get clip time information
//    tinfosize -  size of tinfo. If this value is zero, return value is required size of the tinfo array
// return
//    Return actual cliptimeinfo size. if return value is smaller or equal than tinfosize, then all time information is copied to tinfo. Otherwise, only parts of time info is copied. Return 0 if no video detected in specified time range.
//
int dvrfilestream::gettimeinfo( struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize)
{
	int maxlen ;

	if( dvrtime_compare( &m_endtime, begintime)<=0 ||		
		dvrtime_compare( &m_begintime, endtime )>=0 ) {
			return 0 ;
	}

	if( tinfo==NULL || tinfosize==0 ) {		// if no buffer provided
		return 1;
	}

	tinfo->on_time = dvrtime_diff(begintime, &m_begintime) ;
	if( tinfo->on_time<0 ) {
		tinfo->on_time=0;
	}

	maxlen = dvrtime_diff( begintime, endtime );
	tinfo->off_time = dvrtime_diff( begintime, &m_endtime) ;
	if( tinfo->off_time > maxlen ) {
		tinfo->off_time = maxlen ;
	}
	return 1;
}

// get locked file availability information
int dvrfilestream::getlockfileinfo(struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize )	
{
	return 0 ;			// no locked files
}
