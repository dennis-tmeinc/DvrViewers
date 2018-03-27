#include "stdafx.h"
#include <crtdbg.h>
#include <errno.h>
#include <process.h>
#include <shellapi.h>
#include "directx.h"
#include "vout_picture.h"
#include "trace.h"

//#pragma comment(lib, "ddraw.lib")
//#pragma comment(lib, "dxguid.lib")

/*****************************************************************************
 * picture_sys_t: direct buffer method descriptor
 *****************************************************************************
 * This structure is part of the picture descriptor, it describes the
 * DirectX specific properties of a direct buffer.
 *****************************************************************************/
struct picture_sys_t
{
    LPDIRECTDRAWSURFACE7 p_surface;
    LPDIRECTDRAWSURFACE7 p_front_surface;
    DDSURFACEDESC2        ddsd;
};

static long FAR PASCAL DirectXEventProc( HWND hwnd, UINT message,
                                         WPARAM wParam, LPARAM lParam );
BOOL WINAPI DirectXEnumCallback( GUID* p_guid, LPTSTR psz_desc,
                                 LPTSTR psz_drivername, VOID* p_context,
                                 HMONITOR hmon );
static void SetPalette( UINT16 *red, UINT16 *green, UINT16 *blue );

unsigned WINAPI DirectXEventThreadFunc(PVOID pvoid) {
	CDirectX *p = (CDirectX *)pvoid;
	p->DirectXEventThread();
	_RPT0(_CRT_WARN, "DirectX Event Thread Ended\n");
	_endthreadex(0);
	return 0;
}

CDirectX::CDirectX(HWND hwnd, video_format_t *p_fmt, int iPtsDelay, bool bResizeParent, bool bUseOverlay)
			: CVideoOutput(hwnd, p_fmt, iPtsDelay, bResizeParent)
{
	tme_thread_init(&m_ipc);

	// Initialize and call StartThread virtual function
	m_bHwYuvConf = true;
	m_b3BufferingConf = true;
	m_bUseSysmemConf = m_bWallpaperConf = false;
	m_bOnTopChange = false;

	m_bDisplayInitialized = false;

	m_bHwYuv = false;
	m_bEventDead = m_bEventDie = false;
	m_bUsingOverlay = bUseOverlay;
	m_bUseSysmem = m_b3BufOverlay = false;
	m_iAlignSrcBoundary = m_iAlignSrcSize = m_iAlignDestBoundary = m_iAlignDestSize = 0;

    m_pDDObject = NULL;
    m_pDisplay = NULL;
    m_pCurrentSurface = NULL;
    m_pClipper = NULL;
    m_hwnd = m_hvideownd = NULL;
    m_hfswnd = NULL;
    m_iChanges = 0;
    m_bWallpaper = false;
    tme_mutex_init( &m_ipc, &m_lock );
    SetRectEmpty( &m_rectDisplay );
    SetRectEmpty( &m_rectParent );

    /* Multimonitor stuff */
    m_hmonitor = NULL;
    m_pDisplayDriver = NULL;
    m_MonitorFromWindow = NULL;
    m_GetMonitorInfo = NULL;

}

CDirectX::~CDirectX()
{
	if (m_hThread) {
		m_bDie = true;
        tme_thread_join(m_hThread);
	}

	CloseVideo();

    tme_mutex_destroy( &m_lock );
	tme_mutex_destroy(&(m_ipc.object_lock));
	tme_cond_destroy(&(m_ipc.object_wait));
}

int CDirectX::OpenVideo()
{
	HMODULE huser32;
    if( (huser32 = GetModuleHandle( _T("USER32") ) ) )
    {
        m_MonitorFromWindow = (HMONITOR (WINAPI *)( HWND, DWORD ))
            GetProcAddress( huser32, "MonitorFromWindow" );
        m_GetMonitorInfo = 
            (BOOL (WINAPI*)(HMONITOR, LPMONITORINFO))GetProcAddress( huser32, "GetMonitorInfoA" );
    }

    m_bCursorHidden = false;
    m_iLastmoved = mdate();

	m_iWindowWidth = CVideoOutput::m_iWindowWidth;
	m_iWindowHeight = CVideoOutput::m_iWindowHeight;

	m_hEventThread = tme_thread_create(&m_ipc, "DirectXEventThread",
		DirectXEventThreadFunc, this, 0, true);
	if (m_hEventThread == NULL) {
		_RPT0(_CRT_WARN, "Creating DirectXEventThread failed\n");
		return EGENERIC;
	}

    /* Initialise DirectDraw */
    if( DirectXInitDDraw() )
    {
        _RPT0(_CRT_WARN, "cannot initialize DirectDraw\n" );
        return EGENERIC;
    }

    /* Create the directx display */
    if( DirectXCreateDisplay() )
    {
        _RPT0(_CRT_WARN, "cannot initialize DirectDraw\n" );
        return EGENERIC;
    }

	StartThread(); // call this at the end of the constructor

	return SUCCESS;
}

void CDirectX::ShowVideoWindow(int nCmdShow)
{
	if (m_hwnd != NULL) {
		ShowWindow(m_hwnd, nCmdShow);
	}
}

void CDirectX::CloseVideo()
{
    _RPT0(_CRT_WARN, "CloseVideo()\n" );
    if( m_hEventThread )
    {
        /* Kill DirectXEventThread */
        m_bEventDie = true;

        /* we need to be sure DirectXEventThread won't stay stuck in
         * GetMessage, so we send a fake message */
        if( m_hwnd )
        {
			/* Messages sent with PostMessage gets lost sometimes,
			 * so, do SendMessage to the windows procedure
			 * and the windows procedure will relay the message 
			 * with PostMessage call
			 */
			SendMessage(m_hwnd, WM_NULL, 0, 0);
        }

        tme_thread_join( m_hEventThread );
    }
}

int CDirectX::Init()
{
    int i_chroma_backup;

	// TODO: make these settings configurable
	m_bUseSysmem = false;
	m_bHwYuv = true;
	m_b3BufOverlay = true;

	/* Initialise DirectDraw if not already done.
     * We do this here because on multi-monitor systems we may have to
     * re-create the directdraw surfaces. */
    if( !m_pDDObject &&
        DirectXInitDDraw() != SUCCESS )
    {
        _RPT0(_CRT_WARN, "cannot initialize DirectDraw\n" );
        return EGENERIC;
    }

    /* Create the directx display */
    if( !m_pDisplay &&
        DirectXCreateDisplay() != SUCCESS )
    {
        _RPT0(_CRT_WARN, "cannot initialize DirectDraw\n" );
        return EGENERIC;
    }

    /* Initialize the output structure.
     * Since DirectDraw can do rescaling for us, stick to the default
     * coordinates and aspect. */
    m_output.i_width  = m_render.i_width;
    m_output.i_height = m_render.i_height;
    m_output.i_aspect = m_render.i_aspect;
    m_fmtOut = m_fmtIn;
    DirectXUpdateRects(true);

#define MAX_DIRECTBUFFERS 1
    /* Right now we use only 1 directbuffer because we don't want the
     * video decoder to decode directly into direct buffers as they are
     * created into video memory and video memory is _really_ slow */

    /* Choose the chroma we will try first. */
    switch( m_render.i_chroma )
    {
        case TME_FOURCC('Y','U','Y','2'):
        case TME_FOURCC('Y','U','N','V'):
            m_output.i_chroma = TME_FOURCC('Y','U','Y','2');
            break;
        case TME_FOURCC('U','Y','V','Y'):
        case TME_FOURCC('U','Y','N','V'):
        case TME_FOURCC('Y','4','2','2'):
            m_output.i_chroma = TME_FOURCC('U','Y','V','Y');
            break;
        case TME_FOURCC('Y','V','Y','U'):
            m_output.i_chroma = TME_FOURCC('Y','V','Y','U');
            break;
        default:
            m_output.i_chroma = TME_FOURCC('Y','V','1','2');
            break;
    }

    NewPictureVec( m_pPicture, MAX_DIRECTBUFFERS );

    i_chroma_backup = m_output.i_chroma;

    if( !I_OUTPUTPICTURES )
    {
        /* hmmm, it didn't work! Let's try commonly supported chromas */
        if( m_output.i_chroma != TME_FOURCC('I','4','2','0') )
        {
            m_output.i_chroma = TME_FOURCC('Y','V','1','2');
            NewPictureVec( m_pPicture, MAX_DIRECTBUFFERS );
        }
        if( !I_OUTPUTPICTURES )
        {
            /* hmmm, it still didn't work! Let's try another one */
            m_output.i_chroma = TME_FOURCC('Y','U','Y','2');
            NewPictureVec( m_pPicture, MAX_DIRECTBUFFERS );
        }
    }

    if( !I_OUTPUTPICTURES )
    {
        /* If it still didn't work then don't try to use an overlay */
        m_output.i_chroma = i_chroma_backup;
        m_bUsingOverlay = false;
        NewPictureVec( m_pPicture, MAX_DIRECTBUFFERS );
    }

    m_fmtOut.i_chroma = m_output.i_chroma;

	return 0;
}

/*****************************************************************************
 * End: terminate Sys video thread output method
 *****************************************************************************
 * Terminate an output method created by Create.
 * It is called at the end of the thread.
 *****************************************************************************/
void CDirectX::End()
{
    FreePictureVec( m_pPicture, I_OUTPUTPICTURES );

    DirectXCloseDisplay();
    DirectXCloseDDraw();

    return;
}

