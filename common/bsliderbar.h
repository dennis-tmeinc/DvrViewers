#pragma once

#pragma once

// Custom Slider Bar

#include "../common/cwin.h"
#include "../common/cstr.h"
#include "../common/util.h"
#include "Screen.h"

#define BUTTOMBARCOLOR			( RGB(0x83, 0x8d, 0x3c) )

// dont use it now
class BSliderBar : public Window
{
protected:
	int		m_pos ;
	int		m_min ;
	int		m_max ;
	int		m_touchdown ;
	HWND	m_hparent ;
	
    Screen * m_screen ;

    Bitmap * m_thumbimg ;
	struct dvrtime m_playertime ;
    TextureBrush  * m_barBrush;
    Pen    *  m_videopen ;
    Pen    *  m_markpen ;

	int     m_thumbwidth ;
	RECT    m_barRect ;

	int     m_inzoom ;
	int     m_zoompos ;

	int     m_inpan ;
	int     m_panpos ;

public:
// Constructors
	BSliderBar()
	{
		m_pos=0;
		m_min=0;
		m_max=100;
		m_lowfilepos = 0 ;
		m_highfilepos = 0 ;
		m_touchdown=0 ;
		m_inpan = 0 ;
		m_inzoom = 0 ;
   
		Bitmap * img  ;
		img = loadbitmap(_T("SLIDER_BAR"));
		m_barBrush = new TextureBrush(img);
		delete img ;

		m_videopen = new Pen( Color(255,255,180,0), 24 );
		m_videopen->SetAlignment(PenAlignmentCenter);
		m_markpen = new Pen( Color(255,248,80,0), 24 );
		m_markpen->SetAlignment(PenAlignmentCenter);

		img = loadbitmap(_T("SLIDER_VIDEO"));
		if( img ) {
			TextureBrush  tBrush(img);
			m_videopen->SetWidth( img->GetHeight() );
			m_videopen->SetBrush(&tBrush);
			delete img ;
		}
		img = loadbitmap(_T("SLIDER_MARK"));
		if( img ) {
			TextureBrush  tBrush(img);
			m_markpen->SetWidth( img->GetHeight() );
			m_markpen->SetBrush(&tBrush);
			delete img ;
		}
		m_thumbimg = loadbitmap(_T("SLIDER_THUMB"));
		m_thumbwidth = m_thumbimg->GetWidth();

		m_screen=NULL;
		memset(&m_playertime,0,sizeof(&m_playertime));
		m_playertime.year=1980 ;
		m_playertime.month=1;
		m_playertime.day=1;

	}
	
	~BSliderBar() {
		delete m_thumbimg ;
		delete m_barBrush ;
		delete m_videopen ;
		delete m_markpen ;
	}

	void Create( HWND hparent, int id ) 
	{	
		m_hparent = hparent ;

		HWND hwnd = CreateWindowEx(0, WinClass(), _T("Slider"), WS_CHILD,
			0 , 0, 300, 24, 
			hparent, (HMENU)id, AppInst(), NULL );
		attach(hwnd);

		// adjust slider height
		Bitmap * img  ;
		img = loadbitmap(_T("SLIDER_BAR"));
		MoveWindow(m_hWnd, 0, 0, 100, img->GetHeight(),FALSE);
		delete img ;

		// setup touch gesture supports
		// set up our want / block settings
		DWORD dwPanWant  = GC_PAN_WITH_SINGLE_FINGER_VERTICALLY | GC_PAN_WITH_SINGLE_FINGER_HORIZONTALLY  | GC_PAN_WITH_INERTIA ;
		DWORD dwPanBlock = GC_PAN_WITH_GUTTER ;

		// set the settings in the gesture configuration
		GESTURECONFIG gc[] = {{ GID_ZOOM, GC_ZOOM, 0 },
							  { GID_PAN, dwPanWant , dwPanBlock}                     
							 };    
                     
		SetGestureConfig(m_hWnd, 0, 2, gc, sizeof(GESTURECONFIG));  

		ShowWindow(m_hWnd, SW_SHOW);

	}

    int m_lowfilepos ;
    int m_highfilepos ;

    void SetScreen(Screen * sc) {
        m_screen = sc ;
    }

    int GetPos() {
		m_touchdown = 0 ;		// clear touch down 
		return m_pos ;
    }

    void SetPos(dvrtime * dvrt) ;

    int SetPos(int pos) {
		if( m_pos != pos ) {
			RECT xrt, nrt ;
			GetThumbRect( &xrt );
			m_pos = pos ;
			GetThumbRect( &nrt );
			if( xrt.left != nrt.left ) {
				InvalidateRect(m_hWnd, &xrt, FALSE );
				InvalidateRect(m_hWnd, &nrt, FALSE );
			}
		}
		return TRUE ;
    }

