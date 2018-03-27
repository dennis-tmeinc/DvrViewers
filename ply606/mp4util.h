#ifndef _MP4UTIL_H_
#define _MP4UTIL_H_

#include "block.h"

block_t *getMpegFrameDataFromPacket(PBYTE buf, int *size, char *vosc, int voscSize, int ch);
block_t *getMpegFrameDataFromPacket2(PBYTE buf, int size);

void parse_video_object_layer(unsigned char *d, int len,
															DWORD *vop_time_increment_resolution, DWORD *vop_time_increment,
															DWORD *width, DWORD *height);
inline bool isVideoObjectStartCode(PBYTE p) {
  return ((p[0] == 0) &&
		(p[1] == 0) &&
		(p[2] == 1) &&
		(p[3] >= 0x00 && p[3] <= 0x1F));
}

#endif