#pragma once

#include "stdafx.h"

#include "../common/cstr.h"
#include "../common/cwin.h"
#include "../common/util.h"

#include "dvrclient.h"
#include "dvrclientDlg.h"
#include "screen.h"
#include "decoder.h"

#include "cliplist.h"

static DWORD WINAPI cliplist_snapshot_thread( LPVOID lpParameter );

Cliplist::Cliplist(DvrclientDlg * parent, Screen * screen, int lockclip, dvrtime * playtime, char * tmpdir)
{
    int i;
    m_parent = parent ;
    m_screen = screen ;
    m_clipday = *playtime ;
    m_tmpdir = tmpdir ;
    m_select = 0 ;          // by default, select first clip
    m_mode = 0 ;
    m_clip_perline = NUM_CLIP_PERLINE ;

    HWND hWnd = CreateWindowEx( 0, 
        WinClass(),
        _T("CLIPLIST"),
        WS_CHILD|WS_VSCROLL|WS_BORDER,
        0,          // default horizontal position  
        0,          // default vertical position    
        1,          // default width                
        1,          // default height    
        m_parent->getHWND(), 
        NULL, 
        AppInst(), 
        NULL );
    attach(hWnd) ;

    int eventnumber = EVENT_NUMBER ;
    string eventnumberfile ("eventnumber") ;
    if( getfilepath(eventnumberfile) ) {
        FILE * fen = fopen( eventnumberfile, "r" );
        if( fen ) {
            fscanf(fen, "%d", &eventnumber );
            fclose(fen);
        }
    }

    m_elist=NULL ;
    if( m_screen->m_decoder ) {
        m_elistsize = m_screen->m_decoder->geteventtimelist(NULL, eventnumber, NULL, 0 );
        if( m_elistsize > 0 ) {
            m_elist = new int [m_elistsize] ;
            m_elistsize = m_screen->m_decoder->geteventtimelist(NULL, eventnumber, m_elist, m_elistsize );

            // verify if event time are within valid video area
            struct cliptimeinfo * cti ;
            int clipnum = m_screen->getcliptime( playtime, &cti );
            int vi, ci, i ;
            vi=ci=0;
            for( i=0 ; i<clipnum && ci<m_elistsize ; i++ ) {
                // skip invalid time
                while( m_elist[ci]<cti[i].on_time && ci<m_elistsize ) { 
                    ci++ ;
                }
                // copy valid time
                while( m_elist[ci]<cti[i].off_time && ci<m_elistsize ) { 
                    m_elist[vi] = m_elist[ci] ;
                    vi++ ;
                    ci++ ;
                }
            }
            m_elistsize = vi ;      // valid time list size
        }
        else {
            m_elistsize=0 ;
        }
    }
    else {
        m_elistsize=0;
    }

    if( m_elistsize>0 ) {
        m_updlist = new int [m_elistsize] ;
        for(i=0;i<m_elistsize;i++) {
            m_updlist[i]=0;
        }
    }
    else {
        m_updlist = NULL ;
    }

    int pages = (m_elistsize+m_clip_perline-1)/m_clip_perline ;          // one row pics as a page
    int ptime = playtime->hour*3600 + playtime->min*60 + playtime->second ;
    for( m_select = m_elistsize-1 ; m_select>0; m_select-- ) {
        if( m_elist[m_select]<ptime ) {
            break ;
        }
    }

    SCROLLINFO scif ;
    scif.cbSize = sizeof(scif);
    scif.fMask = SIF_RANGE | SIF_PAGE | SIF_POS ;
    scif.nMin = 0 ;
    scif.nMax = pages*NUM_CLIP_PAGE-1 ;                // total lines
    scif.nPage = NUM_CLIP_PAGE ;                       // lines per page
    scif.nPos = NUM_CLIP_PAGE * (m_select / 3) ;
    SetScrollInfo( m_hWnd, SB_VERT, &scif, TRUE );

    m_screenpage=1 ;

    // Seek player to first clip time
    if( m_select>=0 && m_elist!=NULL ) {
        dvrtime dvrt ;
        getClipTime( & dvrt ) ;
        m_parent->SeekTime( &dvrt ) ;
    }

    m_selpen = new Gdiplus::Pen( Color(250,250,50) );
    m_nselpen = new Gdiplus::Pen( Color(0,0,0) );
    m_waitBrush = new Gdiplus::SolidBrush( Color(56, 119, 190));

    m_handgrabcursor = LoadCursor( ResInst(), _T("CURSORHANDGRAB"));

    for( i=0;i<MAX_CACHE;i++ ) {
        m_imgcache[i]=NULL ;
    }
    m_snapshot_run = 0 ;
    m_loadingimage = loadimage( _T("SPINNING_WAIT" ));
    m_loadingframe = 0 ;

    // init event 
    m_snapshot_event = CreateEvent(NULL, FALSE, FALSE, NULL );

    // create thread
    m_snapshot_thread = CreateThread( NULL, 0, cliplist_snapshot_thread, this, 0, NULL );

    ::ShowWindow(m_hWnd, SW_SHOW);

}

