#ifndef _VIDEO_CLIP_H
#define _VIDEO_CLIP_H

#include "../ply301.h"

#ifndef DVRFILE_SAVE_INFO
#define DVRFILE_SAVE_INFO

enum {FILETYPE_UNKNOWN, FILETYPE_DVR, FILETYPE_AVI};

// this struct is used to transfer parameter to dvr saving thread
struct dvrfile_save_info {
	dvrplayer *player;
	struct dvrtime * begintime ;
	struct dvrtime * endtime ;
	char * duppath ;
	int    flags;
	struct dup_state * dupstate ;
	char *password;
	HANDLE hRunEvent ;
	char *channels;
};

unsigned WINAPI dvrfile_save_thread(void *p);

#endif

#endif