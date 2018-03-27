
#ifndef __DVRHARDDRIVESTREAM_H__
#define __DVRHARDDRIVESTREAM_H__

#include "PLY301.h"
#include "crypt.h"
#include "../common/cstr.h"
#include "../common/cdir.h"

// DVR preivew stream
//     Single channel dvr data streaming for dvr preivew

struct dvrfile {
	char filename [MAX_PATH]; 
	int  filedate ;				// bcd, example 20080101 for 2008-01-01
	int  filetime ;				// in seconds
	int  filelen ;				// in seconds
	int  locklen ;				// locked length
	int  filetype ;				// 0: N file, 1: L file
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

	int m_channel ;						// stream channel
	char * m_rootpath[26] ;				// harddrive root path		
	int m_disknum ;						// number of disks ( multi-disk support )

//	list of file ;

	list   <struct dvrfile> m_filelist ;					// list of current day file, member : struct dvrfile
	int    m_curfile ;					// current file index
	FILE * m_file ;						// current opened file

	list   <int> m_dayinfo ;			// day info list (BCD)
	int		m_day ;						// current day in BCD. 

	struct dvrtime m_curtime ;			// current playing time (frame time)

//	int    m_filetime ;					// file time
//	unsigned long  m_timestamp ;		// current timestamp ;
//	unsigned long  m_filetimestamp ;		// first frame time stamp of the file, use for play time calculation

	struct sync_timer {
		struct dvrtime m_synctime ;
		DWORD		   m_synctimestamp ;
		int			   m_insync ;			// if timer is in cync with all other channel
	} m_sync ;							// sync timer, use to synchronize other stream and calculate stream time in sync

	int m_maxframesize ;

	// file encryption support, (Windows mode)
	char m_fbuf[4096] ;
	OFF_T m_fbufpos ;
	OFF_T m_fbufsize ;

protected:
    // GPS location support
    string m_servername ;
    FILE * m_gpsfile ;
    struct dvrtime m_gpstime ;                          // saved previous read gps time 

protected:
	// operators

	int unloadfile();	// unload file list
	int loadfile(int day);	// load file lists on specified day
	int openfile() ;	// open video file
	int opennextfile();	// open next video file
	int readfile(void * buf, int size, FILE * fh);	// read .264 file
	int nextday(int day);	// find next day with video
	OFF_T find_frame( FILE * fd );	// find next frame in file
	OFF_T next_frame(FILE * fd);		// seek to next frame
	int loadfilelist(struct dvrtime * day, list <struct dvrfile> * dflist );

public:
	dvrharddrivestream(char * servername, int channel, int type=0);		// open harddrives. type==0 servername is root directory of video files
	~dvrharddrivestream();

	int seek( struct dvrtime * dvrt ); 		// seek to specified time.
	OFF_T prev_keyframe();						// seek to previous key frame position.
	OFF_T next_keyframe();						// seek to next key frame position.
	int getdata(char **framedata, int * framesize, int * frametype);	// get dvr data frame
	int gettime( struct dvrtime * dvrt );	// get current stream time, (precise to miliseconds)
	int getserverinfo(struct player_info *ppi) ;						// get server information
	int getchannelinfo(struct channel_info * pci);						// get stream channel information
	int getdayinfo(dvrtime * daytime);		// get stream day data availability information

	int gettimeinfo_help( struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize, int lock); // get data availability information
	int gettimeinfo( struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize); // get data availability information
	int getlockfileinfo(struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize );	// get locked file availability information

	void loadkey();		// load key index of all files

	// provide and internal structure to sync other stream
	void * getsynctimer();
	// use synctimer to setup internal sync timer
	void setsynctimer(void * synctimer) ;	

    // get gps location
    virtual int getlocation( struct dvrtime * dvrt, char * locationbuffer, int buffersize );
    // get event time list
    virtual int geteventtimelist( struct dvrtime * date, int eventnum, int * elist, int elistsize) ;
};

int harddrive_finddevice();

#endif	// __DVRHARDDRIVESTREAM_H__