#pragma once

#include "../common/cstr.h"
#include "../common/cwin.h"
#include "../common/util.h"

class Covertscreen : public Window
{
public:

	int result ;
	int m_screenoff ;
	int m_sleeptime ;

	Covertscreen( HWND hparent )
	{
		result = 0 ;
		HWND hwnd = CreateWindowEx( 0, 
			WinClass((HBRUSH)GetStockObject(BLACK_BRUSH),0),
			_T("Covert"),
			WS_POPUP,
			CW_USEDEFAULT,          // default horizontal position  
			CW_USEDEFAULT,          // default vertical position    
			CW_USEDEFAULT,          // default width                
			CW_USEDEFAULT,          // default height    
			hparent, 
			NULL, 
			AppInst(), 
			NULL );
		attach(hwnd) ;
		m_screenoff = 0 ;
		ShowWindow(hwnd, SW_SHOWMAXIMIZED );
		m_sleeptime = 1800000 ;
		SetTimer(m_hWnd, 1, m_sleeptime, NULL );
	}
	
protected:
		
	LRESULT OnLButtonDown( int button, int x, int y )
	{
		SendMessage((HWND)m_hWnd, WM_SYSCOMMAND, SC_MONITORPOWER, -1);

		if( x<20 && y<20 ) {
			result = 1 ;	// to ask close application
		}

		PostMessage(m_hWnd, WM_CLOSE, 0, 0 );

		return 0 ;
	}


	LRESULT OnTimer()
	{
		SendMessage((HWND)m_hWnd, WM_SYSCOMMAND, SC_MONITORPOWER, 2);
		KillTimer(m_hWnd, 1);
		m_screenoff = 1 ;
		return 0 ;
	}

	LRESULT OnActivate( int t )
	{
		if(m_screenoff && t==1 ) {
			SendMessage((HWND)m_hWnd, WM_SYSCOMMAND, SC_MONITORPOWER, -1);
			PostMessage(m_hWnd, WM_CLOSE, 0, 0 );
		}
		return 0 ;
	}

	virtual LRESULT WndProc(UINT uMsg, WPARAM wParam, LPARAM lParam) {
		LRESULT res ;
        switch (uMsg)
        {
        case WM_LBUTTONDOWN:
            res = OnLButtonDown( wParam,  LOWORD(lParam), HIWORD(lParam) );
			break ;

		case WM_TIMER:
			res = OnTimer();
			break;
		
		case WM_ACTIVATE :
			res = OnActivate(wParam);
			break;

		default:
			res = DefWndProc(uMsg, wParam, lParam);
		}
		return res ;
    }

    
};