Cliplist::~Cliplist() 
{
    // wait if snapshot thread is running
    while( m_snapshot_run>0 ) {
        Sleep(10);
    }

    m_snapshot_run = -1 ;
    SetEvent(m_snapshot_event);

    WaitForSingleObject( m_snapshot_thread, 10000 );

    CloseHandle(m_snapshot_thread);
    m_snapshot_thread=NULL;
    CloseHandle(m_snapshot_event);
    m_snapshot_event=NULL;

    if( m_updlist ) {
        delete [] m_updlist ;
    }

    DestroyCursor(m_handgrabcursor);
    if( m_elist ) {
        delete m_elist ;
    }
    delete m_selpen ;
    delete m_nselpen ;

    int i;
    for( i=0;i<MAX_CACHE;i++ ) {
        if( m_imgcache[i]!=NULL ) {
            delete m_imgcache[i];
        }
    }
    delete m_loadingimage ;
}

int Cliplist::getClipTime( dvrtime * dvrt )
{
    int i=0 ;
    if( m_select>=0 && m_select<m_elistsize ) {
        i=m_select ;
    }

    *dvrt = m_clipday ;
    dvrt->hour = m_elist[i]/3600 ;
    dvrt->min = (m_elist[i]%3600)/60 ;
    dvrt->second = m_elist[i]%60 ;
    dvrt->millisecond = 0 ;
    return i ;
}

int Cliplist::getClipname( string & imgname )
{
    // retieve begin and end time of .DVR file
    dvrtime cliptime ;
    int i = getClipTime( &cliptime );
    imgname.printf("%s\\%s.%d.%04d%02d%02d%02d%02d%02d.bmp",
        (char *)m_tmpdir, 
        m_screen->m_decoder->m_playerinfo.servername,
        m_screen->m_channel,
        cliptime.year, cliptime.month, cliptime.day,
        cliptime.hour, cliptime.min, cliptime.second );
    return i ;
}

int Cliplist::getSelect( dvrtime * dvrt ) 
{
    if( m_select>=0 && m_select<m_elistsize ) {
        *dvrt=m_clipday ;
        dvrt->hour = m_elist[m_select]/3600 ;
        dvrt->min = (m_elist[m_select]%3600)/60 ;
        dvrt->second = m_elist[m_select]%60 ;
        dvrt->millisecond = 0 ;
        return 1 ;
    }
    else {
        return 0 ;
    }
}

static DWORD WINAPI cliplist_snapshot_thread( LPVOID lpParameter )
{
    ((Cliplist *)lpParameter) ->snapshot_thread() ;
    return 0;
}

void Cliplist::snapshot_thread()
{
    while(m_snapshot_run>=0) {
        WaitForSingleObject( m_snapshot_event, 2000 ) ;
        if( m_snapshot_run>0 ) {
            int retry ;
            int decframes ;
            m_screen->m_decoder->seek( &m_snapshot_time );
            m_screen->m_decoder->oneframeforward();
            decframes = m_screen->m_decoder->getdecframes();

            // try create snapshot, retry 5 times
            if( decframes>=0 ) {
                for( retry=0;retry<20;retry++) {
                    m_screen->m_decoder->oneframeforward();
                    int f = m_screen->m_decoder->getdecframes();
                    if( (f-decframes)>1 ) {
                        m_screen->m_decoder->capture( m_screen->m_channel, m_snapshot_filename );
                        break ;
                    }
                }
            }
            else {
                // do play back for half seconds
                m_screen->m_decoder->play();
                Sleep(500);
                m_screen->m_decoder->capture( m_screen->m_channel, m_snapshot_filename );
                m_screen->m_decoder->pause();
            }

            m_snapshot_run = 0 ;
        }
    }
}

void Cliplist::OnDestroy()
{
    // wait until snapshot finished
    while( m_snapshot_run>0 ) {
        SetEvent( m_snapshot_event ) ;
        Sleep(10);
    }
}

