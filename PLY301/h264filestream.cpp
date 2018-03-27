
// hard drive stream in module
//

#include "stdafx.h"
#include <io.h>
#include <process.h>
#include "player.h"
#include "PLY301.h"
#include "dvrnet.h"
#include "h264filestream.h"

// open a .DVR file stream for playback
h264filestream::h264filestream(char * filename, int channel)
{
	DWORD  dvrheader[10] ;				// dvr clip header
	DWORD  xfilehead[10] ;				// dvr clip header
	OFF_T  lastframe ;
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

		/*
		if( m_encryptmode == ENC_MODE_UNKNOWN ) {
			fclose( m_file );
			m_file=NULL ;
			return ;
		}
		*/

		fread( &frame, 1, sizeof(frame), m_file);
		if( frame.flag==FRAMESYNC ) {
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

		file_seek(m_file, 0, SEEK_END );
		m_filesize = file_tell(m_file) ;

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

		if( strstr(basefilename, "_L_" ) ) {
			m_lockfile = 1 ;
		}
		else {
			m_lockfile = 0 ;
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
			file_seek(m_file, 0, SEEK_END );
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

		loadindex( filename, m_keyindex );

		// seek to the beginning of data
		if( m_keyindex.size()>0 ) {
			file_seek( m_file, m_keyindex[0].offset, SEEK_SET );
		}
		else {
			file_seek( m_file, 40, SEEK_SET );
		}
	}
}

h264filestream::~h264filestream()
{
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
	struct hd_frame frame ;
	DWORD seektimestamp ;		// seeking time stamp
	OFF_T ffsize ;			// file size in double precision
	OFF_T pos ;

	if( m_file==NULL ) {
		return -1;
	}
	if( m_encryptmode == ENC_MODE_UNKNOWN ) {
		DWORD  xfilehead[10] ;				// dvr clip header
		file_seek( m_file, 0, SEEK_SET );
		fread( xfilehead, 1, sizeof(xfilehead), m_file );
		// try RC4 password
		RC4_crypt( (unsigned char *)xfilehead,  sizeof(xfilehead) );
		if( xfilehead[0] == H264FILEFLAG ) {
			m_encryptmode = ENC_MODE_RC4FRAME ;
		}
		else {
			return DVR_ERROR_FILEPASSWORD ;
		}
	}

	file_seek( m_file, 40, SEEK_SET );

	m_curtime = *dvrt ;
	m_curtime.millisecond=0 ;		// make sure we don't have insane milisecond

	int diff = dvrtime_diff( &m_begintime, dvrt );

	if( diff>=m_filelen ) {
		file_seek( m_file, 0, SEEK_END ) ;
		return 1;
	}

    lock();

	if( m_keyindex.size()>0 ) {		// key index available
		for(int i=m_keyindex.size()-1; i>=0; i-- ) {
			struct keyentry * key = m_keyindex.at(i);
			if( key->frametime/1000 <= diff ) {				
				file_seek( m_file, key->offset, SEEK_SET );
				break ;
			}
		}
	}
	else {
		seektimestamp = diff*64 + m_begintimestamp ;

		ffsize = m_filesize-40 ;
		pos = (int) (diff*ffsize/m_filelen + 40 ) ;
		pos &= 0xfffffffc ;
		file_seek( m_file, pos, SEEK_SET );
		if( find_frame()>0 ) {
			pos = next_keyframe() ;
			if( pos>=m_filesize || pos<=0 ) {
				pos = prev_keyframe();
			}
			fread( &frame, sizeof(frame), 1, m_file ) ;
			file_seek( m_file, pos, SEEK_SET );

			if( frame.timestamp<seektimestamp ) {
				while( frame.timestamp<seektimestamp ) {	// seek to next key frame
					pos = file_tell( m_file );
					OFF_T npos = next_keyframe();
					if( npos>0 ) {
						fread( &frame, sizeof(frame), 1, m_file ) ;
						file_seek( m_file, npos, SEEK_SET );
					}
					else break;
				}
				file_seek(m_file, pos, SEEK_SET );
			}
			else {
				while( frame.timestamp>seektimestamp && 
					   (frame.timestamp-seektimestamp)>100 ) {	// seek to previous key frame
					pos = prev_keyframe();
					if( pos>0 ) {
						fread( &frame, sizeof(frame), 1, m_file ) ;
						file_seek( m_file, pos, SEEK_SET );
					}
					else break;
				}
			}
		}
	}

    unlock();
	return 1; 
}

// return new position
//         return -1 on error 
OFF_T h264filestream::find_frame()
{
	int pos ;
	int * buf ;
	int i;

	if( m_file==NULL ) {
		return -1 ;
	}
    lock();
	pos = file_tell( m_file ) & 0xfffffffc;		// align to 4 bytes 

	buf= new int [1024] ;
	
	file_seek(m_file, pos, SEEK_SET) ;
	while( fread( buf, sizeof(int), 1024, m_file)==1024 ) {
		for(i=0; i<1024; i++) {
			if( buf[i]==FRAMESYNC ) {					// found one frame flag
				pos += i*4 ;
				file_seek( m_file, pos, SEEK_SET );
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
OFF_T h264filestream::prev_keyframe()						
{
	// this is just a approximattly
	OFF_T opos, pos ;
	struct hd_frame frame ;

	if( m_file==NULL ) {
		return 0;
	}

    lock();

	opos = file_tell(m_file);
	pos=opos-m_maxframesize ;		// backward one frame size
	if( pos<=40 ) {
		pos=40 ;
		file_seek( m_file, pos, SEEK_SET );
        unlock();
		return pos ;
	}
	file_seek( m_file, pos, SEEK_SET );
	pos = find_frame();
	if( pos<=(40) ) {
		pos=(40) ;
		file_seek( m_file, pos, SEEK_SET );
        unlock();
		return pos ;
	}
	fread( &frame, sizeof(frame), 1, m_file);
	if( frame.type==3 ) {	// key frame?
		file_seek( m_file, pos, SEEK_SET );
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

OFF_T h264filestream::next_frame()
{
	struct hd_frame frame ;		// frame header ;
	struct hd_subframe subframe ;	// sub frame header ;
	OFF_T opos ;

	if( m_file==NULL ) {
		return 0;
	}

    lock();
	opos = file_tell( m_file ) ;

	if( fread( &frame, 1,  sizeof(frame), m_file )==sizeof(frame) ) {
		if( frame.flag ==1 ) {
			file_seek( m_file, frame.framesize, SEEK_CUR );
			while( frame.frames>1 ) {
				fread( &subframe, sizeof(subframe), 1, m_file ) ;
				file_seek( m_file, subframe.framesize, SEEK_CUR ) ;
				frame.frames-- ;
			}
            unlock();
			return file_tell(m_file);
		}
		else {
			file_seek(m_file, opos, SEEK_SET);
            unlock();
			return find_frame();
		}
	}
	file_seek(m_file, opos, SEEK_SET);
    unlock();
	return -1 ;
}

// seek to next key frame position.
OFF_T h264filestream::next_keyframe()						
{
	struct hd_frame frame ;		// frame header ;
	OFF_T opos, pos ;

	if( m_file==NULL ){
		return 0;
	}
    lock();
	opos = file_tell( m_file ) ;

	while( (pos=next_frame())>0 ) {
		if( fread( &frame, sizeof(frame), 1, m_file) >0 ) {
			if( frame.type==3 ) {						// found a key frame
				file_seek( m_file, pos, SEEK_SET);
				if( (pos-opos)>m_maxframesize ) {
					m_maxframesize = pos-opos ;			// update maxframesize 
				}
                unlock();
				return pos ;
			}
		}
		file_seek( m_file, pos, SEEK_SET );
	}
	// restort old pos
	file_seek( m_file, opos, SEEK_SET );
    unlock();
	return 0 ;				// no next key frame
}


// get dvr data frame
int h264filestream::getdata(char **framedata, int * framesize, int * frametype)
{
	struct hd_frame frame ;		// frame header ;
	struct hd_subframe subframe ;	// sub frame header ;
	OFF_T framestart, frameend ; ;

	* frametype = FRAME_TYPE_UNKNOWN ;

	if( m_file==NULL ) {
		return 0 ;
	}

    lock();
	framestart = file_tell( m_file ) ;

	if( framestart< 40 ) {
		framestart = 40 ;
		file_seek( m_file, framestart, SEEK_SET ) ;
	}

	if( fread( &frame, 1, sizeof(frame), m_file )==sizeof(frame) ) {
		if( frame.flag ==FRAMESYNC ) {
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

			file_seek( m_file, frame.framesize, SEEK_CUR );
			while( frame.frames>1 ) {
				fread( &subframe, sizeof(subframe), 1, m_file ) ;
				file_seek( m_file, subframe.framesize, SEEK_CUR ) ;
				frame.frames-- ;
			}
			frameend = file_tell( m_file );

			* framesize=(int)(frameend-framestart) ;
			if( *framesize>0 && *framesize<1000000 ) {
				* framedata = new char [*framesize] ;
				file_seek( m_file, framestart, SEEK_SET );
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
int h264filestream::getchannelinfo(struct channel_info * pci)
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
int h264filestream::gettimeinfo_help( struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize, int lockfile)
{
	int getsize = 0 ;
	int daybegintime, dayendtime ;		// convert begintime, endtime to seconds of the day
	int filetime ;

	struct dvrtime fileendtime = m_begintime ;
	dvrtime_add(&fileendtime, m_filelen) ;

	if( dvrtime_compare( &fileendtime, begintime)<=0 ||		
		dvrtime_compare( &m_begintime, endtime )>=0 ) {
			return 0 ;
	}

	// convert begintime, endtime to seconds of the day
	daybegintime = begintime->hour * 3600 +
				   begintime->min * 60 +
				   begintime->second ;
	dayendtime =   endtime->hour * 3600 +
				   endtime->min * 60 +
				   endtime->second ;
	filetime = m_begintime.hour * 3600 +
				m_begintime.min * 60 +
				m_begintime.second ;

    lock();
	int s = m_keyindex.size() ;
	if( s>2 ) {
		int clipon = -100000 ;				// clip time of day in milliseconds
		int clipoff = - 100000 ;			// clip off time
		int cliplock = m_lockfile ;
		int ncliplock = m_lockfile ;
		int kon = 0 ;

		int k ;
		for( k=0; k<s; k++ ) {
			if( m_keyindex[k].type == 'L' ||  m_keyindex[k].type == 'N' ) {

				if( m_keyindex[k].type == 'L' ) {
					ncliplock = 1 ;
				}
				else {
					ncliplock = 0 ;
				}

				kon = filetime*1000 + m_keyindex[k].frametime ;
				if( kon - clipoff > 5000 || ncliplock != cliplock ) {
					if( clipon >=0 && clipoff > clipon ) {
						if(lockfile==0 || cliplock ) {
							if( getsize < tinfosize && tinfo!=NULL  ) {
								tinfo[getsize].on_time = clipon/1000 - daybegintime ;
								tinfo[getsize].off_time =  clipoff/1000 - daybegintime ;
							}
							getsize++ ;
						}
					}
					clipon = kon ;
					cliplock = ncliplock ;
				}
				if( k+1 < s ) {
					clipoff = kon + (m_keyindex[k+1].frametime - m_keyindex[k].frametime) ;
				}
				else {
					clipoff = kon + 1000 ;
				}
			}
		}
		// last clip time
		if( clipon >=0 && clipoff > clipon ) {
			if(lockfile==0 || cliplock ) {
				if( getsize < tinfosize && tinfo!=NULL  ) {
					tinfo[getsize].on_time = clipon/1000 - daybegintime ;
					tinfo[getsize].off_time =  clipoff/1000 - daybegintime ;
				}
				getsize++ ;
			}
		}
	}
	else {
		if(lockfile==0 || m_lockfile ) {
			if( getsize < tinfosize && tinfo!=NULL  ) {
				tinfo[getsize].on_time = filetime - daybegintime ;
				tinfo[getsize].off_time = tinfo[getsize].on_time + m_filelen ;
			}
			getsize++ ;
		}
	}
    unlock();
	return getsize;
}

int h264filestream::gettimeinfo( struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize)
{
	return gettimeinfo_help( begintime, endtime, tinfo, tinfosize, 0);
}

int h264filestream::getlockfileinfo(struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize )	
{
	return gettimeinfo_help( begintime, endtime, tinfo, tinfosize, 1);
}