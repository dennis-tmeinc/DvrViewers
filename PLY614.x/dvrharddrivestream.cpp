
// hard drive stream in module
//

#include "stdafx.h"
#include <io.h>
#include <process.h>
#include <crtdbg.h>
#include "player.h"
#include "ply614.h"
#include "dvrnet.h"
#include "dvrharddrivestream.h"
#include "utils.h"
#include <algorithm>

struct dir_entry {
	intptr_t findhandle ;
	struct _finddata_t fileinfo ;
	char dirpath[MAX_PATH] ;
	char filepath[MAX_PATH] ;
};

typedef dir_entry * dir_handle ;

bool is_duplicate(list <struct dvrfile> *dflist, struct dvrfile *df)
{
	int i;

	for( i=0;i<dflist->size();i++) {	
		struct dvrfile * pdf = dflist->at(i);
		if ((pdf->filedate == df->filedate) &&
			(pdf->filetime == df->filetime) &&
			(pdf->filelen == df->filelen)) {
			return true;
		}
	}

	return false;
}

// create a file find handle
// return NULL for failed
dir_handle dir_open(char * dirpath, char * filenamepattern)
{
	int len ;
	dir_handle handle = NULL ;
	handle = new struct dir_entry ;
	handle->findhandle=-1 ;
	if( dirpath==NULL || strlen(dirpath)==0 ) {
		handle->dirpath[0]='\0' ;
	}
	else {
		strncpy( handle->dirpath, dirpath, sizeof(handle->dirpath) );
	}
	len = (int)strlen( handle->dirpath ) ;
	if( len>0 && handle->dirpath[len-1]!='\\' ) {
		handle->dirpath[len]='\\' ;
		handle->dirpath[len+1]='\0' ;
	}
	if( filenamepattern==NULL || strlen( filenamepattern )==0 ) {
		strcpy(handle->fileinfo.name, "*.*");
	}
	else {
		strncpy(handle->fileinfo.name, filenamepattern, sizeof( handle->fileinfo.name ));
	}
	return handle ;
}

// find next file
// return 1: success
//        0: failed
int dir_find( dir_handle handle )
{
	if( handle->findhandle==-1 ) {												// find first time
		strncpy( handle->filepath, handle->dirpath, sizeof(handle->filepath) );
		strcat( handle->filepath, handle->fileinfo.name );
		handle->findhandle = _findfirst( handle->filepath, &(handle->fileinfo) ) ;
		if(handle->findhandle!=-1 )
			return 1 ;			// success
	}
	else if(_findnext( handle->findhandle, &(handle->fileinfo) )== 0 ) {		// success
		return 1 ;				// success
	}
	return 0 ;		//failed
}

// return file path
char * dir_filepath( dir_handle handle )
{
	strncpy( handle->filepath, handle->dirpath, sizeof(handle->filepath) );
	strcat( handle->filepath, handle->fileinfo.name );
	return handle->filepath ;
}

// return file name
char * dir_filename( dir_handle handle )
{
	return handle->fileinfo.name ;
}

// is a sub directory ?
int dir_isdir( dir_handle handle )
{
	if( handle->fileinfo.name[0]=='.' ) 
		return 0 ;												// skip directory . , ..
	return (( handle->fileinfo.attrib & _A_SUBDIR )!=0) ;
}

// is a file ?
int dir_isfile( dir_handle handle )
{
	return (( handle->fileinfo.attrib & _A_SUBDIR )==0) ;
}

// file find close
int dir_close( dir_handle handle )
{
	if( handle->findhandle!=-1 ) {										
		_findclose( handle->findhandle );
	}
	delete handle ;
	return 0 ;
}

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
	dir_handle findhandle ;

	for( drive='C'; drive<='Z'; drive++, drives>>=1 ) {
		if( (drives&1)==0 ) continue ;
		sprintf( dirpath, "%c:\\", drive );
		findhandle = dir_open( dirpath, "_*_" );
		while( dir_find( findhandle ) ) {
			if( dir_isdir( findhandle ) ) {
				strcpy( filename, dir_filename(findhandle) );
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
						if( g_dvr_number < MAX_DVR_DEVICE && i>=g_dvr_number ) {
							if (g_dvr[g_dvr_number].name == NULL)
								g_dvr[g_dvr_number].name = new char[DVRNAME_LEN];
							strncpy( g_dvr[g_dvr_number].name, servername, DVRNAME_LEN );
							g_dvr[g_dvr_number].type = DVR_DEVICE_TYPE_LOCALSERVER ;
							g_dvr_number++;
						}
					}
				}
			}
		}
		dir_close( findhandle );
	}
	return 1 ;
}