void Cliplist::OnTimer(UINT_PTR nIDEvent)
{
    int active = 0 ;

    if( nIDEvent==TIMER_DRAW && m_mode==0 ) {
        int pos_y = start_y();;
        RECT rect;
        GetClientRect(m_hWnd, &rect);

        Graphics g(m_hWnd);
        int frame = (int)((GetTickCount()/120)%12) ;
        if( frame != m_loadingframe ) {
            m_loadingframe = frame ;
            GUID dim = FrameDimensionTime ;
            m_loadingimage->SelectActiveFrame( &dim, frame );
            frame = 1 ;
        }
        else {
            frame = 0;
        }
        int idx;
        for(idx=0;idx<m_elistsize;idx++) {
            if( m_updlist[idx] ) {
                // do update this image
                int x, y ;
                x = (idx%m_clip_perline) * m_gridwidth ;
                y = (idx/m_clip_perline)* m_gridheight - pos_y ;
                if( y>=rect.bottom || (y+m_gridheight)<=rect.top ) {
                    if( m_snapshot_run != idx+1 ) {
                        m_updlist[idx]=0 ;
                    }
                    continue ;
                }

                // retieve begin and end time of .DVR file
                dvrtime cliptime = m_clipday ;
                cliptime.hour = m_elist[idx]/3600 ;
                cliptime.min = (m_elist[idx]%3600)/60 ;
                cliptime.second = m_elist[idx]%60 ;
                cliptime.millisecond = 0 ;

                Image * clipimg ;
                string clipfilename ;
				clipfilename.printf( "%s\\%s.%d.%04d%02d%02d%02d%02d%02d.bmp", 
                    (char *)m_tmpdir, 
                    m_screen->m_decoder->m_playerinfo.servername,
                    m_screen->m_channel,
                    cliptime.year, cliptime.month, cliptime.day,
                    cliptime.hour, cliptime.min, cliptime.second );

                FILE * clipfile ;
                if( m_snapshot_run == idx+1 ) {
                    clipfile = NULL ;
                }
                else {
                    clipfile = fopen( clipfilename, "r" ) ;
                }
                if( clipfile==NULL ) {  // 
                    // create clip snapshot
                    if( m_snapshot_run == 0 ) {
                        m_snapshot_time = cliptime ;
                        m_snapshot_filename = clipfilename ;
                        m_snapshot_run = idx+1 ;
                        SetEvent( m_snapshot_event );
                    }
                }
                else {
                    fclose( clipfile );
                }
                clipimg = Image::FromFile( clipfilename );
                if( clipimg && clipimg->GetLastStatus()!= Ok ) {  // 
                    delete clipimg ;
                    clipimg=NULL ;
                }

                if( clipimg ) {
                    // now draw the picture
                    g.DrawImage( clipimg, x+IMG_X, y+IMG_Y, m_imgwidth, m_imgheight );
                    addImg( idx, clipimg );
                    m_updlist[idx]=0 ;        // mark finished
                    return ;
                }
                else {
                    // draw waiting picture
                    if( frame ) {
                        Rect r ;
                        r.Width = m_loadingimage->GetWidth() ;
                        r.Height = m_loadingimage->GetHeight() ;
                        r.X = x+ IMG_X + (m_imgwidth - r.Width)/2 ;
                        r.Y = y+ IMG_Y + (m_imgheight - r.Height )/2 ;
                        g.DrawImage( m_loadingimage, r.X, r.Y, r.Width, r.Height );
                    }
                }
                active = 1 ;
            }
        }
        if( active == 0 ) {
            KillTimer( m_hWnd, TIMER_DRAW );
        }
    }
}

