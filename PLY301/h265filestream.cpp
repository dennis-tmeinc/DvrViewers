
// hard drive stream in module
//

#include "stdafx.h"
#include <io.h>
#include <process.h>
#include "player.h"
#include "PLY301.h"
#include "dvrnet.h"
#include "h265filestream.h"

// open a .DVR file stream for playback
h265filestream::h265filestream(char * filename, int channel)
{
	DWORD  dvrheader[10] ;				// dvr clip header
	DWORD  xfilehead[10] ;				// dvr clip header
	int    lastframe ;
	struct hd_frame frame ;				// .264 frame header

	m_channel=channel ;
	m_filesize = 0 ;

	m_file = fopen( filename, "rb" );
	if( m_file ) {
		fread( dvrheader, 1, sizeof(dvrheader), m_file );
		m_encryptmode = ENC_MODE_UNKNOWN ;
		if( dvrheader[0]==H264FILEFLAG ) {			// hik .264 file tag
			m_encryptmode = ENC_MODE_NONE ;			// no encryption
		}
		else {
			// try RC4 password
			memcpy( xfilehead, dvrheader, sizeof(dvrheader) );
			RC4_crypt( (unsigned char *)xfilehead,  sizeof(dvrheader) );
			if( xfilehead[0] == H264FILEFLAG ) {
				m_encryptmode = ENC_MODE_RC4FRAME ;
			}
		}
		if( m_encryptmode == ENC_MODE_UNKNOWN ) {
			fclose( m_file );
			m_file=NULL ;
			return ;
		}

		fread( &frame, 1, sizeof(frame), m_file);
		if( frame.flag==1 ) {
			m_begintimestamp=frame.timestamp ;
		}
		else {
			fclose( m_file );
			m_file=NULL;
			return;
		}

		m_totalchannel = 1 ;

		if( channel>=m_totalchannel ) {
			fclose( m_file );
			m_file=NULL ;
		}

		fseek(m_file, 0, SEEK_END );
		m_filesize = ftell(m_file) ;

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
		if( sscanf( &basefilename[5], "%04d%02d%02d%02d%02d%02d_%d", 
			&m_begintime.year,
			&m_begintime.month,
			&m_begintime.day,
			&m_begintime.hour,
			&m_begintime.min,
			&m_begintime.second,
			&m_filelen ) != 7 ) {		// can't get the file time
			time_t t ;
			dvrtime_localtime( &m_begintime, time(&t) );
			m_begintime.hour=0;
			m_begintime.min=0;
			m_begintime.second=0;
			m_filelen=0;
			// search last frame
			fseek(m_file, 0, SEEK_END );
			lastframe=prev_keyframe();
			fread( &frame, 1, sizeof(frame), m_file);
			if( frame.flag==1 ) {
				m_filelen = (frame.timestamp - m_begintimestamp)/64 ;
				if( m_filelen<0 ) {
					m_filelen=0;
				}
				m_filelen+=10 ;
			}
		}

		m_curtime = m_begintime ;
	
		// initialize extra information
		strcpy(m_servername, "H264FILE");	// default servername
		strncpy(m_cameraname, basefilename, sizeof(m_cameraname) );

		// seek to the beginning of data
		fseek( m_file, 40, SEEK_SET );
	}
}

h265filestream::~h265filestream()
{
	if( m_file ) {
		fclose( m_file );
	}
}

// seek to specified time.
// return 1: success
//        0: error
//        DVR_ERROR_FILEPASSWORD: need password/password error
int h265filestream::seek( struct dvrtime * dvrt ) 		
{
	struct hd_frame frame ;
	DWORD seektimestamp ;		// seeking time stamp
	long long ffsize ;			// file size in double precision
	int pos ;
	int endpos ;

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

    lock();
	seektimestamp = diff*64 + m_begintimestamp ;

	ffsize = m_filesize-40 ;
	pos = (int) (diff*ffsize/m_filelen + 40 ) ;
	pos &= 0xfffffffc ;
	fseek( m_file, pos, SEEK_SET );
	if( find_frame()>0 ) {
		pos = next_keyframe() ;
		if( pos>=m_filesize || pos<=0 ) {
			pos = prev_keyframe();
		}
		fread( &frame, sizeof(frame), 1, m_file ) ;
		fseek( m_file, pos, SEEK_SET );

		if( frame.timestamp<seektimestamp ) {
			while( frame.timestamp<seektimestamp ) {	// seek to next key frame
				pos = ftell( m_file );
				int npos = next_keyframe();
				if( npos>0 ) {
					fread( &frame, sizeof(frame), 1, m_file ) ;
					fseek( m_file, npos, SEEK_SET );
				}
				else break;
			}
			fseek(m_file, pos, SEEK_SET );
		}
		else {
			while( frame.timestamp>seektimestamp && 
				   (frame.timestamp-seektimestamp)>100 ) {	// seek to previous key frame
				pos = prev_keyframe();
				if( pos>0 ) {
					fread( &frame, sizeof(frame), 1, m_file ) ;
					fseek( m_file, pos, SEEK_SET );
				}
				else break;
			}
		}
	}
    unlock();
	return 1; 
}

