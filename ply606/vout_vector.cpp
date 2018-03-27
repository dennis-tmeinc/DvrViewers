#include "stdafx.h"
#include "vout_vector.h"
#include "tme_thread.h"

std::vector<struct vout_set> g_voutset;
tme_object_t g_voutipc;
tme_mutex_t g_voutlock;

void voutSetInit()
{
	tme_thread_init(&g_voutipc);
    tme_mutex_init(&g_voutipc, &g_voutlock);
}

void voutSetClear()
{
    tme_mutex_destroy(&g_voutlock);
	tme_mutex_destroy(&g_voutipc.object_lock);
	tme_cond_destroy(&g_voutipc.object_wait);
}

struct vout_set *getVoutSetForWindowHandle(HWND hwnd, struct vout_set *vs)
{
	int i;
	struct vout_set *pvs = NULL;

	tme_mutex_lock(&g_voutlock);
	for (i = 0; i < g_voutset.size(); i++) {
		if (hwnd == g_voutset[i].hwnd) {
			pvs = &g_voutset[i];
			*vs = g_voutset[i];
			break;
		}
	}
	tme_mutex_unlock(&g_voutlock);

	return pvs;
}

void setVoutSetForWindowHandle(struct vout_set vs)
{
	int i;
	bool found = false;

	// find the hwnd if it already exists
	tme_mutex_lock(&g_voutlock);
	for (i = 0; i < g_voutset.size(); i++) {
		if (vs.hwnd == g_voutset[i].hwnd) {
			g_voutset[i] = vs;
			found = true;
			break;
		}
	}

	// not found, add one
	if (!found) g_voutset.push_back(vs);

	tme_mutex_unlock(&g_voutlock);

	return;
}

void removeVoutForWindowHandle(HWND hwnd)
{
	std::vector<struct vout_set>::iterator iter;

	tme_mutex_lock(&g_voutlock);
	for (iter = g_voutset.begin(); iter != g_voutset.end(); iter++) {
		if (hwnd == iter->hwnd) {
			g_voutset.erase(iter);
			break;
		}
	}
	tme_mutex_unlock(&g_voutlock);

	return;
}

void setVoutStepPlayForWindowHandle(HWND hwnd, bool stepplay)
{
	int i;

	// find the hwnd if it already exists
	tme_mutex_lock(&g_voutlock);
	for (i = 0; i < g_voutset.size(); i++) {
		if (hwnd == g_voutset[i].hwnd) {
			g_voutset[i].b_input_stepplay = stepplay;
			break;
		}
	}

	tme_mutex_unlock(&g_voutlock);

	return;
}

void setVoutPtsDelayForWindowHandle(HWND hwnd, int delay)
{
	int i;

	// find the hwnd if it already exists
	tme_mutex_lock(&g_voutlock);
	for (i = 0; i < g_voutset.size(); i++) {
		if (hwnd == g_voutset[i].hwnd) {
			g_voutset[i].input_pts_delay = delay;
			break;
		}
	}

	tme_mutex_unlock(&g_voutlock);

	return;
}

void setVoutRateForWindowHandle(HWND hwnd, int rate)
{
	int i;

	// find the hwnd if it already exists
	tme_mutex_lock(&g_voutlock);
	for (i = 0; i < g_voutset.size(); i++) {
		if (hwnd == g_voutset[i].hwnd) {
			g_voutset[i].input_rate = rate;
			break;
		}
	}

	tme_mutex_unlock(&g_voutlock);

	return;
}

void setVoutStateForWindowHandle(HWND hwnd, int state)
{
	int i;

	// find the hwnd if it already exists
	tme_mutex_lock(&g_voutlock);
	for (i = 0; i < g_voutset.size(); i++) {
		if (hwnd == g_voutset[i].hwnd) {
			g_voutset[i].input_state = state;
			break;
		}
	}

	tme_mutex_unlock(&g_voutlock);

	return;
}