LRESULT Cliplist::OnPaint()
{
    PAINTSTRUCT ps;
    HDC hdc;
    hdc = BeginPaint(m_hWnd, &ps);
    Graphics g(hdc);
    Gdiplus::FontFamily  fontFamily(_T("Times New Roman"));
    Gdiplus::Font        font(&fontFamily, 18, FontStyleRegular, UnitPixel);
    Gdiplus::SolidBrush  txtBrush(Color(255, 230, 230, 200));
    Gdiplus::SolidBrush  bkBrush(Color(255, 37, 49, 30 ));

    string clipfilename;

    // calculate line size
    RECT rect;
    GetClientRect(m_hWnd, &rect);

    if( m_mode ) {      // single clip mode

        g.Clear(Color(255, 37, 49, 30 ));

        // display single picture
        int imgwidth = rect.right-rect.left-8 ;
        int imghight = rect.bottom - rect.top - 8 ;

        if( imgwidth*3 > imghight*4 ) {
            imgwidth = imghight*4/3 ;
        }
        else {
            imghight = imgwidth * 3 /4 ;
        }

        // retieve begin and end time of .DVR file
        dvrtime cliptime = m_clipday ;
        cliptime.hour = m_elist[m_select]/3600 ;
        cliptime.min = (m_elist[m_select]%3600)/60 ;
        cliptime.second = m_elist[m_select]%60 ;
        cliptime.millisecond = 0 ;

		clipfilename.printf( "%s\\%s.%d.%04d%02d%02d%02d%02d%02d.bmp",
            (char *)m_tmpdir, 
            m_screen->m_decoder->m_playerinfo.servername,
            m_screen->m_channel,
            cliptime.year, cliptime.month, cliptime.day,
            cliptime.hour, cliptime.min, cliptime.second );

        Image * clipimg = Image::FromFile( clipfilename );
        if( clipimg ) {
            if( clipimg->GetLastStatus()== Ok ) {  // 
                // now draw the picture
                int x, y ;
                x = (rect.right + rect.left - imgwidth )/2 ;
                y = (rect.bottom + rect.top - imghight )/2 ;

                g.DrawImage( clipimg, x, y, imgwidth, imghight );
            }

            delete clipimg ;
        }
    }
    else {
        int pos_y = start_y();;
        int startpage = pos_y / m_gridheight ;
        int endpage   = (pos_y + (rect.bottom-rect.top)) / m_gridheight + 1 ;
        int idx ;
        int delayupd = 0 ;

        //            g.FillRectangle( &bkBrush, ps.rcPaint.left, ps.rcPaint.top, 
        //                ps.rcPaint.right - ps.rcPaint.left,
        //                ps.rcPaint.bottom - ps.rcPaint.top );

        for( idx=startpage*m_clip_perline; idx<(endpage)*m_clip_perline ; idx++ ) {
            int x, y ;
            x = (idx%m_clip_perline) * m_gridwidth ;
            y = (idx/m_clip_perline) * m_gridheight - pos_y ;

            if( y>=ps.rcPaint.bottom ||
                (y+m_gridheight)<=ps.rcPaint.top ||
                x>=ps.rcPaint.right || 
                (x+m_gridwidth)<=ps.rcPaint.left ) 
            {
                continue ;
            }
            //                g.FillRectangle( &bkBrush, x, y, m_gridwidth+3, m_gridheight );
            if( idx<m_elistsize ) {
                // retieve begin and end time of .DVR file
                dvrtime cliptime = m_clipday ;
                cliptime.hour = m_elist[idx]/3600 ;
                cliptime.min = (m_elist[idx]%3600)/60 ;
                cliptime.second = m_elist[idx]%60 ;
                cliptime.millisecond = 0 ;

                if( idx==m_select ) {
                    g.DrawRectangle( m_selpen, x+IMG_X-1, y+IMG_Y-1, m_imgwidth+1, m_imgheight+1 );
                }
                else {
                    g.DrawRectangle( m_nselpen, x+IMG_X-1, y+IMG_Y-1, m_imgwidth+1, m_imgheight+1 );
                }

                Image * clipimg = findImg( idx );
                if( clipimg ) {
                    // now draw the picture
                    g.DrawImage( clipimg, x+IMG_X, y+IMG_Y, m_imgwidth, m_imgheight );
                }
                else {
                    g.FillRectangle( m_waitBrush, x+IMG_X, y+IMG_Y, m_imgwidth, m_imgheight );
                    Rect r ;
                    r.Width = m_loadingimage->GetWidth() ;
                    r.Height = m_loadingimage->GetHeight() ;
                    r.X = x+ IMG_X + (m_imgwidth - r.Width)/2 ;
                    r.Y = y+ IMG_Y + (m_imgheight - r.Height )/2 ;
                    g.DrawImage( m_loadingimage, r.X, r.Y, r.Width, r.Height );

                    // to update this image
                    m_updlist[idx] = 1 ;
                    delayupd = 1 ;
                }

                Rect rect ;
                rect.X = x+IMG_X-1 ;
                rect.Y = y+IMG_Y-1 ;
                rect.Width = m_imgwidth+2 ;
                rect.Height = m_imgheight+2 ;
                g.ExcludeClip( rect );
                g.FillRectangle( &bkBrush, x, y, m_gridwidth+m_clip_perline, m_gridheight );
                string timestr ;
				timestr.printf( "%02d:%02d:%02d",
                    cliptime.hour, cliptime.min, cliptime.second );
                g.DrawString(timestr, -1, &font, Gdiplus::PointF(x+2,y+2), &txtBrush);

				timestr.printf( "(%d)", idx+1 );
                g.DrawString(timestr, -1, &font, Gdiplus::PointF(x+75,y+2), &txtBrush);
            }
            else {
                g.FillRectangle( &bkBrush, x, y, m_gridwidth+m_clip_perline, m_gridheight );
            }
        }

        if( delayupd ) {
            ::SetTimer(m_hWnd, TIMER_DRAW, 10, NULL );	
        }

        if( m_elistsize == 0 ) {
            g.DrawString(_T("No Video Clip Available!"), -1, &font, Gdiplus::PointF(30.0f, 10.0f), &txtBrush);
        }
    }

    EndPaint(m_hWnd, &ps);
    return 0 ;
}

