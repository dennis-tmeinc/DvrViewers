// Player.cpp : implementation file
//

#include "stdafx.h"
#include "dvrclient.h"
#include "dvrclientDlg.h"
#include "decoder.h"
#include "SliderBar.h"

void SliderBar::OnAttach()
{
    RECT barRect ;
    RECT channelRect ;
    GetClientRect(m_hWnd, &barRect);
    SendMessage(m_hWnd, TBM_GETCHANNELRECT, 0, (LPARAM)&channelRect);

    m_lowfilepos = 0 ;
    m_highfilepos = 0 ;

    m_videopen = new Pen( Color(255,255,180,0),barRect.bottom-barRect.top );
    m_videopen->SetAlignment(PenAlignmentCenter);
    m_markpen = new Pen( Color(255,248,80,0),barRect.bottom-barRect.top );
    m_markpen->SetAlignment(PenAlignmentCenter);
    
    Bitmap * img  ;
    img = loadbitmap(_T("SLIDER_BAR"));
    m_barBrush = new TextureBrush(img);
#ifdef APP_TVS
    MoveWindow(m_hWnd, 0, 0, 100, 36, FALSE);
#else
    MoveWindow(m_hWnd, 0, 0, 100, img->GetHeight(),FALSE);
#endif
    delete img ;

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
    SendMessage(m_hWnd, TBM_SETTHUMBLENGTH, 24, 0 );
    m_screen=NULL;
    memset(&m_playertime,0,sizeof(&m_playertime));
    m_playertime.year=1980 ;
    m_playertime.month=1;
    m_playertime.day=1;
}