int CDirectX::Manage() {
    WINDOWPLACEMENT window_placement;

    /* If we do not control our window, we check for geometry changes
     * ourselves because the parent might not send us its events. */
    tme_mutex_lock( &m_lock );
    if( m_hparent && !m_bFullscreen )
    {
        RECT rect_parent;
        POINT point;

        tme_mutex_unlock( &m_lock );

        GetClientRect( m_hparent, &rect_parent );
        point.x = point.y = 0;
        ClientToScreen( m_hparent, &point );
        OffsetRect( &rect_parent, point.x, point.y );

        if( !EqualRect( &rect_parent, &m_rectParent ) )
        {
            m_rectParent = rect_parent;

            /* This one is to force the update even if only
             * the position has changed */
            SetWindowPos( m_hwnd, 0, 1, 1,
                          rect_parent.right - rect_parent.left,
                          rect_parent.bottom - rect_parent.top, 0 );

            SetWindowPos( m_hwnd, 0, 0, 0,
                          rect_parent.right - rect_parent.left,
                          rect_parent.bottom - rect_parent.top, 0 );
        }
    }
    else
    {
        tme_mutex_unlock( &m_lock );
    }

    /*
     * Position Change
     */
    if( m_iChanges & DX_POSITION_CHANGE )
    {
        m_iChanges &= ~DX_POSITION_CHANGE;

        /* Check if we are still on the same monitor */
        if( m_MonitorFromWindow &&
            m_hmonitor !=
                m_MonitorFromWindow( m_hwnd, MONITOR_DEFAULTTONEAREST ) )
        {
            /* This will force the vout core to recreate the picture buffers */
			CVideoOutput::m_iChanges |= VOUT_PICTURE_BUFFERS_CHANGE;
        }
    }

    /* Check for cropping / aspect changes */
    if( CVideoOutput::m_iChanges & VOUT_CROP_CHANGE ||
        CVideoOutput::m_iChanges & VOUT_ASPECT_CHANGE )
    {
        CVideoOutput::m_iChanges &= ~VOUT_CROP_CHANGE;
        CVideoOutput::m_iChanges &= ~VOUT_ASPECT_CHANGE;

        m_fmtOut.i_x_offset = m_fmtIn.i_x_offset;
        m_fmtOut.i_y_offset = m_fmtIn.i_y_offset;
        m_fmtOut.i_visible_width = m_fmtIn.i_visible_width;
        m_fmtOut.i_visible_height = m_fmtIn.i_visible_height;
        m_fmtOut.i_aspect = m_fmtIn.i_aspect;
        m_fmtOut.i_sar_num = m_fmtIn.i_sar_num;
        m_fmtOut.i_sar_den = m_fmtIn.i_sar_den;
        m_output.i_aspect = m_fmtIn.i_aspect;
        DirectXUpdateRects( true );
    }
#if 1 // can be removed
    /* We used to call the Win32 PeekMessage function here to read the window
     * messages. But since window can stay blocked into this function for a
     * long time (for example when you move your window on the screen), I
     * decided to isolate PeekMessage in another thread. */

    if( m_iChanges & DX_WALLPAPER_CHANGE )
    {
        //SwitchWallpaperMode( p_vout, !p_vout->p_sys->b_wallpaper );
        m_iChanges &= ~DX_WALLPAPER_CHANGE;
        DirectXUpdateOverlay();
    }

    /*
     * Fullscreen change
     */
    if( CVideoOutput::m_iChanges & VOUT_FULLSCREEN_CHANGE
        || m_iChanges & VOUT_FULLSCREEN_CHANGE )
    {
        HWND hwnd = (m_hparent && m_hfswnd) ?
            m_hfswnd : m_hwnd;

        m_bFullscreen = ! m_bFullscreen;

        /* We need to switch between Maximized and Normal sized window */
        window_placement.length = sizeof(WINDOWPLACEMENT);
        GetWindowPlacement( hwnd, &window_placement );
        if( m_bFullscreen )
        {
            /* Change window style, no borders and no title bar */
            int i_style = WS_CLIPCHILDREN | WS_VISIBLE;
            SetWindowLong( hwnd, GWL_STYLE, i_style );

            if( m_hparent )
            {
                /* Retrieve current window position so fullscreen will happen
                 * on the right screen */
                POINT point = {0,0};
                RECT rect;
                ClientToScreen( m_hwnd, &point );
                GetClientRect( m_hwnd, &rect );
                SetWindowPos( hwnd, 0, point.x, point.y,
                              rect.right, rect.bottom,
                              SWP_NOZORDER|SWP_FRAMECHANGED );
                GetWindowPlacement( hwnd, &window_placement );
            }

            /* Maximize window */
            window_placement.showCmd = SW_SHOWMAXIMIZED;
            SetWindowPlacement( hwnd, &window_placement );
            SetWindowPos( hwnd, 0, 0, 0, 0, 0,
                          SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);

            if( m_hparent )
            {
                RECT rect;
                GetClientRect( hwnd, &rect );
                SetParent( m_hwnd, hwnd );
                SetWindowPos( m_hwnd, 0, 0, 0,
                              rect.right, rect.bottom,
                              SWP_NOZORDER|SWP_FRAMECHANGED );
            }

            SetForegroundWindow( hwnd );
        }
        else
        {
            /* Change window style, no borders and no title bar */
            SetWindowLong( hwnd, GWL_STYLE, m_iWindowStyle );

            /* Normal window */
            window_placement.showCmd = SW_SHOWNORMAL;
            SetWindowPlacement( hwnd, &window_placement );
            SetWindowPos( hwnd, 0, 0, 0, 0, 0,
                          SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);

            if( m_hparent )
            {
                RECT rect;
                GetClientRect( m_hparent, &rect );
                SetParent( m_hwnd, m_hparent );
                SetWindowPos( m_hwnd, 0, 0, 0,
                              rect.right, rect.bottom,
                              SWP_NOZORDER|SWP_FRAMECHANGED );

                ShowWindow( hwnd, SW_HIDE );
                SetForegroundWindow( m_hparent );
            }

            /* Make sure the mouse cursor is displayed */
            PostMessage( m_hwnd, WM_TME_SHOW_MOUSE, 0, 0 );
        }

        /* Update the object variable and trigger callback */
        //val.b_bool = m_bFullscreen;
        //var_Set( p_vout, "fullscreen", val );

        CVideoOutput::m_iChanges &= ~VOUT_FULLSCREEN_CHANGE;
        m_iChanges &= ~VOUT_FULLSCREEN_CHANGE;
    }

    /*
     * Pointer change
     */
    if( m_bFullscreen && !m_bCursorHidden &&
        (mdate() - m_iLastmoved) > 5000000 )
    {
        POINT point;
        HWND hwnd;

        /* Hide the cursor only if it is inside our window */
        GetCursorPos( &point );
        hwnd = WindowFromPoint(point);
        if( hwnd == m_hwnd || hwnd == m_hvideownd )
        {
            PostMessage( m_hwnd, WM_TME_HIDE_MOUSE, 0, 0 );
        }
        else
        {
            m_iLastmoved = mdate();
        }
    }

    /*
     * "Always on top" status change
     */
	bool bVideoOnTop = false;
    if( m_bOnTopChange )
    {
        HMENU hMenu = GetSystemMenu( m_hwnd, FALSE );

        //var_Get( p_vout, "video-on-top", &val );

        /* Set the window on top if necessary */
        if( bVideoOnTop && !( GetWindowLong( m_hwnd, GWL_EXSTYLE )
                           & WS_EX_TOPMOST ) )
        {
            CheckMenuItem( hMenu, IDM_TOGGLE_ON_TOP,
                           MF_BYCOMMAND | MFS_CHECKED );
            SetWindowPos( m_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                          SWP_NOSIZE | SWP_NOMOVE );
        }
        else
        /* The window shouldn't be on top */
        if( !bVideoOnTop && ( GetWindowLong( m_hwnd, GWL_EXSTYLE )
                           & WS_EX_TOPMOST ) )
        {
            CheckMenuItem( hMenu, IDM_TOGGLE_ON_TOP,
                           MF_BYCOMMAND | MFS_UNCHECKED );
            SetWindowPos( m_hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                          SWP_NOSIZE | SWP_NOMOVE );
        }

        m_bOnTopChange = false;
    }
#endif
    /* Check if the event thread is still running */
    if( m_bEventDie )
    {
        return EGENERIC; /* exit */
    }

    return SUCCESS;
}

void CDirectX::Render(picture_t *) {}

void CDirectX::Display(picture_t *p_pic)
{
    HRESULT dxresult;

    if( (m_pDisplay == NULL) )
    {
        _RPT0( _CRT_WARN, "no display!\n" );
        return;
    }

    /* Our surface can be lost so be sure to check this
     * and restore it if need be */
    if( IDirectDrawSurface7_IsLost( m_pDisplay )
        == DDERR_SURFACELOST )
    {
        if( IDirectDrawSurface7_Restore( m_pDisplay ) == DD_OK &&
            m_bUsingOverlay )
            DirectXUpdateOverlay();
    }

    if( !m_bUsingOverlay )
    {
        DDBLTFX  ddbltfx;

        /* We ask for the "NOTEARING" option */
        memset( &ddbltfx, 0, sizeof(DDBLTFX) );
        ddbltfx.dwSize = sizeof(DDBLTFX);
        ddbltfx.dwDDFX = DDBLTFX_NOTEARING;

       /* Blit video surface to display */
        dxresult = IDirectDrawSurface7_Blt( m_pDisplay,
                                            &m_rectDestClipped,
                                            p_pic->p_sys->p_surface,
                                            &m_rectSrcClipped,
                                            DDBLT_ASYNC, &ddbltfx );
        if( dxresult != DD_OK )
        {
            _RPT1( _CRT_WARN, "could not blit surface (error %li)\n", dxresult );
            return;
        }

    }
    else /* using overlay */
    {
        /* Flip the overlay buffers if we are using back buffers */
        if( p_pic->p_sys->p_front_surface == p_pic->p_sys->p_surface )
        {
            return;
        }

        dxresult = IDirectDrawSurface7_Flip( p_pic->p_sys->p_front_surface,
                                             NULL, DDFLIP_WAIT );
        if( dxresult != DD_OK )
        {
            _RPT1( _CRT_WARN, "could not flip overlay (error %li)\n", dxresult );
        }

        /* set currently displayed pic */
        m_pCurrentSurface = p_pic->p_sys->p_front_surface;

        /* Lock surface to get all the required info */
        if( DirectXLockSurface( p_pic ) )
        {
            /* AAARRGG */
            _RPT0( _CRT_WARN, "cannot lock surface\n" );
            return;
        }
        DirectXUnlockSurface( p_pic );

		if (!m_bDisplayInitialized) {
			if (m_bUsingOverlay) {
				IDirectDraw_WaitForVerticalBlank(m_pDDObject,
			        DDWAITVB_BLOCKBEGIN, NULL);

				/* set the colorkey as the backgound brush for the video window */
				SetClassLong( m_hvideownd, GCL_HBRBACKGROUND,
					      (LONG)CreateSolidBrush( m_iRgbColorkey ) );
				InvalidateRect( m_hvideownd, NULL, TRUE );
			}
			m_bDisplayInitialized = true;
		}
    }
}

int CDirectX::Lock(picture_t *p)
{
	return DirectXLockSurface(p);
}

int CDirectX::Unlock(picture_t *p)
{
	return DirectXUnlockSurface(p);
}
/*****************************************************************************
 * DirectXInitDDraw: Takes care of all the DirectDraw initialisations
 *****************************************************************************
 * This function initialise and allocate resources for DirectDraw.
 *****************************************************************************/