// Scroll up / down
int Cliplist::scrollPos( int offset )
{
    // Get all the vertial scroll bar information.
    int nPos, oPos ;
    SCROLLINFO si ;
    si.cbSize = sizeof (si);
    si.fMask  = SIF_POS;
    GetScrollInfo (m_hWnd, SB_VERT, &si);
    oPos = si.nPos ;
    si.fMask = SIF_POS;
    si.nPos = oPos+offset ;
    SetScrollInfo (m_hWnd, SB_VERT, &si, TRUE);
    GetScrollInfo (m_hWnd, SB_VERT, &si);
    nPos = si.nPos ;

    // Save the position for comparison later on.
    int oyPos = Pos2Y(oPos);
    int nyPos = Pos2Y(nPos);

    // If the position has changed, scroll window and update it.
    if (nyPos != oyPos)
    {
        //            ScrollWindowEx(m_hWnd, 0, (oyPos-nyPos), NULL, NULL, NULL, NULL, 
        //                SW_INVALIDATE );
        RECT rd ;
        GetWindowRect(GetDesktopWindow(), &rd);
        MapWindowPoints(GetDesktopWindow(), m_hWnd, (LPPOINT)&rd, 2 );
        RECT rc ;
        GetClientRect( m_hWnd, &rc );
        if( rd.top < rc.top ) {
            rd.top = rc.top;
        }
        if( rd.left < rc.left ) {
            rd.left = rc.left;
        }
        if( rd.bottom > rc.bottom ) {
            rd.bottom = rc.bottom;
        }
        if( rd.right > rc.right ) {
            rd.right = rc.right;
        }
        // scroll visible area only
        ScrollWindowEx(m_hWnd, 0, (oyPos-nyPos), &rd, NULL, NULL, &rc, SW_INVALIDATE );
        //            ScrollWindow(m_hWnd, 0, (oyPos-nyPos), &rd, NULL );
        return 1 ;
    }
    return 0 ;
}

LRESULT Cliplist::OnVscroll(int code)
{
    // Get all the vertial scroll bar information.
    if( m_mode ) 
        return 0 ;
    SCROLLINFO si ;
    si.cbSize = sizeof (si);
    si.fMask  = SIF_ALL;
    GetScrollInfo (m_hWnd, SB_VERT, &si);
    int offset = 0 ;
    switch (code)
    {
        // User clicked the HOME keyboard key.
    case SB_TOP:
        offset = -si.nMax;
        break;

        // User clicked the END keyboard key.
    case SB_BOTTOM:
        offset = si.nMax;
        break;

        // User clicked the top arrow.
    case SB_LINEUP:
        offset = -(int)(m_screenpage*si.nPage/8);
        break;

        // User clicked the bottom arrow.
    case SB_LINEDOWN:
        offset = (int)(m_screenpage*si.nPage/8);
        break;

        // User clicked the scroll bar shaft above the scroll box.
    case SB_PAGEUP:
        offset = -(int)si.nPage*m_screenpage;
        break;

        // User clicked the scroll bar shaft below the scroll box.
    case SB_PAGEDOWN:
        offset = (int)si.nPage*m_screenpage;
        break;

        // User dragged the scroll box.
    case SB_THUMBTRACK:
        offset = si.nTrackPos - si.nPos ;
        break;

    default:
        offset = 0 ;
        break; 
    }

    scrollPos( offset );
    return 0 ;
}

