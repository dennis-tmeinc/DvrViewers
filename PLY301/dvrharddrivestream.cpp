
// hard drive stream in module
//

#include "stdafx.h"
#include <io.h>
#include <process.h>
#include "player.h"
#include "PLY301.h"
#include "dvrnet.h"
#include "dvrharddrivestream.h"


// find local hard drive devices
// return 0: faile
//        1: success
// will modify 	g_dvr and g_dvr_number
int harddrive_finddevice()
{
	int i;
	int drive ;
	DWORD drives = GetLogicalDrives();
	drives>>=2 ;		// Skip 'A', 'B' drive

	char filename[256] ;
	char * servername;
    char dirpath[16] ;

	for( drive='C'; drive<='Z'; drive++, drives>>=1 ) {
		if( (drives&1)==0 ) continue ;
		sprintf( dirpath, "%c:\\", drive );
        dirfind dir(dirpath, "_*_");
		while( g_dvr_number<MAX_DVR_DEVICE && dir.finddir() ) {
			strcpy( filename, dir.filename() );
			if( filename[0]=='_' ) {
				servername=&filename[1] ;
				i=strlen(servername) ;
				if( servername[i-1]=='_' ) {
					servername[i-1]=0 ;
					// see if server name alread exists 
					for( i=0; i<g_dvr_number; i++ ) {
						if( g_dvr[i].type == DVR_DEVICE_TYPE_LOCALSERVER &&
							strcmp( servername, g_dvr[i].name)==0 ) {
								break;
						}
					}
					if( i>=g_dvr_number ) {
						strncpy( g_dvr[g_dvr_number].name, servername, sizeof(g_dvr[g_dvr_number].name) );
						g_dvr[g_dvr_number].type = DVR_DEVICE_TYPE_LOCALSERVER ;
						g_dvr_number++;
					}
				}
			}
		}
	}
	return 1 ;
}

int harddrive_finddevice1()
{
	int drive ;
	DWORD drives = GetLogicalDrives();
	drives>>=2 ;		// Skip 'A', 'B' drive

    char dirpath[16] ;

	for( drive='C'; drive<='Z'; drive++, drives>>=1 ) {
		if( (drives&1)==0 ) continue ;
		sprintf( dirpath, "%c:\\", drive );
        dirfind dir(dirpath, "_*_");
		while( g_dvr_number<MAX_DVR_DEVICE && dir.finddir() ) {
            strncpy( g_dvr[g_dvr_number].name, dir.filepath(), sizeof(g_dvr[g_dvr_number].name) );
			g_dvr[g_dvr_number].type = DVR_DEVICE_TYPE_HARDDRIVE;
			g_dvr_number++;
		}
	}
	return 1 ;
}

// get totol channel number on dvr root directory
int harddrive_getchannelnum( char * rootpath )
{
	int chnum = 0 ;
	int cycle = 0 ;

// Do we need to detected if the directory contain valid 301 DVR files?
/*
	FILE * dvrlog ;
	char * dvrlogfile ;
	dvrlogfile = new char [strlen(rootpath)+12] ;
	strcpy( dvrlogfile, rootpath );
	strcat( dvrlogfile, "\\dvrlog.txt");
	dvrlog = fopen(dvrlogfile,"r");
	delete dvrlogfile ;
	if( dvrlog==NULL ) {			// could not found log file, so not a 301 DVR
		return 0 ;
	}
	fclose(dvrlog);
*/
    dirfind dir( rootpath,  "20??????" ) ;       // open date directory ex, \\_DVR_\\20080202
	while( dir.finddir() && cycle<10 ) {		// search in 10 date directories
        // found a directory
        dirfind chdir(dir.filepath(), "CH??");
        while( chdir.finddir() ) {
            int num ;
            if( sscanf( chdir.filename(), "CH%02d", &num )>0 ) {
                dirfind vdir( chdir.filepath(), "CH*.264" );
                if( vdir.findfile() ) {
                    if( chnum<num+1 ){
                        chnum=num+1 ;
                    }
                }
            }
        }
        cycle++ ;
	}
	return chnum ;
}

// return
//    >0 success,
//    <=0 failed
int harddrive_getservername( char * searchpath, char * servername )
{
	int chnum = 0 ;
	int cycle = 0 ;
	char * p ;
	char * fn ;
	int res = 0 ;
    dirfind dir( searchpath, "CH*.264" ); 
	if( dir.findfile() ) {
		fn=dir.filename();
		p = fn+strlen(fn)-4;
		*p--=0 ;
		while( p>fn && *p!='_' ) {
			p--;
		}
		if( *p=='_' ) p++ ;
		strcpy( servername, p );
		return 1 ;
	}
	else {
        dir.open(searchpath);
		while( dir.finddir() ) {
			res = harddrive_getservername( dir.filepath(), servername ) ;
			if( res ) 
				break ;
		}
		return res ;
	}
}

