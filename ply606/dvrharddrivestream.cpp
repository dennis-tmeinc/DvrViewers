
// hard drive stream in module
//

#include "stdafx.h"
#include <stdio.h>
#include <io.h>
#include <process.h>
#include <crtdbg.h>
#include "player.h"
#include "ply606.h"
#include "dvrnet.h"
#include "dvrharddrivestream.h"
#include "trace.h"
#include "utils.h"
#include <algorithm>

struct dir_entry {
	intptr_t findhandle ;
	struct _finddata_t fileinfo ;
	char dirpath[MAX_PATH] ;
	char filepath[MAX_PATH] ;
};

typedef dir_entry * dir_handle ;

bool is_duplicate(xlist <struct dvrfile> *dflist, struct dvrfile *df)
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
		if(handle->findhandle!=-1 ) {
			return 1 ;			// success
		}
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
	dvrtime_init(&m_filetime, 1980);
	m_curfile=0 ;
	m_day=0 ;
	m_file=NULL ;
	m_maxframesize=50*1024 ;		// set default max frame size as 50K
	m_sync.m_insync = 0 ;			// timer not in sync
	m_encryptmode=0;
	m_keydata = NULL;

	m_backup = NULL;
	m_backupSize = m_backupBufSize = 0;
	m_backupType = 0;
 
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
	int ret ;

	if( m_curfile>=m_filelist.size() ){
		return 0;
	}

	pdf = m_filelist.at(m_curfile);

	m_encryptmode = ENC_MODE_UNKNOWN ;

	m_ps.Init();
	m_file=fopen( pdf->filename, "rb" );
	if( m_file ) {
		struct mp5_header fileHeader;
		ret = fread( &fileHeader, 1, sizeof(fileHeader), m_file );

		if (ret < sizeof(fileHeader)) {
			fclose( m_file );
			m_file=NULL ;
			m_ps.SetStream(NULL);
			return 0;
		}

		if( fileHeader.fourcc_main == FOURCC_HKMI ) {	
			m_encryptmode = ENC_MODE_NONE ;			// no encryption
		} else {
			// try RC4 password
			RC4_crypt( (unsigned char *)&fileHeader,  12 );
			if( fileHeader.fourcc_main == FOURCC_HKMI ) {
				m_encryptmode = ENC_MODE_RC4FRAME ;
				int encryptsize = fileHeader.stream_info.encryption;
				if ((encryptsize < 0) || (encryptsize > 1024)) {
					encryptsize = 1024;
				}
				m_ps.SetEncrptTable(m_crypt_table, encryptsize);
			}
		}

		if( m_encryptmode == ENC_MODE_UNKNOWN ) {
			fclose( m_file );
			m_file=NULL;
			m_ps.SetStream(NULL);
			return DVR_ERROR_FILEPASSWORD ;
		}

		//m_ps.SetFilename(pdf->filename);
		m_ps.SetStream(m_file);
		m_ps.ParseFileHeader(&fileHeader);


		__int64 timestamp, len;
		len = m_ps.ReadIndexFromFile(pdf->filename, &timestamp);
		if (len == 0LL) {
			fclose( m_file );
			m_file=NULL;
			m_ps.SetStream(NULL);
			return 0;
		}

		filetime.year   = pdf->filedate / 10000 ;
		filetime.month  = pdf->filedate % 10000 /100 ;
		filetime.day    = pdf->filedate % 100 ;
		filetime.hour   = pdf->filetime / 1000 / 3600 ;
		filetime.min    = pdf->filetime / 1000 % 3600 / 60 ;
		filetime.second = pdf->filetime / 1000 % 60 ;
		filetime.millisecond = pdf->filetime % 1000 ;
		filetime.tz=0;
		m_filetime = filetime ;
#ifdef USE_SYNC
		// check sync timer
		if( m_sync.m_insync ) {
			// check if this file is in sync
			__int64 d1, d2 ;
			d1 = dvrtime_diffms( &(m_sync.m_synctime), &filetime ) ;
			d2 = (timestamp - m_sync.m_synctimestamp) / 1000;
			if( d1-d2 > 2000 || d2-d1>2000 ) {		// within +- 2 seconds? I consider that is in sync
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
#endif

		/* gps support */
		read_gps_data(&filetime, pdf->filename);

		return 1 ;
	}

	return 0 ;
}

// seek to specified time.
// return 1: success, 0: error, DVR_ERROR_FILEPASSWORD: if password error
int dvrharddrivestream::seek( struct dvrtime * dvrt ) 		
{
	int i;
	struct dvrfile * pdf ;
	__int64 tim ;					// seek time in milliseconds
	DWORD seektimestamp ;		// seeking time stamp
	int fsize ;
	double ffsize ;				// file size in double precision
	int pos ;
	int res=0 ;
	int seekday ;

	//TRACE(_T("Seek:(%d)"), m_channel); printDvrtime(dvrt);

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
	tme_mutex_lock(&m_lockTime);
	m_curtime = *dvrt ;
	//m_curtime.millisecond=0 ;		// make sure we don't have insane milisecond
	//m_sync.m_insync = 0 ;			// make synctimer invalid
	tme_mutex_unlock(&m_lockTime);

	if( m_filelist.size()>0 ) {
		// find the file in correct time

		tim = dvrt->hour * 3600000LL + dvrt->min * 60000LL + dvrt->second * 1000LL + dvrt->millisecond;
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
				_fseeki64( m_file, sizeof(struct mp5_header), SEEK_SET ) ;		// skip fileheader
			}
			else if( tim >= ( pdf->filetime + pdf->filelen) ) {
				_fseeki64( m_file, 0, SEEK_END );		// seek to end of file
			}
			else {									// seek inside the file
				m_ps.Seek((tim - pdf->filetime) * 1000LL, 0);		
				return 1 ;
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
#ifdef USE_SYNC
	if (timestamp > 0) {
		m_curtime = m_sync.m_synctime ;
		__int64 diff = timestamp-m_sync.m_synctimestamp;
		__int64 diff_in_milsec = diff / 1000;

		dvrtime_addmilisecond(&m_curtime, (int)diff_in_milsec) ;
	}
#else
	tme_mutex_lock(&m_lockTime);
	if (timestamp > 0) {
		m_curtime = m_filetime ;
		__int64 diff = timestamp-1;
		__int64 diff_in_milsec = diff / 1000;

		dvrtime_addmilisecond(&m_curtime, (int)diff_in_milsec) ;
	}
	tme_mutex_unlock(&m_lockTime);
#endif
}

block_t *dvrharddrivestream::getdatablock()
{
	__int64 framestart;
	block_t *pBlock;

	if( m_file==NULL ) {
		return NULL;
	}

	pBlock = m_ps.GetFrame();
	if (pBlock) {
		if ((pBlock->i_pts > 0) || (pBlock->i_dts > 0)) {
			__int64 ts = max(pBlock->i_pts, pBlock->i_dts);
			adjustCurtime(ts);
#if 0 // done in mpeg4.cpp
			if( m_encryptmode == ENC_MODE_RC4FRAME ) {
				int fsize = pBlock->i_buffer;
				if( fsize>1024 ) {
					fsize=1024 ;
				}
				RC4_crypt(pBlock->p_buffer, fsize);
			}
#endif
		}
		return pBlock;
	} else {
		opennextfile();
	}

	return getdatablock();
}

// get current time and timestamp
// return 1: in sync,
//        0: not in sync
int dvrharddrivestream::gettime( struct dvrtime * dvrt )
{
	tme_mutex_lock(&m_lockTime);
	*dvrt = m_curtime ;
	tme_mutex_unlock(&m_lockTime);
#ifdef USE_SYNC
	return  m_sync.m_insync ;
#else
	return 1;
#endif
}

// provide and internal structure to sync other stream
void * dvrharddrivestream::getsynctimer()
{
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
	int i, len, nch;
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
	xlist <struct dvrfile> dflist ;

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
				cti.on_time = pdf->filetime / 1000 ;
				cti.off_time = (pdf->filetime+pdf->filelen) / 1000 ;
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
	xlist <struct dvrfile> dflist ;

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
				cti.on_time = pdf->filetime / 1000;
				cti.off_time = (pdf->filetime+pdf->filelen) / 1000 ;
				//cti.on_time = cti.off_time-pdf->locklen ;
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
int dvrharddrivestream::loadfilelist(struct dvrtime * day, xlist <struct dvrfile> * dflist )
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
				sscanf( &filename[13], "%I64d", &(df.filetime) );	// in BCD 
#ifdef USE_LONG_FILENAME
				// adjust file time to seconds of the day
				df.filetime = (df.filetime/10000000) * 3600000 +		// hours
							  (df.filetime%10000000/100000) * 60000 +	// minutes
							  (df.filetime%100000)	;				// milliseconds
				sscanf( &filename[23], "%u", &(df.filelen) );
#else
				// adjust file time to seconds of the day
				df.filetime = (df.filetime/10000) * 3600000 +		// hours
							  (df.filetime%10000/100) * 60000 +	// minutes
							  (df.filetime%100) * 1000;		// seconds
				sscanf( &filename[20], "%u", &(df.filelen) );
				df.filelen *= 1000;
#endif
				strncpy( df.filename, dir_filepath( findch ), sizeof( df.filename) );

				if( (lockstr=strstr(filename, "_L_"))!=NULL ) { /* old file name used in 510x */
					df.filetype=1 ;					// set locked file type
#if 0
					int locklength;
					char underscore ;
     				df.locklen=df.filelen ;
					if( sscanf( &lockstr[3], "%d%c", &locklength, &underscore )==2 ) {
						if( underscore == '_' ) {
							df.locklen = locklength ;
						}
					}
#endif
				}
				else {
					df.filetype=0 ;					// set normal file type
				}

				if( df.filelen>0 ) {
					if (!is_duplicate(dflist, &df)) {
						res=1;
						dflist->add( df );
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


// string class
#include "../common/cstr.h"
#include "../common/cdir.h"

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

    FILE * gpsfile = NULL ;
    int disk ;
    for(disk=0; disk<m_disknum; disk++ ) {
        string smartlogfilename ;
        string filepattern ;
        sprintf( smartlogfilename.strsize(512), "%s..\\smartlog", m_rootpath[disk] );    // smartlog directory
        sprintf( filepattern.strsize(512), "*_%04d%02d%02d_?.001", dvrt->year, dvrt->month, dvrt->day );

        dirfind dir( smartlogfilename, filepattern) ;
    	while( dir.find() ) {		
            if( dir.isfile() ){
                gpsfile = fopen(dir.filepath(), "rb");
                if( gpsfile ) {
                    break;
                }
            }
        }
        if( gpsfile ) {
            break;
        }
    }

    if( gpsfile ) {
        int numevent = 0 ;
        string line ;
        line.strsize(256);
        while( fgets( line, 255, gpsfile) ) {
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
        fclose( gpsfile ) ;
        return numevent ;
    }
    return DVR_ERROR ;
}
