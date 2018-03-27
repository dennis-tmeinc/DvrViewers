
// hard drive stream in module
//

#include "stdafx.h"
#include <io.h>
#include <process.h>
#include "player.h"
#include "PLY301.h"
#include "dvrnet.h"
#include "dvrfilestream.h"

// return new position
//         return -1 on error 
static OFF_T find_frame( FILE * fd )
{
	OFF_T pos = file_tell( fd ) & ~((OFF_T)3);		// align to 4 bytes 
	int * buf ;
	int i;

	buf= new int [1024] ;
	
	file_seek(fd, pos, SEEK_SET) ;
	while( fread( buf, sizeof(int), 1024, fd)==1024 ) {
		for(i=0; i<1024; i++) {
			if( buf[i]==1 ) {					// found one frame flag
				pos += i*4 ;
				file_seek( fd, pos, SEEK_SET );
				delete buf ;
				return pos ;
			}
		}
		pos+=1024*4 ;
	}
	delete buf ;
	return -1 ;
}


// open a .DVR file stream for playback
dvrfilestream::dvrfilestream(char * filename, int channel)
{
	struct DVRFILEHEADER fileheader ;
	struct DVRFILECHANNEL channelhd ;
	struct DVRFILEHEADEREX  fileheaderex ;

	OFF_T fpos ;							// store file position

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
		file_seek( m_file, sizeof( fileheader )+m_channel*sizeof(struct DVRFILECHANNEL), SEEK_SET );
		fread( &channelhd, sizeof(struct DVRFILECHANNEL), 1, m_file);

		m_beginpos = channelhd.begin ;
		m_endpos = channelhd.end ;

		// initialize extra information
		m_version[0]=m_version[1]=m_version[2]=m_version[3]=0;
		strcpy(m_servername, "DVR");	// default servername
		sprintf(m_cameraname, "camera%d", m_channel+1 );	// default camera name

		m_encryptmode = ENC_MODE_NONE ;

		// seek to extra file header
		fpos = sizeof( fileheader ) + m_totalchannel * sizeof(struct DVRFILECHANNEL) ;
		file_seek( m_file, fpos, SEEK_SET );
		fread( &fileheaderex, sizeof( fileheaderex ), 1, m_file );
		if( fileheaderex.flagex == DVRFILEFLAGEX ) {			// found file extra header
			struct DVRFILECHANNELEX64 channelhdex ;
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
			m_endtime = m_begintime ;
			dvrtime_add( &m_endtime, fileheaderex.length ) ;

			// go to extra channel info header
			fpos+=fileheaderex.size;										// fpos is channel extra header position
			file_seek( m_file, fpos, SEEK_SET );
			fread( &channelhdex, 1, 8, m_file );							// only need to read in size of the structure
			file_seek( m_file, fpos+m_channel*channelhdex.size, SEEK_SET );		
			fread( &channelhdex, 1, channelhdex.size, m_file );				// read this channel ex header
			strncpy( m_cameraname, channelhdex.cameraname, sizeof(m_cameraname) );

			if( channelhdex.size >= sizeof( DVRFILECHANNELEX64 ) && channelhdex.ex64 == DVRFILECHANNELEX64_FLAG ) {
				m_beginpos = channelhdex.begin64 ;
				m_endpos = channelhdex.end64 ;
				// 64 bits key index
				if( channelhdex.keybegin64>=m_endpos && 
					channelhdex.keyend64>channelhdex.keybegin64 ) {		// 64 bits key frame index available ?
					file_seek(m_file, channelhdex.keybegin64, SEEK_SET );
					m_keyindex.clean();
					struct DVRFILEKEYENTRY64 keyent64 ;
					while( file_tell( m_file )+sizeof(keyent64) <= channelhdex.keyend64 ) {
						fread( &keyent64, 1, sizeof(keyent64), m_file );
						m_keyindex.add( keyent64 );
					}
					channelhdex.keybegin = channelhdex.keyend = 0 ;		// disable 32 bits index, (should not happen)
				}
			}
			else {
				// 32 bits channel header
				m_beginpos = channelhdex.begin ;
				m_endpos = channelhdex.end ;

				channelhdex.clipbegin64 = channelhdex.clipbegin ;
				channelhdex.clipend64 = channelhdex.clipend ;
			}

			if( channelhdex.keybegin >= m_endpos && 
				channelhdex.keyend>channelhdex.keybegin ) {		// 32 bits key index available ?
				file_seek(m_file, channelhdex.keybegin, SEEK_SET );
				m_keyindex.clean();
				struct DVRFILEKEYENTRY keyent ;
				struct DVRFILEKEYENTRY64 keyent64 ;
				while( file_tell( m_file )+sizeof(keyent) <= channelhdex.keyend ) {
					fread( &keyent, 1, sizeof(keyent), m_file );
					keyent64.time = keyent.time ;
					keyent64.offset = keyent.offset ;
					m_keyindex.add( keyent64 );
				}
			}

			// clip info
			if( channelhdex.clipbegin64>=m_endpos && 
				channelhdex.clipend64>channelhdex.clipbegin64 ) {		// clip index available ?
				DVRFILECLIPENTRY clipent ;
				file_seek(m_file, channelhdex.clipbegin64, SEEK_SET );
				while( file_tell( m_file )+sizeof(clipent) <= channelhdex.clipend ) {
					fread( &clipent, 1, sizeof(clipent), m_file );
					m_clipindex.add( clipent );
				}
			}
		}

		seek( &m_begintime );
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
	struct hd_frame frame ;
	DWORD seektimestamp ;		// seeking time stamp
	DWORD filetimestamp ;
	OFF_T ffsize ;			// file size in long long
	OFF_T pos ;
	DWORD  dvrheader[10] ;				// dvr clip header

	m_curtime = *dvrt ;
	m_curtime.millisecond=0 ;		// make sure we don't have insane milisecond
	m_sync.m_insync = 0 ;			// make synctimer invalid

	if( m_file==NULL ) {
		return -1 ;
	}
    
	int diff = dvrtime_diff( &m_begintime, dvrt );
	int flen = dvrtime_diff( &m_begintime, &m_endtime );
    
    if( diff>=flen ) {
        file_seek( m_file, m_endpos, SEEK_SET );
        return 0 ;
    }

    if( diff<0){
        m_curtime = m_begintime ;
    }

    lock();
	file_seek( m_file, m_beginpos, SEEK_SET );
	dvrheader[0]=0;
	fread( dvrheader, 1, sizeof( dvrheader ), m_file );
	if( m_encryptmode == ENC_MODE_RC4FRAME ) {
		RC4_crypt((unsigned char *)dvrheader, 40);
		if( dvrheader[0] != 0x484b4834 ) {
            unlock();
			return DVR_ERROR_FILEPASSWORD ; 
		}
	}

	if( dvrheader[0] != 0x484b4834 ) {
        unlock();
		return 0;
	}

    if( m_keyindex.size()>0 ) {				// key frame index available
        for( i=0; i<m_keyindex.size(); i++) {
            if( m_keyindex[i].time+2 > diff ) 
                break;
        }
        if( i>=m_keyindex.size() ){
            i=m_keyindex.size()-1 ;
        }
        file_seek( m_file, m_beginpos+m_keyindex[i].offset, SEEK_SET );
        fread( &frame, sizeof(frame), 1, m_file );
        if( frame.flag == 1 ) {
            m_sync.m_synctime = m_begintime ;
            dvrtime_add(&m_sync.m_synctime, m_keyindex[i].time);
            m_sync.m_synctimestamp = frame.timestamp ;
            m_curtime = m_sync.m_synctime ;
            file_seek( m_file, m_beginpos+m_keyindex[i].offset, SEEK_SET );         // restore to frame position
            unlock();
            return 1 ;
        }
    }

    // no key index available.
	file_seek( m_file, m_beginpos+40, SEEK_SET );
	pos = find_frame( m_file );					// fpos is first frame position
	fread( &frame, 1, sizeof(frame), m_file );
	if( frame.flag == 1 ) {
		m_sync.m_synctime = m_begintime ;
		m_sync.m_synctimestamp = frame.timestamp ;
	}
	else {
        unlock();
		return 0 ;
	}

	file_seek( m_file, pos, SEEK_SET );

	// no key frame index found
	if( diff<=0 ) {
        unlock();
		return 1 ;
	}

	fread( &frame, sizeof(frame), 1, m_file );
	filetimestamp = frame.timestamp ;
	seektimestamp = diff*64 + frame.timestamp;

	ffsize = m_endpos-m_beginpos-40 ;
	pos = (diff*ffsize/flen + m_beginpos+40 ) ;
	pos &= ~((OFF_T)3) ;
	file_seek( m_file, pos, SEEK_SET );
	if( find_frame( m_file )>0 ) {
		pos = next_keyframe() ;
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
    unlock();
	return 1; 
}

// seek to previous key frame position.
OFF_T dvrfilestream::prev_keyframe()						
{
	// this is just a approximatly
	OFF_T opos, pos ;
	struct hd_frame frame ;

	if( m_file==NULL ) {
		return 0;
	}

    lock();
	opos = file_tell(m_file);
	pos=opos-m_maxframesize ;		// reward one frame size
	if( pos<=(m_beginpos+40) ) {
		pos=m_beginpos+40 ;
		file_seek( m_file, pos, SEEK_SET );
        unlock();
		return pos ;
	}
	file_seek( m_file, pos, SEEK_SET );
	pos = find_frame(m_file);
	if( pos<=(m_beginpos+40) ) {
		pos=(m_beginpos+40) ;
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
	if( pos>=opos ){
		m_maxframesize*=2 ;
        unlock();
		return prev_keyframe();
	}
    unlock();
	return pos ;
}

static OFF_T next_frame(FILE * fd)
{
	struct hd_frame frame ;		// frame header ;
	struct hd_subframe subframe ;	// sub frame header ;
	OFF_T opos ;

	opos = file_tell( fd ) ;

	if( fread( &frame, sizeof(frame), 1, fd )>0 ) {
		if( frame.flag ==1 ) {
			file_seek( fd, frame.framesize, SEEK_CUR );
			while( frame.frames>1 ) {
				fread( &subframe, sizeof(subframe), 1, fd ) ;
				file_seek( fd, subframe.framesize, SEEK_CUR ) ;
				frame.frames-- ;
			}
			return file_tell(fd);
		}
		else {
			file_seek(fd, opos, SEEK_SET);
			return find_frame(fd);
		}
	}
	file_seek(fd, opos, SEEK_SET);
	return -1 ;
}

// seek to next key frame position.
OFF_T dvrfilestream::next_keyframe()						
{
	struct hd_frame frame ;		// frame header ;
	OFF_T opos, pos ;

	if( m_file==NULL ){
		return 0;
	}

    lock();
	opos = file_tell( m_file ) ;

	while( (pos=next_frame(m_file))>0 ) {
		if( fread( &frame, sizeof(frame), 1, m_file) >0 ) {
			if( frame.type==3 ) {						// found a key frame
				file_seek( m_file, pos, SEEK_SET);
				if( (pos-opos)>m_maxframesize ) {
					m_maxframesize = (int)(pos-opos) ;			// update maxframesize 
				}
                unlock();
				return pos ;
			}
		}
		file_seek( m_file, pos, SEEK_SET );
	}
    unlock();
	return 0 ;				// no next key frame
}


// get dvr data frame
int dvrfilestream::getdata(char **framedata, int * framesize, int * frametype)
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

	if( framestart< (m_beginpos+40) ) {
		framestart = m_beginpos+40 ;
		file_seek( m_file, framestart, SEEK_SET ) ;
	}
	else if( framestart >= m_endpos ) {
        unlock();
		return 0 ;
	}

	if( fread( &frame, sizeof(frame), 1, m_file )>0 ) {
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

            dvrtime syntime = m_sync.m_synctime ;
            dvrtime_addmilisecond(&syntime, tstamp2ms((int)frame.timestamp-(int)m_sync.m_synctimestamp)) ;
            int outofsync = dvrtime_diff(&syntime, &m_curtime) ;
            if( outofsync<-2 || outofsync>2 ) {
                // out of sync
                m_sync.m_insync=0 ;	
                m_sync.m_synctime = m_curtime ;
                m_sync.m_synctimestamp = frame.timestamp ;

                // try resync to next key frame ;
                int i;
                for( i=0 ; i<m_keyindex.size(); i++ ) {
                    if( framestart <= m_keyindex[i].offset+ m_beginpos ) {      // match a key index offset
                        struct hd_frame syncframe ;
                        syncframe.flag=0 ;
                        file_seek( m_file, m_keyindex[i].offset+ m_beginpos, SEEK_SET );
                        fread( &syncframe, sizeof(syncframe), 1, m_file ) ;
                        if( syncframe.flag==1 ) {
                            m_sync.m_synctime = m_begintime ;
                            dvrtime_add(&m_sync.m_synctime, m_keyindex[i].time);
                            m_sync.m_synctimestamp = syncframe.timestamp ;
                        }
                        break ;
                    }
                }
            }

            m_curtime = m_sync.m_synctime ;
            dvrtime_addmilisecond(&m_curtime, tstamp2ms((int)frame.timestamp-(int)m_sync.m_synctimestamp)) ;

            file_seek( m_file, framestart+sizeof(frame)+frame.framesize, SEEK_SET );
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
//				* frametype=frame.type ;
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
int dvrfilestream::gettime( struct dvrtime * dvrt )
{
	*dvrt = m_curtime ;
	return  m_sync.m_insync ;
}

// provide and internal structure to sync other stream
void * dvrfilestream::getsynctimer()
{
	if( m_sync.m_insync ) {
		return (void *)&m_sync ;
	}
	else {
		return NULL ;
	}
}

// use synctimer to setup internal sync timer
void dvrfilestream::setsynctimer(void * synctimer)
{
    lock();
	if( synctimer!=NULL ) {
        struct sync_timer * st = (struct sync_timer *)synctimer ;
        int diffms = dvrtime_diffms( &st->m_synctime, &m_sync.m_synctime );
        int syncts = st->m_synctimestamp + ms2tstamp( diffms ) ;
        int diffts = syncts-m_sync.m_synctimestamp ;
        if( diffts<200 && diffts>-200 ) {
            // ok 
            m_sync.m_synctimestamp = syncts ;
        }
    }
	m_sync.m_insync = 1 ;
    unlock();
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
    lock();
	memset( ppi, 0, sizeof( struct player_info ) );
	ppi->size = sizeof(struct player_info) ;
	ppi->type = 0 ;
	ppi->serverversion[0] = m_version[0] ;
	ppi->serverversion[1] = m_version[1] ;
	ppi->serverversion[2] = m_version[2] ;
	ppi->serverversion[3] = m_version[3] ;
	ppi->total_channel = m_totalchannel ;
	strncpy( ppi->servername, m_servername, sizeof(ppi->servername) );
    unlock();
	return 1 ;
}

// get stream channel information
int dvrfilestream::getchannelinfo(struct channel_info * pci)
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
int dvrfilestream::getdayinfo( dvrtime * daytime )			
{
    dvrtime daybegin, dayend ;
    daybegin=dayend=*daytime ;
    daybegin.hour = 0 ;
    daybegin.min = 0 ;
    daybegin.second = 0 ;
    daybegin.millisecond = 0 ;
    dayend.hour=23 ;
    dayend.min=59 ;
    dayend.second = 59 ;
    dayend.millisecond = 999 ;
    if( dvrtime_compare( &dayend, &m_begintime )<0 ||
        dvrtime_compare( &daybegin, &m_endtime )>0 ) 
    {
        return 0 ;
	}
	else {
		return 1;
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
	int maxlen, i ;

	if( dvrtime_compare( &m_endtime, begintime)<=0 ||		
		dvrtime_compare( &m_begintime, endtime )>=0 ) {
			return 0 ;
	}

    if( dvrtime_compare( &m_endtime, endtime )>0 ) {
        maxlen = dvrtime_diff( begintime, endtime );
    }
    else {
        maxlen = dvrtime_diff( begintime, &m_endtime );
    }

    if( m_clipindex.size()>0 ) {
        int clipsize = 0 ;
        struct cliptimeinfo cti, pti ;
        pti.on_time = -100 ;
        pti.off_time = -100 ;		
        for( i=0;i<m_clipindex.size();i++) {
            dvrtime cliptime = m_begintime ;
            dvrtime_add(&cliptime, m_clipindex[i].cliptime );
            cti.on_time = dvrtime_diff( begintime, &cliptime );
            cti.off_time = cti.on_time + m_clipindex[i].length ;
            if( cti.on_time<=pti.off_time ) {
                // merge
                if( cti.off_time>pti.off_time ) {
                    pti.off_time = cti.off_time ;
                }
            }
            else {
                if( pti.on_time<maxlen && pti.off_time>=0) {
                    if(pti.on_time<0) pti.on_time=0 ;
                    if(pti.off_time>maxlen) pti.off_time=maxlen ;
                    // store pti ;
                    if( tinfo!=NULL && tinfosize>clipsize ) {
                        tinfo[clipsize]=pti ;
                    }
                    clipsize++;
                }
                pti=cti ;
            }
        }
        if( pti.on_time<maxlen && pti.off_time>=0) {
            if(pti.on_time<0) pti.on_time=0 ;
            if(pti.off_time>maxlen) pti.off_time=maxlen ;
            // store pti ;
            if( tinfo!=NULL && tinfosize>clipsize ) {
                tinfo[clipsize]=pti ;
            }
            clipsize++;
        }
        return clipsize ;
    }

    if( m_keyindex.size()>0 ) {
        int clipsize = 0 ;
        struct cliptimeinfo cti, pti ;
        pti.on_time = -100 ;
        pti.off_time = -100 ;		
        for( i=0;i<m_keyindex.size();i++) {
            dvrtime keybtime = m_begintime ;
            dvrtime_add(&keybtime, m_keyindex[i].time);
            cti.on_time = dvrtime_diff( begintime, &keybtime );
            cti.off_time = cti.on_time + 5 ;
            // try to merge clip times if overlap
            if( cti.on_time<=pti.off_time ) {
                // merge
                if( cti.off_time>pti.off_time ) {
                    pti.off_time = cti.off_time ;
                }
            }
            else {
                if( pti.on_time<maxlen && pti.off_time>=0) {
                    if(pti.on_time<0) pti.on_time=0 ;
                    if(pti.off_time>maxlen) pti.off_time=maxlen ;
                    // store pti ;
                    if( tinfo!=NULL && tinfosize>clipsize ) {
                        tinfo[clipsize]=pti ;
                    }
                    clipsize++;
                }
                pti=cti ;
            }
        }
        if( pti.on_time<maxlen && pti.off_time>=0) {
            if(pti.on_time<0) pti.on_time=0 ;
            if(pti.off_time>maxlen) pti.off_time=maxlen ;
            // store pti ;
            if( tinfo!=NULL && tinfosize>clipsize ) {
                tinfo[clipsize]=pti ;
            }
            clipsize++;
        }
        return clipsize ;
    }

	if( tinfo==NULL || tinfosize==0 ) {		// if no buffer provided
		return 1;
	}
    lock();
	tinfo->on_time = dvrtime_diff(begintime, &m_begintime) ;
	if( tinfo->on_time<0 ) {
		tinfo->on_time=0;
	}

	tinfo->off_time = dvrtime_diff( begintime, &m_endtime) ;
	if( tinfo->off_time > maxlen ) {
		tinfo->off_time = maxlen ;
	}
    unlock();
	return 1;
}

// get locked file availability information
int dvrfilestream::getlockfileinfo(struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize )	
{
	return 0 ;			// no locked files
}
