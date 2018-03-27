#ifndef _VIDEO_CLIP_H
#define _VIDEO_CLIP_H

#define PWSIZE 256
enum {FILETYPE_UNKNOWN, FILETYPE_DVR, FILETYPE_AVI};

// this struct is used to transfer parameter to dvr saving thread
class dvrplayer;
struct dvrfile_save_info {
	dvrplayer *player;
	struct dvrtime * begintime ;
	struct dvrtime * endtime ;
	char * duppath ;
	int    flags;
	struct dup_state * dupstate ;
	char   password[PWSIZE] ;
	HANDLE hRunEvent ;
	int filetype;
};

unsigned WINAPI dvrfile_save_thread(void *p);

#endif