int CDirectX::DirectXInitDDraw()
{
    HRESULT dxresult;
    HRESULT (WINAPI *OurDirectDrawCreate)(GUID *,LPDIRECTDRAW *,IUnknown *);
    HRESULT (WINAPI *OurDirectDrawEnumerateEx)( LPDDENUMCALLBACKEX, LPVOID,
                                                DWORD );
    LPDIRECTDRAW p_ddobject;

	_RPT0(_CRT_WARN, "DirectXInitDDraw\n");

    /* Load direct draw DLL */
    m_hDDrawDll = LoadLibrary(_T("DDRAW.DLL"));
    if( m_hDDrawDll == NULL )
    {
        _RPT0(_CRT_WARN, "DirectXInitDDraw failed loading ddraw.dll\n" );
        goto error;
    }

    OurDirectDrawCreate =
      (HRESULT (WINAPI *)(GUID *,LPDIRECTDRAW *,IUnknown *))
	  GetProcAddress( m_hDDrawDll, "DirectDrawCreate" );
    if( OurDirectDrawCreate == NULL )
    {
        _RPT0(_CRT_WARN, "DirectXInitDDraw failed GetProcAddress\n" );
        goto error;
    }

    OurDirectDrawEnumerateEx =
		(HRESULT (WINAPI *)( LPDDENUMCALLBACKEX, LPVOID, DWORD ))
		GetProcAddress( m_hDDrawDll,
#ifndef UNICODE
                              "DirectDrawEnumerateExA" );
#else
                              "DirectDrawEnumerateExW" );
#endif

	if( OurDirectDrawEnumerateEx && m_MonitorFromWindow )
    {
        m_hmonitor =
            m_MonitorFromWindow( m_hwnd, MONITOR_DEFAULTTONEAREST );

        /* Enumerate displays */
        OurDirectDrawEnumerateEx( DirectXEnumCallback, this,
                                  DDENUM_ATTACHEDSECONDARYDEVICES );
    }

    /* Initialize DirectDraw now */
    dxresult = OurDirectDrawCreate( m_pDisplayDriver,
                                    &p_ddobject, NULL );
    if( dxresult != DD_OK )
    {
        _RPT0(_CRT_WARN, "DirectXInitDDraw cannot initialize DDraw\n" );
        goto error;
    }

   /* Get the IDirectDraw7 interface */
    dxresult = IDirectDraw_QueryInterface( p_ddobject, IID_IDirectDraw7,
                                        (LPVOID *)&m_pDDObject );
    /* Release the unused interface */
    IDirectDraw_Release( p_ddobject );
    if( dxresult != DD_OK )
    {
        _RPT0(_CRT_WARN, "cannot get IDirectDraw7 interface\n" );
        goto error;
    }

    /* Set DirectDraw Cooperative level, ie what control we want over Windows
     * display */
    dxresult = IDirectDraw7_SetCooperativeLevel( m_pDDObject,
                                                 NULL, DDSCL_NORMAL ); 
    if( dxresult != DD_OK )
    {
        _RPT0(_CRT_WARN, "cannot set direct draw cooperative level\n" );
        goto error;
    }

    /* Get the size of the current display device */
    if( m_hmonitor && m_GetMonitorInfo )
    {
        MONITORINFO monitor_info;
        monitor_info.cbSize = sizeof( MONITORINFO );
        m_GetMonitorInfo( m_hmonitor, &monitor_info );
        m_rectDisplay = monitor_info.rcMonitor;
    }
    else
    {
        m_rectDisplay.left = 0;
        m_rectDisplay.top = 0;
        m_rectDisplay.right  = GetSystemMetrics(SM_CXSCREEN);
        m_rectDisplay.bottom = GetSystemMetrics(SM_CYSCREEN);
    }

    _RPT4(_CRT_WARN, "screen dimensions (%lix%li,%lix%li)\n",
             m_rectDisplay.left,
             m_rectDisplay.top,
             m_rectDisplay.right,
             m_rectDisplay.bottom );

    /* Probe the capabilities of the hardware */
    DirectXGetDDrawCaps();

    return SUCCESS;

 error:
    if( m_pDDObject )
        IDirectDraw7_Release( m_pDDObject );
    if( m_hDDrawDll )
        FreeLibrary( m_hDDrawDll );
    m_hDDrawDll = NULL;
    m_pDDObject = NULL;

    return EGENERIC;
}

/*****************************************************************************
 * DirectXGetDDrawCaps: Probe the capabilities of the hardware
 *****************************************************************************
 * It is nice to know which features are supported by the hardware so we can
 * find ways to optimize our rendering.
 *****************************************************************************/
void CDirectX::DirectXGetDDrawCaps()
{
    DDCAPS ddcaps;
    HRESULT dxresult;

    /* This is just an indication of whether or not we'll support overlay,
     * but with this test we don't know if we support YUV overlay */
    memset( &ddcaps, 0, sizeof( DDCAPS ));
    ddcaps.dwSize = sizeof(DDCAPS);
    dxresult = IDirectDraw7_GetCaps( m_pDDObject,
                                     &ddcaps, NULL );
    if(dxresult != DD_OK )
    {
        _RPT0(_CRT_WARN, "cannot get caps\n" );
    }
    else
    {
        BOOL bHasOverlay, bHasOverlayFourCC, bCanDeinterlace,
             bHasColorKey, bCanStretch, bCanBltFourcc,
             bAlignBoundarySrc, bAlignBoundaryDest,
             bAlignSizeSrc, bAlignSizeDest;

        /* Determine if the hardware supports overlay surfaces */
        bHasOverlay = (ddcaps.dwCaps & DDCAPS_OVERLAY) ? 1 : 0;
        /* Determine if the hardware supports overlay surfaces */
        bHasOverlayFourCC = (ddcaps.dwCaps & DDCAPS_OVERLAYFOURCC) ? 1 : 0;
        /* Determine if the hardware supports overlay deinterlacing */
        bCanDeinterlace = (ddcaps.dwCaps & DDCAPS2_CANFLIPODDEVEN) ? 1 : 0;
        /* Determine if the hardware supports colorkeying */
        bHasColorKey = (ddcaps.dwCaps & DDCAPS_COLORKEY) ? 1 : 0;
        /* Determine if the hardware supports scaling of the overlay surface */
        bCanStretch = (ddcaps.dwCaps & DDCAPS_OVERLAYSTRETCH) ? 1 : 0;
        /* Determine if the hardware supports color conversion during a blit */
        bCanBltFourcc = (ddcaps.dwCaps & DDCAPS_BLTFOURCC) ? 1 : 0;
        /* Determine overlay source boundary alignment */
        bAlignBoundarySrc = (ddcaps.dwCaps & DDCAPS_ALIGNBOUNDARYSRC) ? 1 : 0;
        /* Determine overlay destination boundary alignment */
        bAlignBoundaryDest = (ddcaps.dwCaps & DDCAPS_ALIGNBOUNDARYDEST) ? 1:0;
        /* Determine overlay destination size alignment */
        bAlignSizeSrc = (ddcaps.dwCaps & DDCAPS_ALIGNSIZESRC) ? 1 : 0;
        /* Determine overlay destination size alignment */
        bAlignSizeDest = (ddcaps.dwCaps & DDCAPS_ALIGNSIZEDEST) ? 1 : 0;
 
        TRACE(_T("DirectDraw Capabilities: overlay=%i yuvoverlay=%i ")
                         _T("can_deinterlace_overlay=%i colorkey=%i stretch=%i ")
                         _T("bltfourcc=%i\n"),
                         bHasOverlay, bHasOverlayFourCC, bCanDeinterlace,
                         bHasColorKey, bCanStretch, bCanBltFourcc );

        if( bAlignBoundarySrc || bAlignBoundaryDest ||
            bAlignSizeSrc || bAlignSizeDest )
        {
            if( bAlignBoundarySrc ) m_iAlignSrcBoundary =
                ddcaps.dwAlignBoundarySrc;
            if( bAlignBoundaryDest ) m_iAlignDestBoundary =
                ddcaps.dwAlignBoundaryDest;
            if( bAlignSizeDest ) m_iAlignSrcSize =
                ddcaps.dwAlignSizeSrc;
            if( bAlignSizeDest ) m_iAlignDestSize =
                ddcaps.dwAlignSizeDest;

            TRACE(_T("align_boundary_src=%i,%i ")
                     _T("align_boundary_dest=%i,%i ")
                     _T("align_size_src=%i,%i align_size_dest=%i,%i\n"),
                     bAlignBoundarySrc, m_iAlignSrcBoundary,
                     bAlignBoundaryDest, m_iAlignDestBoundary,
                     bAlignSizeSrc, m_iAlignSrcSize,
                     bAlignSizeDest, m_iAlignDestSize );
        }

        /* Don't ask for troubles */
        if( !bCanBltFourcc ) m_bHwYuv = false;
    }
}

/*****************************************************************************
 * DirectXCreateDisplay: create the DirectDraw display.
 *****************************************************************************
 * Create and initialize display according to preferences specified in the vout
 * thread fields.
 *****************************************************************************/
int CDirectX::DirectXCreateDisplay()
{
    HRESULT              dxresult;
    DDSURFACEDESC2        ddsd;
    LPDIRECTDRAWSURFACE7  p_display;

    //_RPT0(_CRT_WARN, "DirectXCreateDisplay\n" );

    /* Now get the primary surface. This surface is what you actually see
     * on your screen */
    memset( &ddsd, 0, sizeof( DDSURFACEDESC2 ));
    ddsd.dwSize = sizeof(DDSURFACEDESC2);
    ddsd.dwFlags = DDSD_CAPS;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

	dxresult = IDirectDraw7_CreateSurface( m_pDDObject,
                                           &ddsd, &p_display, NULL );
    if( dxresult != DD_OK )
    {
        _RPT1(_CRT_WARN, "cannot get primary surface (error %li)\n", dxresult );
        return EGENERIC;
    }

    dxresult = IDirectDrawSurface_QueryInterface( p_display,
                                         IID_IDirectDrawSurface7,
                                         (LPVOID *)&m_pDisplay );
    /* Release the old interface */
    IDirectDrawSurface_Release( p_display );
    if ( dxresult != DD_OK )
    {
        _RPT1(_CRT_WARN, "cannot query IDirectDrawSurface7 interface (error %li)", dxresult );
        return EGENERIC;
    }

    /* The clipper will be used only in non-overlay mode */
    DirectXCreateClipper();

    /* Make sure the colorkey will be painted */
    m_iColorkey = 1;
    m_iRgbColorkey = DirectXFindColorkey( &m_iColorkey );

    /* use black brush as the video background color,
       if overlay video is used, this will be replaced by the
       colorkey when the first picture is received */
    /* Don't use this (overlay disappears...) */
	//SetClassLong( m_hvideownd, GCL_HBRBACKGROUND,
    //              (LONG)GetStockObject( BLACK_BRUSH ) );
    /* Use this one */
	SetClassLong( m_hvideownd, GCL_HBRBACKGROUND,
		          (LONG)CreateSolidBrush( m_iRgbColorkey ) );
    InvalidateRect( m_hvideownd, NULL, TRUE );
    DirectXUpdateRects( true );

    return SUCCESS;
}

/*****************************************************************************
 * DirectXCreateClipper: Create a clipper that will be used when blitting the
 *                       RGB surface to the main display.
 *****************************************************************************
 * This clipper prevents us to modify by mistake anything on the screen
 * which doesn't belong to our window. For example when a part of our video
 * window is hidden by another window.
 *****************************************************************************/
