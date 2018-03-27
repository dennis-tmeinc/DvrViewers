#pragma once

// Sound VolumeBar

#include "../common/cstr.h"
#include "../common/cwin.h"
#include "../common/util.h"

#define BUTTOMBARCOLOR			( RGB(0x83, 0x8d, 0x3c) )

// dont use it now
class VolumeBar : public Window
{
    HWND    m_hparent ;
	Bitmap * m_barimg ;
	Bitmap * m_knobimg ;
	Bitmap * m_knobselimg ;
public:
// Constructors
   	VolumeBar(){
        m_barimg = NULL ;
        m_knobimg = NULL ;
        m_knobselimg = NULL ;
    }

	~VolumeBar() {
        if( m_barimg) delete m_barimg ;
        if( m_knobimg) delete m_knobimg ;
        if( m_knobselimg) delete m_knobselimg ;
	}

	VolumeBar(HWND hParent, int id){
		m_barimg = NULL;
		m_knobimg = NULL;
		m_knobselimg = NULL;
    	Create(hParent, id);
    }

	void Create(HWND hParent, int id) ;

    int SetPos(int pos) {
        return ::SendMessageA( m_hWnd, TBM_SETPOS, TRUE, pos );
    }

    int GetPos() {
        return ::SendMessageA( m_hWnd, TBM_GETPOS, 0, 0 ) ;
    }

    void Hide() {
		if( m_hWnd )
			ShowWindow(m_hWnd, SW_HIDE);
    }

protected:

	int OnEraseBkgnd(HDC hdc){
        return 1 ;      // do not paint background
    }
	int OnPaint();
	int OnDestroy();

	virtual LRESULT WndProc(UINT message, WPARAM wParam, LPARAM lParam) {
		LRESULT res = 0 ;
		switch (message)
		{
		case WM_ERASEBKGND:
			res = OnEraseBkgnd((HDC)wParam);
			break;
		case WM_PAINT:
			res = OnPaint();
			break;
		case WM_DESTROY:
			res = OnDestroy();
			break;
		default:
			res = DefWndProc(message, wParam, lParam);
		}
		return res ;
    }
};
