
// hard drive stream in module
//

#include "stdafx.h"
#include <io.h>
#include <process.h>
#include "player.h"
#include "ply614.h"
#include "dvrnet.h"
#include "dvrfilestream.h"


// open a .DVR file stream for playback
dvrfilestream::dvrfilestream(char * filename, int channel)
{
	struct DVRFILEHEADER fileheader ;
	struct DVRFILECHANNEL channelhd ;
	struct DVRFILEHEADEREX  fileheaderex ;
	struct DVRFILECHANNELEX channelhdex ;
	DWORD  dvrheader[10] ;				// dvr clip header

	__int64 fpos ;							// store file position

	//m_countK = 0; // debugging

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
			m_begintime.millisecond  = 0 ;
			m_begintime.tz = 0 ;
			m_curtime = m_begintime;
			m_endtime = m_begintime ;
			dvrtime_add( &m_endtime, fileheaderex.length ) ;

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
					//check
				m_keyindex.setsize( (channelhdex.keyend-channelhdex.keybegin)/sizeof(struct DVRFILEKEYENTRY) );
				if( m_keyindex.size()>0 ) {
					_fseeki64(m_file, channelhdex.keybegin, SEEK_SET );
					fread( m_keyindex.at(0), sizeof(struct DVRFILEKEYENTRY), m_keyindex.size(), m_file );	// read key frame index
				}
			}
		}

		int ret;
		// seek to the beginning of channel data
		_fseeki64( m_file, m_beginpos, SEEK_SET );
		ret = fread( dvrheader, 1, sizeof(dvrheader), m_file);
		if (ret < sizeof(dvrheader)) {
			fclose( m_file );
			m_file=NULL ;
			return;
		}

		BYTE startcode[4];
		m_ps.SetStream(m_file);
		if (m_ps.StreamPeek(startcode, 4) < 4) {
	        _RPT0(_CRT_WARN, "No MPEG PS header\n" );
			fclose( m_file );
			m_file=NULL ;
			return;
		}
		if( startcode[0] != 0 || startcode[1] != 0 ||
			startcode[2] != 1 || startcode[3] < 0xb9 )
		{
			fclose( m_file );
			m_file=NULL ;
			return;
		}

		__int64 timestamp = m_ps.FindFirstTimeStamp();
		if (timestamp == 0LL) {
			fclose( m_file );
			m_file=NULL;
			return;
		}

		m_sync.m_insync=0 ;							// not in sync yet
		m_sync.m_synctime = m_begintime ;
		m_sync.m_synctimestamp = timestamp ;
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
	int i;
	__int64 seektimestamp ;		// seeking time stamp
	__int64 filetimestamp ;
	double ffsize ;				// file size in double precision
	__int64 pos ;
	DWORD  dvrheader[10] ;				// dvr clip header

	m_curtime = *dvrt ;
	m_curtime.millisecond=0 ;		// make sure we don't have insane milisecond
	m_sync.m_insync = 0 ;			// make synctimer invalid

	int diff = dvrtime_diff( &m_begintime, dvrt );
	TRACE(_T("seek(%d)-%d:%d:%d, diff:%d\n"), m_channel, dvrt->hour, dvrt->min, dvrt->second, diff);

	if( m_file==NULL ) {
		return -1;
	}

	_fseeki64( m_file, m_beginpos, SEEK_SET );
	dvrheader[0]=0;
	fread( dvrheader, 1, sizeof( dvrheader ), m_file );
	if( m_encryptmode == ENC_MODE_RC4FRAME ) {
		RC4_crypt((unsigned char *)dvrheader, 40);
		if( dvrheader[0] != FOURCC_HKMI ) {
			return DVR_ERROR_FILEPASSWORD ; 
		}
	}

	if( dvrheader[0] != FOURCC_HKMI ) {
		return 0;
	}

	if( m_keyindex.size()>0 ) {				// key frame index available
		_fseeki64( m_file, m_beginpos+m_keyindex[0].offset, SEEK_SET );
		m_sync.m_insync=0 ;						// not in sync yet
		m_sync.m_synctime = m_begintime ;

		if( diff<=0 ) {
			return 1;
		}
		for( i=m_keyindex.size()-1; i>=0 ; i--) {
			//TRACE(_T("(%d),%d,index time:%d,%d\n"), m_channel, i,m_keyindex[i].time,m_keyindex[i].offset);
			if( (int)(m_keyindex[i].time) <= diff ) {				// found the key frame
				//TRACE(_T("(%d),index time:%d\n"), m_channel, m_keyindex[i].time);
				_fseeki64( m_file, m_beginpos+m_keyindex[i].offset, SEEK_SET );
				return 1 ;
			}
		}
		_fseeki64( m_file, m_beginpos+m_keyindex[0].offset, SEEK_SET );
		return 1;
	}
	
	return 0 ;
}

// get dvr data frame
int dvrfilestream::getdata(char **framedata, int * framesize, int * frametype, int *headerlen)
{
	__int64 framestart, timestamp; 
	int headerLen;

	*frametype = FRAME_TYPE_UNKNOWN ;

	if( m_file==NULL ) {
		return 0 ;
	}

	framestart = _ftelli64( m_file ) ;

	if( framestart< (m_beginpos+40) ) {
		framestart = m_beginpos+40 ;
		_fseeki64( m_file, framestart, SEEK_SET ) ;
	}
	else if( framestart >= m_endpos ) {
		return 0 ;
	}

	char *pBuf;
	if (m_ps.GetOneFrame(&pBuf, framesize, frametype, &timestamp, &headerLen)) {
		if (timestamp > 0) {
			m_curtime = m_begintime ;
			__int64 diff = timestamp-m_sync.m_synctimestamp;
			__int64 diff_in_milsec = diff / 1000;
			dvrtime_addmilisecond(&m_curtime, (int)diff_in_milsec) ;
		}
		if (*framesize > 0 && framedata) {
			*framedata = (char *)malloc(*framesize);
			memcpy(*framedata, pBuf, *framesize);
			//if (*frametype == FRAME_TYPE_KEYVIDEO)
			//	m_countK++;

			if( m_encryptmode == ENC_MODE_RC4FRAME ) {
				int fsize = *framesize - headerLen ;
				if( fsize>1024 ) {
					fsize=1024 ;
				}
				RC4_crypt((unsigned char *)(*framedata) + headerLen, fsize);
			}
			if (headerlen) *headerlen = headerLen;
			return 1;
		}
	}

	return 0 ;			// no data
}

// get current time and timestamp
// return 1: in sync,
//        0: not in sync
int dvrfilestream::gettime( struct dvrtime * dvrt )
{
	*dvrt = m_curtime ;
	//TRACE(_T("gettime(%d)-%d:%d:%d\n"), m_channel, dvrt->hour, dvrt->min, dvrt->second);
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