// return new position
//         return -1 on error 
OFF_T h265filestream::find_frame()
{
	int pos ;
	int * buf ;
	int i;

	if( m_file==NULL ) {
		return -1 ;
	}
    lock();
	pos = ftell( m_file ) & 0xfffffffc;		// align to 4 bytes 

	buf= new int [1024] ;
	
	fseek(m_file, pos, SEEK_SET) ;
	while( fread( buf, sizeof(int), 1024, m_file)==1024 ) {
		for(i=0; i<1024; i++) {
			if( buf[i]==1 ) {					// found one frame flag
				pos += i*4 ;
				fseek( m_file, pos, SEEK_SET );
				delete buf ;
                unlock();
				return pos ;
			}
		}
		pos+=1024*4 ;
	}
	delete buf ;
    unlock();
	return -1 ;
}

// seek to previous key frame position.
OFF_T h265filestream::prev_keyframe()						
{
	// this is just a approximattly
	OFF_T opos, pos ;
	struct hd_frame frame ;

	if( m_file==NULL ) {
		return 0;
	}

    lock();

	opos = ftell(m_file);
	pos=opos-m_maxframesize ;		// backward one frame size
	if( pos<=40 ) {
		pos=40 ;
		fseek( m_file, pos, SEEK_SET );
        unlock();
		return pos ;
	}
	fseek( m_file, pos, SEEK_SET );
	pos = find_frame();
	if( pos<=(40) ) {
		pos=(40) ;
		fseek( m_file, pos, SEEK_SET );
        unlock();
		return pos ;
	}
	fread( &frame, sizeof(frame), 1, m_file);
	if( frame.type==3 ) {	// key frame?
		fseek( m_file, pos, SEEK_SET );
        unlock();
		return pos ;
	}
	pos = next_keyframe();
	if( pos>=opos || pos==0 ){
		m_maxframesize*=2 ;
        unlock();
		return prev_keyframe();
	}
    unlock();
	return pos ;
}

OFF_T h265filestream::next_frame()
{
	struct hd_frame frame ;		// frame header ;
	struct hd_subframe subframe ;	// sub frame header ;
	int opos ;

	if( m_file==NULL ) {
		return 0;
	}

    lock();
	opos = ftell( m_file ) ;

	if( fread( &frame, 1,  sizeof(frame), m_file )==sizeof(frame) ) {
		if( frame.flag ==1 ) {
			fseek( m_file, frame.framesize, SEEK_CUR );
			while( frame.frames>1 ) {
				fread( &subframe, sizeof(subframe), 1, m_file ) ;
				fseek( m_file, subframe.framesize, SEEK_CUR ) ;
				frame.frames-- ;
			}
            unlock();
			return ftell(m_file);
		}
		else {
			fseek(m_file, opos, SEEK_SET);
            unlock();
			return find_frame();
		}
	}
	fseek(m_file, opos, SEEK_SET);
    unlock();
	return -1 ;
}

// seek to next key frame position.
OFF_T h265filestream::next_keyframe()						
{
	struct hd_frame frame ;		// frame header ;
	int opos, pos ;

	if( m_file==NULL ){
		return 0;
	}
    lock();
	opos = ftell( m_file ) ;

	while( (pos=next_frame())>0 ) {
		if( fread( &frame, sizeof(frame), 1, m_file) >0 ) {
			if( frame.type==3 ) {						// found a key frame
				fseek( m_file, pos, SEEK_SET);
				if( (pos-opos)>m_maxframesize ) {
					m_maxframesize = pos-opos ;			// update maxframesize 
				}
                unlock();
				return pos ;
			}
		}
		fseek( m_file, pos, SEEK_SET );
	}
	// restort old pos
	fseek( m_file, opos, SEEK_SET );
    unlock();
	return 0 ;				// no next key frame
}


