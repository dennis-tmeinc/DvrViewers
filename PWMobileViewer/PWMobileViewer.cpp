// PWViewerInCar.cpp : Defines the entry point for the application.
//
#include "stdafx.h"
#include "PWMobileViewer.h"
#include "dvrclient.h"
#include "dvrclientDlg.h"
#include "UsbPassword.h"

struct btbar_ctl btbar_ctls[] = {
	{0, 0, NULL }
} ;

const int control_align = 0 ;       // control layout, 0: left, 1: right ;
const int btbar_refid = IDC_PIC_SPEAKER_VOLUME;	// Volue Slider as ref
const int btbar_align = -174;		// >=0: left align, <0: right align

struct bt_layout playrect_ctls[] = {
	// id,					posx, posy,	w,	h, showmask
	{IDC_STATIC_PANORAMA,   10,  -322, 296,120, 255},
	{IDC_BUTTON_PANLEFT,	10,  -302, 96, 80, 255},
	{IDC_BUTTON_PANCENTER,	110, -302, 96, 80, 255},
	{IDC_BUTTON_PANRIGHT,	210, -302, 96, 80, 255},

	{IDC_BUTTON_REARVIEW,  10,   -210, 96, 80, 255},
	{IDC_BUTTON_HOODVIEW,  110,  -210, 96, 80, 255},
	{IDC_BUTTON_AUXVIEW,   210,  -210, 96, 80, 255},
	
	{IDC_BUTTON_BTAG,      10,   -110, 96, 96, 255},
	{IDC_BUTTON_BOFFICER,  110,  -110, 96, 96, 255},
	{IDC_BUTTON_BCOVERT,   210,  -110, 96, 96, 255},

	{IDC_BUTTON_TRIOVIEW, -5000, 5000, 96, 80, 255},
	
	{IDC_BUTTON_CLOSE,     4,     4,   68, 68, 255},

	{-1,   0,0, 0, 0, 0},
};

struct bt_layout bottom_ctls[] = {
	// id,					posx, posy,	w,	h, showmask
	{IDC_BUTTON_BPLAYLIVE,  8,  0, 242, 64, 255},
	{IDC_BUTTON_BPLAYBACK,  8,  0, 242, 64, 255},
	{IDC_BUTTON_CAM1,       258, 0, 96, 96, 255},
	{IDC_BUTTON_BCAL,       258, 0, 96, 96, 255},

	{IDC_BUTTON_CAM2,       358, 0, 96, 96, 255},
	{IDC_BUTTON_BDAYRANGE,  358, 0, 96, 96, 255},
	{IDC_BUTTON_BHOURRANGE, 358, 0, 96, 96, 255},
	{IDC_BUTTON_BQUATRANGE, 358, 0, 96, 96, 255},

	{IDC_SLOW,              458, 0, 96, 96, 255},
	{IDC_BUTTON_TM,         458, 0, 96, 96, 255},

	{IDC_BACKWARD,          558, 0, 96, 96, 255},
	{IDC_BUTTON_LP,         558, 0, 96, 96, 255},

	{IDC_PAUSE,             654, 0, 110, 110, 255},
	{IDC_PLAY,              654, 0, 110, 110, 255},

	{IDC_FORWARD,           764, 0, 96, 96, 255},
	{IDC_FAST,              864, 0, 96, 96, 255},
	{IDC_STEP,              964, 0, 96, 96, 255},
	{IDC_PIC_SPEAKER_VOLUME,1064, 0, 80, 80, 255},
	{0, 0, 0, 0, 0, 0 }
};

struct bmp_button_type bmp_buttons[] = {
	{ IDC_BUTTON_TRIOVIEW, _T("PW_TRIOVIEW"),   _T("PW_TRIOVIEW_SEL") },
	{ IDC_BUTTON_PANLEFT,  _T("PW_PANLEFT"),    _T("PW_PANLEFT_SEL") },
	{ IDC_BUTTON_PANCENTER, _T("PW_PANCENTER"), _T("PW_PANCENTER_SEL") },
	{ IDC_BUTTON_PANRIGHT, _T("PW_PANRIGHT"), _T("PW_PANRIGHT_SEL") },
	{ IDC_BUTTON_HOODVIEW, _T("PW_HOODVIEW"), _T("PW_HOODVIEW_SEL") },
	{ IDC_BUTTON_REARVIEW, _T("PW_REARVIEW"), _T("PW_REARVIEW_SEL") },
	{ IDC_BUTTON_AUXVIEW, _T("PW_AUXVIEW"), _T("PW_AUXVIEW_SEL") },
	{ IDC_BUTTON_CLOSE, _T("PW_APPCLOSE"), _T("PW_APPCLOSE") },
	{ IDC_BUTTON_BTAG, _T("PW_BTAG"), _T("PW_BTAG") },
	{ IDC_BUTTON_BOFFICER, _T("PW_BOFFICER"), _T("PW_BOFFICER") },
	{ IDC_BUTTON_BCOVERT, _T("PW_BCOVERT"), _T("PW_BCOVERT") },