int CDirectX::DirectXCreateClipper()
{
    HRESULT dxresult;

	//_RPT0(_CRT_WARN, "DirectXCreateClipper\n");

    /* Create the clipper */
    dxresult = IDirectDraw7_CreateClipper( m_pDDObject, 0,
                                           &m_pClipper, NULL );
    if( dxresult != DD_OK )
    {
        _RPT1(_CRT_WARN, "cannot create clipper (error %li)\n", dxresult );
        goto error;
    }

    /* Associate the clipper to the window */
    dxresult = IDirectDrawClipper_SetHWnd( m_pClipper, 0,
                                           m_hvideownd );
    if( dxresult != DD_OK )
    {
        _RPT1(_CRT_WARN, "cannot attach clipper to window (error %li)",
                          dxresult );
        goto error;
    }

    /* associate the clipper with the surface */
    dxresult = IDirectDrawSurface7_SetClipper(m_pDisplay,
                                             m_pClipper);
    if( dxresult != DD_OK )
    {
        _RPT1(_CRT_WARN, "cannot attach clipper to surface (error %li)",
                          dxresult );
        goto error;
    }

    return SUCCESS;

 error:
    if( m_pClipper )
    {
        IDirectDrawClipper_Release( m_pClipper );
    }
    m_pClipper = NULL;
    return EGENERIC;
}

/*****************************************************************************
 * DirectXFindColorkey: Finds out the 32bits RGB pixel value of the colorkey
 *****************************************************************************/
DWORD CDirectX::DirectXFindColorkey( UINT32 *pi_color )
{
    DDSURFACEDESC2 ddsd;
    HRESULT dxresult;
    COLORREF i_rgb = 0;
    UINT32 i_pixel_backup;
    HDC hdc;

    ddsd.dwSize = sizeof(ddsd);
    dxresult = IDirectDrawSurface7_Lock( m_pDisplay, NULL,
                                         &ddsd, DDLOCK_WAIT, NULL );
    if( dxresult != DD_OK ) return 0;

    i_pixel_backup = *(UINT32 *)ddsd.lpSurface;

	_RPT2(_CRT_WARN, "DirectXFindColorkey:color:%u,%u\n", *pi_color,ddsd.ddpfPixelFormat.dwRGBBitCount);
    switch( ddsd.ddpfPixelFormat.dwRGBBitCount )
    {
    case 4:
        *(BYTE *)ddsd.lpSurface = *pi_color | (*pi_color << 4);
        break;
    case 8:
        *(BYTE *)ddsd.lpSurface = *pi_color;
        break;
    case 15:
    case 16:
        *(UINT16 *)ddsd.lpSurface = *pi_color;
        break;
    case 24:
        /* Seems to be problematic so we'll just put black as the colorkey */
        *pi_color = 0;
    default:
        *(UINT32 *)ddsd.lpSurface = *pi_color;
        break;
    }

    IDirectDrawSurface7_Unlock( m_pDisplay, NULL );

    if( IDirectDrawSurface7_GetDC( m_pDisplay, &hdc ) == DD_OK )
    {
        i_rgb = GetPixel( hdc, 0, 0 );
        IDirectDrawSurface7_ReleaseDC( m_pDisplay, hdc );
    }

    ddsd.dwSize = sizeof(ddsd);
    dxresult = IDirectDrawSurface7_Lock( m_pDisplay, NULL,
                                         &ddsd, DDLOCK_WAIT, NULL );
	if( dxresult != DD_OK ) {
		_RPT2(_CRT_WARN, "IDirectDrawSurface7_Lock error %u, using %x\n", dxresult,i_rgb);
		return i_rgb;
	}

    *(UINT32 *)ddsd.lpSurface = i_pixel_backup;
	_RPT1(_CRT_WARN, "IDirectDrawSurface7_Lock OK, using %x\n", i_pixel_backup);

    IDirectDrawSurface7_Unlock( m_pDisplay, NULL );

    return i_rgb;
}

/*****************************************************************************
 * DirectXCloseDisplay: close and reset the DirectX display device
 *****************************************************************************
 * This function returns all resources allocated by DirectXCreateDisplay.
 *****************************************************************************/
void CDirectX::DirectXCloseDisplay()
{
    _RPT0(_CRT_WARN, "DirectXCloseDisplay\n" );

    if( m_pClipper != NULL )
    {
        _RPT0(_CRT_WARN, "DirectXCloseDisplay clipper\n" );
        IDirectDrawClipper_Release( m_pClipper );
        m_pClipper = NULL;
    }

    if( m_pDisplay != NULL )
    {
        _RPT0(_CRT_WARN, "DirectXCloseDisplay display\n" );
        IDirectDrawSurface7_Release( m_pDisplay );
        m_pDisplay = NULL;
    }
}

/*****************************************************************************
 * DirectXCloseDDraw: Release the DDraw object allocated by DirectXInitDDraw
 *****************************************************************************
 * This function returns all resources allocated by DirectXInitDDraw.
 *****************************************************************************/
void CDirectX::DirectXCloseDDraw()
{
    _RPT0(_CRT_WARN, "DirectXCloseDDraw\n" );
    if( m_pDDObject != NULL )
    {
        IDirectDraw7_Release(m_pDDObject);
        m_pDDObject = NULL;
    }

    if( m_hDDrawDll != NULL )
    {
        FreeLibrary( m_hDDrawDll );
        m_hDDrawDll = NULL;
    }

    if( m_pDisplayDriver != NULL )
    {
        free( m_pDisplayDriver );
        m_pDisplayDriver = NULL;
    }

    m_hmonitor = NULL;
}

/*****************************************************************************
 * DirectXCloseSurface: close the YUV overlay or RGB surface.
 *****************************************************************************
 * This function returns all resources allocated for the surface.
 *****************************************************************************/
void CDirectX::DirectXCloseSurface( LPDIRECTDRAWSURFACE7 p_surface )
{
    _RPT0(_CRT_WARN, "DirectXCloseSurface\n" );
    if( p_surface != NULL )
    {
        IDirectDrawSurface7_Release( p_surface );
    }
}

/*****************************************************************************
 * DirectXCreateSurface: create an YUV overlay or RGB surface for the video.
 *****************************************************************************
 * The best method of display is with an YUV overlay because the YUV->RGB
 * conversion is done in hardware.
 * You can also create a plain RGB surface.
 * ( Maybe we could also try an RGB overlay surface, which could have hardware
 * scaling and which would also be faster in window mode because you don't
 * need to do any blitting to the main display...)
 *****************************************************************************/
int CDirectX::DirectXCreateSurface( LPDIRECTDRAWSURFACE7 *pp_surface_final,
                                 int i_chroma, int b_overlay,
                                 int i_backbuffers )
{
    HRESULT dxresult;
    LPDIRECTDRAWSURFACE7 p_surface;
    DDSURFACEDESC2 ddsd;

    /* Create the video surface */
    if( b_overlay )
    {
        /* Now try to create the YUV overlay surface.
         * This overlay will be displayed on top of the primary surface.
         * A color key is used to determine whether or not the overlay will be
         * displayed, ie the overlay will be displayed in place of the primary
         * surface wherever the primary surface will have this color.
         * The video window has been created with a background of this color so
         * the overlay will be only displayed on top of this window */

        memset( &ddsd, 0, sizeof( DDSURFACEDESC2 ));
        ddsd.dwSize = sizeof(DDSURFACEDESC2);
        ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
        ddsd.ddpfPixelFormat.dwFlags = DDPF_FOURCC;
        ddsd.ddpfPixelFormat.dwFourCC = i_chroma;
        ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
        ddsd.dwFlags |= (i_backbuffers ? DDSD_BACKBUFFERCOUNT : 0);
        ddsd.ddsCaps.dwCaps = DDSCAPS_OVERLAY | DDSCAPS_VIDEOMEMORY;
        ddsd.ddsCaps.dwCaps |= (i_backbuffers ? DDSCAPS_COMPLEX | DDSCAPS_FLIP
                                : 0 );
        ddsd.dwHeight = m_render.i_height;
        ddsd.dwWidth = m_render.i_width;
        ddsd.dwBackBufferCount = i_backbuffers;

        dxresult = IDirectDraw7_CreateSurface( m_pDDObject,
                                               &ddsd, &p_surface, NULL );
        if( dxresult != DD_OK )
        {
            *pp_surface_final = NULL;
            return EGENERIC;
        }
    }

    if( !b_overlay )
    {
        bool b_rgb_surface =
            ( i_chroma == TME_FOURCC('R','G','B','2') )
          || ( i_chroma == TME_FOURCC('R','V','1','5') )
           || ( i_chroma == TME_FOURCC('R','V','1','6') )
            || ( i_chroma == TME_FOURCC('R','V','2','4') )
             || ( i_chroma == TME_FOURCC('R','V','3','2') );

        memset( &ddsd, 0, sizeof( DDSURFACEDESC2 ) );
        ddsd.dwSize = sizeof(DDSURFACEDESC2);
        ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
        ddsd.dwFlags = DDSD_HEIGHT | DDSD_WIDTH | DDSD_CAPS;
        ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
        ddsd.dwHeight = m_render.i_height;
        ddsd.dwWidth = m_render.i_width;

        if( m_bUseSysmem )
            ddsd.ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;
        else
            ddsd.ddsCaps.dwCaps |= DDSCAPS_VIDEOMEMORY;

        if( !b_rgb_surface )
        {
            ddsd.dwFlags |= DDSD_PIXELFORMAT;
            ddsd.ddpfPixelFormat.dwFlags = DDPF_FOURCC;
            ddsd.ddpfPixelFormat.dwFourCC = i_chroma;
        }

        dxresult = IDirectDraw7_CreateSurface( m_pDDObject,
                                               &ddsd, &p_surface, NULL );
        if( dxresult != DD_OK )
        {
            *pp_surface_final = NULL;
            return EGENERIC;
        }
    }

    /* Now that the surface is created, try to get a newer DirectX interface */
    dxresult = IDirectDrawSurface_QueryInterface( p_surface,
                                     IID_IDirectDrawSurface7,
                                     (LPVOID *)pp_surface_final );
    IDirectDrawSurface_Release( p_surface );    /* Release the old interface */
    if ( dxresult != DD_OK )
    {
        _RPT1(_CRT_WARN, "cannot query IDirectDrawSurface7 interface (error %li)\n", dxresult );
        *pp_surface_final = NULL;
        return EGENERIC;
    }

    if( b_overlay )
    {
        /* Check the overlay is useable as some graphics cards allow creating
         * several overlays but only one can be used at one time. */
        m_pCurrentSurface = *pp_surface_final;
        if( DirectXUpdateOverlay() != SUCCESS )
        {
            IDirectDrawSurface7_Release( *pp_surface_final );
            *pp_surface_final = NULL;
            _RPT0(_CRT_WARN, "overlay unuseable (might already be in use)\n" );
            return EGENERIC;
        }
    }

    return SUCCESS;
}