// get dvr data frame
int h265filestream::getdata(char **framedata, int * framesize, int * frametype)
{
	struct hd_frame frame ;		// frame header ;
	struct hd_subframe subframe ;	// sub frame header ;
	int framestart, frameend ; ;

	* frametype = FRAME_TYPE_UNKNOWN ;

	if( m_file==NULL ) {
		return 0 ;
	}

    lock();
	framestart = ftell( m_file ) ;

	if( framestart< 40 ) {
		framestart = 40 ;
		fseek( m_file, framestart, SEEK_SET ) ;
	}

	if( fread( &frame, 1, sizeof(frame), m_file )==sizeof(frame) ) {
		if( frame.flag ==1 ) {
			if( frame.type == 3 ) {	// key frame
				* frametype =  FRAME_TYPE_KEYVIDEO ;
			}
			else if( frame.type == 1 ) {
				* frametype = FRAME_TYPE_AUDIO ;
			}
			else if( frame.type == 4 ) {
				* frametype = FRAME_TYPE_VIDEO ;
			}

			m_curtime = m_begintime ;
			dvrtime_addmilisecond(&m_curtime, (int)((frame.timestamp-m_begintimestamp)*1000.0/64.0)) ;

			fseek( m_file, frame.framesize, SEEK_CUR );
			while( frame.frames>1 ) {
				fread( &subframe, sizeof(subframe), 1, m_file ) ;
				fseek( m_file, subframe.framesize, SEEK_CUR ) ;
				frame.frames-- ;
			}
			frameend = ftell( m_file );

			* framesize=frameend-framestart ;
			if( *framesize>0 && *framesize<1000000 ) {
				* framedata = new char [*framesize] ;
				fseek( m_file, framestart, SEEK_SET );
				fread( *framedata, 1, *framesize, m_file );
				// decrypt
				if( m_encryptmode == ENC_MODE_RC4FRAME ) {
					struct hd_frame * pframe = (struct hd_frame *) *framedata ;
					int fsize = pframe->framesize ;
					if( fsize>1024 ) {
						fsize=1024 ;
					}
					RC4_crypt((unsigned char *)(*framedata) + sizeof(struct hd_frame), fsize);
				}
                unlock();
				return 1 ;		// success

			}
		}
	}
    unlock();
	return 0 ;			// no data
}

// get current time and timestamp
// return 1: in sync,
//        0: not in sync
int h265filestream::gettime( struct dvrtime * dvrt )
{
	*dvrt = m_curtime ;
	return  1 ;
}

// get stream information
// set ppi->total_channel, ppi->servername 
// return 0: failed
//        1: success ;
int h265filestream::getserverinfo(struct player_info *ppi)
{
	if( ppi->size<sizeof(struct player_info) ){
		return 0 ;
	}
    lock();
	memset( ppi, 0, sizeof( struct player_info ) );
	ppi->size = sizeof(struct player_info) ;
	ppi->type = 0 ;
	ppi->total_channel = m_totalchannel ;
	strncpy( ppi->servername, m_servername, sizeof(ppi->servername) );
    unlock();
	return 1 ;
}

// get stream channel information
int h265filestream::getchannelinfo(struct channel_info * pci)
{
	if( pci->size<sizeof(struct channel_info) ) {
		return 0 ;
	}
    lock();
	memset( pci, 0, sizeof( struct channel_info ));
	pci->size = sizeof( struct channel_info ) ;
	pci->features = 1 ;
	strncpy( pci->cameraname, m_cameraname, sizeof( pci->cameraname ) );
    unlock();
	return 1 ;
}

// get stream day data availability information
int h265filestream::getdayinfo( dvrtime * daytime )			
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
int h265filestream::gettimeinfo( struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize)
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

    lock();
	tinfo->on_time = dvrtime_diff(begintime, &m_begintime) ;
	if( tinfo->on_time<0 ) {
		tinfo->on_time=0;
	}

	maxlen = dvrtime_diff( begintime, endtime );
	tinfo->off_time = dvrtime_diff( begintime, &fileendtime) ;
	if( tinfo->off_time > maxlen ) {
		tinfo->off_time = maxlen ;
	}
    unlock();
	return 1;
}