int harddrive_finddevice1()
{
	int drive ;
	DWORD drives = GetLogicalDrives();
	drives>>=2 ;		// Skip 'A', 'B' drive

    char dirpath[16] ;
	dir_handle findhandle ;

	for( drive='C'; drive<='Z'; drive++, drives>>=1 ) {
		if( (drives&1)==0 ) continue ;
		sprintf( dirpath, "%c:\\", drive );
		findhandle = dir_open( dirpath, "_*_" );
		while( dir_find( findhandle ) ) {
			if( dir_isdir( findhandle ) ) {
				if (g_dvr_number < MAX_DVR_DEVICE) {
					if (g_dvr[g_dvr_number].name == NULL)
						g_dvr[g_dvr_number].name = new char[DVRNAME_LEN];
					strncpy( g_dvr[g_dvr_number].name, dir_filepath(findhandle), DVRNAME_LEN );
					g_dvr[g_dvr_number].type = DVR_DEVICE_TYPE_HARDDRIVE;
					g_dvr_number++;
				}
			}
		}
		dir_close( findhandle );
	}
	return 1 ;
}

// get totol channel number on dvr root directory
int harddrive_getchannelnum( char * rootpath )
{
	int chnum = 0 ;
	int cycle = 0 ;
	dir_handle root ;

	root = dir_open( rootpath, "20??????" );		// open date directory ex, \\_DVR_\\20080202
	while( dir_find( root ) && cycle<5 ) {		// search in 5 date directories
		if( dir_isfile( root ) ){
			continue ;
		}
		// found a directory
		dir_handle chroot ;
		chroot = dir_open( dir_filepath(root), "CH??" );
		while( dir_find( chroot ) ) {
			int num ;
			if( dir_isdir( chroot ) && sscanf( dir_filename(chroot), "CH%02d", &num )>0 ) {
				dir_handle vroot ;
				vroot = dir_open( dir_filepath(chroot), "CH*."FILE_EXT );
				if( dir_find(vroot) ) {
					if( chnum<num+1 ){
						chnum=num+1 ;
					}
				}
				dir_close( vroot );
			}
		}
		dir_close( chroot ) ;
		cycle++ ;
	}
	dir_close( root );
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
	dir_handle sdir ;
	int res = 0;

	sdir = dir_open( searchpath, "CH*."FILE_EXT);
	if( dir_find(sdir) ) {
		fn=dir_filename( sdir );
		p = fn+strlen(fn)-4;
		*p--=0 ;
		while( p>fn && *p!='_' ) {
			p--;
		}
		if( *p=='_' ) p++ ;
		strcpy( servername, p );
		dir_close(sdir);
		return 1 ;
	}
	else {
		dir_close( sdir );
		sdir = dir_open( searchpath, "*" );
		while( dir_find( sdir ) ) {
			if( dir_isdir( sdir ) ) {
				res = harddrive_getservername( dir_filepath(sdir), servername );
				if (res) break;
			}
		}
		dir_close(sdir);
		return res ;
	}
}

// open a hard drive stream for playback
dvrharddrivestream::dvrharddrivestream(char * servername, int channel, int type)
{
	m_channel=channel ;
	if( type==0 ) {
		m_disknum = 1 ;
		m_rootpath[0] = new char [ strlen(servername)+1 ] ;
		strcpy( m_rootpath[0], servername );
	}
	else {		// open local server
		m_disknum = 0 ;

		int drive ;
		DWORD drives = GetLogicalDrives();
		drives>>=2 ;		// Skip 'A', 'B' drive

		char dirpath[16] ;
		char searchname[256] ;
		dir_handle findhandle ;
		sprintf( searchname, "_%s_", servername );
		for( drive='C'; drive<='Z'; drive++, drives>>=1 ) {
			if( (drives&1)==0 ) continue ;
			sprintf( dirpath, "%c:\\", drive );
			findhandle = dir_open( dirpath, searchname );
			while( dir_find( findhandle ) ) {
				if( dir_isdir( findhandle ) ) {
					char * rpath = dir_filepath(findhandle) ;
					m_rootpath[m_disknum] = new char [strlen( rpath )+ 3 ] ;
					sprintf(m_rootpath[m_disknum], "%s\\", rpath );
					m_disknum++ ;
				}
			}
			dir_close( findhandle );
		}
	}
	dvrtime_init(&m_curtime, 1980);
#ifdef CHECK_TIME
	dvrtime_init(&m_curtimesave, 1980);
#endif
	dvrtime_init(&m_filetime, 1980);
	m_filetimestamp = 0LL;

	m_curfile=0 ;
	m_day=0 ;
	m_file=NULL ;
	m_maxframesize=50*1024 ;		// set default max frame size as 50K
	m_sync.m_insync = 0 ;			// timer not in sync
	m_encryptmode=0;

	m_backup = NULL;
	m_backupSize = m_backupBufSize = 0;
	m_backupType = 0;
	m_keydata = NULL;

	/* gps support */
	m_gpsday = 0;

	//tme_thread_init(&m_ipc);
    tme_mutex_init(&g_ipc, &m_lockTime);

	m_ps.Init();
}