	{ -1, NULL, NULL }
};

class TouchWindow1 : public Window {
public:
	TouchWindow1() {
		TouchDown = 0 ;
		HWND hwnd = CreateWindowEx(0, WinClass(), _T("Touch"), WS_OVERLAPPEDWINDOW | WS_VSCROLL, 100, 100, 500, 500, 
			NULL, NULL, AppInst(), NULL );
		attach(hwnd);
	}

	int TouchDown ;
	POINT pt ;

protected:

	// use OnAttach() in place of WM_CREATE message
    virtual void OnAttach() {
		ShowWindow(m_hWnd, SW_SHOWNORMAL);

				// test for touch
		int value = GetSystemMetrics(SM_DIGITIZER);
		if (value & NID_READY){ /* stack ready */}
		if (value  & NID_MULTI_INPUT){
			/* digitizer is multitouch */ 
			MessageBoxW(m_hWnd, L"Multitouch found", L"IsMulti!", MB_OK);
		}

		RegisterTouchWindow( m_hWnd, 0 );

	} ;

	void onDestroy()
	{
		UnregisterTouchWindow( m_hWnd );
	}

	void OnTouch( WPARAM wParam, LPARAM lParam )
	{
		BOOL bHandled = FALSE;
		UINT cInputs = LOWORD(wParam);
		PTOUCHINPUT pInputs = new TOUCHINPUT[cInputs];
		if (pInputs){
			if (GetTouchInputInfo((HTOUCHINPUT)lParam, cInputs, pInputs, sizeof(TOUCHINPUT))){
				for (UINT i=0; i < cInputs; i++){
					TOUCHINPUT ti = pInputs[i];
					//do something with each touch input entry

					if( ti.dwFlags & TOUCHEVENTF_PRIMARY ) {
						Gdiplus::Graphics * g = new Graphics (m_hWnd);
						Gdiplus::Pen * p = new Pen( Color( 255, 0, 0 ) );

						POINT npt ;
						npt.x = ti.x/100  ;
						npt.y = ti.y/100  ;
						MapWindowPoints(NULL, m_hWnd, &npt, 1);
						
						if( ti.dwFlags & TOUCHEVENTF_DOWN ) {
							TouchDown = 1 ;
							g->DrawArc(p, pt.x, pt.y, 30, 30, 0, 350 );
							pt = npt ;
						}
						else if(  ti.dwFlags & TOUCHEVENTF_MOVE ) {
							TouchDown = 1 ;
							g->DrawLine( p, pt.x, pt.y, npt.x, npt.y );
							pt = npt ;
						}
						else if(  ti.dwFlags & TOUCHEVENTF_UP ) {
							TouchDown = 0 ;
						}

						delete p ;
						delete g ;
					}

				}            
			}
			delete [] pInputs;
		}
	}

	virtual LRESULT WndProc(UINT uMsg, WPARAM wParam, LPARAM lParam) {
		switch( uMsg ) {
		case WM_TOUCH:
			OnTouch( wParam, lParam );
			break;

		case WM_CLOSE:
			DestroyWindow(m_hWnd);
			break;

		}
        return DefWndProc(uMsg, wParam, lParam);
    }

} ;



class TouchWindow : public Window {
public:
	TouchWindow() {
		TouchDown = 0 ;
		HWND hwnd = CreateWindowEx(0, WinClass(), _T("Touch"), WS_OVERLAPPEDWINDOW | WS_VSCROLL, 100, 100, 500, 500, 
			NULL, NULL, AppInst(), NULL );
		attach(hwnd);
	}

	int TouchDown ;
	POINT pt ;

protected:

