#pragma once

#include "../common/cstr.h"
#include "../common/cwin.h"
#include "../common/util.h"

#include "cJSON.h"

#include "decoder.h"

class SurfaceWindow : public Window {
protected:
	virtual LRESULT WndProc(UINT uMsg, WPARAM wParam, LPARAM lParam) {
		if (uMsg == WM_MOUSEMOVE) {
			HWND hparent = GetParent(m_hWnd);
			PostMessage(hparent, uMsg, wParam, lParam);
		}
		return Window::WndProc(uMsg, wParam, lParam);
	}
};


#define TIMEBARCACHESIZE (1)
#define MAX_BLUR_AREA   (10)

#define TIMER_BLUR_AGAIN	(301)
#define TIMER_CHANNEL_INFO	(401)

#define MOUSE_OP_NONE	(0)
#define MOUSE_OP_BLUR	(1)
#define MOUSE_OP_ZOOM	(2)
#define MOUSE_OP_ROC	(4)
#define MOUSE_OP_AOI	(5)

// maximum polygon number
#define MAX_POLYGON_NUMBER	(100)
// maximum points per plygon
#define MAX_POLYGON_POINTS	(1000)

struct  AOI_point {
	float x, y;
};

struct  AOI_polygon {
	int points;
	struct AOI_point poly[MAX_POLYGON_POINTS];
	string name;
public:
	AOI_polygon() {
		points = 0;
	}
	int closeable() {	// return if tail point is closed to start point
		if (points > 1 ) {
			return poly[0].x == poly[points].x && poly[0].y == poly[points].y;
		}
		return 0;
	}
	void tail(float x, float y) {	// set tailing point
		if (points > 0 && points<MAX_POLYGON_POINTS-1) {
			float dx = poly[0].x - x;
			float dy = poly[0].y - y;
			if ((dx*dx + dy*dy) < 0.001) {
				poly[points].x = poly[0].x;
				poly[points].y = poly[0].y;
			}
			else {
				poly[points].x = x;
				poly[points].y = y;
			}
		}
	}
	void add(float x, float y) {
		if (points<MAX_POLYGON_POINTS-1) {
			poly[points].x = x;
			poly[points].y = y;
			poly[points + 1].x = x;
			poly[points + 1].y = y;
			points++;
		}
	}
};

class Screen : public Window
{
protected:
	static Image * m_backgroundimg ;
    static int m_count ;
    int    m_flashrec ;
	int    m_audio ;
	HWND   m_hwndToolTip;

	SurfaceWindow m_surface;

public:
    Screen(HWND hparent);
	virtual ~Screen();

	decoder * m_decoder ;
	int		m_channel ;
	int		m_xchannel ;		// previous saved channel
	int     m_attached ;
	struct  channel_info m_channelinfo ;
	int		m_get_chinfo_retry;

	// day info cache
	int     m_dayinfocache_year[12] ;
	int     m_dayinfocache[12] ;

	class tbcache {
	public:
        int    m_day ;      // in bcd format: 20100101
		struct cliptimeinfo * m_cliptimeinfo ;
		int    m_timeinfosize ;
		struct cliptimeinfo * m_locktimeinfo ;
		int    m_lockinfosize ;
		tbcache() 
			:m_cliptimeinfo(NULL)
			,m_timeinfosize(0)
            ,m_locktimeinfo(NULL)
			,m_lockinfosize(0),
            m_day(0)
		{
		}
		void clean(){
            m_day = 0 ;
			if( m_timeinfosize>0 ) {
				delete m_cliptimeinfo ;
                m_cliptimeinfo=NULL ;
				m_timeinfosize=0 ;
			}
			if( m_lockinfosize>0 ) {
				delete m_locktimeinfo ;
                m_locktimeinfo=NULL ;
				m_lockinfosize=0 ;
			}
		}
		~tbcache() {
			clean();
		}
/*		tbcache & operator = (tbcache & tbc) {
			if( m_timeinfosize>0 ) {
				delete m_cliptimeinfo ;
			}
			if( m_lockinfosize>0 ) {
				delete m_locktimeinfo ;
			}
			m_timebardate  =tbc.m_timebardate ;
			m_cliptimeinfo =tbc.m_cliptimeinfo ;
			m_timeinfosize =tbc.m_timeinfosize;
			m_locktimeinfo =tbc.m_locktimeinfo;
			m_lockinfosize =tbc.m_lockinfosize;
		}
*/
    } m_timebarcache[TIMEBARCACHESIZE] ;
	int m_nexttimebarcache ;
    void clearcache();

public:
	int AttachDecoder(decoder * pdecoder, int channel);
	int DetachDecoder();
	int Show();
	int Hide();
	void suspend();
	void resume();
	void reattach();
	int  selectfocus();
	int  setaudio(int onoff);
    void Move(int x, int y, int w, int h ) {
        MoveWindow( m_hWnd, x, y, w, h, TRUE );
//        InvalidateRect( m_hWnd, NULL, TRUE );
    }
    int isVisible() {
        return IsWindowVisible(m_hWnd);
    }
	int getcliptime( struct dvrtime * day, struct cliptimeinfo ** pclipinfo);
	int getlocktime( struct dvrtime * day, struct cliptimeinfo ** plockinfo);
	int gettimeinfo( struct dvrtime * day);
	int getdayinfo( struct dvrtime * daytime );

