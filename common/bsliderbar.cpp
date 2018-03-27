// Player.cpp : implementation file
//

#include "stdafx.h"
#include "dvrclient.h"
#include "dvrclientDlg.h"
#include "decoder.h"
#include "SliderBar.h"

int BSliderBar::OnSetPos( int x )
{	
	int npos ;
	RECT tRect ;
	GetChannelRect(&tRect);
	npos = (x-tRect.left)*(m_max-m_min)/(tRect.right-tRect.left)+m_min ;
	if( npos<m_min ) npos = m_min ;
	if( npos>=m_max ) npos = m_max-1 ;
	if( m_pos != npos ) {
		SetPos( npos ) ;
		m_touchdown = 1 ;

		SetTimer(m_hparent, TIMER_SEEK, 30, NULL );			// seek player
		SetTimer(m_hparent, TIMER_UPDATE, 2000, NULL );		// delay TIMER_UPDATE
	}

	return 0 ;
}

void BSliderBar::OnPan( int flags, int x, int y )
{
	DvrclientDlg * mdlg ;
	POINT npt ;
	npt.x = x  ;
	npt.y = y  ;
	MapWindowPoints(NULL, m_hWnd, &npt, 1);

	if( flags & GF_BEGIN ) {
		int tpos = GetThumbPos();
		if( (tpos-npt.x)<60 && (tpos-npt.x)>-60 ) {
			OnSetPos( npt.x );
		}
		else {
			m_inpan = 1 ;
			m_panpos = npt.x ;
		}
	}
	else if( flags & GF_END ) {
		if( m_inpan ) {
			if( m_inpan > 200 ) {
				mdlg = (DvrclientDlg *)Window::getWindow( m_hparent );
				mdlg->OnBnClickedPrev() ;
			}
			else if( m_inpan < -200 ) {
				mdlg = (DvrclientDlg *)Window::getWindow( m_hparent );
				mdlg->OnBnClickedNext() ;
			}
			else {
				InvalidateRect( m_hWnd, NULL, false );
			}
			m_inpan = 0 ;
		}
	}
	else if( m_touchdown )  {
		OnSetPos( npt.x );
	}
	else if( m_inpan ) {
		m_inpan = npt.x - m_panpos ;
		InvalidateRect( m_hWnd, NULL, false );
	}

}

void BSliderBar::OnZoom( int flags, int dist )
{
	DvrclientDlg * mdlg ;
	int trange ;
	if( flags & GF_BEGIN ) {
		m_inzoom = dist ;
		m_zoompos = dist ;
	}
	else if( flags & GF_END ) {
		int set = 0 ;
		DvrclientDlg * mdlg = (DvrclientDlg *)Window::getWindow( m_hparent );
		if( dist - m_inzoom > 100 ) {
			// zoom in
			trange = mdlg->GetTimeRange();
			if( trange<2 ) {
				mdlg->SetTimeRange( trange+1 );
				set = 1 ;
			}
		}
		else if( dist - m_inzoom < -100 ) {
			// zoom out
			trange = mdlg->GetTimeRange();
			if( trange>0 ) {
				mdlg->SetTimeRange( trange-1 );
				set = 1 ;
			}
		}
		if( set==0 ) 
			InvalidateRect(m_hWnd, NULL, false );
		m_inzoom = 0 ;
	}
	else if( m_inzoom ) {
		m_zoompos = dist ;
		InvalidateRect(m_hWnd, NULL, false );
	}
}

void BSliderBar::OnPaint()
{
    PAINTSTRUCT ps;
    HDC hdc;
    hdc = BeginPaint(m_hWnd, &ps);
    Gdiplus::Graphics * g = new Graphics (hdc);
	Gdiplus::Graphics * gbar ;
	Gdiplus::Bitmap * bar ;

    int i;
    RECT thumbRect, channelRect ;

    GetClientRect(m_hWnd, &m_barRect);
	GetChannelRect( &channelRect );

	bar = new Bitmap( (m_barRect.right-m_barRect.left), (m_barRect.bottom-m_barRect.top), g );
	gbar = new Graphics (bar);
		
	// Paint Backgound
	gbar->FillRectangle( m_barBrush, (INT)ps.rcPaint.left, m_barRect.top, (ps.rcPaint.right-ps.rcPaint.left), (m_barRect.bottom-m_barRect.top) );

	int SliderMax, SliderMin ;
	SliderMin = GetRangeMin();
	SliderMax = GetRangeMax();

	if( IsWindowEnabled(m_hWnd) && SliderMax>SliderMin ) {

		// Paint Channel
		int cy = (channelRect.top+channelRect.bottom)/2 ;
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
					gbar->DrawLine(m_videopen, x1, cy, x2, cy);
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
					gbar->DrawLine(m_markpen, x1, cy, x2, cy);
				}
			}
		}

		// output tic text
		Gdiplus::Font        font( FontFamily::GenericSansSerif(), (18), FontStyleRegular, UnitPixel);
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

		sprintf( ticmark.strsize(10), "%02d:%02d", ptime/3600, ptime%3600/60 );
		pointF.X = pos ;
		pointF.Y = (channelRect.top+channelRect.bottom)/2 - 12  ;
		stringFormat.SetAlignment(StringAlignmentNear);
		gbar->DrawString(ticmark, -1, &font, pointF, &stringFormat, &brush);

		SliderMax++ ;
		stringFormat.SetAlignment(StringAlignmentCenter);
		for( i=2; i<=ticknum; i+=1 ) {
			pos += channelwidth/ticknum ;
			ptime += (SliderMax-SliderMin)/ticknum ;
			sprintf( ticmark.strsize(10), "%02d:%02d", ptime/3600, ptime%3600/60 );
			pointF.X = pos ;
			gbar->DrawString(ticmark, -1, &font, pointF, &stringFormat, &brush);
		}

		stringFormat.SetAlignment(StringAlignmentFar);
		sprintf( ticmark.strsize(10), "%02d:%02d", SliderMax/3600, SliderMax%3600/60 );
		pointF.X = channelmax ;
		gbar->DrawString(ticmark, -1, &font, pointF, &stringFormat, &brush);

		// draw thumb 
		GetThumbRect( &thumbRect );
        gbar->DrawImage(m_thumbimg, (INT)thumbRect.left, thumbRect.top, (thumbRect.right-thumbRect.left), (thumbRect.bottom-thumbRect.top) );

	}
	else {
		memset(&m_playertime,0,sizeof(&m_playertime));
		m_playertime.year=1980 ;
		m_playertime.month=1;
		m_playertime.day=1;
	}
	
	g->DrawImage( bar ,  
        (INT)ps.rcPaint.left, (INT)ps.rcPaint.top,  
        (INT)ps.rcPaint.left, (INT)ps.rcPaint.top, 
        ps.rcPaint.right-ps.rcPaint.left, ps.rcPaint.bottom-ps.rcPaint.top, UnitPixel);

	delete gbar ;
	delete bar ;
	delete g ;

    EndPaint(m_hWnd, &ps);
}

void BSliderBar::SetPos(dvrtime * dvrt)
{
	if( m_touchdown==1 ) return ;
    m_playertime = * dvrt ;
	SetPos(dvrt->hour * 3600 + dvrt->min * 60 + dvrt->second) ;
}