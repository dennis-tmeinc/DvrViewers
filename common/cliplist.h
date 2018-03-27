#pragma once

#include "../common/cstr.h"
#include "../common/cwin.h"
#include "../common/util.h"

#include "dvrclientDlg.h"
#include "screen.h"
#include "decoder.h"

#define NUM_CLIP_PERLINE     (3)
#define NUM_CLIP_PAGE      (5000)
#define MAX_CACHE (80)
#define TIMER_DRAW (7)
#define EVENT_NUMBER (5)

class Cliplist : public Window
{
protected:
    DvrclientDlg * m_parent ;
    Screen * m_screen ;
    dvrtime  m_clipday ;
    string   m_tmpdir ;
    int      m_lockclip ;
    int      m_select ;         // clip select result ;
    int      m_mode ;           // 0: mulit clip, 1: single clip
    int      m_clip_perline ;   // NUM_CLIP_PERLINE

    int *  m_elist ;
    int    m_elistsize ;
    int *  m_updlist ;

    Gdiplus::Pen *m_selpen ;
    Gdiplus::Pen *m_nselpen ;
    Gdiplus::SolidBrush  *m_waitBrush ;

    Image * m_imgcache[MAX_CACHE] ;
    int     m_imgcacheidx[MAX_CACHE] ;

    HCURSOR m_handgrabcursor ;

    // draw helper

#define TITLE_X (2)
#define TITLE_Y (2)
#define IMG_X   (4)
#define IMG_Y   (24)

    int     m_gridwidth ;
    int     m_gridheight ;
    int     m_imgwidth ;
    int     m_imgheight ;
    int     m_screenpage ;

    int m_graby ;
    int m_grabypos ;

    // loading image
    Image * m_loadingimage ;
    int     m_loadingframe ;

public:

    Cliplist(DvrclientDlg * parent, Screen * screen, int lockclip, dvrtime * playtime, char * tmpdir);

    ~Cliplist() ;

    // find img from idx
    Image * findImg( int idx ) {
        int i = idx%MAX_CACHE ;
        if( m_imgcacheidx[i] == idx ) {
            return m_imgcache[i] ;
        }
        return NULL ;
    }

    // add (replace) img to idx
    void addImg( int idx, Image * img ) {
        int i = idx%MAX_CACHE ;
        if( m_imgcache[i] != NULL ) {
            delete m_imgcache[i] ;
        }
        m_imgcacheidx[i] = idx ;
        m_imgcache[i] = img ;
    }

    int getlistsize() {
        return m_elistsize ;
    }

    int getClipTime( dvrtime * dvrt );
    int getClipname( string & imgname ) ;

    void Show() {
        ::ShowWindow(m_hWnd, SW_SHOW);
    }

    void Move(RECT * rect) {
        MoveWindow(m_hWnd, rect->left, rect->top, rect->right-rect->left, rect->bottom-rect->top, TRUE);
    }

    int getSelect( dvrtime * dvrt ) ;

    int m_snapshot_run ;
    struct dvrtime m_snapshot_time ;
    string m_snapshot_filename ;
    HANDLE m_snapshot_event ;
    HANDLE m_snapshot_thread ;

    void snapshot_thread();

protected:
    // convert start pos to Y pixels value
    int Pos2Y(int pos) {
        return pos * m_gridheight / NUM_CLIP_PAGE ;
    }

    // convert Y pixel to pos
    int Y2Pos(int y) {
        return y * (NUM_CLIP_PAGE) / m_gridheight ;
    }

    int start_y() 
    {
        SCROLLINFO si ;
        si.cbSize = sizeof(si);
        si.fMask = SIF_POS;
        GetScrollInfo (m_hWnd, SB_VERT, &si);
        return Pos2Y( si.nPos );
    }

    void OnDestroy();

    void OnTimer(UINT_PTR nIDEvent);

    LRESULT OnPaint();

    // Scroll up / down
    int scrollPos( int offset );
    
    LRESULT OnVscroll(int code);

    int SelectClip( int x, int y ) ;
    LRESULT OnLButtonDblClk(UINT nFlags,int x, int y );
    LRESULT OnLButtonDown(UINT nFlags, int x, int y );
    LRESULT OnLButtonUp(UINT nFlags,int x, int y );
    LRESULT OnMouseMove(UINT nFlags,int x, int y );
	LRESULT OnMouseWheel(UINT nFlags, int x, int y);
	LRESULT OnSize() ;

	virtual LRESULT WndProc(UINT message, WPARAM wParam, LPARAM lParam) {
		LRESULT res = FALSE ;
		switch (message)
		{
		case WM_PAINT:
			res = OnPaint();
			break;
		case WM_ERASEBKGND:
			res = 1;            //  don't do ERASEBKGND
			break;
        case WM_LBUTTONDBLCLK:
			res = OnLButtonDblClk(wParam, MOUSE_POSX(lParam), MOUSE_POSY(lParam) );
			break;
        case WM_MOUSEWHEEL:
			res = OnMouseWheel(wParam, MOUSE_POSX(lParam), MOUSE_POSY(lParam));
			break;
        case WM_MOUSEMOVE:
            res = OnMouseMove(wParam, MOUSE_POSX(lParam), MOUSE_POSY(lParam) );
			break;
        case WM_LBUTTONDOWN:
            res = OnLButtonDown(wParam, MOUSE_POSX(lParam), MOUSE_POSY(lParam) );
			break;
        case WM_LBUTTONUP:
            res = OnLButtonUp(wParam, MOUSE_POSX(lParam), MOUSE_POSY(lParam) );
			break;
		case WM_VSCROLL:
			res = OnVscroll( LOWORD(wParam) );
			break;
        case WM_SIZE:
			res = OnSize();
			break;
        case WM_TIMER:
            OnTimer(wParam);
            res=TRUE ;
            break;
		case WM_DESTROY:
			OnDestroy();
			break;
		default:
			res = Window::WndProc(message, wParam, lParam);
		}
		return res ;
	}
    
};