/*****************************************************************************
 * DirectXUpdateOverlay: Move or resize overlay surface on video display.
 *****************************************************************************
 * This function is used to move or resize an overlay surface on the screen.
 * Ususally the overlay is moved by the user and thus, by a move or resize
 * event (in Manage).
 *****************************************************************************/
int CDirectX::DirectXUpdateOverlay()
{
    DDOVERLAYFX     ddofx;
    DWORD           dwFlags;
    HRESULT         dxresult;
    RECT            rect_src = m_rectSrcClipped;
    RECT            rect_dest = m_rectDestClipped;

    if( !m_bUsingOverlay ) return EGENERIC;

    if( m_bWallpaper )
    {
        unsigned int i_x, i_y, i_width, i_height;

        rect_src.left = m_fmtOut.i_x_offset;
        rect_src.top = m_fmtOut.i_y_offset;
        rect_src.right = rect_src.left + m_fmtOut.i_visible_width;
        rect_src.bottom = rect_src.top + m_fmtOut.i_visible_height;

        rect_dest = m_rectDisplay;
        vout_PlacePicture( this, rect_dest.right, rect_dest.bottom,
                           &i_x, &i_y, &i_width, &i_height );

        rect_dest.left += i_x;
        rect_dest.right = rect_dest.left + i_width;
        rect_dest.top += i_y;
        rect_dest.bottom = rect_dest.top + i_height;
    }

    tme_mutex_lock( &m_lock );
    if( m_pCurrentSurface == NULL )
    {
        tme_mutex_unlock( &m_lock );
        return EGENERIC;
    }

    /* The new window dimensions should already have been computed by the
     * caller of this function */

    /* Position and show the overlay */
    memset(&ddofx, 0, sizeof(DDOVERLAYFX));
    ddofx.dwSize = sizeof(DDOVERLAYFX);
    ddofx.dckDestColorkey.dwColorSpaceLowValue = m_iColorkey;
    ddofx.dckDestColorkey.dwColorSpaceHighValue = m_iColorkey;

    dwFlags = DDOVER_SHOW | DDOVER_KEYDESTOVERRIDE;

    dxresult = IDirectDrawSurface7_UpdateOverlay(
                   m_pCurrentSurface,
                   &rect_src, m_pDisplay, &rect_dest,
                   dwFlags, &ddofx );

	//_RPT4(_CRT_WARN, "Update src:%d, %d, %d, %d\n", rect_src.left, rect_src.top, rect_src.right, rect_src.bottom);
	//_RPT4(_CRT_WARN, "Update dest:%d, %d, %d, %d\n", rect_dest.left, rect_dest.top, rect_dest.right, rect_dest.bottom);
    tme_mutex_unlock( &m_lock );

    if(dxresult != DD_OK)
    {
        _RPT0(_CRT_WARN, "DirectXUpdateOverlay cannot move/resize overlay\n" );
        return EGENERIC;
    }

    return SUCCESS;
}

/*****************************************************************************
 * NewPictureVec: allocate a vector of identical pictures
 *****************************************************************************
 * Returns 0 on success, 1 otherwise
 *****************************************************************************/
int CDirectX::NewPictureVec( picture_t *p_pic, int i_num_pics )
{
    int i;
    int i_ret = SUCCESS;
    LPDIRECTDRAWSURFACE7 p_surface;

    _RPT2(_CRT_WARN, "NewPictureVec overlay:%s chroma:%.4s\n",
             m_bUsingOverlay ? "yes" : "no",
             (char *)&m_output.i_chroma );

    I_OUTPUTPICTURES = 0;

    /* First we try to use an YUV overlay surface.
     * The overlay surface that we create won't be used to decode directly
     * into it because accessing video memory directly is way to slow (remember
     * that pictures are decoded macroblock per macroblock). Instead the video
     * will be decoded in picture buffers in system memory which will then be
     * memcpy() to the overlay surface. */
    if( m_bUsingOverlay )
    {
        /* Triple buffering rocks! it doesn't have any processing overhead
         * (you don't have to wait for the vsync) and provides for a very nice
         * video quality (no tearing). */
		if( m_b3BufOverlay )
            i_ret = DirectXCreateSurface( &p_surface,
                                          m_output.i_chroma,
                                          m_bUsingOverlay,
                                          2 /* number of backbuffers */ );

        if( !m_b3BufOverlay || i_ret != SUCCESS )
        {
            /* Try to reduce the number of backbuffers */
            i_ret = DirectXCreateSurface( &p_surface,
                                          m_output.i_chroma,
                                          m_bUsingOverlay,
                                          0 /* number of backbuffers */ );
        }

        if( i_ret == SUCCESS )
        {
            DDSCAPS2 dds_caps;
            picture_t front_pic;
            picture_sys_t front_pic_sys;
            front_pic.p_sys = &front_pic_sys;

            /* Allocate internal structure */
            p_pic[0].p_sys = (picture_sys_t *)malloc( sizeof( picture_sys_t ) );
            if( p_pic[0].p_sys == NULL )
            {
                DirectXCloseSurface( p_surface );
                return ENOMEM;
            }

            /* set front buffer */
            p_pic[0].p_sys->p_front_surface = p_surface;

            /* Get the back buffer */
            memset( &dds_caps, 0, sizeof( DDSCAPS ) );
            dds_caps.dwCaps = DDSCAPS_BACKBUFFER;
            if( DD_OK != IDirectDrawSurface7_GetAttachedSurface(
                                                p_surface, &dds_caps,
                                                &p_pic[0].p_sys->p_surface ) )
            {
                _RPT0(_CRT_WARN, "NewPictureVec could not get back buffer\n" );
                /* front buffer is the same as back buffer */
                p_pic[0].p_sys->p_surface = p_surface;
            }


            m_pCurrentSurface = front_pic.p_sys->p_surface =
                p_pic[0].p_sys->p_front_surface;

            /* Reset the front buffer memory */
            if( DirectXLockSurface( &front_pic ) == SUCCESS )
            {
                int i,j;
                for( i = 0; i < front_pic.i_planes; i++ )
                    for( j = 0; j < front_pic.p[i].i_visible_lines; j++)
                        memset( front_pic.p[i].p_pixels + j *
                                front_pic.p[i].i_pitch, 127,
                                front_pic.p[i].i_visible_pitch );

                DirectXUnlockSurface( &front_pic );
            }

            DirectXUpdateOverlay();
            I_OUTPUTPICTURES = 1;
			_RPT0(_CRT_WARN, "YUV overlay created successfully\n" );
        }
    }

    /* As we can't have an overlay, we'll try to create a plain offscreen
     * surface. This surface will reside in video memory because there's a
     * better chance then that we'll be able to use some kind of hardware
     * acceleration like rescaling, blitting or YUV->RGB conversions.
     * We then only need to blit this surface onto the main display when we
     * want to display it */
    if( !m_bUsingOverlay )
    {
        if( m_bHwYuv )
        {
            DWORD i_codes;
            DWORD *pi_codes;
            bool b_result = false;

            /* Check if the chroma is supported first. This is required
             * because a few buggy drivers don't mind creating the surface
             * even if they don't know about the chroma. */
            if( IDirectDraw7_GetFourCCCodes( m_pDDObject,
                                             &i_codes, NULL ) == DD_OK )
            {
                pi_codes = (DWORD *)malloc( i_codes * sizeof(DWORD) );
                if( pi_codes && IDirectDraw7_GetFourCCCodes(
                    m_pDDObject, &i_codes, pi_codes ) == DD_OK )
                {
                    for( i = 0; i < (int)i_codes; i++ )
                    {
                        if( m_output.i_chroma == pi_codes[i] )
                        {
                            b_result = true;
                            break;
                        }
                    }
                }
            }

            if( b_result )
                i_ret = DirectXCreateSurface( &p_surface,
                                              m_output.i_chroma,
                                              0 /* no overlay */,
                                              0 /* no back buffers */ );
            else
                m_bHwYuv = false;
        }

        if( i_ret || !m_bHwYuv )
        {
            /* Our last choice is to use a plain RGB surface */
            DDPIXELFORMAT ddpfPixelFormat;

            ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
            IDirectDrawSurface7_GetPixelFormat( m_pDisplay,
                                                &ddpfPixelFormat );

            if( ddpfPixelFormat.dwFlags & DDPF_RGB )
            {
                switch( ddpfPixelFormat.dwRGBBitCount )
                {
                case 8:
                    m_output.i_chroma = TME_FOURCC('R','G','B','2');
                    m_output.pf_setpalette = SetPalette;
                    break;
                case 15:
                    m_output.i_chroma = TME_FOURCC('R','V','1','5');
                    break;
                case 16:
                    m_output.i_chroma = TME_FOURCC('R','V','1','6');
                    break;
                case 24:
                    m_output.i_chroma = TME_FOURCC('R','V','2','4');
                    break;
                case 32:
                    m_output.i_chroma = TME_FOURCC('R','V','3','2');
                    break;
                default:
                    _RPT0(_CRT_WARN, "unknown screen depth\n" );
                    return EGENERIC;
                }
                m_output.i_rmask = ddpfPixelFormat.dwRBitMask;
                m_output.i_gmask = ddpfPixelFormat.dwGBitMask;
                m_output.i_bmask = ddpfPixelFormat.dwBBitMask;
            }

            m_bHwYuv = false;

            i_ret = DirectXCreateSurface( &p_surface,
                                          m_output.i_chroma,
                                          0 /* no overlay */,
                                          0 /* no back buffers */ );

            if( i_ret && !m_bUseSysmem )
            {
                /* Give it a last try with b_use_sysmem enabled */
                m_bUseSysmem = true;

                i_ret = DirectXCreateSurface( &p_surface,
                                              m_output.i_chroma,
                                              0 /* no overlay */,
                                              0 /* no back buffers */ );
            }
        }

        if( i_ret == SUCCESS )
        {
            /* Allocate internal structure */
            p_pic[0].p_sys = (picture_sys_t *)malloc( sizeof( picture_sys_t ) );
            if( p_pic[0].p_sys == NULL )
            {
                DirectXCloseSurface( p_surface );
                return ENOMEM;
            }

            p_pic[0].p_sys->p_surface = p_pic[0].p_sys->p_front_surface
                = p_surface;

            I_OUTPUTPICTURES = 1;

			_RPT1(_CRT_WARN, "created plain surface of chroma:%.4s\n",
                     (char *)&m_output.i_chroma );
        }
    }


    /* Now that we've got all our direct-buffers, we can finish filling in the
     * picture_t structures */
    for( i = 0; i < I_OUTPUTPICTURES; i++ )
    {
        p_pic[i].i_status = DESTROYED_PICTURE;
        p_pic[i].i_type   = DIRECT_PICTURE;
        p_pic[i].b_slow   = true;
		// changed to use virtual fuction Lock, Unlock
        //p_pic[i].pf_lock  = &CDirectX::DirectXLockSurface;
        //p_pic[i].pf_unlock = &CDirectX::DirectXUnlockSurface;
        PP_OUTPUTPICTURE[i] = &p_pic[i];

        if( DirectXLockSurface( &p_pic[i] ) != SUCCESS )
        {
            /* AAARRGG */
            FreePictureVec( p_pic, I_OUTPUTPICTURES );
            I_OUTPUTPICTURES = 0;
            _RPT0(_CRT_WARN, "cannot lock surface\n" );
            return EGENERIC;
        }
        DirectXUnlockSurface( &p_pic[i] );
    }

    _RPT1(_CRT_WARN, "End NewPictureVec (%s)\n",
             I_OUTPUTPICTURES ? "succeeded" : "failed" );

    return SUCCESS;
}

