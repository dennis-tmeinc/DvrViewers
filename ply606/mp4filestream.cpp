
// hard drive stream in module
//

#include "stdafx.h"
#include <stdio.h>
#include <time.h>
#include <io.h>
#include <process.h>
#include <crtdbg.h>
#include "player.h"
#include "ply606.h"
#include "dvrnet.h"
#include "mp4filestream.h"


// open a .mp5 file stream for playback
mp4filestream::mp4filestream(char * filename, int channel)
{
	int    lastframe ;
	int ret;

	m_maxframesize = 50*1024 ;
	m_totalchannel = 1 ;
	m_channel=channel ;
	if( channel>=1 ) {
		return;
	}	

	m_encryptmode = -1;
	m_keydata = NULL;

	m_ps.Init();

	m_file = fopen( filename, "rb" );
	if( m_file ) {

		ret = fread( &m_fileHeader, 1, sizeof(struct mp5_header), m_file );

		if (ret < sizeof(struct mp5_header)) {
			fclose( m_file );
			m_file=NULL ;
			return;
		}

		if( m_fileHeader.fourcc_main == FOURCC_HKMI ) {	
			m_encryptmode = ENC_MODE_NONE ;			// no encryption
		}

		//m_ps.SetFilename(filename);
		m_ps.SetStream(m_file);
		m_ps.ParseFileHeader(&m_fileHeader);

		m_filelen = m_ps.ReadIndexFromFile(filename, &m_begintimestamp);

		// get file time and length from file name. (if possible)
		char * basefilename ;
		int l=(int)strlen(filename); 
		while( --l>=0 ) {
			if( filename[l]=='\\' ) {
				break;
			}
		}
		if( l<0 ) {
			basefilename = filename ;
		}
		else if( filename[l]=='\\' ) {
			basefilename = &filename[l+1] ;
		}
		else {
			basefilename = &filename[l] ;
		}

		dvrtime_init(&m_begintime, 2000 );

		int len = 0;
#ifdef USE_LONG_FILENAME
		ret = sscanf_s(&basefilename[5],
			"%04d%02d%02d%02d%02d%02d%03d_%u_",
			&m_begintime.year,
			&m_begintime.month,
			&m_begintime.day,
			&m_begintime.hour,
			&m_begintime.min,
			&m_begintime.second,
			&m_begintime.millisecond,
			&len);

		if( ret != 8 ) {
#else
		ret = sscanf_s(&basefilename[5],
			"%04d%02d%02d%02d%02d%02d_%u_",
			&m_begintime.year,
			&m_begintime.month,
			&m_begintime.day,
			&m_begintime.hour,
			&m_begintime.min,
			&m_begintime.second,
			&len);
		len *= 1000;

		if( ret != 7 ) {
#endif
			// can't get the file time
			time_t t ;
			dvrtime_localtime( &m_begintime, time(&t) );
			m_begintime.hour=0;
			m_begintime.min=0;
			m_begintime.second=0;
		}
		if (m_filelen == 0)
			m_filelen = len;

		m_curtime = m_begintime ;
	
		// initialize extra information
		strcpy(m_servername, "606FILE");	// default servername
		strncpy(m_cameraname, basefilename, sizeof(m_cameraname) );
	}
#ifdef AUDIO_TEST
	m_fp = fopen("D:\\audiotest606.raw", "wb");
#endif
}

mp4filestream::~mp4filestream()
{
	if( m_file ) {
		fclose( m_file );
	}
#ifdef AUDIO_TEST
	if (m_fp) fclose(m_fp);
#endif
}

// seek to specified time.
// return 1: success
//        0: error
//        DVR_ERROR_FILEPASSWORD: need password/password error
int mp4filestream::seek( struct dvrtime * dvrt ) 		
{
	double ffsize ;				// file size in double precision
	__int64 pos ;
	__int64 endpos ;

	if (m_encryptmode == -1) {
		return DVR_ERROR_FILEPASSWORD;
	}

	if( m_file==NULL ) {
		return -1;
	}

	if (m_filelen == 0)
		return 1;

	m_curtime = *dvrt ;

	__int64 diff = dvrtime_diffms( &m_begintime, dvrt );

	if( diff<=0 ) {
		diff = 0 ;
	}
	else if( diff>=m_filelen ) {
		diff = m_filelen;
	}

	m_ps.Seek(diff * 1000LL, 0);

	return 1; 
}

block_t *mp4filestream::getdatablock()
{
	__int64 framestart;
	block_t *pBlock;

	if( m_file==NULL ) {
		return NULL;
	}

	framestart = _ftelli64(m_file);

	if( framestart < 1024 ) {
		framestart = 1024 ;
		_fseeki64( m_file, framestart, SEEK_SET ) ;
	}

	pBlock = m_ps.GetFrame();
	if (pBlock) {
		if ((pBlock->i_pts > 0) || (pBlock->i_dts > 0)) {
			__int64 ts = max(pBlock->i_pts, pBlock->i_dts);
			m_curtime = m_begintime ;
			__int64 diff = ts-m_begintimestamp;
			__int64 diff_in_milsec = diff / 1000;
			dvrtime_addmilisecond(&m_curtime, (int)diff_in_milsec) ;
		}
#ifdef AUDIO_TEST
		if (pBlock->i_flags == BLOCK_FLAG_TYPE_AUDIO && m_fp) {
			fwrite(pBlock->p_buffer, 1, pBlock->i_buffer, m_fp);
		}
#endif
		return pBlock;
	}

	return NULL;
}

// get current time and timestamp
// return 1: in sync,
//        0: not in sync
int mp4filestream::gettime( struct dvrtime * dvrt )
{
	*dvrt = m_curtime ;
	return  1 ;
}

// get stream information
// set ppi->total_channel, ppi->servername 
// return 0: failed
//        1: success ;
int mp4filestream::getserverinfo(struct player_info *ppi)
{
	if( ppi->size<sizeof(struct player_info) ){
		return 0 ;
	}
	memset( ppi, 0, sizeof( struct player_info ) );
	ppi->size = sizeof(struct player_info) ;
	ppi->type = 0 ;
	ppi->total_channel = m_totalchannel ;
	strncpy( ppi->servername, m_servername, sizeof(ppi->servername) );
	return 1 ;
}

// get stream channel information
int mp4filestream::getchannelinfo(struct channel_info * pci)
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
int mp4filestream::getdayinfo( dvrtime * daytime )			
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
int mp4filestream::gettimeinfo( struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize)
{
	int maxlen ;

	struct dvrtime fileendtime =m_begintime ;
	dvrtime_addmilisecond(&fileendtime, m_filelen) ;

	if( dvrtime_compare( &fileendtime, begintime)<=0 ||		
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
	tinfo->off_time = dvrtime_diff( begintime, &fileendtime) ;
	if( tinfo->off_time > maxlen ) {
		tinfo->off_time = maxlen ;
	}
	return 1;
}

void mp4filestream::setpassword(int passwdtype, void * password, int passwdsize )
{
	if( passwdtype==ENC_MODE_RC4FRAME ) {			// Frame RC4
		RC4_gentable((char *)password);

		struct mp5_header fileHeader;
		memcpy(&fileHeader, &m_fileHeader, sizeof(struct mp5_header));
		RC4_crypt( (unsigned char *)&fileHeader,  12 );
		if( fileHeader.fourcc_main == FOURCC_HKMI ) {
			m_encryptmode = ENC_MODE_RC4FRAME;
			m_ps.SetEncrptTable(m_crypt_table);
		}
	}
}

// set key block
void mp4filestream::setkeydata( struct key_data * keydata )
{
	unsigned char * k256 ;
	m_keydata = keydata ;
	if( m_keydata ) {
		m_encryptmode = ENC_MODE_RC4FRAME ;
		k256 = (unsigned char *)m_keydata->videokey ;
		RC4_crypt_table( m_crypt_table, sizeof(m_crypt_table), k256 );

		struct mp5_header fileHeader;
		memcpy(&fileHeader, &m_fileHeader, sizeof(struct mp5_header));
		RC4_crypt( (unsigned char *)&fileHeader,  12 );
		if( fileHeader.fourcc_main == FOURCC_HKMI ) {
			m_encryptmode = ENC_MODE_RC4FRAME;
			m_ps.SetEncrptTable(m_crypt_table);
		}
	}
}

