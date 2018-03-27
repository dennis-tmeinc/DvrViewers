#ifndef _VOUT_VECTOR_H
#define _VOUT_VECTOR_H
#include <vector>

class CVideoOutput;

struct vout_set {
	HWND hwnd;
	int width, height;
	CVideoOutput *vout;
	int input_pts_delay; /* for CVideoOutput */
	int input_rate; /* for CVideoOutput */
	int input_state; /* for CVideoOutput */
	int b_input_stepplay; /* for CVideoOutput */
};

extern std::vector<struct vout_set> g_voutset;

void voutSetInit();
void voutSetClear();
struct vout_set *getVoutSetForWindowHandle(HWND hwnd, vout_set *vs);
void setVoutSetForWindowHandle(struct vout_set vs);
void removeVoutForWindowHandle(HWND hwnd);
void setVoutStepPlayForWindowHandle(HWND hwnd, bool stepplay);
void setVoutPtsDelayForWindowHandle(HWND hwnd, int delay);
void setVoutRateForWindowHandle(HWND hwnd, int rate);
void setVoutStateForWindowHandle(HWND hwnd, int state);

#endif