/*****************************************************************************
 * DirectXLockSurface: Lock surface and get picture data pointer
 *****************************************************************************
 * This function locks a surface and get the surface descriptor which amongst
 * other things has the pointer to the picture data.
 *****************************************************************************/
int CDirectX::DirectXLockSurface( picture_t *p_pic )
{
    HRESULT dxresult;

    /* Lock the surface to get a valid pointer to the picture buffer */
    memset( &p_pic->p_sys->ddsd, 0, sizeof( DDSURFACEDESC2 ));
    p_pic->p_sys->ddsd.dwSize = sizeof(DDSURFACEDESC2);
    dxresult = IDirectDrawSurface7_Lock( p_pic->p_sys->p_surface,
                                         NULL, &p_pic->p_sys->ddsd,
                                         DDLOCK_NOSYSLOCK | DDLOCK_WAIT,
                                         NULL );
    if( dxresult != DD_OK )
    {
        if( dxresult == DDERR_INVALIDPARAMS )
        {
            /* DirectX 3 doesn't support the DDLOCK_NOSYSLOCK flag, resulting
             * in an invalid params error */
            dxresult = IDirectDrawSurface7_Lock( p_pic->p_sys->p_surface, NULL,
                                             &p_pic->p_sys->ddsd,
                                             DDLOCK_WAIT, NULL);
        }
        if( dxresult == DDERR_SURFACELOST )
        {
            /* Your surface can be lost so be sure
             * to check this and restore it if needed */

            /* When using overlays with back-buffers, we need to restore
             * the front buffer so the back-buffers get restored as well. */
            if( m_bUsingOverlay  )
                IDirectDrawSurface7_Restore( p_pic->p_sys->p_front_surface );
            else
                IDirectDrawSurface7_Restore( p_pic->p_sys->p_surface );

            dxresult = IDirectDrawSurface7_Lock( p_pic->p_sys->p_surface, NULL,
                                                 &p_pic->p_sys->ddsd,
                                                 DDLOCK_WAIT, NULL);
#if 0
            if( dxresult == DDERR_SURFACELOST )
                _RPT0(_CRT_WARN, "DirectXLockSurface: DDERR_SURFACELOST\n" );
#endif
        }
        if( dxresult != DD_OK )
        {
            return EGENERIC;
        }
    }

    /* Now we have a pointer to the surface memory, we can update our picture
     * structure. */
    if( UpdatePictureStruct( p_pic, m_output.i_chroma )
        != SUCCESS )
    {
        DirectXUnlockSurface( p_pic );
        return EGENERIC;
    }
    else
        return SUCCESS;
}

/*****************************************************************************
 * DirectXUnlockSurface: Unlock a surface locked by DirectXLockSurface().
 *****************************************************************************/
int CDirectX::DirectXUnlockSurface( picture_t *p_pic )
{
    /* Unlock the Surface */
    if( IDirectDrawSurface7_Unlock( p_pic->p_sys->p_surface, NULL ) == DD_OK )
        return SUCCESS;
    else
        return EGENERIC;
}

/*****************************************************************************
 * FreePicture: destroy a picture vector allocated with NewPictureVec
 *****************************************************************************
 *
 *****************************************************************************/
void CDirectX::FreePictureVec( picture_t *p_pic, int i_num_pics )
{
    int i;

    tme_mutex_lock( &m_lock );
    m_pCurrentSurface = NULL;
    tme_mutex_unlock( &m_lock );

    for( i = 0; i < i_num_pics; i++ )
    {
        DirectXCloseSurface( p_pic[i].p_sys->p_front_surface );

        for( i = 0; i < i_num_pics; i++ )
        {
            free( p_pic[i].p_sys );
        }
    }
}

/*****************************************************************************
 * UpdatePictureStruct: updates the internal data in the picture_t structure
 *****************************************************************************
 * This will setup stuff for use by the video_output thread
 *****************************************************************************/
int CDirectX::UpdatePictureStruct( picture_t *p_pic, int i_chroma )
{
    switch( m_output.i_chroma )
    {
        case TME_FOURCC('R','G','B','2'):
        case TME_FOURCC('R','V','1','5'):
        case TME_FOURCC('R','V','1','6'):
        case TME_FOURCC('R','V','2','4'):
        case TME_FOURCC('R','V','3','2'):
            p_pic->p->p_pixels = (PBYTE)p_pic->p_sys->ddsd.lpSurface;
            p_pic->p->i_lines = m_output.i_height;
            p_pic->p->i_visible_lines = m_output.i_height;
            p_pic->p->i_pitch = p_pic->p_sys->ddsd.lPitch;
            switch( m_output.i_chroma )
            {
                case TME_FOURCC('R','G','B','2'):
                    p_pic->p->i_pixel_pitch = 1;
                    break;
                case TME_FOURCC('R','V','1','5'):
                case TME_FOURCC('R','V','1','6'):
                    p_pic->p->i_pixel_pitch = 2;
                    break;
                case TME_FOURCC('R','V','2','4'):
                    p_pic->p->i_pixel_pitch = 3;
                    break;
                case TME_FOURCC('R','V','3','2'):
                    p_pic->p->i_pixel_pitch = 4;
                    break;
                default:
                    return EGENERIC;
            }
            p_pic->p->i_visible_pitch = m_output.i_width *
              p_pic->p->i_pixel_pitch;
            p_pic->i_planes = 1;
            break;

        case TME_FOURCC('Y','V','1','2'):
        case TME_FOURCC('I','4','2','0'):

            /* U and V inverted compared to I420
             * Fixme: this should be handled by the vout core */
            m_output.i_chroma = TME_FOURCC('I','4','2','0');

            p_pic->Y_PIXELS = (PBYTE)p_pic->p_sys->ddsd.lpSurface;
            p_pic->p[Y_PLANE].i_lines = m_output.i_height;
            p_pic->p[Y_PLANE].i_visible_lines = m_output.i_height;
            p_pic->p[Y_PLANE].i_pitch = p_pic->p_sys->ddsd.lPitch;
            p_pic->p[Y_PLANE].i_pixel_pitch = 1;
            p_pic->p[Y_PLANE].i_visible_pitch = m_output.i_width *
              p_pic->p[Y_PLANE].i_pixel_pitch;

            p_pic->V_PIXELS =  p_pic->Y_PIXELS
              + p_pic->p[Y_PLANE].i_lines * p_pic->p[Y_PLANE].i_pitch;
            p_pic->p[V_PLANE].i_lines = m_output.i_height / 2;
            p_pic->p[V_PLANE].i_visible_lines = m_output.i_height / 2;
            p_pic->p[V_PLANE].i_pitch = p_pic->p[Y_PLANE].i_pitch / 2;
            p_pic->p[V_PLANE].i_pixel_pitch = 1;
            p_pic->p[V_PLANE].i_visible_pitch = m_output.i_width / 2 *
              p_pic->p[V_PLANE].i_pixel_pitch;

            p_pic->U_PIXELS = p_pic->V_PIXELS
              + p_pic->p[V_PLANE].i_lines * p_pic->p[V_PLANE].i_pitch;
            p_pic->p[U_PLANE].i_lines = m_output.i_height / 2;
            p_pic->p[U_PLANE].i_visible_lines = m_output.i_height / 2;
            p_pic->p[U_PLANE].i_pitch = p_pic->p[Y_PLANE].i_pitch / 2;
            p_pic->p[U_PLANE].i_pixel_pitch = 1;
            p_pic->p[U_PLANE].i_visible_pitch = m_output.i_width / 2 *
              p_pic->p[U_PLANE].i_pixel_pitch;

            p_pic->i_planes = 3;
            break;

        case TME_FOURCC('I','Y','U','V'):

            p_pic->Y_PIXELS = (PBYTE)p_pic->p_sys->ddsd.lpSurface;
            p_pic->p[Y_PLANE].i_lines = m_output.i_height;
            p_pic->p[Y_PLANE].i_visible_lines = m_output.i_height;
            p_pic->p[Y_PLANE].i_pitch = p_pic->p_sys->ddsd.lPitch;
            p_pic->p[Y_PLANE].i_pixel_pitch = 1;
            p_pic->p[Y_PLANE].i_visible_pitch = m_output.i_width *
              p_pic->p[Y_PLANE].i_pixel_pitch;

            p_pic->U_PIXELS = p_pic->Y_PIXELS
              + p_pic->p[Y_PLANE].i_lines * p_pic->p[Y_PLANE].i_pitch;
            p_pic->p[U_PLANE].i_lines = m_output.i_height / 2;
            p_pic->p[U_PLANE].i_visible_lines = m_output.i_height / 2;
            p_pic->p[U_PLANE].i_pitch = p_pic->p[Y_PLANE].i_pitch / 2;
            p_pic->p[U_PLANE].i_pixel_pitch = 1;
            p_pic->p[U_PLANE].i_visible_pitch = m_output.i_width / 2 *
              p_pic->p[U_PLANE].i_pixel_pitch;

            p_pic->V_PIXELS =  p_pic->U_PIXELS
              + p_pic->p[U_PLANE].i_lines * p_pic->p[U_PLANE].i_pitch;
            p_pic->p[V_PLANE].i_lines = m_output.i_height / 2;
            p_pic->p[V_PLANE].i_visible_lines = m_output.i_height / 2;
            p_pic->p[V_PLANE].i_pitch = p_pic->p[Y_PLANE].i_pitch / 2;
            p_pic->p[V_PLANE].i_pixel_pitch = 1;
            p_pic->p[V_PLANE].i_visible_pitch = m_output.i_width / 2 *
              p_pic->p[V_PLANE].i_pixel_pitch;

            p_pic->i_planes = 3;
            break;

        case TME_FOURCC('U','Y','V','Y'):
        case TME_FOURCC('Y','U','Y','2'):

            p_pic->p->p_pixels = (PBYTE)p_pic->p_sys->ddsd.lpSurface;
            p_pic->p->i_lines = m_output.i_height;
            p_pic->p->i_visible_lines = m_output.i_height;
            p_pic->p->i_pitch = p_pic->p_sys->ddsd.lPitch;
            p_pic->p->i_pixel_pitch = 2;
            p_pic->p->i_visible_pitch = m_output.i_width *
              p_pic->p->i_pixel_pitch;

            p_pic->i_planes = 1;
            break;

        default:
            /* Unknown chroma, tell the guy to get lost */
            _RPT2(_CRT_WARN, "never heard of chroma 0x%.8x (%4.4s)\n",
                     m_output.i_chroma,
                     (char*)&m_output.i_chroma );
            return EGENERIC;
    }

    return SUCCESS;
}

