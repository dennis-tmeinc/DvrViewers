
#ifndef __DVRHARDDRIVESTREAM_H__
#define __DVRHARDDRIVESTREAM_H__

#include "ply606.h"
#include "crypt.h"
#include "mpeg4.h"
#include "tme_thread.h"
#include <vector>

// DVR preivew stream
//     Single channel dvr data streaming for dvr preivew

struct keyentry {
	DWORD frametime ;			// frame time frome begining of file
	DWORD offset ;			// offset in data file
} ;

struct dvrfile {
	char filename [MAX_PATH]; 
	int  filedate ;				// bcd, example 20080101 for 2008-01-01
	int  locklen ;				// locked length
	int  filetype ;				// 0: N file, 1: L file
	unsigned int  filelen ;				// in milliseconds
	__int64  filetime ;				// in milliseconds
};

inline int operator < ( struct dvrfile & f1, struct dvrfile & f2 )
{
	if( f1.filedate != f2.filedate ) {
		return ( f1.filedate<f2.filedate );
	}
	else {
		return ( f1.filetime<f2.filetime );
	}
}

class dvrharddrivestream : public dvrstream{ 
protected:

	//tme_object_t m_ipc;
    tme_mutex_t		m_lockTime;

	int m_channel ;						// stream channel
	char * m_rootpath[26] ;				// harddrive root path		
	int m_disknum ;						// number of disks ( multi-disk support )

//	list of file ;

	xlist   <struct dvrfile> m_filelist ;					// list of current day file, member : struct dvrfile
	int    m_curfile ;					// current file index
	FILE * m_file ;						// current opened file

	xlist   <int> m_dayinfo ;			// day info list (BCD)
	int		m_day ;						// current day in BCD. 

	struct dvrtime m_curtime, m_filetime ;			// current playing time (frame time)
	CMpegPs m_ps;

//	int    m_filetime ;					// file time
//	unsigned long  m_timestamp ;		// current timestamp ;
//	unsigned long  m_filetimestamp ;		// first frame time stamp of the file, use for play time calculation

	struct sync_timer {
		struct dvrtime m_synctime ;
		int			   m_insync ;			// if timer is in cync with all other channel
		__int64		   m_synctimestamp ;
	} m_sync ;							// sync timer, use to synchronize other stream and calculate stream time in sync

	int m_maxframesize ;

	// file encryption support
	int m_encryptmode;					// 0: no encryption, 1: windows mode, 2: Frame data first 1k bytes RC4
	struct key_data * m_keydata ;
	char m_fbuf[4096] ;
	int  m_fbufpos ;
	int  m_fbufsize ;
	char *m_backup;
	int m_backupSize, m_backupBufSize, m_backupType;

	/* gps support */
	int m_gpsday; 
	std::vector <struct gpsinfo> m_gpslist;

	// file encryption support, RC4 frame data
	unsigned char m_crypt_table[CRYPT_TABLE_SIZE] ;
	void RC4_crypt( unsigned char * text, int textsize ) {
		RC4_block_crypt( text, textsize, 0, m_crypt_table, sizeof(m_crypt_table));
	}
	// generate cryption table
	void RC4_gentable(char * password) {
		unsigned char k256[256] ;
		key_256(password, k256);
		RC4_crypt_table( m_crypt_table, sizeof(m_crypt_table), k256);
	}

protected:
	// operators

	int unloadfile();	// unload file list
	int loadfile(int day);	// load file lists on specified day
	int openfile() ;	// open video file
	int opennextfile();	// open next video file
	int readfile(void * buf, int size, FILE * fh);	// read .264 file
	int nextday(int day);	// find next day with video
	int loadfilelist(struct dvrtime * day, xlist <struct dvrfile> * dflist );
	void adjustCurtime(__int64 timestamp);
public:
	dvrharddrivestream(char * servername, int channel, int type=0);		// open harddrives. type==0 servername is root directory of video files
	~dvrharddrivestream();

	int seek( struct dvrtime * dvrt ); 		// seek to specified time.
	int gettime( struct dvrtime * dvrt );	// get current stream time, (precise to miliseconds)
	int getserverinfo(struct player_info *ppi) ;						// get server information
	int getchannelinfo(struct channel_info * pci);						// get stream channel information
	int getdayinfo(dvrtime * daytime);		// get stream day data availability information
	int gettimeinfo( struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize); // get data availability information
	int getlockfileinfo(struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize );	// get locked file availability information

	void loadkey();		// load key index of all files

	// provide and internal structure to sync other stream
	void * getsynctimer();
	// use synctimer to setup internal sync timer
	void setsynctimer(void * synctimer) ;	
	void setpassword(int passwdtype, void * password, int passwdsize );
	virtual block_t *getdatablock();	
	virtual void setkeydata( struct key_data * keydata );

	/* gps support */
	int getlocation(struct dvrtime *time, struct gpsinfo *gps);
	void read_gps_data(struct dvrtime *filetime, char *avfilename);
	void read_gps_lines(FILE *fp, struct dvrtime *filetime);

    int geteventtimelist( struct dvrtime * date, int eventnum, int * elist, int elistsize);

};

int harddrive_finddevice();

#endif	// __DVRHARDDRIVESTREAM_H__