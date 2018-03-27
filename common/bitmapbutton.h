// Bitmap Button
//
#ifndef __BITMAPBUTTON_H__
#define __BITMAPBUTTON_H__

#include "../common/cstr.h"
#include "../common/cwin.h"
#include "../common/util.h"

// BitmapButton - push-button with 2 bitmap images
class BitmapButton : public Window
{
protected:
	// all bitmaps must be the same size
	string m_bitmap;           // normal image
	string m_bitmapSel;        // selected image

public:
	BitmapButton() {
	}

	BitmapButton(LPCTSTR lpszBitmapResource,
			LPCTSTR lpszBitmapResourceSel )
    {
       m_bitmap = lpszBitmapResource ;
       m_bitmapSel = lpszBitmapResourceSel ;
    }

	void SetBitmap(LPCTSTR lpszBitmapResource,
			LPCTSTR lpszBitmapResourceSel)
    {
       m_bitmap = lpszBitmapResource ;
       m_bitmapSel = lpszBitmapResourceSel ;
    }

protected:

	void OnPaint()
	{
		PAINTSTRUCT ps;
		HDC hdc;
		hdc = BeginPaint(m_hWnd, &ps);
        Gdiplus::Graphics * g = new Graphics (hdc);
        RECT rt ;
		Bitmap* bm ;

        int state = ::SendMessage(m_hWnd, BM_GETSTATE, 0, 0 );
        if( (state&BST_PUSHED)!=0 ) {
			bm = loadbitmap( m_bitmapSel );
            if( bm ){
                ::GetClientRect(m_hWnd, &rt);
                g->DrawImage( bm, 0, 0, rt.right, rt.bottom);
				delete bm ;
            }
        }
        else {
			bm = loadbitmap( m_bitmap );
            if( bm ){
                ::GetClientRect(m_hWnd, &rt);
                g->DrawImage( bm, 0, 0, rt.right, rt.bottom);
				delete bm ;
            }
        }

        delete g ;
		EndPaint(m_hWnd, &ps);
		return  ;
    }

	virtual LRESULT WndProc(UINT message, WPARAM wParam, LPARAM lParam)
    {
        LRESULT res = 0 ;
		switch (message)
		{
		case WM_PAINT:
		    OnPaint();
			break;
		case WM_ERASEBKGND:
			res = 1 ;
			break;
		default:
            return Window::WndProc( message, wParam, lParam );
		}
		return res ;
	}
};

class AutoBitmapButton : public BitmapButton
{
protected:
    virtual void OnDetach() {
		delete this ;
	} ;
};

#endif  // __BITMAPBUTTON_H__