void SliderBar::OnPaint()
{
    PAINTSTRUCT ps;
    HDC hdc;
    hdc = BeginPaint(m_hWnd, &ps);
    Gdiplus::Graphics * gbar = new Graphics (hdc);

    int i;
    RECT barRect, thumbRect, channelRect ;

    GetClientRect(m_hWnd, &barRect);
    Bitmap paintbmp( (barRect.right-barRect.left), (barRect.bottom-barRect.top), gbar );
    Gdiplus::Graphics * g = new Graphics (&paintbmp);

    // Paint Backgound
    g->FillRectangle( m_barBrush, (INT)ps.rcPaint.left, barRect.top, (ps.rcPaint.right-ps.rcPaint.left), (barRect.bottom-barRect.top) );
    int SliderMax, SliderMin ;
    SliderMin = GetRangeMin();
    SliderMax = GetRangeMax();

    if( IsWindowEnabled(m_hWnd) && SliderMax>SliderMin ) {
        SendMessage(m_hWnd, TBM_GETCHANNELRECT, 0, (LPARAM)&channelRect);
        SendMessage(m_hWnd, TBM_GETTHUMBRECT, 0,(LPARAM)&thumbRect );

        // Paint Channel
        int cy = (barRect.top+barRect.bottom)/2 ;
        int poswidth =	SliderMax-SliderMin+1; // in seconds
        int channelmin, channelmax ;
        channelmin = channelRect.left+4 ;
        channelmax = channelRect.right-4 ;
        int channelwidth = channelmax - channelmin ;

        m_lowfilepos = SliderMax ;
        m_highfilepos = SliderMin ;

        int infosize=0 ;
        struct cliptimeinfo * timeinfo ;
        if( m_screen && m_playertime.year>=2000 && m_playertime.year<2100 ) {
            infosize = m_screen->getcliptime( &m_playertime, &timeinfo );
        }

        for( i=0; i<infosize; i++ ) {
            // retieve begin and end time of .DVR file
            if( m_lowfilepos>timeinfo[i].on_time ){
                m_lowfilepos = timeinfo[i].on_time ;
            }
            if( m_highfilepos<timeinfo[i].off_time ) {
                m_highfilepos=timeinfo[i].off_time;
            }
            if( timeinfo[i].on_time>=SliderMax ){
                break;
            }
            if( timeinfo[i].off_time<=SliderMin ) {
                continue ;
            }

            if( timeinfo[i].on_time<=timeinfo[i].off_time ) {
                int x1, x2 ;
                x1 = channelmin + (timeinfo[i].on_time-SliderMin) * channelwidth / poswidth ;
                x2 = channelmin + (timeinfo[i].off_time-SliderMin+2) * channelwidth / poswidth ;
                if( x1 < channelmin ){
                    x1 = channelmin-4 ;
                }
                if( x2 > channelmax ) {
                    x2 = channelmax+4 ;
                }
                if( x1<ps.rcPaint.left) {
                    x1 = ps.rcPaint.left ;
                }
                if( x2>ps.rcPaint.right ) {
                    x2 = ps.rcPaint.right ;
                }
                if( x2>=x1 ) {
                    g->DrawLine(m_videopen, x1, cy, x2, cy);
                }
            }
        }

        infosize = 0 ;
        if( m_screen && m_playertime.year>=2000 && m_playertime.year<2100 ) {
            infosize = m_screen->getlocktime( &m_playertime, &timeinfo );
        }
        for( i=0; i<infosize; i++ ) {
            // retieve begin and end time of .DVR file
            if( m_lowfilepos>timeinfo[i].on_time ){
                m_lowfilepos = timeinfo[i].on_time ;
            }
            if( m_highfilepos<timeinfo[i].off_time ) {
                m_highfilepos=timeinfo[i].off_time;
            }
            if( timeinfo[i].on_time>=SliderMax ){
                break;
            }
            if( timeinfo[i].off_time<=SliderMin ) {
                continue ;
            }

            if( timeinfo[i].on_time<=timeinfo[i].off_time ) {
                int x1, x2 ;
                x1 = channelmin + (timeinfo[i].on_time-SliderMin) * channelwidth / poswidth ;
                x2 = channelmin + (timeinfo[i].off_time-SliderMin+2) * channelwidth / poswidth ;
                if( x1 < channelmin ){
                    x1 = channelmin-4 ;
                }
                if( x2 > channelmax ) {
                    x2 = channelmax+4 ;
                }
                if( x1<ps.rcPaint.left) {
                    x1 = ps.rcPaint.left ;
                }
                if( x2>ps.rcPaint.right ) {
                    x2 = ps.rcPaint.right ;
                }
                if( x2>=x1 ) {
                    g->DrawLine(m_markpen, x1, cy, x2, cy);
                }
            }
        }

        // output tic text
		int fontheight = (barRect.bottom-barRect.top-6) ;
		if( fontheight > 28 ) fontheight = 28 ;
        Gdiplus::Font        font( FontFamily::GenericSansSerif(), fontheight, FontStyleRegular, UnitPixel);
        Gdiplus::PointF      pointF(30.0f, 10.0f);
        StringFormat stringFormat;

        SolidBrush brush( Color(0xff,0,0,0) );
        int ticknum ;
        ticknum=channelwidth / 120 ;
        if( ticknum>=12 )
            ticknum=12 ;
        else if( ticknum>=8 )
            ticknum=8 ;
        else if( ticknum>=6)
            ticknum=6 ;
        else if( ticknum>=4 )
            ticknum=4 ;

        string ticmark ;
        int pos = channelmin ;
        int ptime = SliderMin ;

		ticmark.printf( "%02d:%02d", ptime/3600, ptime%3600/60 );
        pointF.X = pos ;
        pointF.Y = barRect.top ;
        stringFormat.SetAlignment(StringAlignmentNear);
        g->DrawString(ticmark, -1, &font, pointF, &stringFormat, &brush);

        SliderMax++ ;
        stringFormat.SetAlignment(StringAlignmentCenter);
        for( i=2; i<=ticknum; i+=1 ) {
            pos += channelwidth/ticknum ;
            ptime += (SliderMax-SliderMin)/ticknum ;
			ticmark.printf("%02d:%02d", ptime/3600, ptime%3600/60 );
            pointF.X = pos ;
            g->DrawString(ticmark, -1, &font, pointF, &stringFormat, &brush);
        }

        stringFormat.SetAlignment(StringAlignmentFar);
		ticmark.printf( "%02d:%02d", SliderMax/3600, SliderMax%3600/60 );
        pointF.X = channelmax ;
        g->DrawString(ticmark, -1, &font, pointF, &stringFormat, &brush);

        // draw Thumb
        g->DrawImage(m_thumbimg, (INT)thumbRect.left, thumbRect.top, (thumbRect.right-thumbRect.left), (barRect.bottom-thumbRect.top) );
    }
    else {
        memset(&m_playertime,0,sizeof(&m_playertime));
        m_playertime.year=1980 ;
        m_playertime.month=1;
        m_playertime.day=1;
    }

    gbar->DrawImage(&paintbmp,  
        (INT)ps.rcPaint.left, (INT)ps.rcPaint.top,  
        (INT)ps.rcPaint.left, (INT)ps.rcPaint.top, 
        ps.rcPaint.right-ps.rcPaint.left, ps.rcPaint.bottom-ps.rcPaint.top, UnitPixel);
    delete g ;
    delete gbar ;

    EndPaint(m_hWnd, &ps);
}

void SliderBar::SetPos(dvrtime * dvrt)
{
    m_playertime = * dvrt ;
    int npos = dvrt->hour * 3600 + dvrt->min * 60 + dvrt->second ;
    SetPos(npos);
}