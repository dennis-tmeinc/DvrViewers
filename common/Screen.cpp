// Player.cpp : implementation file
//

#include "stdafx.h"

#include <math.h>

#include "cwin.h"
#include "cstr.h"
#include "dvrclient.h"
#include "dvrclientdlg.h"
#include "decoder.h"
#include "Screen.h"

#define MIN_ZOOM_SIZE (0.01)

// Screen
Image * Screen::m_backgroundimg;
int     Screen::m_count = 0;

Screen::Screen(HWND hparent)
{
	m_audio = 1;		// turn on audio by default
	m_decoder = NULL;
	m_channel = -1;
	m_xchannel = -1;
	m_attached = 0;
	memset(&m_channelinfo, 0, sizeof(m_channelinfo));
	m_channelinfo.size = sizeof(m_channelinfo);
	clearcache();
	m_count++;
	if (m_backgroundimg == NULL) {
		m_backgroundimg = loadbitmap(_T("BACKGROUND"));
	}
	HWND hwnd = CreateWindowEx(0,
		WinClass(),
		_T("SCREEN"),
		WS_CHILD | WS_CLIPCHILDREN,
		CW_USEDEFAULT,          // default horizontal position  
		CW_USEDEFAULT,          // default vertical position    
		CW_USEDEFAULT,          // default width                
		CW_USEDEFAULT,          // default height    
		hparent,
		NULL,
		AppInst(),
		this);
	attach(hwnd);

	// create tooltip window
	m_hwndToolTip = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL,
		WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		NULL, NULL, AppInst(), NULL);

	m_blurshape = 0;      // rectangle
	m_blurcursor = LoadCursor(ResInst(), _T("CURSORHANDPOINT"));
	m_ba_number = 0;

	// zoom in feature

	// polygons  (ROC? AOI?)
	m_num_polygon = 0;
	m_polygon = NULL;

	m_mouse_op = MOUSE_OP_NONE;

	m_rotate_degree = 0;
}

Screen::~Screen()
{
	DetachDecoder();
	if (--m_count == 0 && m_backgroundimg) {
		delete m_backgroundimg;
		m_backgroundimg = NULL;
	}
	if (IsWindow(m_hwndToolTip)) {
		DestroyWindow(m_hwndToolTip);
	}

	// polygons  (ROC? AOI?)
	if (m_polygon != NULL) {
		delete[] m_polygon;
	}

	if (m_hWnd) {
		::DestroyWindow(m_hWnd);
	}
}

void Screen::clearcache()
{
	int i;
	memset(&m_dayinfocache_year, 0, sizeof(m_dayinfocache_year));		// clear month info cache
	memset(&m_dayinfocache, 0, sizeof(m_dayinfocache));
	for (i = 0; i < TIMEBARCACHESIZE; i++) {
		m_timebarcache[i].clean();
	}
	m_nexttimebarcache = 0;
}

int Screen::getchinfo()
{
	memset(&m_channelinfo, 0, sizeof(m_channelinfo));
	m_channelinfo.size = sizeof(m_channelinfo);
	m_decoder->getchannelinfo(m_channel, &m_channelinfo);
	if (g_ratiox == 0) {
		if (m_channelinfo.xres < 100 || m_channelinfo.xres > 3840) {	// 3840 is 4k UHD, consider it to be the maximum valid value for now.
			if (m_get_chinfo_retry > 0) {
				m_get_chinfo_retry--;
				SetTimer(m_hWnd, TIMER_CHANNEL_INFO, 500, NULL);
			}
		}
		else {
			// g_maindlg->SetScreenFormat();
			PostMessage(g_maindlg->getHWND(), WM_SETSCREENFORMAT, NULL, NULL);
		}
	}
	return 0;
}

int Screen::attach_channel(decoder * pdecoder, int channel)
{
	if (pdecoder && pdecoder->attach(channel, m_hWnd) >= 0) {
		m_decoder = pdecoder;
		m_channel = channel;
		m_get_chinfo_retry = 10;
		getchinfo();
		m_attached = 1;
		setaudio(m_audio);

		HWND hsurface = FindWindowEx(m_hWnd, NULL, NULL, NULL);
		if (hsurface != NULL) {
			m_surface.attach(hsurface);
		}

		return 1;
	}
	return 0;
}

int Screen::AttachDecoder(decoder * pdecoder, int channel)
{
	if (m_attached && m_decoder == pdecoder && m_channel == channel) {
		return 1;
	}

	DetachDecoder();

	m_mouse_op = MOUSE_OP_NONE;

	m_ba_number = 0;         // clear blurring area

	// clear zoom in area
	m_zoomarea.x = 0.0;
	m_zoomarea.y = 0.0;
	m_zoomarea.z = 1.0;

	m_rotate_degree = 0;

	memset(&m_channelinfo, 0, sizeof(m_channelinfo));
	m_channelinfo.size = sizeof(m_channelinfo);

	pdecoder->getchannelinfo(channel, &m_channelinfo);
	if ((m_channelinfo.features & 1) != 0) {
		if (isVisible()) {
			if (attach_channel(pdecoder, channel)) {
				return 1;
			}
		}
		else {
			// attached as hided mode
			m_decoder = pdecoder;
			m_channel = channel;
			m_attached = 0;
			return 0;
		}
	}

	// failed
	m_decoder = NULL;
	m_channel = -1;
	m_attached = 0;
	return -1;

	/*
	if( pdecoder->attach(channel, m_hWnd)>=0 ){
	m_decoder=pdecoder ;
	m_channel=channel ;
	m_attached=1 ;
	setaudio(1);		// assume to turn on audio for all new attached channel
	m_channelinfo.size = sizeof( m_channelinfo );
	pdecoder->getchannelinfo( channel, &m_channelinfo );
	return 0 ;
	}
	else {
	m_attached=0;
	return -1;
	}
	*/
}

int Screen::DetachDecoder()
{
	if (m_decoder && m_attached) {
		m_decoder->detachwindow(m_hWnd);
		InvalidateRect(m_hWnd, NULL, FALSE);
	}
	memset(&m_channelinfo, 0, sizeof(m_channelinfo));
	clearcache();
	m_attached = 0;
	m_decoder = NULL;
	m_channel = -1;
	m_ba_number = 0;         // clear blurring area
	return 0;
}

// for PLY606 delayed player window
LRESULT Screen::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == TIMER_BLUR_AGAIN) {
		if (m_ba_number > 0) {
			m_decoder->setblurarea(m_channel, m_blur_area, m_ba_number);
		}
	}
	else if (nIDEvent == TIMER_CHANNEL_INFO) {
		getchinfo();
	}
	KillTimer(m_hWnd, nIDEvent);
	return 0;
}

