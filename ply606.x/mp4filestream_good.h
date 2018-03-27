
#ifndef __mp4filestream_H__
#define __mp4filestream_H__

#include "ply606.h"
#include "crypt.h"
#include "mpeg4.h"
#include "block.h"

class mp4filestream : public dvrstream{ 
protected:

	int m_channel ;						// stream channel
	FILE * m_file ;						// file handle
	int		m_totalchannel;				// total channel

	int m_maxframesize ;				// used for get preivew frame

	struct dvrtime m_begintime ;
	int            m_filelen ;
	__int64		   m_begintimestamp ;


	struct dvrtime m_curtime ;			// current playing time
	CMpegPs m_ps;

	// extra information
	int		m_version[4] ;				// file version
	char	m_servername[12] ;			// Server name of the original data
	char	m_cameraname[256] ;			// stream camera name
	struct mp5_header m_fileHeader;

	// file encryption support, RC4 frame data
	unsigned char m_crypt_table[CRYPT_TABLE_SIZE] ;
	int m_encryptmode;	// 0: no encryption, 1: Frame data first 1k bytes RC4
	void RC4_crypt( unsigned char * text, int textsize ) {
		RC4_block_crypt( text, textsize, 0, m_crypt_table, sizeof(m_crypt_table));
	}
	// generate cryption table
	void RC4_gentable(char * password) {
		unsigned char k256[256] ;
		key_256(password, k256);
		RC4_crypt_table( m_crypt_table, sizeof(m_crypt_table), k256);
	}

public:
	mp4filestream(char * filename, int channel);	// open dvr file stream for playback
	~mp4filestream();

	int seek( struct dvrtime * dvrt ); 		// seek to specified time.
	int gettime( struct dvrtime * dvrt );	// get current stream time, (precise to miliseconds)
	int getserverinfo(struct player_info *ppi) ;						// get server information
	int getchannelinfo(struct channel_info * pci);						// get stream channel information
	int getdayinfo(dvrtime * daytime);		// get stream day data availability information
	int gettimeinfo( struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize); // get data availability information
	virtual block_t *getdatablock();	
	void setpassword(int passwdtype, void * password, int passwdsize );
};

#endif	// __mp4filestream_H__