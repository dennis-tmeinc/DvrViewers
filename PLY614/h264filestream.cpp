
// hard drive stream in module
//

#include "stdafx.h"
#include <io.h>
#include <process.h>
#include <crtdbg.h>
#include "player.h"
#include "ply614.h"
#include "dvrnet.h"
#include "h264filestream.h"

// open a .265 file stream for playback
h264filestream::h264filestream(char * filename, int channel)
{
	DWORD  dvrheader[10] ;				// dvr clip header
	int    lastframe ;

	//audio = 0;
	m_keydata = NULL;
	m_encryptmode = -1;
	m_ps.Init();

	m_channel=channel ;
	m_file = fopen( filename, "rb" );
	if( m_file ) {
		fread( dvrheader, 1, sizeof(dvrheader), m_file );

		if( dvrheader[0] == FOURCC_HKMI ) {				// hik .265 file tag
			m_encryptmode = ENC_MODE_NONE ;			// no encryption
		}

		BYTE startcode[4];
		m_ps.SetStream(m_file);
		if (m_ps.StreamPeek(startcode, 4) < 4) {
	        _RPT0(_CRT_WARN, "No MPEG PS header\n" );
			fclose( m_file );
			m_file=NULL ;
			return ;
		}
		if( startcode[0] != 0 || startcode[1] != 0 ||
			startcode[2] != 1 || startcode[3] < 0xb9 )
		{
			fclose( m_file );
			m_file=NULL ;
			return ;
		}

		__int64 len = m_ps.FindLength();

		if (len == 0LL) {
			fclose( m_file );
			m_file=NULL;
			return;
		}


		m_begintimestamp = m_ps.GetFirstTimeStamp();

		m_totalchannel = 1 ;

		if( channel>=m_totalchannel ) {
			fclose( m_file );
			m_file=NULL ;
			return;
		}

		m_maxframesize = 50*1024 ;

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
		if( sscanf( &basefilename[5], "%04d%02d%02d%02d%02d%02d_%d_", 
			&m_begintime.year,
			&m_begintime.month,
			&m_begintime.day,
			&m_begintime.hour,
			&m_begintime.min,
			&m_begintime.second,
			&m_filelen ) != 7 ) {
			// can't get the file time
			time_t t ;
			dvrtime_localtime( &m_begintime, time(&t) );
			m_begintime.hour=0;
			m_begintime.min=0;
			m_begintime.second=0;
			m_filelen=(int)(len/1000000);
		}

		m_curtime = m_begintime ;
	
		// initialize extra information
		strcpy(m_servername, "H265FILE");	// default servername
		strncpy(m_cameraname, basefilename, sizeof(m_cameraname) );

		// seek to the beginning of data
		fseek( m_file, 40, SEEK_SET );
	}
}

h264filestream::~h264filestream()
{
				//TRACE(_T("audio:%d\n"), audio);
	if( m_file ) {
		fclose( m_file );
	}
}

// seek to specified time.
// return 1: success
//        0: error
//        DVR_ERROR_FILEPASSWORD: need password/password error
int h264filestream::seek( struct dvrtime * dvrt ) 		
{
	__int64 seektimestamp ;		// seeking time stamp
	double ffsize ;				// file size in double precision
	__int64 pos ;
	__int64 endpos ;

	if (m_encryptmode == -1) {
		return DVR_ERROR_FILEPASSWORD;
	}

	m_curtime = *dvrt ;
	m_curtime.millisecond=0 ;		// make sure we don't have insane milisecond

	int diff = dvrtime_diff( &m_begintime, dvrt );

	if( m_file==NULL ) {
		return -1;
	}

	if( diff<=0 ) {
		fseek( m_file, 40, SEEK_SET );
		return 1 ;
	}
	else if( diff>=m_filelen ) {
		fseek( m_file, 0, SEEK_END ) ;
		return 1;
	}

	seektimestamp = diff*1000000LL + m_begintimestamp ;

	endpos = m_ps.StreamSize();

	ffsize = endpos-40 ;
	pos = (__int64) (diff*ffsize/m_filelen + 40 ) ;
	_fseeki64(m_file, pos, SEEK_SET);	
	return 1; 
}


// get dvr data frame
int h264filestream::getdata(char **framedata, int * framesize, int * frametype, int *headerlen)
{
	__int64 framestart, timestamp;

	*frametype = FRAME_TYPE_UNKNOWN ;
	*framesize = 0;

	if( m_file==NULL ) {
		return 0 ;
	}

	framestart = _ftelli64(m_file);

	if( framestart < 40 ) {
		framestart = 40 ;
		_fseeki64( m_file, framestart, SEEK_SET ) ;
	}

	char *pBuf;
	int headerLen;
	if (m_ps.GetOneFrame(&pBuf, framesize, frametype, &timestamp, &headerLen)) {		
		if (timestamp > 0) {
			if( m_encryptmode == ENC_MODE_RC4FRAME ) {
				int fsize = *framesize - headerLen ;
				if( fsize>1024 ) {
					fsize=1024 ;
				}
				RC4_crypt((unsigned char *)(pBuf + headerLen), fsize);
			}
			if (headerlen) *headerlen = headerLen;

			m_curtime = m_begintime ;
			__int64 diff = timestamp-m_begintimestamp;
			__int64 diff_in_milsec = diff / 1000;
			dvrtime_addmilisecond(&m_curtime, (int)diff_in_milsec) ;
			/*
			if (*frametype == FRAME_TYPE_AUDIO) {
				TRACE(_T("audio ts:%I64d\n"), timestamp);
				audio++;
			}*/
		}
		if (*framesize > 0 && framedata) {
			*framedata = (char *)malloc(*framesize);
			memcpy(*framedata, pBuf, *framesize);
			return 1;
		}
	}
	return 0;
}

// get current time and timestamp
// return 1: in sync,
//        0: not in sync
int h264filestream::gettime( struct dvrtime * dvrt )
{
	*dvrt = m_curtime ;
	return  1 ;
}

// get stream information
// set ppi->total_channel, ppi->servername 
// return 0: failed
//        1: success ;
int h264filestream::getserverinfo(struct player_info *ppi)
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
int h264filestream::getchannelinfo(struct channel_info * pci)
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
int h264filestream::getdayinfo( dvrtime * daytime )			
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
int h264filestream::gettimeinfo( struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize)
{
	int maxlen ;

	struct dvrtime fileendtime =m_begintime ;
	dvrtime_add(&fileendtime, m_filelen) ;

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

// set key block
void h264filestream::setkeydata( struct key_data * keydata )
{
	unsigned char * k256 ;
	m_keydata = keydata ;
	if( m_keydata ) {
		m_encryptmode = ENC_MODE_RC4FRAME ;
		k256 = (unsigned char *)m_keydata->videokey ;
		RC4_crypt_table( m_crypt_table, sizeof(m_crypt_table), k256 );
	}
}