	// use OnAttach() in place of WM_CREATE message
    virtual void OnAttach() {
		ShowWindow(m_hWnd, SW_SHOWNORMAL);

				// test for touch
		int value = GetSystemMetrics(SM_DIGITIZER);
		if (value & NID_READY){ /* stack ready */}
		if (value  & NID_MULTI_INPUT){
			/* digitizer is multitouch */ 
			MessageBoxW(m_hWnd, L"Multitouch found", L"IsMulti!", MB_OK);
		}

		//RegisterTouchWindow( m_hWnd, 0 );

		// set up our want / block settings
		DWORD dwPanWant  = GC_PAN_WITH_SINGLE_FINGER_VERTICALLY | GC_PAN_WITH_SINGLE_FINGER_HORIZONTALLY  | GC_PAN_WITH_INERTIA ;
		DWORD dwPanBlock = GC_PAN_WITH_GUTTER ;

		// set the settings in the gesture configuration
		GESTURECONFIG gc[] = {{ GID_ZOOM, GC_ZOOM, 0 },
							  { GID_PAN, dwPanWant , dwPanBlock}                     
							 };    
                     
		UINT uiGcs = 2;
		BOOL bResult = SetGestureConfig(m_hWnd, 0, uiGcs, gc, sizeof(GESTURECONFIG));  

		if (!bResult){                
			DWORD err = GetLastError();                                       
		}

	} ;

	void OnDestroy()
	{
		//UnregisterTouchWindow( m_hWnd );
	}
	
	int OnLButtonDown( int x, int y ) 
	{
		return 0 ;
	}

	int OnLButtonUp( int x, int y ) 
	{
		return 0 ;
	}

	int OnMouseMove( int x, int y ) 
	{
		return 0 ;
	}
				
	void hittest( int x, int y ) 
	{
		POINT npt ;
		npt.x = x  ;
		npt.y = y  ;
		MapWindowPoints(NULL, m_hWnd, &npt, 1);
	}

	void OnPan( int flags, int x, int y )
	{

		POINT npt ;
		npt.x = x  ;
		npt.y = y  ;
		MapWindowPoints(NULL, m_hWnd, &npt, 1);

		if( TouchDown ) {
			Gdiplus::Graphics * g = new Graphics (m_hWnd);
			Gdiplus::Pen * p = new Pen( Color( 255, 0, 0 ) );
						
			g->DrawLine( p, pt.x, pt.y, npt.x, npt.y );
			pt = npt ;

			delete p ;
			delete g ;
		}

		if( flags & GF_BEGIN ) {
			pt = npt ;
			TouchDown = 1 ;
		}
		if( flags & GF_END ) {
			TouchDown = 0 ;
		}

	}

	int OnGesture(  WPARAM wParam, LPARAM lParam )
	{
				// Create a structure to populate and retrieve the extra message info.
		GESTUREINFO gi;  
    
		ZeroMemory(&gi, sizeof(GESTUREINFO));
    
		gi.cbSize = sizeof(GESTUREINFO);

		BOOL bResult  = GetGestureInfo((HGESTUREINFO)lParam, &gi);
		BOOL bHandled = FALSE;

		if (bResult){
			// now interpret the gesture
			switch (gi.dwID){
			   case GID_ZOOM:
				   // Code for zooming goes here     
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
			   case GID_TWOFINGERTAP:
				   // Code for two-finger tap goes here
				   bHandled = TRUE;
				   break;
			   case GID_PRESSANDTAP:
				   // Code for roll over goes here
				   bHandled = TRUE;
				   break;
			   default:
				   // A gesture was not recognized
				   break;
			}
		}else{
			DWORD dwErr = GetLastError();
			if (dwErr > 0){
				//MessageBoxW(hWnd, L"Error!", L"Could not retrieve a GESTUREINFO structure.", MB_OK);
			}
		}
		if (bHandled){
			return 0;
		}else{
			return DefWndProc( WM_GESTURE, wParam, lParam);
		}
	}

	virtual LRESULT WndProc(UINT uMsg, WPARAM wParam, LPARAM lParam) {
		LRESULT res = 0 ;
		switch( uMsg ) {

		case WM_GESTURE:
			res = OnGesture( wParam, lParam );
			break;

		case WM_NCHITTEST:
			hittest( HIWORD(lParam), LOWORD(lParam) );
			return Window::WndProc(uMsg, wParam, lParam);

		case WM_LBUTTONDOWN:
			res = OnLButtonDown(HIWORD(lParam), LOWORD(lParam) );
			break ;

		case WM_LBUTTONUP:
			res = OnLButtonUp(HIWORD(lParam), LOWORD(lParam) );
			break ;

		case WM_MOUSEMOVE:
			res = OnMouseMove(HIWORD(lParam), LOWORD(lParam) );
			break;

		case WM_CLOSE:
			DestroyWindow(m_hWnd);
			break;

		case WM_DESTROY:
			OnDestroy();
			break ;

		default :
			return Window::WndProc(uMsg, wParam, lParam);
		}
        return res ;
    }

} ;


int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hInstance);
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(nCmdShow);

	DvrclientDlg dlg;
	dlg.DoModal();

	return 0 ;
}