int CDirectX::DirectXEventThread()
{
    MSG msg;
    POINT old_mouse_pos = {0,0};//, mouse_pos;
    //unsigned int i_width, i_height, i_x, i_y;
	HMODULE hkernel32;

    /* Create a window for the video */
    /* Creating a window under Windows also initializes the thread's event
     * message queue */
    if( DirectXCreateWindow() )
    {
        _RPT0(_CRT_WARN, "out of memory or wrong window handle\n" );
        m_bEventDead = true;
    }

	tme_thread_ready(&m_ipc);

    /* Set power management stuff */
    if( (hkernel32 = GetModuleHandle( _T("KERNEL32") ) ) )
    {
        ULONG (WINAPI* OurSetThreadExecutionState)( ULONG );

        OurSetThreadExecutionState = (ULONG (WINAPI*)( ULONG ))
            GetProcAddress( hkernel32, "SetThreadExecutionState" );

        if( OurSetThreadExecutionState )
            /* Prevent monitor from powering off */
            OurSetThreadExecutionState( ES_DISPLAY_REQUIRED | ES_CONTINUOUS );
        else
            _RPT0(_CRT_WARN, "no support for SetThreadExecutionState()\n" );
    }

    /* Main loop */
    /* GetMessage will sleep if there's no message in the queue */
    while( !m_bEventDie)
    {
		 int ret = GetMessage( &msg, 0, 0, 0 );
		 if (!ret) // 0: received WM_QUIT, -1: error
			 break;

        /* Check if we are asked to exit */
        if( m_bEventDie )
            break;


		switch( msg.message )
        {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
			PostMessage(m_hparent, msg.message, msg.wParam, msg.lParam);
            break;

        default:
            /* Messages we don't handle directly are dispatched to the
             * window procedure */
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            //break;

        } /* End Switch */
    } /* End Main loop */

    /* Check for WM_QUIT if we created the window */
    if( !m_hparent && msg.message == WM_QUIT )
    {
        _RPT0(_CRT_WARN, "WM_QUIT... should not happen!!\n" );
        m_hwnd = NULL; /* Window already destroyed */
    }

    _RPT0(_CRT_WARN, "DirectXEventThread terminating\n" );

    /* clear the changes formerly signaled */
    m_iChanges = 0;

    DirectXCloseWindow();

	return 0;
}

/*****************************************************************************
 * DirectXCreateWindow: create a window for the video.
 *****************************************************************************
 * Before creating a direct draw surface, we need to create a window in which
 * the video will be displayed. This window will also allow us to capture the
 * events.
 *****************************************************************************/
int CDirectX::DirectXCreateWindow()
{
    HINSTANCE  hInstance;
    //HMENU      hMenu;
    RECT       rect_window;
    WNDCLASS   wc;                            /* window class components */
    HICON      tme_icon = NULL;
    TCHAR       tme_path[MAX_PATH+1];
    int        i_style, i_stylex;

    _RPT0(_CRT_WARN, "DirectXCreateWindow\n" );

    /* Get this module's instance */
    hInstance = GetModuleHandle(NULL);

	if (m_hparent) {
		// specify client area coordinate here
		m_iWindowX = 0;
		m_iWindowY = 0;

		//_RPT3(_CRT_WARN, "parent window(%08x) x:%d, y:%d\n", m_hparent, m_iWindowX, m_iWindowY);
	}

    /* We create the window ourself, there is no previous window proc. */
    m_pfWndproc = NULL;

    /* Get the Icon from the main app */
    if( GetModuleFileName( NULL, tme_path, MAX_PATH ) )
    {
		tme_icon = ::ExtractIcon( hInstance, tme_path, 0 );
    }

    /* Fill in the window class structure */
    wc.style         = CS_OWNDC | CS_DBLCLKS;          /* style: dbl click */
    wc.lpfnWndProc   = (WNDPROC)DirectXEventProc;       /* event handler */
    wc.cbClsExtra    = 0;                         /* no extra class data */
    wc.cbWndExtra    = 0;                        /* no extra window data */
    wc.hInstance     = hInstance;                            /* instance */
    wc.hIcon         = tme_icon;                /* load the vlc big icon */
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);    /* default cursor */
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);  /* background color */
    wc.lpszMenuName  = NULL;                                  /* no menu */
    wc.lpszClassName = _T("TME DirectX");         /* use a special class */

    /* Register the window class */
    if( !RegisterClass(&wc) )
    {
        WNDCLASS wndclass;

        if( tme_icon ) DestroyIcon( tme_icon );

        /* Check why it failed. If it's because one already exists
         * then fine, otherwise return with an error. */
        if( !GetClassInfo( hInstance, _T("TME DirectX"), &wndclass ) )
        {
            _RPT1(_CRT_WARN, "DirectXCreateWindow RegisterClass FAILED (err=%lu)\n", GetLastError() );
            return EGENERIC;
        }
    }

    /* Register the video sub-window class */
    wc.lpszClassName = _T("TME DirectX video"); wc.hIcon = 0;
    if( !RegisterClass(&wc) )
    {
        WNDCLASS wndclass;

        /* Check why it failed. If it's because one already exists
         * then fine, otherwise return with an error. */
        if( !GetClassInfo( hInstance, _T("TME DirectX video"), &wndclass ) )
        {
            _RPT1(_CRT_WARN, "DirectXCreateWindow RegisterClass FAILED (err=%lu)\n", GetLastError() );
            return EGENERIC;
        }
    }

    /* When you create a window you give the dimensions you wish it to
     * have. Unfortunatly these dimensions will include the borders and
     * titlebar. We use the following function to find out the size of
     * the window corresponding to the useable surface we want */
    rect_window.top    = 10;
    rect_window.left   = 10;
    rect_window.right  = rect_window.left + m_iWindowWidth;
    rect_window.bottom = rect_window.top + m_iWindowHeight;

	bool video_deco = false;
    if( video_deco )
    {
        /* Open with window decoration */
        AdjustWindowRect( &rect_window, WS_OVERLAPPEDWINDOW|WS_SIZEBOX, 0 );
        i_style = WS_OVERLAPPEDWINDOW|WS_SIZEBOX|WS_VISIBLE|WS_CLIPCHILDREN;
        i_stylex = 0;
    }
    else
    {
        /* No window decoration */
        AdjustWindowRect( &rect_window, WS_POPUP, 0 );
        i_style = WS_POPUP|WS_VISIBLE|WS_CLIPCHILDREN;
        i_stylex = 0; // WS_EX_TOOLWINDOW; Is TOOLWINDOW really needed ?
                      // It messes up the fullscreen window.
    }

    if( m_hparent )
    {
        i_style = WS_VISIBLE|WS_CLIPCHILDREN|WS_CHILD;
        i_stylex = 0;
    }

    m_iWindowStyle = i_style;

    _RPT0(_CRT_WARN, "Creating Main window\n");
    /* Create the window */
    m_hwnd =
        CreateWindowEx( WS_EX_NOPARENTNOTIFY | i_stylex,
                    _T("TME DirectX"),               /* name of window class */
                    _T("(DirectX Output1)"),  /* window title */
                    i_style,                                 /* window style */
                    (m_iWindowX < 0) ? CW_USEDEFAULT :
                        (UINT)m_iWindowX,   /* default X coordinate */
                    (m_iWindowY < 0) ? CW_USEDEFAULT :
                        (UINT)m_iWindowY,   /* default Y coordinate */
                    rect_window.right - rect_window.left,    /* window width */
                    rect_window.bottom - rect_window.top,   /* window height */
                    m_hparent,                 /* parent window */
                    NULL,                          /* no menu in this window */
                    hInstance,            /* handle of this program instance */
                    (LPVOID)this );            /* send p_vout to WM_CREATE */

    if( !m_hwnd )
    {
        _RPT1(_CRT_WARN, "DirectXCreateWindow create window FAILED (err=%lu)\n", GetLastError() );
        return EGENERIC;
    }

    if( m_hparent )
    {
        LONG i_style;
		HWND hParentOwner = GetParent(m_hparent);

		if (hParentOwner != NULL) {
	        i_style = GetWindowLong( hParentOwner, GWL_STYLE );

			if( !(i_style & WS_CLIPCHILDREN) ) {
				SetWindowLong( hParentOwner, GWL_STYLE,
					           i_style | WS_CLIPCHILDREN );
			}
		}

        /* We don't want the window owner to overwrite our client area */
        i_style = GetWindowLong( m_hparent, GWL_STYLE );

        if( !(i_style & WS_CLIPCHILDREN) )
            /* Hmmm, apparently this is a blocking call... */
            SetWindowLong( m_hparent, GWL_STYLE,
                           i_style | WS_CLIPCHILDREN );

    }

    /* Create video sub-window. This sub window will always exactly match
     * the size of the video, which allows us to use crazy overlay colorkeys
     * without having them shown outside of the video area. */
    m_hvideownd =
    CreateWindow( _T("TME DirectX video"), _T(""),   /* window class */
        WS_CHILD | WS_VISIBLE,                   /* window style */
        0, 0,
        m_render.i_width,         /* default width */
        m_render.i_height,        /* default height */
        m_hwnd,                    /* parent window */
        NULL, hInstance,
        (LPVOID)this );            /* send p_vout to WM_CREATE */

    if( !m_hvideownd )
		_RPT0(_CRT_WARN, "can't create video sub-window\n" );
    else
    	_RPT0(_CRT_WARN, "created video sub-window\n" );

    /* Now display the window */
    ShowWindow( m_hwnd, SW_SHOW );

    return SUCCESS;
}

/*****************************************************************************
 * DirectXCloseWindow: close the window created by DirectXCreateWindow
 *****************************************************************************
 * This function returns all resources allocated by DirectXCreateWindow.
 *****************************************************************************/
void CDirectX::DirectXCloseWindow()
{
    _RPT0(_CRT_WARN, "DirectXCloseWindow\n" );

    DestroyWindow( m_hwnd );
    if( m_hfswnd ) DestroyWindow( m_hfswnd );

    //if( m_hparent )
    //    vout_ReleaseWindow( p_vout, (void *)p_vout->p_sys->hparent );

    m_hwnd = NULL;

    /* We don't unregister the Window Class because it could lead to race
     * conditions and it will be done anyway by the system when the app will
     * exit */
}

/*****************************************************************************
 * DirectXEventProc: This is the window event processing function.
 *****************************************************************************
 * On Windows, when you create a window you have to attach an event processing
 * function to it. The aim of this function is to manage "Queued Messages" and
 * "Nonqueued Messages".
 * Queued Messages are those picked up and retransmitted by vout_Manage
 * (using the GetMessage and DispatchMessage functions).
 * Nonqueued Messages are those that Windows will send directly to this
 * procedure (like WM_DESTROY, WM_WINDOWPOSCHANGED...)
 *****************************************************************************/