	int GetRangeMin() {
		return m_min ;
    }

    int GetRangeMax() {
		return m_max ;
    }

    void SetRange( int min, int max ) {
		m_min = min ;
		m_max = max ;
		m_touchdown = 0 ;
		InvalidateRect(m_hWnd, NULL, FALSE );
    }

protected:

    virtual void OnPaint();

	virtual int OnEraseBkgnd(HDC hdc){
        return 1 ;      // do not paint background
    }

	void GetChannelRect( RECT * cr )
	{
		cr->left=m_barRect.left+m_thumbwidth/2+2  ;
		cr->right=m_barRect.right-m_thumbwidth/2-2 ;
		cr->top=m_barRect.top ;
		cr->bottom=m_barRect.bottom ;

		if( m_inpan ) {
			cr->left += m_inpan ;
			cr->right += m_inpan ;
		}

		if( m_inzoom ) {
			int zoom = m_inzoom - m_zoompos ;
			cr->left += zoom/2 ;
			cr->right -= zoom/2 ;
		}
	}

	int XtoPos( int x )
	{
		RECT channelrect ;
		GetChannelRect( &channelrect );
		return (x-channelrect.left)*(m_max-m_min)/(channelrect.right-channelrect.left)+m_min ;
	}

	int PostoX( int pos ) 
	{
		RECT channelrect ;
		GetChannelRect( &channelrect );
		return (pos-m_min) * (channelrect.right-channelrect.left) / (m_max-m_min) + channelrect.left ;
	}

	int GetThumbPos()
	{
		return PostoX( m_pos );
	}

	void GetThumbRect( RECT * cr )
	{
		int thumbpos ;
		thumbpos = GetThumbPos() ;
		cr->left = thumbpos - m_thumbwidth/2 ;
		cr->right = cr->left + m_thumbwidth ;
		cr->top = m_barRect.top ;
		cr->bottom = m_barRect.bottom ; 
	}

	int OnSetPos( int x );

	int OnLButtonDown( int x, int y ) 
	{
		OnSetPos( x );
		return 0 ;
	}

	int OnLButtonUp( int x, int y ) 
	{
		m_touchdown = 0 ;
		return 0 ;
	}

	int OnMouseMove( int x, int y ) 
	{
		OnSetPos( x );
		return 0 ;
	}

	void OnPan( int flags, int x, int y );

	void OnZoom( int flags, int dist );

	BOOL getGestureInfo(HGESTUREINFO hGestureInfo, PGESTUREINFO pGestureInfo) {
		return GetGestureInfo( hGestureInfo, pGestureInfo );
	}

	int OnGesture(  WPARAM wParam, LPARAM lParam )
	{
				// Create a structure to populate and retrieve the extra message info.
		GESTUREINFO gi;  
    
		ZeroMemory(&gi, sizeof(GESTUREINFO));
    
		gi.cbSize = sizeof(GESTUREINFO);

		BOOL bResult  = getGestureInfo((HGESTUREINFO)lParam, &gi);
		BOOL bHandled = FALSE;

		if (bResult){
			// now interpret the gesture
			switch (gi.dwID){
			   case GID_ZOOM:
				   // Code for zooming goes here     
				   OnZoom( gi.dwFlags, (int)gi.ullArguments );
				   bHandled = TRUE;
				   break;
			   case GID_PAN:
				   // Code for panning goes here
				   OnPan( gi.dwFlags, gi.ptsLocation.x, gi.ptsLocation.y );
				   bHandled = TRUE;
				   break;
			   case GID_ROTATE:
				   // Code for rotation goes here
				   bHandled = TRUE;
				   break;
			   default:
				   // A gesture was not recognized
				   break;
			}
		}
		return DefWndProc( WM_GESTURE, wParam, lParam);
	}


	virtual LRESULT WndProc(UINT message, WPARAM wParam, LPARAM lParam) {
		LRESULT res = 0 ;
		switch (message)
		{
		case WM_ERASEBKGND:
			res = OnEraseBkgnd((HDC)wParam);
			break;
		case WM_PAINT:
			OnPaint();
            res = 1 ;
			break;
		
		case WM_GESTURE:
			res = OnGesture( wParam, lParam );
			break;

		case WM_LBUTTONDOWN:
			res = OnLButtonDown(MOUSE_POSX(lParam), MOUSE_POSY(lParam) );
			break ;

		case WM_LBUTTONUP:
			res = OnLButtonUp(MOUSE_POSX(lParam), MOUSE_POSY(lParam) );
			break ;

		case WM_MOUSEMOVE:
			if( wParam&MK_LBUTTON )
				res = OnMouseMove(MOUSE_POSX(lParam), MOUSE_POSY(lParam) );
			break;

		default:
			res = DefWndProc(message, wParam, lParam);
		}
		return res ;
    }
};