// open a hard drive stream for playback
dvrharddrivestream::dvrharddrivestream(char * servername, int channel, int type)
{
	m_channel=channel ;
	if( type==0 ) {					// open specified directory, servername contain root path
        int l ;
		m_disknum = 1 ;
        l=strlen(servername);
		m_rootpath[0] = new char [ l+1 ] ;
		strcpy( m_rootpath[0], servername );
        // make m_servername, used by getlocation()
        m_servername="*" ;
        string ts=servername ;
        char * t = ts ;
        l=strlen(t);
        if( t[l-1] == '\\' ) {
            l--;
            t[l]=0;
        }
        if( t[l-1] == '_' ) {
            l-- ;
            t[l]=0 ;
            while(--l>0) {
                if( t[l]=='\\' ) {
                    break;
                }
                else if( t[l]=='_' ) {
                    m_servername=&t[l+1] ;
                    break;
                }
            }
        }
	}
	else {		// open local server
		m_disknum = 0 ;

		int drive ;
		DWORD drives = GetLogicalDrives();
		drives>>=2 ;		// Skip 'A', 'B' drive

		char dirpath[16] ;
		char searchname[256] ;
		sprintf( searchname, "_%s_", servername );
		for( drive='C'; drive<='Z'; drive++, drives>>=1 ) {
			if( (drives&1)==0 ) continue ;
			sprintf( dirpath, "%c:\\", drive );
            dirfind dir(dirpath, searchname);
			while( dir.finddir() ) {
				char * rpath = dir.filepath() ;
				m_rootpath[m_disknum] = new char [strlen( rpath )+ 3 ] ;
				sprintf(m_rootpath[m_disknum], "%s\\", rpath );
				m_disknum++ ;
			}
		}
        m_servername=servername ;
	}
	m_curtime.year=1980 ;
	m_curtime.month=1 ;
	m_curtime.day= 0;				// set to an invalid day
	m_curtime.hour=0;
	m_curtime.min=0;
	m_curtime.second=0;
	m_curtime.millisecond=0;
	m_curtime.tz=0;
	m_curfile=0 ;
	m_day=0 ;
	m_file=NULL ;
	m_maxframesize=50*1024 ;		// set default max frame size as 50K
	m_sync.m_insync = 0 ;			// timer not in sync
	m_encryptmode=0;

    // GPS file
    m_gpsfile = NULL ;  
}

dvrharddrivestream::~dvrharddrivestream()
{
	int i ;
    lock();
	unloadfile();
	for( i=0; i<m_disknum; i++) {
		delete m_rootpath[i] ;
	}
    unlock();

    // close GPS file
    if( m_gpsfile ) {
        fclose( m_gpsfile );
    }
}