int Screen::Show()
{
	ShowWindow(m_hWnd, SW_SHOW);
	if (m_decoder == NULL)
		RedrawWindow(m_hWnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
	if (m_decoder && m_attached == 0 && m_channel >= 0) {
		if (attach_channel(m_decoder, m_channel)) {
			if (m_ba_number > 0) {
				m_decoder->setblurarea(m_channel, m_blur_area, m_ba_number);
				// this call failed on PLY606, so redo it after 3 seconds
				SetTimer(m_hWnd, TIMER_BLUR_AGAIN, 3000, NULL);
			}
		}
		else {
			m_decoder = NULL;
			m_attached = 0;
		}
	}
	return 1;
}

int Screen::Hide()
{
	if (m_decoder && m_attached && m_channel >= 0) {
		m_decoder->detachwindow(m_hWnd);
		m_attached = 0;
	}
	ShowWindow(m_hWnd, SW_HIDE);
	return 0;
}

int  Screen::selectfocus()
{
	if (m_decoder && m_attached && m_channel >= 0) {
		m_decoder->selectfocus(m_hWnd);
	}
	return 0;
}

int  Screen::setaudio(int onoff)
{
	m_audio = onoff;
	if (m_decoder && m_attached) {
		if (m_decoder->audioonwindow(m_hWnd, m_audio) < 0) {
			m_decoder->audioon(m_channel, m_audio);
		}
	}
	return m_audio;
}

void Screen::suspend()
{
	if (isVisible() && m_decoder && m_attached && m_channel >= 0) {
		m_decoder->detachwindow(m_hWnd);
		m_attached = 0;
	}
}

void Screen::resume()
{
	if (isVisible() && m_decoder && m_attached == 0 && m_channel >= 0) {
		attach_channel(m_decoder, m_channel);
	}
}

void Screen::reattach()
{
	suspend();
	resume();
}

// return index in timebar cache
int Screen::gettimeinfo(struct dvrtime * day)
{
	int i;
	struct dvrtime begintime, endtime;
	int bday = day->year * 10000 + day->month * 100 + day->day;

	for (i = 0; i < TIMEBARCACHESIZE; i++) {
		if (bday == m_timebarcache[i].m_day) {
			return i;
		}
	}

	m_nexttimebarcache++;
	if (m_nexttimebarcache >= TIMEBARCACHESIZE) {
		m_nexttimebarcache = 0;
	}

	m_timebarcache[m_nexttimebarcache].clean();
	m_timebarcache[m_nexttimebarcache].m_day = bday;

	begintime.year = endtime.year = day->year;
	begintime.month = endtime.month = day->month;
	begintime.day = endtime.day = day->day;

	begintime.hour = 0;
	begintime.min = 0;
	begintime.second = 0;
	begintime.millisecond = 0;
	begintime.tz = 0;

	endtime.hour = 23;
	endtime.min = 59;
	endtime.second = 59;
	endtime.millisecond = 999;
	endtime.tz = 0;

	int infosize;
	infosize = m_decoder->getcliptimeinfo(m_channel, &begintime, &endtime, NULL, 0);
	if (infosize > 0) {
		m_timebarcache[m_nexttimebarcache].m_timeinfosize = infosize;
		m_timebarcache[m_nexttimebarcache].m_cliptimeinfo = new struct cliptimeinfo[infosize];
		m_decoder->getcliptimeinfo(m_channel, &begintime, &endtime, m_timebarcache[m_nexttimebarcache].m_cliptimeinfo, infosize);
	}
	infosize = m_decoder->getlockfiletimeinfo(m_channel, &begintime, &endtime, NULL, 0);
	if (infosize > 0) {
		m_timebarcache[m_nexttimebarcache].m_lockinfosize = infosize;
		m_timebarcache[m_nexttimebarcache].m_locktimeinfo = new struct cliptimeinfo[infosize];
		m_decoder->getlockfiletimeinfo(m_channel, &begintime, &endtime, m_timebarcache[m_nexttimebarcache].m_locktimeinfo, infosize);
	}
	return m_nexttimebarcache;
}

int Screen::getcliptime(struct dvrtime * day, struct cliptimeinfo ** pclipinfo)
{
	int i;
	if (m_decoder == NULL) {
		return 0;
	}
	i = gettimeinfo(day);
	*pclipinfo = m_timebarcache[i].m_cliptimeinfo;
	return m_timebarcache[i].m_timeinfosize;
}

int Screen::getlocktime(struct dvrtime * day, struct cliptimeinfo ** plockinfo)
{
	int i;
	if (m_decoder == NULL) {
		return 0;
	}
	i = gettimeinfo(day);
	*plockinfo = m_timebarcache[i].m_locktimeinfo;
	return m_timebarcache[i].m_lockinfosize;
}

int Screen::getdayinfo(struct dvrtime * daytime)
{
	int day;
	int month = daytime->month - 1;
	if (month < 0 || month >= 12) return 0;
	if (m_decoder) {
		if (m_dayinfocache_year[month] != daytime->year) {		// miss cache
			m_dayinfocache_year[month] = daytime->year;
			m_dayinfocache[month] = 0;
			struct dvrtime t = *daytime;
			for (day = 1; day <= 31; day++) {
				t.day = day;
				if (m_decoder->getclipdayinfo(m_channel, &t) > 0) {
					m_dayinfocache[month] |= 1 << (day - 1);
				}
			}
		}
		return (m_dayinfocache[month] & (1 << (daytime->day - 1))) != 0;
	}
	return 0;
}

// Screen message handlers
LRESULT Screen::OnPaint()
{
	PAINTSTRUCT ps;
	HDC hdc;
	hdc = BeginPaint(m_hWnd, &ps);

	int refreshed = 0;
	if (m_attached && m_decoder != NULL) {
		refreshed = m_decoder->refresh(m_channel) >= 0;
	}

	if (!refreshed && m_backgroundimg) {
		Graphics g(hdc);
		g.DrawImage(m_backgroundimg, 0, 0, m_width, m_height);
	}

	EndPaint(m_hWnd, &ps);
	return 0;
}

LRESULT Screen::OnEraseBkgnd(HDC hdc)
{
	return 1;		// no need to erase background
}

void Screen::showTooltip(char * msg)
{
	// Tooltip test, Set up "tool" information. In this case, the "tool" is the entire parent window.
	TOOLINFO ti = { 0 };
	ti.cbSize = sizeof(TOOLINFO);
	ti.uFlags = TTF_TRANSPARENT | TTF_CENTERTIP;
	ti.hwnd = m_hWnd;
	ti.hinst = AppInst();
	string mmsg = msg;
	ti.lpszText = mmsg;

	GetClientRect(m_hWnd, &ti.rect);

	// Associate the tooltip with the "tool" window.
	//SendMessage(m_hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)(LPTOOLINFO)&ti);
	//SendMessage(m_hwndToolTip, TTM_POPUP, 0, 0);

}

LRESULT Screen::OnLButtonDblClk(UINT nFlags, int x, int y)
{
	StopBlurArea();

	g_maindlg->ScreenDblClk(this);
	return 0;
}

void Screen::AddBlurArea(int shape)
{
	if (m_decoder && m_ba_number < MAX_BLUR_AREA) {
		m_blurshape = shape;
		SetCapture(m_hWnd);
		m_ba_number++;
		m_mouse_op = MOUSE_OP_BLUR;
	}
}

void Screen::StopBlurArea()
{
	m_mouse_op = MOUSE_OP_NONE;
	if (GetCapture() == m_hWnd) {
		SetCapture(NULL);
	}
}

void Screen::ClearBlurArea()
{
	StopBlurArea();
	m_ba_number = 0;
	if (m_decoder) {
		m_decoder->clearblurarea(m_channel);
	}
}

// draw rectangle onto decoder screen
void Screen::drawzoomarea()
{
	if (m_attached && m_decoder && m_channel >= 0) {
		float z = m_zoomdraw.z;
		if (z < 0.001) z = 0.0;
		selectfocus();
		m_decoder->setdrawcolor(m_channel, RGB(30, 192, 128));
		m_decoder->drawrectangle(m_channel, m_zoomdraw.x, m_zoomdraw.y, z, z);
	}
}

// zoom in / out ( x, y, z are related to current screen, if z==0, stop zooming)
int Screen::ShowZoomarea(float x, float y, float z)
{
	float t;
	// rotation adjust
	if (m_rotate_degree == 90) {
		t = y;
		y = 1.0 - z - x;
		x = t;
	}
	else if (m_rotate_degree == 180) {
		x = 1.0 - z - x;
		y = 1.0 - z - y;
	}
	else if (m_rotate_degree == 270) {
		t = x;
		x = 1.0 - z - y;
		y = t;
	}

	if (z < 0.0001 || z > 10.0) {
		z = 10.0;		// to stop zoom in
	}
	else {
		// new zoomin size
		z *= m_zoomarea.z;
	}
	if (z < MIN_ZOOM_SIZE) {
		return -1;
	}
	else {
		m_zoomarea.x += x * m_zoomarea.z;
		m_zoomarea.y += y * m_zoomarea.z;
		m_zoomarea.z = z;

		if (m_zoomarea.x < 0.0) {
			m_zoomarea.x = 0.0;
		}
		else if (m_zoomarea.x + m_zoomarea.z > 1.0) {
			m_zoomarea.x = 1.0 - m_zoomarea.z;
		}

		if (m_zoomarea.y < 0.0) {
			m_zoomarea.y = 0.0;
		}
		else if (m_zoomarea.y + m_zoomarea.z > 1.0) {
			m_zoomarea.y = 1.0 - m_zoomarea.z;
		}
	}
	if (m_attached && m_decoder != NULL) {
		m_decoder->clearlines(m_channel);
		selectfocus();
		if (m_zoomarea.z >= 1.0) {			// full zoom out
			m_zoomarea.x = 0.0;
			m_zoomarea.y = 0.0;
			m_zoomarea.z = 1.0;
		}
		return m_decoder->showzoomin(m_channel, &m_zoomarea);
	}
	else {
		return -1;
	}
}

void   Screen::StartZoom()
{
	if (m_attached && m_decoder != NULL && m_decoder->supportzoomin()) {
		m_decoder->clearlines(m_channel);
		m_mouse_op = MOUSE_OP_ZOOM;
	}
}

void   Screen::StopZoom()
{
	if (m_mouse_op == MOUSE_OP_ZOOM || m_zoomarea.z < 1.0) {
		selectfocus();
		ShowZoomarea(0, 0, 0);		// reset zoom area
		m_mouse_op = MOUSE_OP_NONE;
	}
}

// update polygons to screen
void Screen::UpdatePolygon()
{
	if (isattached()) {
		m_decoder->clearlines(m_channel);
		if (m_polygon != NULL) {
			int po = m_num_polygon;
			if (m_mouse_op == MOUSE_OP_AOI) {
				po++;
			}
			for (int l = 0; l < po; l++) {
				if (m_polygon[l].points > 0) {
					int colors[3];
					colors[0] = 20;
					colors[1] = 20;
					colors[2] = 20;
					int c = l % 3;
					if (l & 1) {
						colors[c] += 192;
						c = (c + 1) % 3;
						colors[c] += 192;
					}
					else {
						colors[c] += 192;
					}
					m_decoder->setdrawcolor(m_channel, RGB(colors[0], colors[1], colors[2]));
					//m_decoder->setdrawcolor(m_channel, m_polygon[l].color);
					int p;
					for (p = 0; p < m_polygon[l].points - 1; p++) {
						m_decoder->drawline(m_channel,
							m_polygon[l].poly[p].x,
							m_polygon[l].poly[p].y,
							m_polygon[l].poly[p + 1].x,
							m_polygon[l].poly[p + 1].y);
					}
					if (l == m_num_polygon) {		// draw tail line
						if (m_polygon[l].closeable()) {
							m_decoder->setdrawcolor(m_channel, RGB(255, 224, 224));
						}
						m_decoder->drawline(m_channel,
							m_polygon[l].poly[p].x,
							m_polygon[l].poly[p].y,
							m_polygon[l].poly[p + 1].x,
							m_polygon[l].poly[p + 1].y);
					}
				}
			}
		}
	}
}

void   Screen::StartROC()
{
	if (m_polygon == NULL) {
		m_polygon = new AOI_polygon[MAX_POLYGON_NUMBER];
		m_num_polygon = 1;			// use first AOI as ROC
	}
	if (m_polygon)
		m_mouse_op = MOUSE_OP_ROC;
}

void   Screen::StopROC()
{
	m_mouse_op = MOUSE_OP_NONE;
}

void   Screen::StartAOI()
{
	if (m_polygon == NULL) {
		m_polygon = new AOI_polygon[MAX_POLYGON_NUMBER];
		m_num_polygon = 1;
	}

	if (m_num_polygon < MAX_POLYGON_NUMBER) {
		m_polygon[m_num_polygon].points = 0;
		m_mouse_op = MOUSE_OP_AOI;
	}
	else {
		MessageBox(m_hWnd, _T("Maximum AOI number reached"), NULL, MB_OK | MB_ICONSTOP);
	}
}

void   Screen::StopAOI()
{
	if (m_num_polygon > 0 && m_num_polygon < MAX_POLYGON_NUMBER) {
		//m_num_polygon++;		// to be removed ;
		m_polygon[m_num_polygon].points = 0;
	}
	m_mouse_op = MOUSE_OP_NONE;
	UpdatePolygon();
}

void  Screen::RemoveAOI(int idx)
{
	if (idx >= 0 && idx < m_num_polygon && m_polygon != NULL) {
		m_polygon[idx].points = 0;
		if (idx == (m_num_polygon - 1))
			m_num_polygon = idx;
		UpdatePolygon();
	}
}

void   Screen::ClearAOI()
{
	m_num_polygon = 0;
	if (m_polygon != NULL) {
		delete[] m_polygon;
		m_polygon = NULL;
	}
	UpdatePolygon();
}

inline double cJSON_GetFloat(cJSON * j, char * vname, double defaultvalue = 0.0)
{
	cJSON *c;
	if (j) {
		c = cJSON_GetObjectItem(j, vname);
		if (c) {
			return (float)(c->valuedouble);
		}
	}
	return defaultvalue;
}

inline char * cJSON_GetString(cJSON * j, char * vname)
{
	cJSON *c;
	if (j) {
		c = cJSON_GetObjectItem(j, vname);
		if (c) {
			return c->valuestring;
		}
	}
	return NULL;
}

void   Screen::LoadAOI()
{
	cJSON * j_mdu = NULL;
	cJSON * j_sensor = NULL;

	// read from json file
	char * fbuf = new char[1000000];
	FILE *faoi = fopen("r:\\aoi.json", "r");
	int r = 0;
	if (faoi) {
		r = fread(fbuf, 1, 999999, faoi);
		fclose(faoi);
	}
	if (r > 0) {
		j_mdu = cJSON_Parse(fbuf);
		if (j_mdu) {
			cJSON *j_rooms = cJSON_GetObjectItem(j_mdu, "Rooms");
			if (j_rooms && cJSON_GetArraySize(j_rooms) > 0) {
				cJSON * j_room = cJSON_GetArrayItem(j_rooms, 0);
				cJSON * j_sensors = cJSON_GetObjectItem(j_room, "Sensors");
				if (j_sensors && cJSON_GetArraySize(j_sensors) > 0) {
					j_sensor = cJSON_GetArrayItem(j_sensors, 0);
				}
			}
		}
	}
	delete fbuf;

	if (j_sensor) {
		// init AOI from cJSON j_sensor
		ClearAOI();
		StartAOI();
		m_mouse_op = MOUSE_OP_NONE;

		// init ROC
		cJSON * j_ROC = cJSON_GetObjectItem(j_sensor, "ROC");
		if (j_ROC) {
			float top = cJSON_GetFloat(j_ROC, "Top", 0.0);
			float left = cJSON_GetFloat(j_ROC, "Left", 0.0);
			float right = cJSON_GetFloat(j_ROC, "Right", 1.0);
			float bottom = cJSON_GetFloat(j_ROC, "Bottom", 1.0);

			m_polygon[0].points = 0;
			m_polygon[0].add(left, top);
			m_polygon[0].add(right, top);
			m_polygon[0].add(right, bottom);
			m_polygon[0].add(left, bottom);
			m_polygon[0].add(left, top);
		}

		// init AOI
		cJSON * a_AOI = cJSON_GetObjectItem(j_sensor, "AOI");
		int s_AOI = cJSON_GetArraySize(a_AOI);
		for (int ia = 0; ia < s_AOI; ia++) {
			cJSON * aoi = cJSON_GetArrayItem(a_AOI, ia);
			if (aoi) {
				cJSON * points = cJSON_GetObjectItem(aoi, "Points");
				if (points) {
					AOI_polygon * polygon = &m_polygon[m_num_polygon];
					polygon->points = 0;
					polygon->name = cJSON_GetString(aoi, "Name");

					int s_p = cJSON_GetArraySize(points);
					for (int ip = 0; ip < s_p; ip++) {
						cJSON * point = cJSON_GetArrayItem(points, ip);
						if (point) {
							polygon->add(cJSON_GetFloat(point, "x"),
								cJSON_GetFloat(point, "y"));
						}
					}

					m_num_polygon++;
				}
			}
		}
	}

	if (j_mdu)
		cJSON_Delete(j_mdu);

	UpdatePolygon();
}

void   Screen::SaveAOI()
{
	cJSON * j_mdu = NULL;
	cJSON * j_sensor = NULL;

	// read from json file
	char * fbuf = new char[1000000];
	FILE *faoi = fopen("r:\\aoi.json", "r");
	int r = 0;
	if (faoi) {
		r = fread(fbuf, 1, 999999, faoi);
		fclose(faoi);
	}
	if (r > 0) {
		j_mdu = cJSON_Parse(fbuf);
		if (j_mdu == NULL) {
			j_mdu = cJSON_CreateObject();
		}
		cJSON *j_rooms = cJSON_GetObjectItem(j_mdu, "Rooms");
		if (j_rooms == NULL) {
			j_rooms = cJSON_CreateArray();
			cJSON_AddItemToObject(j_mdu, "Rooms", j_rooms);
		}
		if (cJSON_GetArraySize(j_rooms) > 0) {
			cJSON * j_room = cJSON_GetArrayItem(j_rooms, 0);
			cJSON * j_sensors = cJSON_GetObjectItem(j_room, "Sensors");
			if (j_sensors && cJSON_GetArraySize(j_sensors) > 0) {
				j_sensor = cJSON_GetArrayItem(j_sensors, 0);
			}
		}
	}
	delete fbuf;

	if (j_sensor) {

		// add ROC
		cJSON * j_ROC = cJSON_GetObjectItem(j_sensor, "ROC");
		if (j_ROC) {
			cJSON_DeleteItemFromObject(j_sensor, "ROC");
		}
		j_ROC = cJSON_CreateObject();

		float Left = m_polygon[0].poly[0].x;
		float Top = m_polygon[0].poly[0].y;
		float Right = m_polygon[0].poly[2].x;
		float Bottom = m_polygon[0].poly[2].y;
		cJSON_AddNumberToObject(j_ROC, "Top", Top);
		cJSON_AddNumberToObject(j_ROC, "Left", Left);
		cJSON_AddNumberToObject(j_ROC, "Right", Right);
		cJSON_AddNumberToObject(j_ROC, "Bottom", Bottom);
		cJSON_AddItemToObject(j_sensor, "ROC", j_ROC);

		// add AOI
		cJSON * a_AOI = cJSON_GetObjectItem(j_sensor, "AOI");
		if (a_AOI)
			cJSON_DeleteItemFromObject(j_sensor, "AOI");
		a_AOI = cJSON_CreateArray();
		for (int ia = 1; ia < m_num_polygon; ia++) {
			if (m_polygon[ia].points > 1) {
				cJSON * j_aoi = cJSON_CreateObject();
				cJSON_AddStringToObject(j_aoi, "Name", m_polygon[ia].name);
				cJSON * points = cJSON_CreateArray();
				for (int ip = 0; ip < m_polygon[ia].points; ip++) {
					cJSON * point = cJSON_CreateObject();
					cJSON_AddNumberToObject(point, "x", m_polygon[ia].poly[ip].x);
					cJSON_AddNumberToObject(point, "y", m_polygon[ia].poly[ip].y);
					cJSON_AddItemToArray(points, point);
				}
				cJSON_AddItemToObject(j_aoi, "Points", points);
				cJSON_AddItemToArray(a_AOI, j_aoi);
			}
		}
		cJSON_AddItemToObject(j_sensor, "AOI", a_AOI);

		char * jsontext = cJSON_Print(j_mdu);

		faoi = fopen("r:\\aoi_save.json", "w");
		if (faoi) {
			fputs(jsontext, faoi);
			fclose(faoi);
		}
	}

	if (j_mdu) {
		cJSON_Delete(j_mdu);
	}
}

// PTZ when zoom in enabled
int Screen::ZoomPTZ(int pan, int tilt, int zoomin)
{
	if (m_attached && m_decoder && m_decoder->supportzoomin()) {
		float x, y, z;
		x = 0.1 * (float)pan;
		y = 0.1 * (float)tilt;
		if (zoomin > 0) {
			x += 0.1;
			y += 0.1;
			z = 0.8;
		}
		else if (zoomin < 0) {
			x -= 0.125;
			y -= 0.125;
			z = 1.25;
		}
		else {		// zoomin = 0
			z = 1.0;
		}
		return ShowZoomarea(x, y, z);
	}
	return DVR_ERROR;
}

int Screen::setRotate(int degree)
{
	if (m_rotate_degree != degree && m_decoder != NULL && m_channel >= 0) {
		if (m_decoder->setrotation(m_channel, degree) == 0)
			m_rotate_degree = degree;
	}
	return 0;
}

int Screen::rotate()
{
	if (m_decoder != NULL && m_channel >= 0) {
		int deg = m_rotate_degree + 90;
		if (deg >= 360) {
			deg = 0;
		}
		return setRotate(deg);
	}
	return 0;
}

LRESULT Screen::OnMouseMove(UINT nFlags, int x, int y)
{
	if (m_decoder && m_attached) {
		if (m_mouse_op == MOUSE_OP_BLUR && nFlags == MK_LBUTTON) {		// left button down and move
			if (x < 0 ||
				x >= m_width ||
				y < 0 ||
				y >= m_height)
			{
				return 0;
			}

			int x1, x2, y1, y2;
			if (x > m_pointer_x) {
				x1 = m_pointer_x;
				x2 = x;
			}
			else {
				x2 = m_pointer_x;
				x1 = x;
			}
			if (y > m_pointer_y) {
				y1 = m_pointer_y;
				y2 = y;
			}
			else {
				y2 = m_pointer_y;
				y1 = y;
			}

			x1 = x1 * 256 / m_width;
			x2 = x2 * 256 / m_width;
			y1 = y1 * 256 / m_height;
			y2 = y2 * 256 / m_height;

			m_blur_area[m_ba_number - 1].x = x1;
			m_blur_area[m_ba_number - 1].y = y1;
			m_blur_area[m_ba_number - 1].w = x2 - x1;
			m_blur_area[m_ba_number - 1].h = y2 - y1;
			m_blur_area[m_ba_number - 1].radius = 5;
			m_blur_area[m_ba_number - 1].shape = m_blurshape;
			m_decoder->setblurarea(m_channel, m_blur_area, m_ba_number);
		}
		else if (m_mouse_op == MOUSE_OP_ZOOM && (nFlags == MK_LBUTTON)) {			// left button down and move
			// draw zoomin rectangle
			if (x < 0)
				x = 0;
			if (x >= m_width)
				x = m_width - 1;
			if (y < 0)
				y = 0;
			if (y >= m_height)
				y = m_height - 1;

			float dx, dy;
			m_zoomdraw.x = (float)m_pointer_x / (float)m_width;
			dx = (float)(x - m_pointer_x) / (float)m_width;
			m_zoomdraw.y = (float)m_pointer_y / (float)m_height;
			dy = (float)(y - m_pointer_y) / (float)m_height;
			if (fabs(dx) > fabs(dy)) {
				m_zoomdraw.z = fabs(dy);
			}
			else {
				m_zoomdraw.z = fabs(dx);
			}
			if (dx < 0.0) {
				m_zoomdraw.x -= m_zoomdraw.z;
			}
			if (dy < 0.0) {
				m_zoomdraw.y -= m_zoomdraw.z;
			}

			if (m_zoomdraw.z * m_zoomarea.z < MIN_ZOOM_SIZE) {
				m_zoomdraw.x = 0.0;
				m_zoomdraw.y = 0.0;
				m_zoomdraw.z = 0.0;
			}
			drawzoomarea();
		}
		else if (m_mouse_op == MOUSE_OP_ROC && nFlags == MK_LBUTTON) {
			// use first AOI as ROC
			if (m_polygon != NULL) {
				float x1, y1, x2, y2;
				x1 = (float)m_pointer_x / (float)m_width;
				y1 = (float)m_pointer_y / (float)m_height;
				x2 = (float)x / (float)m_width;
				y2 = (float)y / (float)m_height;
				m_polygon[0].points = 0;
				m_polygon[0].add(x1, y1);
				m_polygon[0].add(x2, y1);
				m_polygon[0].add(x2, y2);
				m_polygon[0].add(x1, y2);
				m_polygon[0].add(x1, y1);
				UpdatePolygon();
			}
		}
		else if (m_mouse_op == MOUSE_OP_AOI && m_polygon != NULL && m_num_polygon > 0) {
			float fx, fy;
			fx = (float)x / (float)m_width;
			fy = (float)y / (float)m_height;
			m_polygon[m_num_polygon].tail(fx, fy);
			UpdatePolygon();
		}
		else if ((nFlags == (MK_SHIFT | MK_LBUTTON)) && m_decoder->supportzoomin() && m_zoomarea.z < 1.0) {             // left button down and move
			float dx, dy;
			dx = (float)(m_pointer_x - x) / (float)m_width;
			dy = (float)(m_pointer_y - y) / (float)m_height;
			if (fabs(dx) > 0.001 || fabs(dy) > 0.001) {
				ShowZoomarea(dx, dy, 1.0);
				m_pointer_x = x;
				m_pointer_y = y;
			}
		}
	}

	return 0;
}

#ifdef IDD_DIALOG_AOI_NAME
// private class to enter AOI name
class AoiNameDialog : public Dialog {
protected:
	virtual int OnOK()
	{
		GetDlgItemText(m_hWnd, IDC_COMBO_AOINAME, aoiname.tcssize(500), 500);
		return EndDialog(IDOK);
	}

public:
	string aoiname;
	AoiNameDialog(HWND hparent, int id) {
		CreateDlg(IDD_DIALOG_AOI_NAME, hparent);
		aoiname.printf("AOI %d", id);
		SetDlgItemText(m_hWnd, IDC_COMBO_AOINAME, (LPCTSTR)aoiname);
	}
};

#endif	// IDD_DIALOG_AOI_NAME

LRESULT Screen::OnLButtonDown(UINT nFlags, int x, int y)
{
	m_pointer_x = x;
	m_pointer_y = y;
	if (m_mouse_op == MOUSE_OP_BLUR) {
		if (x < 0 ||
			x >= m_width ||
			y < 0 ||
			y >= m_height) {
			StopBlurArea();
		}
	}
	else if (m_mouse_op == MOUSE_OP_AOI) {
		if (m_polygon != NULL && m_num_polygon > 0) {
			float fx, fy;
			fx = (float)x / (float)m_width;
			fy = (float)y / (float)m_height;
			AOI_polygon * polygon = &m_polygon[m_num_polygon];
			if (polygon->points > 1) {
				polygon->tail(fx, fy);
				if (polygon->closeable()) {
					fx = polygon->poly[0].x;
					fy = polygon->poly[0].y;
					m_mouse_op = MOUSE_OP_NONE;
				}
			}
			polygon->add(fx, fy);
			if (m_mouse_op == MOUSE_OP_NONE) {
#ifdef IDD_DIALOG_AOI_NAME
				// open dialog to confirm new poly (AOI)
				AoiNameDialog d(m_hWnd, m_num_polygon);
				if (d.DoModal() == IDOK) {
					polygon->name = d.aoiname;
					m_num_polygon++;
				}
#endif
			}
			UpdatePolygon();
		}
	}
	else {
		SetCapture(m_hWnd);
		// reset draw 
		m_zoomdraw.z = 0.0;
	}

	g_maindlg->FocusPlayer(this);
	return 0;
}

LRESULT Screen::OnLButtonUp(UINT nFlags, int x, int y)
{
	if (GetCapture() == m_hWnd && m_mouse_op != MOUSE_OP_BLUR)
		SetCapture(NULL);

	if (m_mouse_op == MOUSE_OP_ZOOM) {
		if (m_zoomdraw.z > 0.001)
			ShowZoomarea(m_zoomdraw.x, m_zoomdraw.y, m_zoomdraw.z);
		// clear zoomin rectangle
		m_zoomdraw.z = 0.0;
		drawzoomarea();
	}
	else if (m_mouse_op == MOUSE_OP_ROC) {
		StopROC();
	}


	return 0;
}

LRESULT Screen::OnMButtonDown(UINT nFlags, int x, int y)
{
	m_pointer_x = x;
	m_pointer_y = y;
	if (m_mouse_op == MOUSE_OP_BLUR) {
		StopBlurArea();
	}
	g_maindlg->FocusPlayer(this);
	return 0;
}

LRESULT Screen::OnMButtonUp(UINT nFlags, int x, int y)
{
	return 0;
}

LRESULT Screen::OnMouseWheel(UINT delta, int x, int y)
{
	int ctl = (int)(short)LOWORD(delta);

	if (ctl == MK_SHIFT || (ctl == 0 && m_mouse_op == MOUSE_OP_ZOOM)) {
		POINT pt;
		pt.x = x;
		pt.y = y;
		ScreenToClient(m_hWnd, &pt);
		if (pt.x >= 0 && pt.x < m_width &&
			pt.y >= 0 && pt.y < m_height)
		{
			float dx, dy, z;
			if ((short int)HIWORD(delta) > 0) {
				z = 0.8;
			}
			else {
				z = 1.25;
			}
			dx = (1.0 - z) * (float)pt.x / (float)m_width;
			dy = (1.0 - z) * (float)pt.y / (float)m_height;
			ShowZoomarea(dx, dy, z);
			return 0;
		}
	}
	// not processed
	return 1;
}

LRESULT Screen::OnSetCursor()
{
	HCURSOR hcursor;
	if (m_mouse_op == MOUSE_OP_BLUR ||
		m_mouse_op == MOUSE_OP_AOI ||
		m_mouse_op == MOUSE_OP_ROC
		) {
		hcursor = LoadCursor(NULL, IDC_CROSS);
		SetCursor(hcursor);
	}
	else {
		hcursor = LoadCursor(NULL, IDC_ARROW);
		SetCursor(hcursor);
	}
	return 1;
}

LRESULT Screen::OnRButtonDown(UINT nFlags, int x, int y)
{
	return 0;
}

LRESULT Screen::OnRButtonUp(UINT nFlags, int x, int y)
{

	int cmd = 10000;		// start id
	int flag;
	int aspect_changed = 0;

	// try attache child windows 
	if (m_surface.getHWND() == NULL) {
		HWND hsurf = FindWindowEx(m_hWnd, NULL, NULL, NULL);
		if (hsurf) {
			m_surface.attach(hsurf);
		}
	}

	if (m_mouse_op == MOUSE_OP_BLUR) {
		StopBlurArea();
		return 0;
	}
	else if (m_mouse_op == MOUSE_OP_ROC) {
		StopROC();
	}
	else if (m_mouse_op == MOUSE_OP_AOI) {
		StopAOI();
	}

	if (nFlags != 0) return 0;

	g_maindlg->FocusPlayer(this);
	HMENU hMenuPop = CreatePopupMenu();
	HMENU hMenuScreenMode = CreatePopupMenu();
	HMENU hMenuROC = CreatePopupMenu();
	HMENU hMenuRotate = CreatePopupMenu();

	// Screen Format selection
	int i;

	int id_SCREEN_MODE = cmd;
	int id_SCREEN_MODE_END = 0;
	for (i = 0; i < screenmode_table_num; i++) {
		if (g_screenmode == i) {
			flag = MF_STRING | MF_CHECKED;
		}
		else {
			flag = MF_STRING;
		}
		AppendMenu(hMenuScreenMode, flag, id_SCREEN_MODE + i, screenmode_table[i].screenname);
	}
	if (g_screenmode >= i || g_screenmode < 0) {
		flag = MF_STRING | MF_CHECKED;
	}
	else {
		flag = MF_STRING;
	}
	AppendMenu(hMenuScreenMode, flag, id_SCREEN_MODE + i, _T("Automatic"));
	cmd = id_SCREEN_MODE + i + 1;
	id_SCREEN_MODE_END = cmd;

	AppendMenu(hMenuScreenMode, MF_SEPARATOR, NULL, NULL);
	if (g_maindlg->IsZoom()) {
		flag = MF_STRING | MF_CHECKED;
	}
	else {
		flag = MF_STRING;
	}
	int id_FULL_SCREEN = cmd++;
	AppendMenu(hMenuScreenMode, flag, id_FULL_SCREEN, _T("Full Screen"));

	AppendMenu(hMenuScreenMode, MF_SEPARATOR, NULL, NULL);
	// screen aspect ratios
	if (g_ratiox == 4 && g_ratioy == 3) {
		flag = MF_STRING | MF_CHECKED;
	}
	else {
		flag = MF_STRING;
	}
	int id_STAND_SCREEN = cmd++;
	AppendMenu(hMenuScreenMode, flag, id_STAND_SCREEN, _T("Standard Screen (4:3)"));

	if (g_ratiox == 16 && g_ratioy == 9) {
		flag = MF_STRING | MF_CHECKED;
	}
	else {
		flag = MF_STRING;
	}
	int id_WIDE_SCREEN = cmd++;
	AppendMenu(hMenuScreenMode, flag, id_WIDE_SCREEN, _T("Wide Screen (16:9)"));

#if defined(PAL_SCREEN_SUPPORT) 
	if (g_ratiox == 720 && g_ratioy == 576) {
		flag = MF_STRING | MF_CHECKED;
	}
	else {
		flag = MF_STRING;
	}
	int id_PAL_SCREEN = cmd++;
	AppendMenu(hMenuScreenMode, flag, id_PAL_SCREEN, _T("Pal Screen (720:576)"));
#endif

	if (g_ratiox == 10001) {
		flag = MF_STRING | MF_CHECKED;
	}
	else {
		flag = MF_STRING;
	}
	int id_FILL_SCREEN = cmd++;
	AppendMenu(hMenuScreenMode, flag, id_FILL_SCREEN, _T("Fill Screen"));

	int id_NATIVE_SCREEN = cmd++;
	if (m_decoder != NULL && m_attached) {
		m_channelinfo.size = sizeof(m_channelinfo);
		m_decoder->getchannelinfo(m_channel, &m_channelinfo);
		if (m_channelinfo.xres > 0 && m_channelinfo.yres > 0) {
			string nr;
			if (g_ratiox == 10002) {
				flag = MF_STRING | MF_CHECKED;
			}
			else {
				flag = MF_STRING;
			}
			nr.printf("Native (%d:%d)", m_channelinfo.xres, m_channelinfo.yres);
			AppendMenu(hMenuScreenMode, flag, id_NATIVE_SCREEN, (LPCTSTR)nr);
		}
	}

	if (g_ratiox <= 0) {
		flag = MF_STRING | MF_CHECKED;
	}
	else {
		flag = MF_STRING;
	}
	int id_AUTOMATIC_SCREEN = cmd++;
	AppendMenu(hMenuScreenMode, flag, id_AUTOMATIC_SCREEN, _T("Automatic"));

	AppendMenu(hMenuPop, MF_POPUP, (UINT_PTR)hMenuScreenMode, _T("Screen Format"));

	int id_SETUP_PLAYER = cmd++;
	int id_CLOSE_PLAYER = cmd++;
	int id_DISPLAY_MAP = cmd++;

	int id_SHOW_MARK_CLIP = cmd++;
	int id_SHOW_CLIP = cmd++;

	int id_BLUR_RECTANGULAR = cmd++;
	int id_BLUR_ELLIPSE = cmd++;
	int id_BLUR_CLEAR = cmd++;

	if (m_decoder && m_attached) {
		if (m_decoder->getfeatures() & PLYFEATURE_PLAYERCONFIGURE) {
			AppendMenu(hMenuPop, MF_STRING, id_SETUP_PLAYER, _T("Setup Player"));
		}
		AppendMenu(hMenuPop, MF_STRING, id_CLOSE_PLAYER, _T("Close"));

		// add on Feb 09, 2012, show gps map
		string gpslocation;
		if (m_decoder->getlocation(gpslocation.strsize(514), 512) > 0) {
			int l = strlen(gpslocation);
			if (l > 32) {
				float lati, longi, kmh, direction;
				char  latiD, longiD, cdirection;
				sscanf(((char *)gpslocation) + 12, "%9f%c%10f%c%f%c%6f",
					&lati, &latiD, &longi, &longiD, &kmh, &cdirection, &direction);
				if (lati > 1.0 || longi > 1.0) {
					AppendMenu(hMenuPop, MF_STRING, id_DISPLAY_MAP, _T("Display Map"));
				}
			}
		}

#ifdef WMVIEWER_APP
		// only on debugging version
		AppendMenu(hMenuPop, MF_STRING, id_SHOW_MARK_CLIP, _T("Show Event Clip List"));
#endif

#if defined(DVRVIEWER_APP) || defined( SPARTAN_APP ) || defined( WMVIEWER_APP ) || defined( APP_PWVIEWER )
		if (m_decoder->blur_supported() && m_mouse_op == MOUSE_OP_NONE && m_zoomarea.z == 1.0) {
			AppendMenu(hMenuPop, MF_SEPARATOR, NULL, NULL);

			// Blur area support
			if (m_ba_number < MAX_BLUR_AREA) {
				AppendMenu(hMenuPop, MF_STRING, id_BLUR_RECTANGULAR, _T("Add Rectangular Blur Area"));
				AppendMenu(hMenuPop, MF_STRING, id_BLUR_ELLIPSE, _T("Add Ellipse Blur Area"));
			}
			else {
				AppendMenu(hMenuPop, MF_STRING | MF_GRAYED, id_BLUR_RECTANGULAR, _T("Add Rectangular Blur Area"));
				AppendMenu(hMenuPop, MF_STRING | MF_GRAYED, id_BLUR_ELLIPSE, _T("Add Ellipse Blur Area"));
			}
			AppendMenu(hMenuPop, MF_STRING, id_BLUR_CLEAR, _T("Clear Blur Area"));
		}
#endif  // DVRVIEWER_APP

	}

	AppendMenu(hMenuPop, MF_SEPARATOR, NULL, NULL);

	int id_ZOOM_IN = cmd++;
	if (m_mouse_op == MOUSE_OP_ZOOM) {
		AppendMenu(hMenuPop, MF_STRING | MF_CHECKED, id_ZOOM_IN, _T("Zoom In"));
		AppendMenu(hMenuPop, MF_SEPARATOR, NULL, NULL);
	}
	else if (m_attached && m_decoder != NULL && m_decoder->supportzoomin() && m_channel >= 0 && m_mouse_op == MOUSE_OP_NONE) {
		AppendMenu(hMenuPop, MF_STRING, id_ZOOM_IN, _T("Zoom In"));
		AppendMenu(hMenuPop, MF_SEPARATOR, NULL, NULL);
	}

	int id_ROTATE_0 = cmd++;
	int id_ROTATE_90 = cmd++;
	int id_ROTATE_180 = cmd++;
	int id_ROTATE_270 = cmd++;
	if (m_attached && m_decoder != NULL && m_decoder->supportrotate() && m_channel >= 0) {

		// add 0, 90, 180, 270 rotation menu
		if (m_rotate_degree == 0)
			AppendMenu(hMenuRotate, MF_STRING | MF_CHECKED, id_ROTATE_0, _T("0 degree"));
		else
			AppendMenu(hMenuRotate, MF_STRING, id_ROTATE_0, _T("0 degree"));

		if (m_rotate_degree == 90)
			AppendMenu(hMenuRotate, MF_STRING | MF_CHECKED, id_ROTATE_90, _T("90 degree"));
		else
			AppendMenu(hMenuRotate, MF_STRING, id_ROTATE_90, _T("90 degree"));

		if (m_rotate_degree == 180)
			AppendMenu(hMenuRotate, MF_STRING | MF_CHECKED, id_ROTATE_180, _T("180 degree"));
		else
			AppendMenu(hMenuRotate, MF_STRING, id_ROTATE_180, _T("180 degree"));

		if (m_rotate_degree == 270)
			AppendMenu(hMenuRotate, MF_STRING | MF_CHECKED, id_ROTATE_270, _T("270 degree"));
		else
			AppendMenu(hMenuRotate, MF_STRING, id_ROTATE_270, _T("270 degree"));

		AppendMenu(hMenuPop, MF_POPUP, (UINT_PTR)hMenuRotate, _T("Rotation"));
		AppendMenu(hMenuPop, MF_SEPARATOR, NULL, NULL);
	}


#ifdef SUPPORT_ROC_AOI
	// support of ROC/AOI, value from 8000-8999
	int id_AOI_LOAD = cmd++;
	int id_AOI_SAVE = cmd++;
	int id_ROC = cmd++;
	int id_AOI = cmd++;
	int id_AOI_CLEAR = cmd++;
	int id_AOI_ITEM = cmd;

	if (isattached() && m_mouse_op == MOUSE_OP_NONE && SUPPORT_ROC) {

		AppendMenu(hMenuROC, MF_STRING, id_AOI_LOAD, _T("Load ROC/AOI"));
		if (m_polygon != NULL && m_num_polygon > 0)
			AppendMenu(hMenuROC, MF_STRING, id_AOI_SAVE, _T("Save ROC/AOI"));

		AppendMenu(hMenuROC, MF_STRING, id_AOI_CLEAR, _T("Clear All"));
		AppendMenu(hMenuROC, MF_STRING, id_ROC, _T("Create ROC"));
		AppendMenu(hMenuROC, MF_STRING, id_AOI, _T("Add AOI"));

		if (m_polygon != NULL) {
			for (i = 1; i < m_num_polygon; i++) {
				if (m_polygon[i].points > 2) {
					string name = m_polygon[i].name;
					if (name.isempty()) {
						name = TMPSTR("AOI %d", i);
					}
					cmd = id_AOI_ITEM + i;
					AppendMenu(hMenuROC, MF_STRING, cmd, TMPSTR("Remove AOI - %s", (char *)name));
				}
			}
		}
		AppendMenu(hMenuPop, MF_POPUP, (UINT_PTR)hMenuROC, _T("ROC/AOI"));
		AppendMenu(hMenuPop, MF_SEPARATOR, NULL, NULL);
	}

	cmd++;
	int id_AOI_END = cmd;
#endif

	int id_CHANNEL_START = cmd;
	int channel;
	for (i = 0; i < MAXDECODER; i++) {
		if (g_decoder[i].isopen()) {
			cmd = id_CHANNEL_START + i * 128;
			for (channel = 0; channel < g_decoder[i].getchannel(); channel++) {
				struct channel_info channelinfo;
				memset(&channelinfo, 0, sizeof(channelinfo));
				channelinfo.size = sizeof(channelinfo);
				g_decoder[i].getchannelinfo(channel, &channelinfo);
				if ((channelinfo.features & 1) != 0) {
					string menustring;
					menustring.printf("%s - %s", g_decoder[i].getservername(), channelinfo.cameraname);
					if (m_decoder == &g_decoder[i] && m_attached && m_channel == channel) {
						AppendMenu(hMenuPop, MF_STRING | MF_CHECKED, cmd + channel, menustring);
					}
					else {
						AppendMenu(hMenuPop, MF_STRING, cmd + channel, menustring);
					}
				}
			}
		}
	}
	cmd += 128;
	int id_CHANNEL_END = cmd;

	SetForegroundWindow(m_hWnd);
	POINT pt;
	pt.x = x;
	pt.y = y;
	MapWindowPoints(m_hWnd, NULL, &pt, 1);

	// Popup menu
	cmd = TrackPopupMenu(hMenuPop, TPM_RIGHTBUTTON | TPM_RETURNCMD,
		pt.x, pt.y,
		0, m_hWnd, NULL);
	DestroyMenu(hMenuRotate);
	DestroyMenu(hMenuROC);
	DestroyMenu(hMenuScreenMode);
	DestroyMenu(hMenuPop);

	if (cmd == id_CLOSE_PLAYER) {		// close
		DetachDecoder();
	}
	else if (cmd == id_SETUP_PLAYER) {	// setup player
		m_decoder->configurePlayer();
	}
	else if (cmd == id_DISPLAY_MAP) {	// Display Map
		g_maindlg->DisplayMap();
	}
	else if (cmd == id_SHOW_MARK_CLIP) {	// show marked clip list
		g_maindlg->DisplayClipList(1);
	}
	else if (cmd == id_SHOW_CLIP) {	// show clip list
		g_maindlg->DisplayClipList(0);
	}
	else if (cmd == id_BLUR_RECTANGULAR) {	// Select Rectangle Blur area
		AddBlurArea(0);
	}
	else if (cmd == id_BLUR_ELLIPSE) {	// Select Ellipse Blur Area
		AddBlurArea(1);
	}
	else if (cmd == id_BLUR_CLEAR) {	// Clear Blur Area 
		ClearBlurArea();
	}
	else if (cmd == id_ZOOM_IN) {	// zoom in
		if (m_mouse_op == MOUSE_OP_ZOOM) {
			StopZoom();
		}
		else {
			StartZoom();
		}
	}
	else if (cmd == id_ROTATE_0) {	// rotate 0 degree
		setRotate(0);
	}
	else if (cmd == id_ROTATE_90) {	// rotate 90 degree
		setRotate(90);
	}
	else if (cmd == id_ROTATE_180) {	// rotate 180 degree
		setRotate(180);
	}
	else if (cmd == id_ROTATE_270) {	// rotate 270 degree
		setRotate(270);
	}

#ifdef SUPPORT_ROC_AOI
	else if (cmd == id_AOI_LOAD) {
		LoadAOI();
	}
	else if (cmd == id_AOI_SAVE) {
		SaveAOI();
	}
	else if (cmd == id_ROC) {
		StartROC();
	}
	else if (cmd == id_AOI) {
		StartAOI();
	}
	else if (cmd == id_AOI_CLEAR) {
		ClearAOI();
	}
	else if (cmd >= id_AOI_ITEM && cmd < id_AOI_END) {
		// remove AOI
		RemoveAOI(cmd - id_AOI_ITEM);
	}
#endif
	else if (cmd >= id_CHANNEL_START && cmd < id_CHANNEL_END) {
		i = (cmd - id_CHANNEL_START) / 128;
		channel = (cmd - id_CHANNEL_START) % 128;
		AttachDecoder(&g_decoder[i], channel);
	}
	else if (cmd >= id_SCREEN_MODE && cmd < id_SCREEN_MODE_END) {
		g_singlescreenmode = 0;
		g_screenmode = cmd - id_SCREEN_MODE;
		reg_save("screenmode", g_screenmode);
		g_maindlg->SetScreenFormat();
	}
	else if (cmd == id_FULL_SCREEN) {
		g_maindlg->SetZoom();
	}
	else if (cmd == id_STAND_SCREEN) {		// standard screen 4:3
		g_ratiox = 4;
		g_ratioy = 3;
		aspect_changed = 1;
	}
	else if (cmd == id_WIDE_SCREEN) {		// HD screen 16:9
		g_ratiox = 16;
		g_ratioy = 9;
		aspect_changed = 1;
	}
#if defined(PAL_SCREEN_SUPPORT) 
	else if (cmd == id_PAL_SCREEN) {
		g_ratiox = 720;
		g_ratioy = 576;
		aspect_changed = 1;
	}
#endif
	else if (cmd == id_NATIVE_SCREEN) {		// native
		g_ratiox = 10002;
		g_ratioy = 10002;
		aspect_changed = 1;
	}
	else if (cmd == id_FILL_SCREEN) {		// fit screen
		g_ratiox = 10001;
		g_ratioy = 10001;
		aspect_changed = 1;
	}
	else if (cmd == id_AUTOMATIC_SCREEN) {		// auto matic
		g_ratiox = 0;
		g_ratioy = 0;
		aspect_changed = 1;
	}
	if (aspect_changed) {
		reg_save("ratiox", g_ratiox);
		reg_save("ratioy", g_ratioy);
		g_maindlg->SetScreenFormat();
	}

	return 0;
}