dvrharddrivestream::~dvrharddrivestream()
{
	int i ;
	unloadfile();
	for( i=0; i<m_disknum; i++) {
		delete m_rootpath[i] ;
	}
	if (m_backup) {
		free(m_backup);
	}
	tme_mutex_destroy(&m_lockTime);
	//tme_mutex_destroy(&(m_ipc.object_lock));
	//tme_cond_destroy(&(m_ipc.object_wait));
}

// open a new .265 file.
//    return 1: success
//           0: failed
//			 DVR_ERROR_FILEPASSWORD: password error or need password
int dvrharddrivestream::openfile() 
{
	struct dvrfile * pdf ;
	struct dvrtime filetime ;
	DWORD filehead[10] ;		// 40 bytes header
	DWORD xfilehead[10] ;		// encrypt header
	int rdsize ;

	if( m_curfile>=m_filelist.size() ){
		return 0;
	}

	pdf = m_filelist.at(m_curfile);

	m_encryptmode = ENC_MODE_UNKNOWN ;

	m_ps.Init();
	m_file=fopen( pdf->filename, "rb" );
	if( m_file ) {
		rdsize=fread(filehead, 1, sizeof(filehead), m_file);
		if( rdsize< sizeof(filehead) ) {
			fclose( m_file );
			m_file=NULL ;
			m_ps.SetStream(NULL);
			return 0 ;
		}

		if( filehead[0]==FOURCC_HKMI ) {				// hik .265 file tag
			m_encryptmode = ENC_MODE_NONE ;			// no encryption
		}
		else {
			// try RC4 password
			memcpy( xfilehead, filehead, sizeof(filehead) );
			RC4_crypt( (unsigned char *)xfilehead,  sizeof(filehead) );
			if( xfilehead[0] == FOURCC_HKMI ) {
				m_encryptmode = ENC_MODE_RC4FRAME ;
			}
			else if(m_hVideoKey) {					// assume windows mode encryption
				memcpy( xfilehead, filehead, sizeof(filehead) );
				DWORD datalen = sizeof(filehead) ;
				CryptDecrypt(m_hVideoKey, NULL, TRUE, 0, (BYTE *)xfilehead, &datalen );	
				if( xfilehead[0] == FOURCC_HKMI ) {
					m_encryptmode = ENC_MODE_WIN ;
				}
			}
		}

		if( m_encryptmode == ENC_MODE_UNKNOWN ) {
			fclose( m_file );
			m_file=NULL;
			m_ps.SetStream(NULL);
			return DVR_ERROR_FILEPASSWORD ;
		}
#ifdef TEST_301
		struct hd_frame frame ;
		fread(&frame, 1, sizeof(frame), m_file );
		if( frame.flag==1 ) {
			filetime.year   = pdf->filedate / 10000 ;
			filetime.month  = pdf->filedate % 10000 /100 ;
			filetime.day    = pdf->filedate % 100 ;
			filetime.hour   = pdf->filetime / 3600 ;
			filetime.min    = pdf->filetime % 3600 / 60 ;
			filetime.second = pdf->filetime % 60 ;
			filetime.millisecond = 0 ;
			filetime.tz=0;
			m_curtime = filetime ;
			// check sync timer
			if( m_sync.m_insync ) {
				// check if this file is in sync
				int d1, d2 ;
				d1 = dvrtime_diff( &(m_sync.m_synctime), &filetime ) ;
				d2 = ((int)frame.timestamp - (int)m_sync.m_synctimestamp )/64;
				if( d1-d2 > 2 || d2-d1>2 ) {		// within +- 2 seconds? I consider that is in sync
					// lost in sync, so 
					m_sync.m_insync=0 ;
					m_sync.m_synctime=filetime ;
					m_sync.m_synctimestamp = frame.timestamp ;
				}
			}
			else {
				// update file time to sync timer
				m_sync.m_synctime=filetime ;
				m_sync.m_synctimestamp = frame.timestamp ;
			}
			fseek( m_file, 40, SEEK_SET );			// skip fileheader
			return 1 ;
		}
		else {
			fclose(m_file);
			m_file=NULL;
			m_ps.SetStream(NULL);
		}
#else
		BYTE startcode[4];
		m_ps.SetStream(m_file);
		if (m_ps.StreamPeek(startcode, 4) < 4) {
	        _RPT0(_CRT_WARN, "No MPEG PS header\n" );
			fclose( m_file );
			m_file=NULL ;
			m_ps.SetStream(NULL);
			return 0;
		}
		if( startcode[0] != 0 || startcode[1] != 0 ||
			startcode[2] != 1 || startcode[3] < 0xb9 )
		{
			fclose( m_file );
			m_file=NULL ;
			m_ps.SetStream(NULL);
			return 0;
		}
		__int64 timestamp = m_ps.FindFirstTimeStamp();
		if (timestamp == 0LL) {
			fclose( m_file );
			m_file=NULL;
			m_ps.SetStream(NULL);
			return 0;
		}

		int firstFrameTimeOffset = 0;
		int len = strlen(pdf->filename);
		char * keyfilename = new char [len+4] ;		
		strcpy( keyfilename, pdf->filename );
		strcpy( &keyfilename[len-3], "k");		
		FILE * keyfile = fopen(keyfilename,"r");		
		delete keyfilename ;
		if( keyfile ) {
			int kFrametime, kOffset;		
			if (fscanf(keyfile, "%d,%d\n", &kFrametime, &kOffset )==2 ) {
				if (kFrametime < 0) kFrametime = 0;
				if (kOffset < 0) kOffset = 0;
				firstFrameTimeOffset = kFrametime;
			}
			fclose( keyfile );
		}

		filetime.year   = pdf->filedate / 10000 ;
		filetime.month  = pdf->filedate % 10000 /100 ;
		filetime.day    = pdf->filedate % 100 ;
		filetime.hour   = pdf->filetime / 3600 ;
		filetime.min    = pdf->filetime % 3600 / 60 ;
		filetime.second = pdf->filetime % 60 ;
		filetime.millisecond = firstFrameTimeOffset ;
		filetime.tz=0;
		m_filetime = filetime ;

		m_filetimestamp = timestamp;

#if 0
		tme_mutex_lock(&m_lockTime);
		//m_curtime = filetime ;
		// check sync timer
		if( m_sync.m_insync ) {
			// check if this file is in sync
			int d1, d2 ;
			d1 = dvrtime_diff( &(m_sync.m_synctime), &filetime ) ;
			__int64 diff = timestamp - m_sync.m_synctimestamp;
			__int64 diff_in_sec = diff / 1000000;
			d2 = (timestamp - m_sync.m_synctimestamp) / 1000000;
			_RPT2(_CRT_WARN, "diff:%d,%I64d\n",d2, diff_in_sec );
			if( d1-d2 > 2 || d2-d1>2 ) {		// within +- 2 seconds? I consider that is in sync
				// lost in sync, so 
				m_sync.m_insync=0 ;
				m_sync.m_synctime=filetime ;
				m_sync.m_synctimestamp = timestamp ;
			}
		}
		else {
			_RPT1(_CRT_WARN, "set:%I64d\n",timestamp );
			// update file time to sync timer
			m_sync.m_synctime=filetime ;
			m_sync.m_synctimestamp = timestamp ;
		}
		tme_mutex_unlock(&m_lockTime);
#endif
		_fseeki64( m_file, 40, SEEK_SET );			// skip fileheader

		/* gps support */
		read_gps_data(&filetime, pdf->filename);
		return 1 ;
#endif
	}

	return 0 ;
}

