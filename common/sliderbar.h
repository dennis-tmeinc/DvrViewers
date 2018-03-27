#pragma once

// Sound VolumeBar

#include "../common/cstr.h"
#include "../common/cwin.h"
#include "../common/util.h"
#include "Screen.h"

#define BUTTOMBARCOLOR			( RGB(0x83, 0x8d, 0x3c) )

// dont use it now
class SliderBar : public Window
{
    Bitmap * m_paintbmp ;
    Bitmap * m_thumbimg ;
    Screen * m_screen ;
    struct dvrtime m_playertime ;
    TextureBrush  * m_barBrush;
    Pen    *  m_videopen ;
    Pen    *  m_markpen ;

public:
// Constructors
    int m_lowfilepos ;
    int m_highfilepos ;

	SliderBar(){
		m_thumbimg=NULL ;
		m_barBrush=NULL ;
		m_videopen=NULL ;
		m_markpen=NULL ;
	}

	~SliderBar()
	{
		if(m_thumbimg) delete m_thumbimg ;
		if(m_barBrush) delete m_barBrush ;
		if(m_videopen) delete m_videopen ;
		if(m_markpen) delete m_markpen ;
	}

    void SetScreen(Screen * sc) {
        m_screen = sc ;
    }

    int GetPos() {
        return ::SendMessageA( m_hWnd, TBM_GETPOS, 0, 0 ) ;
    }

    void SetPos(dvrtime * dvrt) ;

    int SetPos(int pos) {
        return ::SendMessageA( m_hWnd, TBM_SETPOS, TRUE, pos );
    }

	int GetChannelRect( PRECT prect ) {
	     return ::SendMessage(m_hWnd, TBM_GETCHANNELRECT, 0, (LPARAM)prect);
	}
	
	int GetRangeMin() {
        return SendMessage(m_hWnd, TBM_GETRANGEMIN, 0, (LPARAM)0);
    }

    int GetRangeMax() {
        return SendMessage(m_hWnd, TBM_GETRANGEMAX, 0, (LPARAM)0);
    }

    void SetRange( int min, int max ) {
        if( max>min ) {
            SendMessage(m_hWnd, TBM_SETRANGEMIN, FALSE, (LPARAM)min);
            SendMessage(m_hWnd, TBM_SETRANGEMAX, TRUE, (LPARAM)max);
        }
		InvalidateRect(m_hWnd, NULL, FALSE );
    }

protected:
	virtual void OnAttach();
    virtual void OnPaint();

	virtual int OnEraseBkgnd(HDC hdc){
        return 1 ;      // do not paint background
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
            res = 0 ;
			break;
		default:
			res = DefWndProc(message, wParam, lParam);
		}
		return res ;
    }
};