static long FAR PASCAL DirectXEventProc( HWND hwnd, UINT message,
                                         WPARAM wParam, LPARAM lParam )
{
	CDirectX *p_vout;

    if( message == WM_CREATE )
    {
        /* Store p_vout for future use */
        p_vout = (CDirectX *)((CREATESTRUCT *)lParam)->lpCreateParams;
        SetWindowLongPtr( hwnd, GWLP_USERDATA, (LONG_PTR)p_vout );
        return TRUE;
    }
    else
    {
        p_vout = (CDirectX *)GetWindowLongPtr( hwnd, GWLP_USERDATA );
        if( !p_vout )
        {
            /* Hmmm mozilla does manage somehow to save the pointer to our
             * windowproc and still calls it after the vout has been closed. */
            return (long)DefWindowProc(hwnd, message, wParam, lParam);
        }
    }

	/* Catch the screensaver and the monitor turn-off */
    if( message == WM_SYSCOMMAND &&
        ( (wParam & 0xFFF0) == SC_SCREENSAVE || (wParam & 0xFFF0) == SC_MONITORPOWER ) )
    {
        return 0; /* this stops them from happening */
    }

	if( hwnd == p_vout->m_hvideownd ) {
        return (long)DefWindowProc(hwnd, message, wParam, lParam);
	}

    switch( message )
    {
	case WM_NULL: /* Relay message received from CloseWindow to this thread's message queue */
		if (InSendMessage()) 
			ReplyMessage(TRUE); 
		return PostMessage(hwnd, WM_NULL, 0,0);

    case WM_WINDOWPOSCHANGED:
        p_vout->DirectXUpdateRects( true );
        return 0;

    /* the user wants to close the window */
    case WM_CLOSE:
    {
        return 0;
    }

    /* the window has been closed so shut down everything now */
    case WM_DESTROY:
        /* just destroy the window */
        PostQuitMessage( 0 );
        return 0;

	case WM_PAINT:
    case WM_NCPAINT:
    case WM_ERASEBKGND:
        /* We do not want to relay these messages to the parent window
         * because we rely on the background color for the overlay. */
        return (long)DefWindowProc(hwnd, message, wParam, lParam);

    default:
        break;
    }

	/* Let windows handle the message */
    return (long)DefWindowProc(hwnd, message, wParam, lParam);
}

/*****************************************************************************
 * DirectXUpdateRects: update clipping rectangles
 *****************************************************************************
 * This function is called when the window position or size are changed, and
 * its job is to update the source and destination RECTs used to display the
 * picture.
 *****************************************************************************/
void CDirectX::DirectXUpdateRects( bool b_force )
{
    unsigned int i_width, i_height, i_x, i_y;

    RECT  rect;
    POINT point;

	//_RPT1(_CRT_WARN, "DirectXUpdateRects:%d\n", b_force );

	/* Retrieve the window size */
    GetClientRect( m_hwnd, &rect );

    /* Retrieve the window position */
    point.x = point.y = 0;
    ClientToScreen( m_hwnd, &point );

    /* If nothing changed, we can return */
    if( !b_force
         && m_iWindowWidth == rect.right
         && m_iWindowHeight == rect.bottom
         && m_iWindowX == point.x
         && m_iWindowY == point.y )
    {
        return;
    }

    /* Update the window position and size */
    m_iWindowX = point.x;
    m_iWindowY = point.y;
    m_iWindowWidth = rect.right;
    m_iWindowHeight = rect.bottom;

    vout_PlacePicture( this, rect.right, rect.bottom,
                       &i_x, &i_y, &i_width, &i_height );

    if( m_hvideownd )
        SetWindowPos( m_hvideownd, HWND_TOP,
                      i_x, i_y, i_width, i_height, 0 );

    /* Destination image position and dimensions */
    m_rectDest.left = point.x + i_x;
    m_rectDest.right = m_rectDest.left + i_width;
    m_rectDest.top = point.y + i_y;
    m_rectDest.bottom = m_rectDest.top + i_height;

#ifdef MODULE_NAME_IS_vout_directx
    /* Apply overlay hardware constraints */
    if( m_bUsingOverlay )
    {
        if( m_iAlignDestBoundary )
            m_rectDest.left = ( m_rectDest.left +
                m_iAlignDestBoundary / 2 ) & 
                ~m_iAlignDestBoundary;

        if( m_iAlignDestSize )
            m_rectDest.right = (( m_rectDest.right - m_rectDest.left +
                m_iAlignDestSize / 2 ) & 
                ~m_iAlignDestSize) + m_rectDest.left;
    }
#endif

    /* UpdateOverlay directdraw function doesn't automatically clip to the
     * display size so we need to do it otherwise it will fail */

    /* Clip the destination window */
    if( !IntersectRect( &m_rectDestClipped, &m_rectDest,
                        &m_rectDisplay ) )
    {
        SetRectEmpty( &m_rectSrcClipped );
        return;
    }

#if 0
    _RPT4(_CRT_WARN, "DirectXUpdateRects image_dst_clipped coords: %i,%i,%i,%i\n",
                     m_rectDestClipped.left, m_rectDestClipped.top,
                     m_rectDestClipped.right, m_rectDestClipped.bottom );
#endif

    /* the 2 following lines are to fix a bug when clicking on the desktop */
    if( (m_rectDestClipped.right - m_rectDestClipped.left)==0 ||
        (m_rectDestClipped.bottom - m_rectDestClipped.top)==0 )
    {
        SetRectEmpty( &m_rectSrcClipped );
        return;
    }

    /* src image dimensions */
    m_rectSrc.left = 0;
    m_rectSrc.top = 0;
    m_rectSrc.right = m_render.i_width;
    m_rectSrc.bottom = m_render.i_height;

    /* Clip the source image */
    m_rectSrcClipped.left = m_fmtOut.i_x_offset +
      (m_rectDestClipped.left - m_rectDest.left) *
      m_fmtOut.i_visible_width / (m_rectDest.right - m_rectDest.left);
    m_rectSrcClipped.right = m_fmtOut.i_x_offset +
      m_fmtOut.i_visible_width -
      (m_rectDest.right - m_rectDestClipped.right) *
      m_fmtOut.i_visible_width / (m_rectDest.right - m_rectDest.left);
    m_rectSrcClipped.top = m_fmtOut.i_y_offset +
      (m_rectDestClipped.top - m_rectDest.top) *
      m_fmtOut.i_visible_height / (m_rectDest.bottom - m_rectDest.top);
    m_rectSrcClipped.bottom = m_fmtOut.i_y_offset +
      m_fmtOut.i_visible_height -
      (m_rectDest.bottom - m_rectDestClipped.bottom) *
      m_fmtOut.i_visible_height / (m_rectDest.bottom - m_rectDest.top);

#ifdef MODULE_NAME_IS_vout_directx
    /* Apply overlay hardware constraints */
    if( m_bUsingOverlay )
    {
        if( m_iAlignSrcBoundary )
            m_rectSrcClipped.left = ( m_rectSrcClipped.left +
                m_iAlignSrcBoundary / 2 ) & 
                ~m_iAlignSrcBoundary;

        if( m_iAlignSrcSize )
            m_rectSrcClipped.right = (( m_rectSrcClipped.right -
                m_rectSrcClipped.left +
                m_iAlignSrcSize / 2 ) & 
                ~m_iAlignSrcSize) + m_rectSrcClipped.left;
    }
#endif

#if 0
    _RPT4(_CRT_WARN, "DirectXUpdateRects image_src_clipped coords: %i,%i,%i,%i\n",
                     m_rectSrcClipped.left, m_rectSrcClipped.top,
                     m_rectSrcClipped.right, m_rectSrcClipped.bottom );
#endif

    /* The destination coordinates need to be relative to the current
     * directdraw primary surface (display) */
    m_rectDestClipped.left -= m_rectDisplay.left;
    m_rectDestClipped.right -= m_rectDisplay.left;
    m_rectDestClipped.top -= m_rectDisplay.top;
    m_rectDestClipped.bottom -= m_rectDisplay.top;

#ifdef MODULE_NAME_IS_vout_directx
    if( m_bUsingOverlay )
        DirectXUpdateOverlay();
#endif

    /* Signal the change in size/position */
    m_iChanges |= DX_POSITION_CHANGE;
}

/*****************************************************************************
 * DirectXEnumCallback: Device enumeration
 *****************************************************************************
 * This callback function is called by DirectDraw once for each
 * available DirectDraw device.
 *****************************************************************************/
BOOL WINAPI DirectXEnumCallback( GUID* p_guid, LPTSTR psz_desc,
                                 LPTSTR psz_drivername, VOID* p_context,
                                 HMONITOR hmon )
{
    CDirectX *p_vout = (CDirectX *)p_context;
	TCHAR *device;

    _RPT2(_CRT_WARN, "DirectXEnumCallback: %s, %s\n", psz_desc, psz_drivername );

    if( hmon )
    {
        device = _tcsdup(_T("")); // set this to specific value if necessary

        if( ( !device || !*device ) &&
            hmon == p_vout->m_hmonitor )
        {
            if( device ) free( device );
        }
        else if( _tcscmp( psz_drivername, device ) == 0 )
        {
            MONITORINFO monitor_info;
            monitor_info.cbSize = sizeof( MONITORINFO );

            if( p_vout->m_GetMonitorInfo( hmon, &monitor_info ) )
            {
                RECT rect;

                /* Move window to the right screen */
                GetWindowRect( p_vout->m_hwnd, &rect );
                if( !IntersectRect( &rect, &rect, &monitor_info.rcWork ) )
                {
                    rect.left = monitor_info.rcWork.left;
                    rect.top = monitor_info.rcWork.top;
                    _RPT2(_CRT_WARN, "DirectXEnumCallback: setting window position to %ld,%ld", rect.left, rect.top );
                    SetWindowPos( p_vout->m_hwnd, NULL,
                                  rect.left, rect.top, 0, 0,
                                  SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE );
                }
            }

            p_vout->m_hmonitor = hmon;
            if( device ) free( device );
        }
        else
        {
            if( device ) free( device );
            return TRUE; /* Keep enumerating */
        }

        _RPT2(_CRT_WARN, "selecting %s, %s", psz_desc, psz_drivername );
        p_vout->m_pDisplayDriver = (GUID *)malloc( sizeof(GUID) );
        if( p_vout->m_pDisplayDriver )
            memcpy( p_vout->m_pDisplayDriver, p_guid, sizeof(GUID) );
    }

    return TRUE; /* Keep enumerating */
}

/*****************************************************************************
 * SetPalette: sets an 8 bpp palette
 *****************************************************************************/
static void SetPalette( UINT16 *red, UINT16 *green, UINT16 *blue )
{
    _RPT0(_CRT_WARN, "FIXME: SetPalette unimplemented\n" );
}
