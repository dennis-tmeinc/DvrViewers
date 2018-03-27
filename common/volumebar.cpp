// Player.cpp : implementation file
//

#include "stdafx.h"
#include "dvrclient.h"
#include "dvrclientDlg.h"
#include "decoder.h"
#include "Volumebar.h"

void VolumeBar::Create(HWND hParent, int id)
{
    m_hparent = hParent ;
    m_barimg = loadbitmap( _T("VOLUME_BAR"));
    m_knobimg = loadbitmap( _T("VOLUME_KNOB"));
    m_knobselimg = loadbitmap( _T("VOLUME_KNOB_SEL"));
    HWND hWnd = CreateWindowEx( 
        0,                             // no extended styles 
        TRACKBAR_CLASS,          
        _T("volume"),           
        WS_CHILD | WS_VISIBLE | 
        TBS_HORZ | TBS_BOTH | TBS_NOTICKS | TBS_TOOLTIPS | WS_TABSTOP,  // style 
        10, 10,                        // position 
        80, 30,                       // size 
        hParent,                       // parent window 
        (HMENU)id,             // control identifier 
        AppInst(),                      // instance 
        NULL                           // no WM_CREATE parameter 
        ); 
    attach(hWnd) ;
    ::SendMessage( m_hWnd, TBM_SETRANGEMIN, FALSE, 0 );
    ::SendMessage( m_hWnd, TBM_SETRANGEMAX, FALSE, 100 );
	int v = reg_read("volume");
	if (v < 0 || v>100)
		v = 100;
		SetPos(v);
    ShowWindow(m_hWnd, SW_SHOW);
}

int VolumeBar::OnDestroy()
{
	reg_save("volume", GetPos());
	return 0;
}

int VolumeBar::OnPaint()
{
    PAINTSTRUCT ps;
    HDC hdc;

    RECT ptrect, imgrect ;

    GetClientRect(m_hWnd, &ptrect);
    Bitmap barbmp(ptrect.right, ptrect.bottom);
    Gdiplus::Graphics * g = new Graphics (&barbmp);

    Image * bkimg = g_maindlg->getbkimg() ;
    if( bkimg ) {
        RECT bottombar ;
        RECT maprect = ptrect ;
        g_maindlg->getBottombar(&bottombar);
        MapWindowPoints(m_hWnd, m_hparent, (LPPOINT)&maprect, 2 );
        g->DrawImage( bkimg, 
            (INT)ptrect.left, ptrect.top, 
            maprect.left-bottombar.left, maprect.top-bottombar.top, ptrect.right-ptrect.left, ptrect.bottom-ptrect.top, 
            UnitPixel );
    }
    else {
        Color color ;
        color.SetFromCOLORREF( BOTTOMBARCOLOR );
        SolidBrush brush( color );
        g->FillRectangle( &brush, (INT)ptrect.left, ptrect.top, (ptrect.right-ptrect.left), (ptrect.bottom-ptrect.top) );
    }

    SendMessage(m_hWnd, TBM_GETCHANNELRECT, 0,(LPARAM)&ptrect );

    imgrect.top=imgrect.left=0;
    imgrect.bottom=m_barimg->GetHeight();
    imgrect.right=m_barimg->GetWidth();
    ptrect.bottom=ptrect.top+imgrect.bottom ;
    g->DrawImage( m_barimg, (INT)ptrect.left, ptrect.top, ptrect.right-ptrect.left, ptrect.bottom-ptrect.top );

    SendMessage(m_hWnd, TBM_GETTHUMBRECT, 0,(LPARAM)&ptrect );
    if( m_hWnd == GetCapture() ) {
        g->DrawImage( m_knobselimg, 
            (int)((ptrect.right+ptrect.left)/2-m_knobselimg->GetWidth()/2) ,
            (int)((ptrect.top+ptrect.bottom)/2-m_knobselimg->GetHeight()/2) );
    }
    else {
        g->DrawImage( m_knobimg, 
            (int)((ptrect.right+ptrect.left)/2-m_knobimg->GetWidth()/2) ,
            (int)((ptrect.top+ptrect.bottom)/2-m_knobimg->GetHeight()/2) );
    }

    InvalidateRect(m_hWnd, NULL, FALSE);
    hdc = BeginPaint(m_hWnd, &ps);
    Gdiplus::Graphics * gbar = new Graphics (hdc);
    gbar->DrawImage( &barbmp, 0,0 );
    delete gbar ;
    delete g ;
    EndPaint(m_hWnd, &ps);
    return 0 ;
}