int Cliplist::SelectClip( int x, int y ) 
{
    if( m_snapshot_run ) {
        return m_select ;
    }
    int pos_y = start_y();
    int page = (pos_y+y)/m_gridheight  ;
    int select = page * m_clip_perline + x / m_gridwidth ;
    if( select >=0 && select < m_elistsize && select != m_select ) {
        WaitCursor wc ;
        Graphics g(m_hWnd);
        int x, y ;
        int imgwidth = m_gridwidth - 6 ;
        int imgheight = imgwidth*3/4 ;

        // clear old selection rect
        x = (m_select%m_clip_perline) * m_gridwidth ;
        y = (m_select/m_clip_perline) * m_gridheight - pos_y ;
        g.DrawRectangle( m_nselpen, x+IMG_X-1, y+IMG_Y-1, m_imgwidth+1, m_imgheight+1 );

        // draw new selection rect
        x = (select%m_clip_perline) * m_gridwidth ;
        y = (select/m_clip_perline) * m_gridheight - pos_y ;
        g.DrawRectangle( m_selpen, x+IMG_X-1, y+IMG_Y-1, m_imgwidth+1, m_imgheight+1 );

        m_select = select ;

        dvrtime cliptime = m_clipday ;
        cliptime.hour = m_elist[select]/3600 ;
        cliptime.min = (m_elist[select]%3600)/60 ;
        cliptime.second = m_elist[select]%60 ;
        cliptime.millisecond = 0 ;
        m_parent->SeekTime( &cliptime );
        m_parent->UpdateMap();
    }
    return m_select ;
}

LRESULT Cliplist::OnLButtonDblClk(UINT nFlags,int x, int y )
{
    if( m_snapshot_run ) {
        return 0 ;
    }
    if( m_mode ) {
        ShowScrollBar(m_hWnd, SB_VERT, TRUE );
        m_mode = 0 ;
    }
    else {
        ShowScrollBar(m_hWnd, SB_VERT, FALSE );
        m_mode = 1 ;        // single clip mode
    }
    InvalidateRect(m_hWnd, NULL, FALSE);
    return 0 ;
}


LRESULT Cliplist::OnLButtonDown(UINT nFlags, int x, int y )
{
    if( m_mode == 0 ) {
        SelectClip( x, y );
        SCROLLINFO si ;
        si.cbSize = sizeof(si);
        si.fMask = SIF_POS ;
        GetScrollInfo (m_hWnd, SB_VERT, &si);
        m_graby = y ;
        m_grabypos = si.nPos ;
        SetCapture(m_hWnd);
        SetCursor(m_handgrabcursor);
    }
    return 0 ;
}

LRESULT Cliplist::OnLButtonUp(UINT nFlags,int x, int y )
{
    if( GetCapture() == m_hWnd ) {	
        ReleaseCapture();
    }
    return 0 ;
}

LRESULT Cliplist::OnMouseMove(UINT nFlags,int x, int y )
{
    if( GetCapture() == m_hWnd ) {			
        int nPos = m_grabypos - Y2Pos(y-m_graby) ;
        SCROLLINFO si ;
        si.cbSize = sizeof(si);
        si.fMask = SIF_POS ;
        GetScrollInfo (m_hWnd, SB_VERT, &si);
        if( scrollPos( nPos - si.nPos  ) == 0 ) {
            m_graby = y ;
            m_grabypos = si.nPos ;
        }
    }
    return 0 ;
}


LRESULT Cliplist::OnMouseWheel(UINT nFlags, int x, int y)
{
	int delta = (int)(short int)HIWORD(nFlags);
	if (delta > 0) {
		PostMessage(m_hWnd, WM_VSCROLL, SB_LINEUP, (LPARAM)0);
	}
	else if (delta < 0) {
		PostMessage(m_hWnd, WM_VSCROLL, SB_LINEDOWN, (LPARAM)0);
	}
	return 0;
}

LRESULT Cliplist::OnSize()
{
    RECT rect;
    GetClientRect(m_hWnd, &rect);
    int width = rect.right-rect.left ;
    m_gridwidth = width / m_clip_perline ;
    m_imgwidth = m_gridwidth - 2*IMG_X ;
    m_imgheight = m_imgwidth*3/4 ;
    m_gridheight = m_imgheight + IMG_Y + 2 ;
    m_screenpage = (rect.bottom-rect.top-150)/m_gridheight ;
    if( m_screenpage<1 ) m_screenpage = 1 ;
    return 0 ;
}