	int attach_channel(decoder * pdecoder, int channel);
	int getchinfo();

	int isattached(){
		return m_attached && m_decoder!=NULL && m_channel>=0 ;
	}

	// window size
	int		m_width;
	int		m_height;
	// mouse click on pointer
	int     m_pointer_x;
	int     m_pointer_y;
	int     m_mouse_op;		// 0, noop, 1:blurring, 2:zoomin, 3:zoomin_on, 4: roc, 5, aoi

	// rotation 
	int     m_rotate_degree; // rotation in degree, may support 0, 90, 180, 270 only

    HCURSOR m_blurcursor;
    struct blur_area m_blur_area[MAX_BLUR_AREA];
    int     m_ba_number ;
    int     m_blurshape ;
    void    AddBlurArea(int shape);
	void    StopBlurArea();
	void    ClearBlurArea();

	// zoom in feature
	int is_zoomin() {
		return m_mouse_op == MOUSE_OP_ZOOM || m_zoomarea.z<1.0 ;
	}

	struct zoom_area m_zoomarea;	// currently zoom area
	struct zoom_area m_zoomdraw;	// zoom drawing area
	int		ShowZoomarea(float x, float y, float z);
	void	drawzoomarea();
	void    StartZoom();
	void    StopZoom();
	int		ZoomPTZ(int pan, int tilt, int zoomin);
	// rotate support
	int		setRotate(int degree);
	int		rotate();

	// ROC/AOI feature
	int		m_num_polygon;
	AOI_polygon * m_polygon;
	void	UpdatePolygon();	// update polygons to screen

	void	LoadAOI();
	void	SaveAOI();

	void	ClearAOI();			// clear all AOI/ROC
	void    StartROC();
	void    StopROC();

	void    StartAOI();
	void    StopAOI();
	void	RemoveAOI(int idx);

	void	showTooltip(char * msg);

protected:
	virtual LRESULT OnPaint() ;
	virtual LRESULT OnEraseBkgnd(HDC hdc);
	virtual LRESULT OnLButtonUp(UINT nFlags, int x, int y );
	virtual LRESULT OnLButtonDown(UINT nFlags, int x, int y );
	virtual LRESULT OnLButtonDblClk(UINT nFlags,int x, int y );
	virtual LRESULT OnRButtonDown(UINT nFlags, int x, int y );
	virtual LRESULT OnRButtonUp(UINT nFlags, int x, int y );
	virtual LRESULT OnMButtonDown(UINT nFlags, int x, int y);
	virtual LRESULT OnMButtonUp(UINT nFlags, int x, int y);
	virtual LRESULT OnMouseMove(UINT nFlags, int x, int y );
	virtual LRESULT OnMouseWheel(UINT delta, int x, int y);
	virtual LRESULT OnSetCursor();
    virtual LRESULT OnTimer(UINT_PTR nIDEvent);

	LRESULT OnSize(int w, int h) {
		m_width = w ;
		m_height = h;
		return FALSE;
	}

	virtual LRESULT WndProc(UINT message, WPARAM wParam, LPARAM lParam) {
		LRESULT res = 0 ;
		switch (message)
		{
		case WM_PAINT:
			res = OnPaint();
			break;
		case WM_SIZE:
			res = OnSize(LOWORD(lParam), HIWORD(lParam));
			break;
		case WM_ERASEBKGND:
			res = OnEraseBkgnd((HDC)wParam);
			break;
		case WM_RBUTTONDOWN:
			res = OnRButtonDown(wParam, MOUSE_POSX(lParam), MOUSE_POSY(lParam) );
			break;
		case WM_RBUTTONUP:
			res = OnRButtonUp(wParam, MOUSE_POSX(lParam), MOUSE_POSY(lParam) );
			break;
		case WM_LBUTTONDOWN:
			res = OnLButtonDown(wParam, MOUSE_POSX(lParam), MOUSE_POSY(lParam) );
			break;
		case WM_LBUTTONUP:
			res = OnLButtonUp(wParam, MOUSE_POSX(lParam), MOUSE_POSY(lParam) );
			break;
        case WM_LBUTTONDBLCLK:
			res = OnLButtonDblClk(wParam, MOUSE_POSX(lParam), MOUSE_POSY(lParam) );
			break;
		case WM_MBUTTONDOWN:
			res = OnMButtonDown(wParam, MOUSE_POSX(lParam), MOUSE_POSY(lParam));
			break;
		case WM_MBUTTONUP:
			res = OnMButtonUp(wParam, MOUSE_POSX(lParam), MOUSE_POSY(lParam));
			break;
		case WM_MOUSEMOVE:
			res = OnMouseMove(wParam, MOUSE_POSX(lParam), MOUSE_POSY(lParam) );
			break;
		case WM_MOUSEWHEEL:
			res = OnMouseWheel(wParam, MOUSE_POSX(lParam), MOUSE_POSY(lParam));
			break;
		case WM_SETCURSOR:
			res = OnSetCursor();
			break;
		case WM_TIMER:
			res = OnTimer(wParam);
			break;
		default:
			res = DefWndProc(message, wParam, lParam);
		}
		return res ;
	}
    
};