// seek to specified time.
// return 1: success, 0: error, DVR_ERROR_FILEPASSWORD: if password error
int dvrharddrivestream::seek( struct dvrtime * dvrt ) 		
{
	int i;
	struct dvrfile * pdf ;
	int tim ;					// seek time in seconds
	DWORD seektimestamp ;		// seeking time stamp
	int fsize ;
	double ffsize ;				// file size in double precision
	int pos ;
	int res=0 ;
	int seekday ;

	m_backupSize = 0;

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

	//TRACE(_T("ch %d++++++++++++seek"), m_channel); printDvrtime(&m_curtime);
	// set current time to dvrt
	tme_mutex_lock(&m_lockTime);
	m_curtime = *dvrt ;
	//m_curtime.millisecond=0 ;		// make sure we don't have insane milisecond
	m_sync.m_insync = 0 ;			// make synctimer invalid
	tme_mutex_unlock(&m_lockTime);

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

			// seek to right position base on seek time
			if( tim <= pdf->filetime ) {
				_fseeki64( m_file, 40, SEEK_SET ) ;		// skip fileheader
			}
			else if( tim >= ( pdf->filetime + pdf->filelen) ) {
				_fseeki64( m_file, 0, SEEK_END );		// seek to end of file
			}
			else {									// seek inside the file
				// try keyframe index
				list <struct keyentry> keyindex ;	// list of key frame of current opened file
				struct keyentry k ;
				i=(int)strlen(pdf->filename);
				char * keyfilename = new char [i+4] ;
				strcpy( keyfilename, pdf->filename );
				strcpy( &keyfilename[i-3], "k");
				FILE * keyfile = fopen(keyfilename,"r");
				delete keyfilename ;
				if( keyfile ) {
					int kFrametime, kOffset;
					while( fscanf(keyfile, "%d,%d\n", &kFrametime, &kOffset )==2 ) {
						if (kFrametime < 0) kFrametime = 0;
						if (kOffset < 0) kOffset = 0;
						k.frametime = kFrametime;
						k.offset = kOffset;
						keyindex.add(k);
					}
					fclose( keyfile );
				}
				if( keyindex.size()>0 ) {		// key index available
					for(i=keyindex.size()-1; i>=0; i-- ) {
						struct keyentry * key = keyindex.at(i);
						if( pdf->filetime+key->frametime/1000 <= tim ) {		// frametime in milliseconds
							_fseeki64( m_file, key->offset, SEEK_SET );
							return 1 ;
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

	return res;	
}

// get next day with video data available
// return 
//        day in BCD0
//        0 : failed
int dvrharddrivestream::nextday(int day)
{
	int i;
	int nday ;
	for( i=0; i<m_dayinfo.size(); i++ ) {
		nday = m_dayinfo[i] ;
		if( day < nday ){
			return nday ;
		}
	}
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
	if( m_file ) {
		fclose( m_file ) ;
		m_file=NULL ;
	}
	m_curfile++;
	while( m_curfile>=m_filelist.size() ) {	// load the next day file
		nday = nextday(m_day) ;
		if( nday<=0 ) {
			return 0 ;						// failed
		}
		loadfile(nday);						// load files of new day
		m_curfile=0 ;						// first file in list
	}
	res = openfile();
	if( res==0 ) {
		return opennextfile();
	}
	return res ;
}

void dvrharddrivestream::adjustCurtime(__int64 timestamp)
{
	tme_mutex_lock(&m_lockTime);
	if (timestamp > 0) {
		m_curtime = m_filetime; //m_sync.m_synctime ;
		__int64 diff = timestamp-m_filetimestamp;//m_sync.m_synctimestamp;
		__int64 diff_in_milsec = diff / 1000;

		dvrtime_addmilisecond(&m_curtime, (int)diff_in_milsec) ;
	}
	tme_mutex_unlock(&m_lockTime);
	//TRACE(_T("ch %d++++++++++++adjustCurtime"),m_channel); printDvrtime(&m_curtime);
}

// get dvr data frame
int dvrharddrivestream::getdata(char **framedata, int * framesize, int * frametype, int *headerlen)
{
#ifdef TEST_301
	struct hd_frame frame ;		// frame header ;
	struct hd_subframe subframe ;	// sub frame header ;
	int framestart, frameend ; ;

	* frametype = FRAME_TYPE_UNKNOWN ;

	if( m_file==NULL ) {
		return 0 ;
	}

	framestart = ftell( m_file ) ;

	if( fread( &frame, 1, sizeof(frame), m_file )>0 ) {
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

			fseek( m_file, frame.framesize, SEEK_CUR );
			while( frame.frames>1 ) {
				fread( &subframe, 1, sizeof(subframe), m_file ) ;
				fseek( m_file, subframe.framesize, SEEK_CUR ) ;
				frame.frames-- ;
			}
			frameend = ftell( m_file );

			* framesize=frameend-framestart ;
			if( *framesize>0 && *framesize<1000000 ) {
				* framedata = new char [*framesize] ;
				fseek( m_file, framestart, SEEK_SET );
				fread( *framedata, 1, *framesize, m_file );
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
				return 1 ;		// success
			}
		}
	}
	else {
		opennextfile();
	}
	return getdata(framedata, framesize, frametype, headerlen);
#else
	__int64 timestamp;
	int headerLen;
	int dataSize = 0;
	int videoFrameType = 0;
	int fSize, fType;
	char *pBuf;
	bool backupFilled = false;

	if (frametype) *frametype = FRAME_TYPE_UNKNOWN ;
	if (framedata) *framedata = NULL;

	if( m_file==NULL ) {
		return 0 ;
	}

	if (m_backupSize) {
		*framedata = (char *)malloc(m_backupSize);
		if (*framedata) {
			memcpy(*framedata, m_backup, m_backupSize);
			videoFrameType = m_backupType;
			dataSize = m_backupSize;
			m_backupSize = 0;
			backupFilled = true;
		}
	}

	if (headerlen) {
		// savedvrfile needs to know headerlen.
		if (m_ps.GetOneFrame(&pBuf, framesize, frametype, &timestamp, &headerLen)) {
			adjustCurtime(timestamp);
			if (*framesize > 0 && framedata) {
				*framedata = (char *)malloc(*framesize);
				memcpy(*framedata, pBuf, *framesize);
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
		} else {
			opennextfile();
		}
	} else {
		while (1) {
			if (!m_ps.GetOneFrame(&pBuf, &fSize, &fType, &timestamp, &headerLen)) {
				break;
			}
			if (fSize <= 0 || !framedata) {
				break;
			}

			if( m_encryptmode == ENC_MODE_RC4FRAME ) {
				int fsize = fSize - headerLen ;
				if( fsize>1024 ) {
					fsize=1024 ;
				}
				RC4_crypt((unsigned char *)(pBuf + headerLen), fsize);
			}

			if (videoFrameType &&
				((fType == FRAME_TYPE_KEYVIDEO) || (fType == FRAME_TYPE_VIDEO)))
			{
				// got second video frame, save it.
				if (m_backupBufSize < fSize) {
					m_backup = (char *)realloc(m_backup, fSize);	
					m_backupBufSize = fSize;
				}
				memcpy(m_backup, pBuf, fSize);
				m_backupSize = fSize;
				m_backupType = fType;
				
				adjustCurtime(timestamp);
				*framesize = dataSize;
				*frametype = videoFrameType ? videoFrameType : fType;
				return 1;
			}

			if ((fType == FRAME_TYPE_KEYVIDEO) || (fType == FRAME_TYPE_VIDEO))
				videoFrameType = fType;

			*framedata = (char *)realloc(*framedata, fSize + dataSize);
			if (*framedata) {
				memcpy(*framedata + dataSize, pBuf, fSize);
				dataSize += fSize;
			}

			continue;
		}

		if (backupFilled) {
			free(*framedata);
			dataSize = 0;
		}

		if (!dataSize)
			opennextfile();
	}

	return getdata(framedata, framesize, frametype, headerlen);
#endif
}

// get current time and timestamp
// return 1: in sync,
//        0: not in sync
int dvrharddrivestream::gettime( struct dvrtime * dvrt )
{
	tme_mutex_lock(&m_lockTime);
	*dvrt = m_curtime ;
	tme_mutex_unlock(&m_lockTime);
#ifdef CHECK_TIME
	int ret = dvrtime_compare(&m_curtime, &m_curtimesave);
	if (ret < 0) {
							TRACE(_T("  -----------------------\n"));
	}
	m_curtimesave = m_curtime;
#endif
	return  1;//m_sync.m_insync ;
}

// provide and internal structure to sync other stream
void * dvrharddrivestream::getsynctimer()
{
	// this function is not used anymore
	m_sync.m_insync=1 ;
	return (void *)&m_sync ;
}

// use synctimer to setup internal sync timer
void dvrharddrivestream::setsynctimer(void * synctimer)
{
	if( synctimer!=NULL ) {
		m_sync = * (struct sync_timer *)synctimer ;
	}
}

// get stream information
// set ppi->total_channel, ppi->servername 
// return 0: failed
//        1: success ;
int dvrharddrivestream::getserverinfo(struct player_info *ppi)
{
	char servername[MAX_PATH] ;
	int i, len, nch ;
	if( ppi->size<sizeof(struct player_info) )
		return 0 ;

	ppi->total_channel = 0 ;
	for (i = 0; i < m_disknum; i++) {
		 nch = harddrive_getchannelnum( m_rootpath[i] );
		 if (nch > ppi->total_channel)
			 ppi->total_channel = nch;
	}

	if( ppi->total_channel>0 ) {
		if( harddrive_getservername(m_rootpath[0], servername)>0 ) {
			strcpy( ppi->servername, servername );
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
					return 1 ;
				}
			}
		}
	}
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
	dir_handle findhandle ;
	if( m_dayinfo.size()==0 ) {
		for( i=0; i<m_disknum; i++ ) {
			findhandle = dir_open( m_rootpath[i], "20??????" ) ;
			while( dir_find( findhandle ) ) {
				char * datename = dir_filename( findhandle );
				if( dir_isdir( findhandle ) &&
					datename[2]>='0' && datename[2]<='4' &&
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
			dir_close( findhandle );
		}
		m_dayinfo.sort();
		m_dayinfo.compact();
	}

	if( m_dayinfo.size()>0 ) {					// day info available
		day=daytime->year*10000+daytime->month*100+daytime->day ;
		for( i=0; i<m_dayinfo.size(); i++) {
			if( day==*(int *)m_dayinfo.at(i) ) {		// found it
				return 1 ;
			}
		}
	}
	
	return 0 ;								// not data
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
int dvrharddrivestream::gettimeinfo( struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize)
{
	int i;
	int getsize = 0 ;
	struct cliptimeinfo cti ;
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

	if(	loadfilelist(begintime, &dflist) ) {
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

				if( tinfosize>0 && tinfo!=NULL ) {
					if( getsize>=tinfosize ) {
						break;
					}
					// set clip on time
					if( cti.on_time > daybegintime ) {
						tinfo[getsize].on_time = cti.on_time - daybegintime ;
					}
					else {
						tinfo[getsize].on_time = 0 ;
					}
					// set clip off time
					tinfo[getsize].off_time = cti.off_time - daybegintime ;
				}
				getsize++ ;
//			}
		}
	}

	return getsize ;
}

// get locked file availability information
int dvrharddrivestream::getlockfileinfo(struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize )	
{	
	int i;
	int getsize = 0 ;
	struct cliptimeinfo cti ;
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

	if(	loadfilelist(begintime, &dflist) ) {
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

				if( tinfosize>0 && tinfo!=NULL ) {
					if( getsize>=tinfosize ) {
						break;
					}
					// set clip on time
					if( cti.on_time > daybegintime ) {
						tinfo[getsize].on_time = cti.on_time - daybegintime ;
					}
					else {
						tinfo[getsize].on_time = 0 ;
					}
					// set clip off time
					tinfo[getsize].off_time = cti.off_time - daybegintime ;
				}
				getsize++ ;
			}
		}
	}

	return getsize ;
}

int dvrharddrivestream::unloadfile()
{
	if( m_file ) {
		fclose(m_file);
		m_file=NULL;
	}
	m_curfile=0;
	m_filelist.clean();
	m_day=-1 ;
	return 0;
}

// load dvr files into name list, return 1 for success, 0 for failed.
int dvrharddrivestream::loadfilelist(struct dvrtime * day, list <struct dvrfile> * dflist )
{
	int i ;
	char dirpath[MAX_PATH] ;
	char dirpattern[30] ;
	dir_handle findch ;
	int res=0;
	char * lockstr ;

	if( getdayinfo( day ) ) {	// if data available ?
		for( i=0; i<m_disknum; i++) {
			sprintf(dirpath, "%s%04d%02d%02d\\CH%02d\\", 
				m_rootpath[i], 
				day->year, day->month, day->day,
				m_channel );
			sprintf(dirpattern, "CH%02d_%04d%02d%02d*."FILE_EXT, 
				m_channel,
				day->year, day->month, day->day );
			findch = dir_open( dirpath, dirpattern );
			while( dir_find( findch ) ) {
				if( dir_isdir( findch ) )
					continue ;
				// found a file
				dvrfile df ;
				df.filedate = day->year*10000+day->month*100+day->day ;
				char * filename = dir_filename( findch );
				sscanf( &filename[13], "%d", &(df.filetime) );	// in BCD 
				// adjust file time to seconds of the day
				df.filetime = (df.filetime/10000) * 3600 +		// hours
							  (df.filetime%10000/100) * 60 +	// minutes
							  (df.filetime%100)	;				// seconds
				sscanf( &filename[20], "%d", &(df.filelen) );
				strncpy( df.filename, dir_filepath( findch ), sizeof( df.filename) );
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
					if (!is_duplicate(dflist, &df)) {
						res=1;
						dflist->add( df );
						//_RPT2(_CRT_WARN, "ch %d added %s\n", m_channel, df.filename);
					}
				}
			}
			dir_close( findch ) ;
		}
		dflist->sort();
	}
	return res;
}

// loadday as BCD format day
int dvrharddrivestream::loadfile(int loadday)
{
	struct dvrtime loadtime ;
	dvrtime_init(&loadtime, loadday/10000, loadday%10000/100, loadday%100);

	unloadfile() ;		// unload files list first
	if( loadfilelist(&loadtime, &m_filelist) ) {
		m_filelist.compact() ;
	}
	m_day = loadday ;
	return 0;
}

void dvrharddrivestream::setpassword(int passwdtype, void * password, int passwdsize )
{
	if( passwdtype==ENC_MODE_RC4FRAME ) {			// Frame RC4
		RC4_gentable((char *)password);
	}
	else if( passwdtype==ENC_MODE_WIN ) {			// Win RC4
		m_hVideoKey = *(HCRYPTKEY *)password ;
	}
}

// set key block
void dvrharddrivestream::setkeydata( struct key_data * keydata )
{
	unsigned char * k256 ;
	m_keydata = keydata ;
	if( m_keydata ) {
		m_encryptmode = ENC_MODE_RC4FRAME ;
		k256 = (unsigned char *)m_keydata->videokey ;
		RC4_crypt_table( m_crypt_table, sizeof(m_crypt_table), k256 );
	}
}

/* gps support */
bool gpsinfo_less(struct gpsinfo a, struct gpsinfo b)
{
	int atime, btime;

	atime = a.hour * 10000 + a.min * 100 + a.second;
	btime = b.hour * 10000 + b.min * 100 + b.second;

	return (atime < btime);
}

/* return # of records */
void dvrharddrivestream::read_gps_lines(FILE *fp, struct dvrtime *filetime)
{
	int ret;
	int hh, mm, ss, type;
	char line[256];
	double latitude, longitude;
	char ns, ew;
	float speed, angle;
	int h = -1, m = -1, s = -1; /* lastly added record time */

	while (fgets(line, sizeof(line), fp) != NULL) {
		ret = sscanf(line, "%02d,%02d:%02d:%02d,%lf%c%lf%c%fD%f", 
			&type, &hh, &mm, &ss, 
			&latitude, &ns, &longitude, &ew,
			&speed, &angle);
		if (ret == 10) {
			/* add only one record for each second */
			if ((hh != h) || (mm != m) || (ss != s)) {
				struct gpsinfo gi;
				gi.hour = hh;
				gi.min = mm;
				gi.second = ss;
				gi.latitude = latitude;
				if ((ns == 'S') || (ns == 's'))
					gi.latitude = -latitude;
				gi.longitude = longitude;
				if ((ew == 'W') || (ns == 'w'))
					gi.longitude = -longitude;
				gi.speed = speed;
				gi.angle = angle;
				m_gpslist.push_back(gi);
				h = hh;
				m = mm;
				s = ss;
			}
		}
	}

	if (m_gpslist.size() > 0) {
		sort(m_gpslist.begin(), m_gpslist.end(), gpsinfo_less);
	}
}

void dvrharddrivestream::read_gps_data(struct dvrtime *filetime, char *avfilename)
{
	char *ptr, filename[MAX_PATH];

	/* check if the gps file is already read in */
	if (m_gpsday == filetime->year * 10000 + filetime->month * 100 + filetime->day)
		return;

	m_gpslist.clear();

	strncpy(filename, avfilename, sizeof(filename)); 
	ptr = strstr(filename, "\\_");
	if (ptr) {
		char *ptr2, servername[MAX_PATH], fname[MAX_PATH];
		*ptr = 0;
		/* get servername */
		ptr2 = strstr(ptr + 1, "_\\");
		if (ptr2) {
			*ptr2 = 0;
			strncpy(servername, ptr + 2, sizeof(servername));
			strcat(filename, "\\smartlog\\");
			_snprintf(fname, sizeof(fname), 
				"%s_%04d%02d%02d_L.001",
				servername, filetime->year, filetime->month, filetime->day); 
			strcat(filename, fname);
			FILE *fp = fopen(filename, "rb");
			if (fp == NULL) {
				/* try N file */
				filename[strlen(filename) - 5] = 'N';
				fp = fopen(filename, "rb");
			}
			if (fp) {
				read_gps_lines(fp, filetime);
				m_gpsday = filetime->year * 10000 + filetime->month * 100 + filetime->day;
				fclose(fp);
			}
		}
	}
}

int dvrharddrivestream::getlocation(struct dvrtime *time, struct gpsinfo *gps)
{
	if (m_gpsday != time->year * 10000 + time->month * 100 + time->day)
		return -1;

	std::vector<struct gpsinfo>::iterator iter;

	gps->hour = time->hour;
	gps->min = time->min;
	gps->second = time->second;
	iter = lower_bound(m_gpslist.begin(), m_gpslist.end(), *gps, gpsinfo_less);
	if (iter != m_gpslist.end()) {
		/* have we found the exact match */
		if ((iter->hour == time->hour) && (iter->min == time->min) && (iter->second == time->second)) {
			*gps = *iter;
		} else if (iter == m_gpslist.begin()) {
			/* 1st record is past the requested time */
			return -1;
		} else {
			/* no exact match, return the record just before */
			*gps = *(--iter);
		}

		return 0;
	}

	return -1;
}