
#ifndef __DVRPLAYBACKSTREAM_H__
#define __DVRPLAYBACKSTREAM_H__

#include "PLY301.h"

#include "crypt.h"

// DVR preivew stream
//     Single channel dvr data streaming for dvr preivew

class dvrplaybackstream : public dvrstream {
protected:
	int m_channel ;
	SOCKET m_socket ;
	int m_streamhandle ;			// stream mode handle
	char m_server[MAX_PATH] ;

	int   m_seekreq ;				// request to seek
	struct dvrtime m_seektime ;
 
	DWORD m_monthinfo ;
	struct dvrtime m_month ;

	// clip info cache
	struct cliptimeinfo * m_clipinfolist ;	// list of day clip info.
	int	m_clipinfolistsize ;
	int	m_clipinfolistday ;

	// locked clip info cache
	struct cliptimeinfo * m_lockinfolist ;	// list of day clip info.
	int	m_lockinfolistsize ;
	int	m_lockinfolistday ;

	struct dvrtime m_curtime ;
	struct dvrtime m_keyframetime ;
	DWORD		   m_keyframestamp ;

	int m_threadstate;
	// frame fifo, make play back smooth
	struct frame_t {
		struct frame_t * next ;
		int frametype;
		char * framedata ;
		int framesize;
		struct dvrtime frametime ;
	};

	struct frame_t * m_fifohead ;
	struct frame_t * m_fifotail ;
	int		m_fifosize ;

	void fifoputframe( struct frame_t * nframe ) {
        lock();
		nframe->next = NULL ;
		m_fifosize++;
		if( m_fifotail==NULL ) {
			m_fifohead=m_fifotail = nframe ;
		}
		else {
			m_fifotail->next = nframe ;
			m_fifotail = nframe ;
		}
        unlock();
	}

	struct frame_t * fifogetframe() {
        lock();
		frame_t * head=m_fifohead ;
		if( m_fifohead ) {
			m_fifohead = m_fifohead->next ;
			if( m_fifohead==NULL ) {
				m_fifotail=NULL;
			}
			m_fifosize--;
		}
        unlock();
		return head ;
	}

	void fifoclean() {
		struct frame_t * f ;
        lock();
		while( (f=fifogetframe())!=NULL ) {
			delete f->framedata ;
			delete f ;
		}
        unlock();
	}

public:
	dvrplaybackstream(char * servername, int channel);	// open dvr server stream for preview
	~dvrplaybackstream();

	void playbackthread();							// playback thread to cache frame data

	int openstream();						// open stream mode play back
	char * openshare();						// open file share play back, return shared root directory

	int seek( struct dvrtime * dvrt ); 		// seek to specified time.
//	int prev_keyframe();						// seek to previous key frame position.
//	int next_keyframe();						// seek to next key frame position.
	int getdata(char **framedata, int * framesize, int * frametype);	// get dvr data frame
	int gettime( struct dvrtime * dvrt );	// get current time and timestamp
	int getserverinfo(struct player_info *ppi) ;	// get server information
	int getchannelinfo(struct channel_info * pci);	// get stream channel information
	int getdayinfo(dvrtime * daytime);							// get stream day data availability information
	int gettimeinfo( struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize); // get data availability information
	int getlockfileinfo(struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize );					// get locked file availability information
};

#endif	// __DVRPLAYBACKSTREAM_H__