// return new position
//         return -1 on error 
OFF_T dvrharddrivestream::find_frame( FILE * fd )
{
	OFF_T pos = file_tell( fd ) & 0xfffffffc;		// align to 4 bytes 
	int * buf ;
	int i;

    lock();
	buf= new int [1024] ;
	
	file_seek(fd, pos, SEEK_SET) ;
	while( readfile(buf, 1024*sizeof(int), fd)==1024*sizeof(int) ) {
		for(i=0; i<1024; i++) {
			if( buf[i]==1 ) {					// found one frame flag
				pos += i*4 ;
				file_seek( fd, pos, SEEK_SET );
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

// open a new .264 file.
//    return 1: success
//           0: failed
//			 DVR_ERROR_FILEPASSWORD: password error or need password
int dvrharddrivestream::openfile() 
{
	struct hd_frame frame ;
	struct dvrfile * pdf ;
	struct dvrtime filetime ;
	DWORD filehead[10] ;		// 40 bytes header
	DWORD xfilehead[10] ;		// encrypt header
	int rdsize ;
	int i ;

	if( m_curfile>=m_filelist.size() ){
		return 0;
	}

    lock();

	pdf = m_filelist.at(m_curfile);

	m_encryptmode = ENC_MODE_UNKNOWN ;

	m_file=fopen( pdf->filename, "rb" );
	if( m_file ) {
		rdsize=readfile(filehead, sizeof(filehead), m_file);
		if( rdsize< sizeof(filehead) ) {
			fclose( m_file );
			m_file=NULL ;
            unlock();
			return 0 ;
		}

		if( filehead[0]==0x484b4834 ) {				// hik .264 file tag
			m_encryptmode = ENC_MODE_NONE ;			// no encryption
		}
		else {
			// try RC4 password
			memcpy( xfilehead, filehead, sizeof(filehead) );
			RC4_crypt( (unsigned char *)xfilehead,  sizeof(filehead) );
			if( xfilehead[0] == 0x484b4834 ) {
				m_encryptmode = ENC_MODE_RC4FRAME ;
			}
			else if(m_hVideoKey) {					// assume windows mode encryption
				memcpy( xfilehead, filehead, sizeof(filehead) );
				DWORD datalen = sizeof(filehead) ;
				CryptDecrypt(m_hVideoKey, NULL, TRUE, 0, (BYTE *)xfilehead, &datalen );	
				if( xfilehead[0] == 0x484b4834 ) {
					m_encryptmode = ENC_MODE_WIN ;
				}
			}
		}
		if( m_encryptmode == ENC_MODE_UNKNOWN ) {
			fclose( m_file );
			m_file=NULL;
            unlock();
			return DVR_ERROR_FILEPASSWORD ;
		}

		// try use .k file
		struct keyentry k ;
		k.offset = 0 ;
		i=(int)strlen(pdf->filename);
		char * keyfilename = new char [i+4] ;
		strcpy( keyfilename, pdf->filename );
		strcpy( &keyfilename[i-3], "k");
		FILE * keyfile = fopen(keyfilename,"r");
		delete keyfilename ;
		if( keyfile ) {
			if( fscanf(keyfile, "%d,%lld", &(k.frametime), &(k.offset) )==2 ) {
				file_seek( m_file, k.offset, SEEK_SET );
			}
			fclose( keyfile );
		}

		OFF_T pos = file_tell(m_file);
		readfile(&frame, sizeof(frame), m_file );
		file_seek( m_file, pos, SEEK_SET );	// restore to first frame
		if( frame.flag==1 ) {
			filetime.year   = pdf->filedate / 10000 ;
			filetime.month  = pdf->filedate % 10000 /100 ;
			filetime.day    = pdf->filedate % 100 ;
			filetime.hour   = pdf->filetime / 3600 ;
			filetime.min    = pdf->filetime % 3600 / 60 ;
			filetime.second = pdf->filetime % 60 ;
			filetime.millisecond = 0 ;
			filetime.tz=0;

			if( k.offset>0) {
				m_sync.m_insync=1 ;
				m_sync.m_synctime=filetime ;
				dvrtime_addmilisecond(&m_sync.m_synctime, k.frametime);
				m_sync.m_synctimestamp = frame.timestamp ;
			}
			else {
				if( m_sync.m_insync ) {
					m_curtime = m_sync.m_synctime ;
					dvrtime_addmilisecond(&m_curtime, tstamp2ms( (int)frame.timestamp-(int)m_sync.m_synctimestamp) ) ;
					int dt = dvrtime_diff(&m_curtime, &filetime);
					if( dt>=-2 && dt<=2) {
						// conside on consistant time stamp 
						m_sync.m_synctime=m_curtime ;
						m_sync.m_synctimestamp = frame.timestamp ;
					}
					else {
						m_sync.m_insync=0;
					}
				}
				if( m_sync.m_insync==0 ) {
					m_sync.m_synctime=filetime ;
					m_sync.m_synctime.millisecond = (frame.timestamp%64)*125/8 ;
					m_sync.m_synctimestamp = frame.timestamp ;
				}
				m_sync.m_insync=0 ;
			}
			m_curtime = m_sync.m_synctime ;
            unlock();
			return 1 ;
		}
		else {
			fclose(m_file);
			m_file=NULL;
		}
	}
    unlock();
	return 0 ;
}

#define BLOCKSIZE 4096

// read .264 file
int dvrharddrivestream::readfile(void * buf, int size, FILE * fh)
{
	DWORD	datalen ;
	char *	cbuf ;
	int		rdsize ;
	OFF_T	pos ;
	OFF_T	bufleft ;

	if( m_encryptmode!=ENC_MODE_WIN ) {
		return (int)fread(buf, 1, size, fh);					// do normal reading if not encrypted on windows mode
	}

	cbuf = (char *)buf;
	rdsize = 0 ;

    lock();
	pos = file_tell( fh );
	// buffer in reading range
	if( pos>=m_fbufpos && pos<m_fbufpos+m_fbufsize ) {
		bufleft = m_fbufsize - (pos-m_fbufpos) ;
		if( bufleft>=size ) {
			memcpy( cbuf, &m_fbuf[pos-m_fbufpos], size );
			rdsize+=size ;
			file_seek( fh, size, SEEK_CUR );
			size=0 ;
		}
		else {
			memcpy( cbuf, &m_fbuf[pos-m_fbufpos], bufleft );
			cbuf+=bufleft ;
			rdsize+=bufleft ;
			size-=bufleft ;
			if( m_fbufsize < BLOCKSIZE ) {
			}
			else {
				file_seek( fh, m_fbufpos+BLOCKSIZE, SEEK_SET );
			}
		}
	} 

	pos = file_tell( fh );
	while( size>0 ) {
		if( pos<m_fbufpos || pos>=m_fbufpos+m_fbufsize) {					// buffer is invalid, read one page
			m_fbufpos = pos-pos%BLOCKSIZE ;
			file_seek( fh, m_fbufpos, SEEK_SET );
			m_fbufsize=(int)fread( m_fbuf, 1, BLOCKSIZE, fh );
			if( m_fbufsize<=0 ) {
				break;
			}
			datalen = m_fbufsize ;
			if( m_hVideoKey ) {
				CryptDecrypt(m_hVideoKey, NULL, TRUE, 0, (BYTE *)m_fbuf, &datalen );	// decrypt buffer
			}
		}
		// buffer in reading range
		if( pos>=m_fbufpos && pos<m_fbufpos+m_fbufsize ) {
			bufleft = m_fbufsize - (pos-m_fbufpos) ;
			if( bufleft>=size ) {
				memcpy( cbuf, &m_fbuf[pos-m_fbufpos], size );
				rdsize+=size ;
				pos+=size ;
				size=0 ;
			}
			else {
				memcpy( cbuf, &m_fbuf[pos-m_fbufpos], bufleft );
				cbuf+=bufleft ;
				rdsize+=bufleft ;
				size-=bufleft ;
				pos+=bufleft ;
			}
		}
		else {
			break;
		}
		
		if( m_fbufsize<BLOCKSIZE ) {		// end of file
			break;
		}
	}
	file_seek(fh, pos, SEEK_SET);
    unlock();
	return rdsize ;
}

// seek to specified time.
// return 1: success, 0: error, DVR_ERROR_FILEPASSWORD: if password error
int dvrharddrivestream::seek( struct dvrtime * dvrt ) 		
{
	int i;
	struct hd_frame frame ;
	struct dvrfile * pdf ;
	int tim ;					// seek time in seconds
	DWORD seektimestamp ;		// seeking time stamp
	OFF_T fsize ;
	OFF_T pos ;
	int res=0 ;
	int seekday ;

    lock();

	// close current opened file
	if( m_file ) {
		fclose(m_file);
		m_file=NULL ;
	}

	// if seek to different day, reload file list
	seekday = dvrt->year*10000 + dvrt->month*100 + dvrt->day ;
	if( seekday != m_day ) {
		loadfile(seekday);
	}
	// set current time to dvrt
	m_curtime = *dvrt ;
	m_curtime.millisecond=0 ;		// make sure we don't have insane milisecond
	m_sync.m_insync = 0 ;			// make synctimer invalid

	if( m_filelist.size()>0 ) {
		// find the file in correct time

		tim = dvrt->hour * 3600 + dvrt->min * 60 + dvrt->second ;
		for( m_curfile=0; m_curfile < m_filelist.size()-1 ; m_curfile++) {
			pdf = m_filelist.at(m_curfile+1) ;
			if( tim < pdf->filetime ) {
				break;
			}
		}

		if( (res=openfile())>0 ) {
			pdf = m_filelist.at(m_curfile);
			readfile( &frame, sizeof(frame), m_file );

			// seek to right position base on seek time
			if( tim <= pdf->filetime ) {
				file_seek( m_file, 40, SEEK_SET ) ;		// skip fileheader
			}
			else if( tim >= ( pdf->filetime + pdf->filelen) ) {
				file_seek( m_file, 0, SEEK_END );		// seek to end of file
			}
			else {									// seek inside the file
				// try keyframe index
				list <struct keyentry> keyindex ;	// list of key frame of current opened file
				loadindex(pdf->filename, keyindex );
				if( keyindex.size()>0 ) {		// key index available
					for(i=keyindex.size()-1; i>=0; i-- ) {
						struct keyentry * key = keyindex.at(i);
						if( pdf->filetime+key->frametime/1000 <= tim ) {		// frametime in milliseconds
							file_seek( m_file, key->offset, SEEK_SET );
							res = 1 ;
							break ;
						}
					}
				}
				else {

					seektimestamp = (tim - pdf->filetime)*64 + frame.timestamp;

					file_seek( m_file, 0, SEEK_END );		// seek to end of file
					fsize=file_tell( m_file )-40 ;
					pos = (int)((fsize/pdf->filelen)*(tim-pdf->filetime))+40 ;
					pos&=~((OFF_T)3) ;
					file_seek( m_file, pos, SEEK_SET );
					if( find_frame( m_file )>0 ) {
						pos = next_keyframe() ;
						readfile( &frame, sizeof(frame), m_file ) ;
						file_seek( m_file, pos, SEEK_SET );

						if( frame.timestamp<seektimestamp ) {
							while( frame.timestamp<seektimestamp ) {	// seek to next key frame
								pos = file_tell( m_file );
								OFF_T npos = next_keyframe();
								if( npos>0 ) {
									readfile( &frame, sizeof(frame), m_file ) ;
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
									readfile( &frame, sizeof(frame), m_file ) ;
									file_seek( m_file, pos, SEEK_SET );
								}
								else break;
							}
						}
					}
				}
			}
		}
		else {
			if( res==0 ) {
				res=opennextfile();
			}
		}
	}
	else {
		res=opennextfile();
	}
    unlock();
	return res;	
}

// seek to previous key frame position.
OFF_T dvrharddrivestream::prev_keyframe()						
{
	// this is just a approximattly
	OFF_T opos, pos ;
	struct hd_frame frame ;

	if( m_file==NULL ) return 0;
    lock();
	opos = file_tell(m_file);
	pos=opos-m_maxframesize ;		// reward one frame size
	if( pos<=40 ) {
		pos=40 ;
		file_seek( m_file, pos, SEEK_SET );
        unlock();
		return pos ;
	}
	file_seek( m_file, pos, SEEK_SET );
	pos = find_frame(m_file);
	if( pos<=0 ) {
		pos=40 ;
		file_seek( m_file, pos, SEEK_SET );
        unlock();
		return pos ;
	}
	readfile( &frame, sizeof(frame), m_file);
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

OFF_T dvrharddrivestream::next_frame(FILE * fd)
{
	struct hd_frame frame ;		// frame header ;
	struct hd_subframe subframe ;	// sub frame header ;
	int opos ;
    
    lock();
	opos = file_tell( fd ) ;

	if( readfile( &frame, sizeof(frame), fd )>0 ) {
		if( frame.flag ==1 ) {
			file_seek( fd, frame.framesize, SEEK_CUR );
			while( frame.frames>1 ) {
				readfile( &subframe, sizeof(subframe), fd ) ;
				file_seek( fd, subframe.framesize, SEEK_CUR ) ;
				frame.frames-- ;
			}
			unlock();
            return file_tell(fd);
		}
		else {
			file_seek(fd, opos, SEEK_SET);
            unlock();
			return find_frame(fd);
		}
	}
	file_seek(fd, opos, SEEK_SET);
    unlock();
	return -1 ;
}

// seek to next key frame position.
OFF_T dvrharddrivestream::next_keyframe()						
{
	struct hd_frame frame ;		// frame header ;
	OFF_T opos, pos ;

	if( m_file==NULL ) return 0;

    lock();
	opos = file_tell( m_file ) ;

	while( (pos=next_frame(m_file))>0 ) {
		if( readfile( &frame, sizeof(frame), m_file) >0 ) {
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
    unlock();
	return 0 ;				// no next key frame
}

// get next day with video data available
// return 
//        day in BCD0
//        0 : failed
int dvrharddrivestream::nextday(int day)
{
	int i;
	int nday ;
    lock();
	for( i=0; i<m_dayinfo.size(); i++ ) {
		nday = m_dayinfo[i] ;
		if( day < nday ){
            unlock();
			return nday ;
		}
	}
    unlock();
	return 0 ;
}

// open next video file
// return 1: success
//        0: failed
//        DVR_ERROR_FILEPASSWORD: if password error
int dvrharddrivestream::opennextfile()
{
	int res = 0 ;
	int nday ;
    lock();
	if( m_file ) {
		fclose( m_file ) ;
		m_file=NULL ;
	}
	m_curfile++;
	while( m_curfile>=m_filelist.size() ) {	// load the next day file
		nday = nextday(m_day) ;
		if( nday<=0 ) {
            unlock();
			return 0 ;						// failed
		}
		loadfile(nday);						// load files of new day
		m_curfile=0 ;						// first file in list
	}
	res = openfile();
	if( res==0 ) {
        unlock();
		return opennextfile();
	}
    unlock();
	return res ;
}

// get dvr data frame
int dvrharddrivestream::getdata(char **framedata, int * framesize, int * frametype)
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

	if( readfile( &frame, sizeof(frame), m_file )>0 ) {
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

			m_curtime = m_sync.m_synctime ;
			dvrtime_addmilisecond(&m_curtime, tstamp2ms( (int)frame.timestamp-(int)m_sync.m_synctimestamp) ) ;

			file_seek( m_file, frame.framesize, SEEK_CUR );
			while( frame.frames>1 ) {
				readfile( &subframe, sizeof(subframe), m_file ) ;
				file_seek( m_file, subframe.framesize, SEEK_CUR ) ;
				frame.frames-- ;
			}
			frameend = file_tell( m_file );

			* framesize=frameend-framestart ;
			if( *framesize>0 && *framesize<1000000 ) {
				* framedata = new char [*framesize] ;
				file_seek( m_file, framestart, SEEK_SET );
				readfile( *framedata, *framesize, m_file );
				if( m_encryptmode==ENC_MODE_RC4FRAME ) {		// decrypt RC4 frame
					struct hd_frame * pframe ;		// frame header pointer ;
					int    fsize ;
					pframe = (struct hd_frame *) *framedata ;
					fsize = pframe->framesize ;
					if( fsize>1024 ) {
						fsize=1024 ;				// maximum encrypt 1024 bytes
					}
					RC4_crypt( (unsigned char *)(*framedata) + sizeof(struct hd_frame), fsize );
				}
                unlock();
				return 1 ;		// success
			}
		}
		else {					// not getting right frame. 
			// scan for next right frame
			find_frame( m_file );
		}
	}
	else {
		opennextfile();
	}
    unlock();
	return getdata(framedata, framesize, frametype);
}

// get current time and timestamp
// return 1: in sync,
//        0: not in sync
int dvrharddrivestream::gettime( struct dvrtime * dvrt )
{
	*dvrt = m_curtime ;
	return  m_sync.m_insync ;
}

// provide and internal structure to sync other stream
void * dvrharddrivestream::getsynctimer()
{
	if( m_sync.m_insync ) {
		return (void *)&m_sync ;
	}
	else {
		return NULL ;
	}
}

// use synctimer to setup internal sync timer
void dvrharddrivestream::setsynctimer(void * synctimer)
{
	if( m_file==NULL ) {
		return ;
	}
    lock();
	if( synctimer ) {
        // to check if time stamp close enough?
		struct sync_timer st = *(struct sync_timer *)synctimer ;
		int dms ;
		if( st.m_synctimestamp < m_sync.m_synctimestamp ) {
			dms = (int)(m_sync.m_synctimestamp-st.m_synctimestamp) ;
			if( dms > 12*3600*64 ) {
				m_sync.m_insync = 1 ;		// self - sync
                unlock();
				return ;
			}
			dms = dms*125/8 ;
			dvrtime_addmilisecond( &st.m_synctime,dms ) ;
		}
		else {
			dms = (int)(st.m_synctimestamp-m_sync.m_synctimestamp) ;
			if( dms > 12*3600*64 ) {
				m_sync.m_insync = 1 ;		// self - sync
                unlock();
				return ;
			}
			dms = dms*125/8 ;
			dvrtime_addmilisecond( &st.m_synctime, -dms ) ;
		}
		dms = dvrtime_diff( &st.m_synctime, &m_sync.m_synctime );
		if( dms<3 && dms>-3 ) {
			// possible in same pace
			m_sync.m_synctime = st.m_synctime ;
        }
    }
    unlock();
    m_sync.m_insync = 1 ;
}

// get stream information
// set ppi->total_channel, ppi->servername 
// return 0: failed
//        1: success ;
int dvrharddrivestream::getserverinfo(struct player_info *ppi)
{
	char servername[MAX_PATH] ;
	int len ;
	if( ppi->size<sizeof(struct player_info) )
		return 0 ;
    lock();
	if( m_disknum>0 ) {
		ppi->total_channel = harddrive_getchannelnum( m_rootpath[0] );
	}
	else {
		ppi->total_channel = 0 ;
	}
	if( ppi->total_channel>0 ) {
		if( harddrive_getservername(m_rootpath[0], servername)>0 ) {
			strcpy( ppi->servername, servername );
            unlock();
			return 1 ;
		}
		strcpy( servername, m_rootpath[0] );
		len = (int)strlen(servername);
		if( servername[len-1] == '\\' ) {
			servername[len-1]='\0' ;
			len-- ;
		}
		if( servername[len-1] == '_' ) {
			len-- ;
			servername[len]='\0' ;
			while( len>3 ) {
				len-- ;
				if( servername[len]=='_' && servername[len-1]=='\\' ) {
					strcpy( ppi->servername, &servername[len+1] );
                    unlock();
					return 1 ;
				}
			}
		}
	}
    unlock();
	return 0 ;					// failed
}

// get stream channel information
int dvrharddrivestream::getchannelinfo(struct channel_info * pci)
{
	return 0;					// not used
}

// get stream day data availability information
int dvrharddrivestream::getdayinfo( dvrtime * daytime )			
{
	int i;
	int year, month, day, iday ;
    lock();
	if( m_dayinfo.size()==0 ) {
		for( i=0; i<m_disknum; i++ ) {
            dirfind dir(m_rootpath[i], "20??????") ;
			while( dir.finddir() ) {
				char * datename = dir.filename();
				if( datename[2]>='0' && datename[2]<='4' &&
					datename[3]>='0' && datename[3]<='9' &&
					datename[4]>='0' && datename[4]<='1' &&
					datename[5]>='0' && datename[5]<='9' &&
					datename[6]>='0' && datename[6]<='3' &&
					datename[7]>='0' && datename[7]<='9' &&
					datename[8]=='\0' ) 
				{
					sscanf(datename, "%04d%02d%02d", &year, &month, &day ) ;
					day = year * 10000 + month * 100 + day ;					// make it BCD
					for( iday=0; iday<m_dayinfo.size(); iday++ ) {
						if( m_dayinfo[iday] == day ) break;
					}
					m_dayinfo[iday] = day ;
				}
			}
		}
		m_dayinfo.sort();
		m_dayinfo.compact();
	}

	if( m_dayinfo.size()>0 ) {					// day info available
		day=daytime->year*10000+daytime->month*100+daytime->day ;
		for( i=0; i<m_dayinfo.size(); i++) {
			if( day==*(int *)m_dayinfo.at(i) ) {		// found it
                unlock();
				return 1 ;
			}
		}
	}
	unlock();
	return 0 ;								// not data
}

int dvrharddrivestream::gettimeinfo_help( struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize, int lockfile)
{
	int i;
	int getsize = 0 ;

	int daybegintime, dayendtime ;		// convert begintime, endtime to seconds of the day
	list <struct dvrfile> dflist ;
	list <struct keyentry> keyindex ;	// list of key frame of current opened file

	// convert begintime, endtime to seconds of the day
	daybegintime = begintime->hour * 3600 +
				   begintime->min * 60 +
				   begintime->second ;
	dayendtime =   endtime->hour * 3600 +
				   endtime->min * 60 +
				   endtime->second ;

	if( dayendtime<=daybegintime ) {				// parameter error!
		return 0 ;
	}
	int clipon = -100000 ;				// clip time of day in milliseconds
	int clipoff = - 100000 ;			// clip off time
	int cliplock = 0 ;
	int ncliplock = 0 ;
	int kon = 0 ;
    lock();
	if(	loadfilelist(begintime, &dflist) ) {
        for( i=0;i<dflist.size();i++) {
			struct dvrfile * pdf = dflist.at(i);
			if( pdf->filetime > dayendtime ) 
				break ;
			if( strstr( pdf->filename, "_L_" ) ) {
				ncliplock = 1 ;
			}
			else {
				ncliplock = 0 ;
			}
			if( loadindex(pdf->filename, keyindex )>1 ) {
				int k ;
				int s = keyindex.size() ;
				for( k=0; k<s; k++ ) {
					if( keyindex[k].type == 'L' ||  keyindex[k].type == 'N' ) {

						if( keyindex[k].type == 'L' ) {
							ncliplock = 1 ;
						}
						else {
							ncliplock = 0 ;
						}

						kon = pdf->filetime*1000 + keyindex[k].frametime ;
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
							clipoff = kon + (keyindex[k+1].frametime - keyindex[k].frametime) ;
						}
						else {
							clipoff = kon + 1000 ;
						}
					}
				}
			}
			else {
				kon = pdf->filetime*1000 ;
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
				clipoff = kon + pdf->filelen * 1000 ;
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
    unlock();
	return getsize ;
}


int dvrharddrivestream::gettimeinfo( struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize)
{
	return gettimeinfo_help( begintime, endtime, tinfo, tinfosize, 0);
}

int dvrharddrivestream::getlockfileinfo(struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize )	
{
	return gettimeinfo_help( begintime, endtime, tinfo, tinfosize, 1);
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
/*
int dvrharddrivestream::gettimeinfo( struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize)
{
	int i;
	int getsize = 0 ;
	struct cliptimeinfo cti, pti ;
	int daybegintime, dayendtime ;		// convert begintime, endtime to seconds of the day
	list <struct dvrfile> dflist ;

	// convert begintime, endtime to seconds of the day
	daybegintime = begintime->hour * 3600 +
				   begintime->min * 60 +
				   begintime->second ;
	dayendtime =   endtime->hour * 3600 +
				   endtime->min * 60 +
				   endtime->second ;

	if( dayendtime<=daybegintime ) {				// parameter error!
		return 0 ;
	}
    lock();
	if(	loadfilelist(begintime, &dflist) ) {
        pti.on_time = -100 ;
        pti.off_time = -100 ;		
        for( i=0;i<dflist.size();i++) {
			struct dvrfile * pdf = dflist.at(i);
//			if( pdf->filetype == 0 ) {		// normal file
				cti.on_time = pdf->filetime ;
				cti.off_time = pdf->filetime+pdf->filelen ;
				// check if time overlapped with begintime - endtime
				if( cti.on_time >= dayendtime ||
					cti.off_time <= daybegintime ) {
					continue ;				// clip time not included
				}

                // try to merge clip times if overlap
                if( cti.on_time<=pti.off_time ) {
                    // merge
                    if( cti.off_time>pti.off_time ) {
                        pti.off_time = cti.off_time ;
                    }
                }
                else {
                    if( pti.on_time>=0 ) {
                        if( tinfosize>0 && tinfo!=NULL ) {
                            if( getsize>=tinfosize ) {
                                break;
                            }
                            // set clip on time
                            if( pti.on_time > daybegintime ) {
                                tinfo[getsize].on_time = pti.on_time - daybegintime ;
                            }
                            else {
                                tinfo[getsize].on_time = 0 ;
                            }
                            // set clip off time
                            tinfo[getsize].off_time = pti.off_time - daybegintime ;
                        }
                        getsize++ ;
                    }
                    pti = cti ;
                }
//			}
		}
        // last clip time
        if( pti.on_time>=0 ) {
            if( tinfosize>0 && tinfo!=NULL ) {
                if( getsize<tinfosize ) {
                    // set clip on time
                    if( pti.on_time > daybegintime ) {
                        tinfo[getsize].on_time = pti.on_time - daybegintime ;
                    }
                    else {
                        tinfo[getsize].on_time = 0 ;
                    }
                    // set clip off time
                    tinfo[getsize].off_time = pti.off_time - daybegintime ;
                }
            }
            getsize++ ;
        }
	}
    unlock();
	return getsize ;
}

// get locked file availability information
int dvrharddrivestream::getlockfileinfo(struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize )	
{	
	int i;
	int getsize = 0 ;
	struct cliptimeinfo cti, pti ;
	int daybegintime, dayendtime ;		// convert begintime, endtime to seconds of the day
	list <struct dvrfile> dflist ;

	// convert begintime, endtime to seconds of the day
	daybegintime = begintime->hour * 3600 +
				   begintime->min * 60 +
				   begintime->second ;
	dayendtime =   endtime->hour * 3600 +
				   endtime->min * 60 +
				   endtime->second ;

	if( dayendtime<=daybegintime ) {				// parameter error!
		return 0 ;
	}
    lock();
	if(	loadfilelist(begintime, &dflist) ) {
        pti.on_time = -100 ;
        pti.off_time = -100 ;		
		for( i=0;i<dflist.size();i++) {
			struct dvrfile * pdf = dflist.at(i);
			if( pdf->filetype == 1 ) {		// locked file
				cti.on_time = pdf->filetime ;
				cti.off_time = pdf->filetime+pdf->filelen ;
				cti.on_time = cti.off_time-pdf->locklen ;
				// check if time overlapped with begintime - endtime
				if( cti.on_time >= dayendtime ||
					cti.off_time <= daybegintime ) {
					continue ;				// clip time not included
				}

                // try to merge clip times if overlap
                if( cti.on_time<=pti.off_time ) {
                    // merge
                    if( cti.off_time>pti.off_time ) {
                        pti.off_time = cti.off_time ;
                    }
                }
                else {
                    if( pti.on_time>=0 ) {
                        if( tinfosize>0 && tinfo!=NULL ) {
                            if( getsize>=tinfosize ) {
                                break;
                            }
                            // set clip on time
                            if( pti.on_time > daybegintime ) {
                                tinfo[getsize].on_time = pti.on_time - daybegintime ;
                            }
                            else {
                                tinfo[getsize].on_time = 0 ;
                            }
                            // set clip off time
                            tinfo[getsize].off_time = pti.off_time - daybegintime ;
                        }
                        getsize++ ;
                    }
                    pti = cti ;
                }
			}
		}
        // last clip time
        if( pti.on_time>=0 ) {
            if( tinfosize>0 && tinfo!=NULL ) {
                if( getsize<tinfosize ) {
                    // set clip on time
                    if( pti.on_time > daybegintime ) {
                        tinfo[getsize].on_time = pti.on_time - daybegintime ;
                    }
                    else {
                        tinfo[getsize].on_time = 0 ;
                    }
                    // set clip off time
                    tinfo[getsize].off_time = pti.off_time - daybegintime ;
                }
            }
            getsize++ ;
        }
	}
    unlock();
	return getsize ;
}
*/

int dvrharddrivestream::unloadfile()
{
    lock();
	if( m_file ) {
		fclose(m_file);
		m_file=NULL;
	}
	m_curfile=0;
	m_filelist.clean();
	m_day=-1 ;
    unlock();
	return 0;
}

// load dvr files into name list, return 1 for success, 0 for failed.
int dvrharddrivestream::loadfilelist(struct dvrtime * day, list <struct dvrfile> * dflist )
{
	int i ;
	char dirpath[MAX_PATH] ;
	char dirpattern[30] ;
	int res=0;
	char * lockstr ;
    lock();
	if( getdayinfo( day ) ) {	// if data available ?
		for( i=0; i<m_disknum; i++) {
			sprintf(dirpath, "%s%04d%02d%02d\\CH%02d\\", 
				m_rootpath[i], 
				day->year, day->month, day->day,
				m_channel );
			sprintf(dirpattern, "CH%02d_%04d%02d%02d*.264", 
				m_channel,
				day->year, day->month, day->day );
            dirfind dir( dirpath, dirpattern) ;
			while( dir.findfile() ) {
				// found a file
				dvrfile df ;
				df.filedate = day->year*10000+day->month*100+day->day ;
				char * filename = dir.filename();
				sscanf( &filename[13], "%d", &(df.filetime) );	// in BCD 
				// adjust file time to seconds of the day
				df.filetime = (df.filetime/10000) * 3600 +		// hours
							  (df.filetime%10000/100) * 60 +	// minutes
							  (df.filetime%100)	;				// seconds
				sscanf( &filename[20], "%d", &(df.filelen) );
				strncpy( df.filename, dir.filepath(), sizeof( df.filename) );
				if( (lockstr=strstr(filename, "_L_"))!=NULL ) {
					int locklength;
					char underscore ;
					df.filetype=1 ;					// set locked file type
     				df.locklen=df.filelen ;
					if( sscanf( &lockstr[3], "%d%c", &locklength, &underscore )==2 ) {
						if( underscore == '_' ) {
							df.locklen = locklength ;
						}
					}
				}
				else {
					df.filetype=0 ;					// set normal file type
				}
				if( df.filelen>0 ) {
					res=1;
					dflist->add( df );
				}
			}
		}
		dflist->sort();
	}
    unlock();
	return res;
}

// loadday as BCD format day
int dvrharddrivestream::loadfile(int loadday)
{
	struct dvrtime loadtime ;
	dvrtime_init(&loadtime, loadday/10000, loadday%10000/100, loadday%100);
    lock();
	unloadfile() ;		// unload files list first
	if( loadfilelist(&loadtime, &m_filelist) ) {
		m_filelist.compact() ;
	}
	m_day = loadday ;
    unlock();
	return 0;
}

// get GPS location
int dvrharddrivestream::getlocation( struct dvrtime * dvrt, char * locationbuffer, int buffersize )
{
    if( m_gpsfile!=NULL ) {
        // check if date changed
        if( (m_gpstime.year!=dvrt->year) ||
            (m_gpstime.month!=dvrt->month) ||
            (m_gpstime.day!=dvrt->day) ) 
        {
            fclose( m_gpsfile );
            m_gpsfile=NULL ;
        }
    }

    if(m_gpsfile == NULL && m_disknum>0) {       // open GPS file
        string smartlogfilename ;
        string filepattern ;
        sprintf( smartlogfilename.strsize(512), "%s..\\smartlog", m_rootpath[0] );    // smartlog directory
        sprintf( filepattern.strsize(512), "%s_%04d%02d%02d_?.001", (char *)m_servername, dvrt->year, dvrt->month, dvrt->day );

        dirfind dir( smartlogfilename, filepattern) ;
    	while( dir.findfile() ) {		
            m_gpsfile = fopen(dir.filepath(), "rb");
            if( m_gpsfile ) {
                m_gpstime=*dvrt ;
                m_gpstime.hour = m_gpstime.min = m_gpstime.second = 0 ;
                break;
            }
        }
    }

    if( m_gpsfile ) {
         int timeofday = dvrt->hour * 3600 + dvrt->min * 60 + dvrt->second ;
         int gpstod = m_gpstime.hour * 3600 + m_gpstime.min * 60 + m_gpstime.second ;
         if( timeofday < gpstod ) {
             // rewind
             file_seek( m_gpsfile, 0, SEEK_SET );
             m_gpstime.hour = m_gpstime.min = m_gpstime.second = 0 ;
             gpstod = 0 ;
         }
         OFF_T off, f1, f2 ;
         off = file_tell(m_gpsfile);
         f1=off ;

         while( fgets(locationbuffer, buffersize, m_gpsfile) ) {
             f2 = file_tell(m_gpsfile);
             int n, code, h,m,s ;
             n=sscanf(locationbuffer, "%d,%d:%d:%d", &code, &h, &m, &s );
             if( n==4 ) {
                 gpstod = h*3600 + m*60 + s ;
                 if( gpstod > timeofday ) {
                     // use previous line 
                     break ;
                 }
                 else {
                     off=f1 ;
                     m_gpstime.hour = h ;
                     m_gpstime.min = m ;
                     m_gpstime.second = s ;
                 }
             }
             f1 = f2 ;
         }

         if( off>14 ) {
            file_seek( m_gpsfile, off, SEEK_SET );
            fgets(locationbuffer, buffersize, m_gpsfile);
            file_seek( m_gpsfile, off, SEEK_SET );
            return strlen( locationbuffer ) ;
         }
    }

    return DVR_ERROR ;
}

// get event time list
int dvrharddrivestream::geteventtimelist( struct dvrtime * date, int eventnum, int * elist, int elistsize)
{
    struct dvrtime * dvrt ;
    if( date==NULL ) {
        dvrt = &m_curtime ;
    }
    else {
        dvrt = date ;
    }

    if( m_gpsfile!=NULL ) {
        // check if date changed
        if( (m_gpstime.year!=dvrt->year) ||
            (m_gpstime.month!=dvrt->month) ||
            (m_gpstime.day!=dvrt->day) ) 
        {
            fclose( m_gpsfile );
            m_gpsfile=NULL ;
        }
    }

    if(m_gpsfile == NULL && m_disknum>0) {       // open GPS file
        dirfind dir( TMPSTR("%s..\\smartlog", m_rootpath[0]), TMPSTR("%s_%04d%02d%02d_?.001", (char *)m_servername, dvrt->year, dvrt->month, dvrt->day)) ;
    	while( dir.findfile() ) {		
            m_gpsfile = fopen(dir.filepath(), "rb");
            if( m_gpsfile ) {
                m_gpstime=*dvrt ;
                m_gpstime.hour = m_gpstime.min = m_gpstime.second = 0 ;
                break;
            }
        }
    }

    if( m_gpsfile ) {
        int numevent = 0 ;
        string line ;
        line.strsize(256);
        rewind(m_gpsfile);
        while( fgets( line, 255, m_gpsfile) ) {
             int n, code, h,m,s ;
             n=sscanf(line, "%d,%d:%d:%d", &code, &h, &m, &s );
             if( n==4 && code==eventnum ) {
                 if( elist!=NULL && elistsize>0 ) {
                     if( numevent<elistsize ) {
                         elist[numevent] = h*3600 + m*60 + s ;
                         numevent++ ;
                     }
                 }
                 else {
                     numevent++;
                 }
             }
        }
        fclose( m_gpsfile ) ;
        m_gpsfile=NULL ;
        return numevent ;
    }
    return DVR_ERROR ;
}
