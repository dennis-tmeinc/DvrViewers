#include "stdafx.h"
#include <stdio.h>
#include <crtdbg.h>
#include <errno.h>
#include <process.h>
#include <shellapi.h>
#include "direct3d.h"
#include "vout_picture.h"
#include "trace.h"
#include "fourcc.h"

typedef struct 
{
    FLOAT       x,y,z;      // vertex untransformed position 
    FLOAT       rhw;        // eye distance
    D3DCOLOR    diffuse;    // diffuse color
    FLOAT       tu, tv;     // texture relative coordinates
} CUSTOMVERTEX;

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1)

struct picture_sys_t
{
    LPDIRECT3DSURFACE9 surface;
    picture_t          *fallback;
};

static const d3d_format_t d3d_formats[] = {
    /* YV12 is always used for planar 420, the planes are then swapped in Lock() */
    { _T("YV12"),       D3DFORMAT('21VY'),    CODEC_YV12,  0,0,0 },
    { _T("YV12"),       D3DFORMAT('21VY'),    CODEC_I420,  0,0,0 },
    { _T("YV12"),       D3DFORMAT('21VY'),    CODEC_J420,  0,0,0 },
    { _T("UYVY"),       D3DFMT_UYVY,    CODEC_UYVY,  0,0,0 },
    { _T("YUY2"),       D3DFMT_YUY2,    CODEC_YUYV,  0,0,0 },
    { _T("X8R8G8B8"),   D3DFMT_X8R8G8B8,CODEC_RGB32, 0xff0000, 0x00ff00, 0x0000ff },
    { _T("A8R8G8B8"),   D3DFMT_A8R8G8B8,CODEC_RGB32, 0xff0000, 0x00ff00, 0x0000ff },
    { _T("8G8B8"),      D3DFMT_R8G8B8,  CODEC_RGB24, 0xff0000, 0x00ff00, 0x0000ff },
    { _T("R5G6B5"),     D3DFMT_R5G6B5,  CODEC_RGB16, 0x1f<<11, 0x3f<<5,  0x1f<<0 },
    { _T("X1R5G5B5"),   D3DFMT_X1R5G5B5,CODEC_RGB15, 0x1f<<10, 0x1f<<5,  0x1f<<0 },
    { NULL, D3DFMT_UNKNOWN, 0,0,0,0}
};

static long FAR PASCAL Direct3DEventProc( HWND hwnd, UINT message,
                                         WPARAM wParam, LPARAM lParam );

unsigned WINAPI Direct3DEventThreadFunc(PVOID pvoid) {
	CDirect3D *p = (CDirect3D *)pvoid;
	p->Direct3DEventThread();
	TRACE(_T("DirectX Event Thread Ended\n"));
	_endthreadex(0);
	return 0;
}
CDirect3D::CDirect3D(tme_object_t *pIpc, HWND hwnd, video_format_t *p_fmt, int iPtsDelay, bool bResizeParent)
			: CVideoOutput(pIpc, hwnd, p_fmt, iPtsDelay, bResizeParent)
{
	//tme_thread_init(&m_ipc);
    tme_mutex_init( m_pIpc, &m_lock );
    tme_mutex_init( m_pIpc, &m_lockEvent );
	m_hEventThread = NULL;

	get_capability_for_osversion();
	// Initialize and call StartThread virtual function
	m_hd3d9_dll = NULL;
	m_d3dobj = NULL;

    m_bResetDevice = false;
    m_bAllowHwYuv = true;

	m_hwnd = m_hvideownd = m_hfswnd = NULL;
    SetRectEmpty( &m_rectDisplay );
    SetRectEmpty( &m_rectParent );
	m_bIsFirstDisplay = true;
	m_bIsOnTop = false;

	m_bCursorHidden = false;
    m_iLastmoved = mdate();

	m_iWindowX = 0;
	m_iWindowY = 0;
	m_iWindowWidth = CVideoOutput::m_iWindowWidth;
	m_iWindowHeight = CVideoOutput::m_iWindowHeight;

	m_bEventHasMoved = false;
	m_bEventDead = m_bEventDie = false;
    m_iChanges = 0;

	ZeroMemory(&m_resource, sizeof(m_resource));
	m_bHwYuv = true;
#if 0


	m_bHwYuvConf = true;
	m_b3BufferingConf = true;
	m_bUseSysmemConf = m_bWallpaperConf = false;
	m_bOnTopChange = false;

	m_bDisplayInitialized = false;

	m_bUseSysmem = m_b3BufOverlay = false;
	m_iAlignSrcBoundary = m_iAlignSrcSize = m_iAlignDestBoundary = m_iAlignDestSize = 0;

    m_pDDObject = NULL;
    m_pDisplay = NULL;
    m_pCurrentSurface = NULL;
    m_pClipper = NULL;
    m_bWallpaper = false;

    /* Multimonitor stuff */
    m_hmonitor = NULL;
    m_pDisplayDriver = NULL;
    m_MonitorFromWindow = NULL;
    m_GetMonitorInfo = NULL;

	HMODULE huser32;
    if( (huser32 = GetModuleHandle( _T("USER32") ) ) )
    {
        m_MonitorFromWindow = (HMONITOR (WINAPI *)( HWND, DWORD ))
            GetProcAddress( huser32, "MonitorFromWindow" );
        m_GetMonitorInfo = 
            (BOOL (WINAPI*)(HMONITOR, LPMONITORINFO))GetProcAddress( huser32, "GetMonitorInfoA" );
    }



    /* Initialise DirectDraw */
    if( DirectXInitDDraw() )
    {
        _RPT0(_CRT_WARN, "cannot initialize DirectDraw\n" );
        return;
    }

    /* Create the directx display */
    if( DirectXCreateDisplay() )
    {
        _RPT0(_CRT_WARN, "cannot initialize DirectDraw\n" );
        return;
    }
#endif
}

CDirect3D::~CDirect3D()
{
	if (m_hThread) {
		m_bDie = true;
        tme_thread_join(m_hThread);
	}

	CloseVideo();

    tme_mutex_destroy( &m_lockEvent );
    tme_mutex_destroy( &m_lock );
	//tme_mutex_destroy(&(m_ipc.object_lock));
	//tme_cond_destroy(&(m_ipc.object_wait));
}

int CDirect3D::OpenVideo()
{
	TRACE(_T("CDirect3D::OpenVideo()\n"));
    if (Direct3DCreate()) {
        TRACE(_T("Direct3D could not be initialized\n"));
        Direct3DDestroy();
        return EGENERIC;
    }
	TRACE(_T("CDirect3D::OpenVideo()2\n"));

    _sntprintf( m_class_main, sizeof(m_class_main)/sizeof(*m_class_main),
               _T("TME MSW %p"), this );
    _sntprintf( m_class_video, sizeof(m_class_video)/sizeof(*m_class_video),
               _T("TME MSW video %p"), this );

	TRACE(_T("CDirect3D::OpenVideo()3\n"));

	m_hEventThread = tme_thread_create(m_pIpc, "DirectXEventThread",
		Direct3DEventThreadFunc, this, 0, true);
	if (m_hEventThread == NULL) {
		TRACE(_T("Creating DirectXEventThread failed\n"));
		return EGENERIC;
	}
	TRACE(_T("CDirect3D::OpenVideo()4\n"));

	StartThread();

	return SUCCESS;
}

int CDirect3D::Direct3DCreate()
{
    m_hd3d9_dll = LoadLibrary(TEXT("D3D9.DLL"));
    if (!m_hd3d9_dll) {
        TRACE(_T("cannot load d3d9.dll, aborting\n"));
        return EGENERIC;
    }

    LPDIRECT3D9 (WINAPI *OurDirect3DCreate9)(UINT SDKVersion);
    OurDirect3DCreate9 =
        (LPDIRECT3D9 (WINAPI *)(UINT))GetProcAddress(m_hd3d9_dll, "Direct3DCreate9");
    if (!OurDirect3DCreate9) {
        TRACE(_T("Cannot locate reference to Direct3DCreate9 ABI in DLL\n"));
        return EGENERIC;
    }

	m_d3dobj = OurDirect3DCreate9(D3D_SDK_VERSION);
    if (!m_d3dobj) {
       TRACE(_T("Could not create Direct3D9 instance.\n"));
       return EGENERIC;
    }

    /*
    ** Get device capabilities
    */
    D3DCAPS9 d3dCaps;
    ZeroMemory(&d3dCaps, sizeof(d3dCaps));
    HRESULT hr = IDirect3D9_GetDeviceCaps(m_d3dobj, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &d3dCaps);
    if (FAILED(hr)) {
      TRACE(_T("Could not read adapter capabilities. (hr=0x%lX)\n"), hr);
       return EGENERIC;
    }
    /* TODO: need to test device capabilities and select the right render function */

    return SUCCESS;
}

/**
 * It setup d3dpp and rect_display
 * from the default adapter.
 */
int CDirect3D::Direct3DFillPresentationParameters()
{
    /*
    ** Get the current desktop display mode, so we can set up a back
    ** buffer of the same format
    */
    D3DDISPLAYMODE d3ddm;
    HRESULT hr = IDirect3D9_GetAdapterDisplayMode(m_d3dobj,
                                                  D3DADAPTER_DEFAULT, &d3ddm);
    if (FAILED(hr)) {
       TRACE(_T("Could not read adapter display mode. (hr=0x%lX)"), hr);
       return EGENERIC;
    }

	m_bbFormat = d3ddm.Format;

    /* Set up the structure used to create the D3DDevice. */
    D3DPRESENT_PARAMETERS *d3dpp = &m_d3dpp;
    ZeroMemory(d3dpp, sizeof(D3DPRESENT_PARAMETERS));
    d3dpp->Flags                  = D3DPRESENTFLAG_VIDEO;
    d3dpp->Windowed               = TRUE;
    d3dpp->hDeviceWindow          = m_hvideownd;
	d3dpp->BackBufferWidth        = m_output.i_width;//max(GetSystemMetrics(SM_CXVIRTUALSCREEN), d3ddm.Width);
    d3dpp->BackBufferHeight       = m_output.i_height;//max(GetSystemMetrics(SM_CYVIRTUALSCREEN), d3ddm.Height);
    d3dpp->SwapEffect             = /*D3DSWAPEFFECT_DISCARD*/D3DSWAPEFFECT_COPY;
    d3dpp->MultiSampleType        = D3DMULTISAMPLE_NONE;
    d3dpp->PresentationInterval   = D3DPRESENT_INTERVAL_DEFAULT;
    d3dpp->BackBufferFormat       = d3ddm.Format;
    d3dpp->BackBufferCount        = 1;
    d3dpp->EnableAutoDepthStencil = FALSE;

    /*
    RECT *display = &m_rectDisplay;
    display->left   = 0;
    display->top    = 0;
    display->right  = d3dpp->BackBufferWidth;
    display->bottom = d3dpp->BackBufferHeight;
*/
    return SUCCESS;
}

int CDirect3D::Direct3DOpen()
{
    if (Direct3DFillPresentationParameters())
        return EGENERIC;

    // Create the D3DDevice
    LPDIRECT3DDEVICE9 d3ddev;
    HRESULT hr = IDirect3D9_CreateDevice(m_d3dobj, D3DADAPTER_DEFAULT,
                                         D3DDEVTYPE_HAL, m_hvideownd,
                                         D3DCREATE_SOFTWARE_VERTEXPROCESSING/*|
                                         D3DCREATE_MULTITHREADED*/,
                                         &m_d3dpp, &d3ddev);
    if (FAILED(hr)) {
       TRACE(_T("Could not create the D3D device! (hr=0x%lX)\n"), hr);
       return EGENERIC;
    }
    m_d3ddev = d3ddev;
#if 0
    if (Direct3DCreateResources()) {
        TRACE(_T("Failed to allocate resources\n"));
        return EGENERIC;
    }
#endif
    TRACE(_T("Direct3D device adapter successfully initialized\n"));
    return SUCCESS;
}

void CDirect3D::Direct3DDestroy()
{
    if (m_d3dobj)
       IDirect3D9_Release(m_d3dobj);
    if (m_hd3d9_dll)
        FreeLibrary(m_hd3d9_dll);

    m_d3dobj = NULL;
    m_hd3d9_dll = NULL;
}

void CDirect3D::Direct3DClose()
{
    //Direct3DDestroyResources(vd);

    if (m_d3ddev)
       IDirect3DDevice9_Release(m_d3ddev);

    m_d3ddev = NULL;
}

void CDirect3D::ShowVideoWindow(int nCmdShow)
{
	if (m_hwnd != NULL) {
		ShowWindow(m_hwnd, nCmdShow);
	}
}

void CDirect3D::CloseVideo()
{
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
	Direct3DDestroy();
}

int CDirect3D::Init()
{
    if (Direct3DOpen()) {
        TRACE(_T("Direct3D could not be opened"));
		return EGENERIC;
    }

    /* Initialize the output structure.
     * Since Direct3D can do rescaling for us, stick to the default
     * coordinates and aspect. */
    m_output.i_width  = m_render.i_width;
    m_output.i_height = m_render.i_height;
    m_output.i_aspect = m_render.i_aspect;
    m_fmtOut = m_fmtIn;

	DirectXUpdateRects(true);

	/* create pictures pool */
	m_output.i_chroma = 0;
	int i_ret = Direct3DVoutCreatePictures(1);
	if (i_ret != SUCCESS) {
        TRACE(_T("Direct3D picture pool initialization failed !\n"));
        return i_ret;
	}

#ifdef USE_NEW3D
    /* create scene */
    i_ret = Direct3DVoutCreateScene();
    if( SUCCESS != i_ret )
    {
        TRACE(_T("Direct3D scene initialization failed !\n"));
        Direct3DVoutReleasePictures();
        return i_ret;
    }
#endif
	m_fmtOut.i_chroma = m_output.i_chroma;

	return 0;
}

/*****************************************************************************
 * End: terminate Sys video thread output method
 *****************************************************************************
 * Terminate an output method created by Create.
 * It is called at the end of the thread.
 *****************************************************************************/
void CDirect3D::End()
{
#ifdef USE_NEW3D
    Direct3DVoutReleaseScene();
#endif
	Direct3DVoutReleasePictures();
	Direct3DClose();
}

int CDirect3D::Manage() {
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

            SetWindowPos( m_hwnd, 0, 0, 0,
                          rect_parent.right - rect_parent.left,
                          rect_parent.bottom - rect_parent.top, SWP_NOZORDER );
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

    /* Check if the event thread is still running */
    if( m_bEventDie )
    {
        return EGENERIC; /* exit */
    }

    return SUCCESS;
}
#ifdef USE_NEW3D
void CDirect3D::Render(picture_t *p_pic) 
{
    LPDIRECT3DDEVICE9       p_d3ddev  = m_d3ddev;
    LPDIRECT3DTEXTURE9      p_d3dtex;
    LPDIRECT3DVERTEXBUFFER9 p_d3dvtc;
    LPDIRECT3DSURFACE9      p_d3dsrc, p_d3ddest;
    CUSTOMVERTEX            *p_vertices;
    HRESULT hr;
    float f_width, f_height;

    // check if device is still available    
    hr = IDirect3DDevice9_TestCooperativeLevel(p_d3ddev);
    if( FAILED(hr) )
    {
        if( (D3DERR_DEVICENOTRESET != hr)
         || (SUCCESS != Direct3DVoutResetDevice()) )
        {
            // device is not usable at present (lost device, out of video mem ?)
            return;
        }
    }
    p_d3dtex  = m_d3dtex;
    p_d3dvtc  = m_d3dvtc;

    /* Clear the backbuffer and the zbuffer */
    hr = IDirect3DDevice9_Clear( p_d3ddev, 0, NULL, D3DCLEAR_TARGET,
                              D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0 );
    if( FAILED(hr) )
    {
        TRACE(_T("IDirect3DDevice9_Clear (hr=0x%0lX)\n"), hr);
        return;
    }

    /*  retrieve picture surface */
    p_d3dsrc = (LPDIRECT3DSURFACE9)p_pic->p_sys;
    if( NULL == p_d3dsrc )
    {
        TRACE(_T("no surface to render ?\n"));
        return;
    }

    /* retrieve texture top-level surface */
    hr = IDirect3DTexture9_GetSurfaceLevel(p_d3dtex, 0, &p_d3ddest);
    if( FAILED(hr) )
    {
        TRACE(_T("IDirect3DTexture9_GetSurfaceLevel (hr=0x%0lX)\n"), hr);
        return;
    }

    /* Copy picture surface into texture surface, color space conversion happens here */
    hr = IDirect3DDevice9_StretchRect(p_d3ddev, p_d3dsrc, NULL, p_d3ddest, NULL, D3DTEXF_NONE);
    IDirect3DSurface9_Release(p_d3ddest);
    if( FAILED(hr) )
    {
        TRACE(_T("IDirect3DDevice9_StretchRect (hr=0x%0lX)\n"), hr);
        return;
    }

    /* Update the vertex buffer */
    hr = IDirect3DVertexBuffer9_Lock(p_d3dvtc, 0, 0, (VOID **)(&p_vertices), D3DLOCK_DISCARD);
    if( FAILED(hr) )
    {
        TRACE(_T("IDirect3DVertexBuffer9_Lock (hr=0x%0lX)\n"), hr);
        return;
    }

    /* Setup vertices */
    f_width  = (float)(m_output.i_width);
    f_height = (float)(m_output.i_height);

    p_vertices[0].x       = 0.0f;       // left
    p_vertices[0].y       = 0.0f;       // top
    p_vertices[0].z       = 0.0f;
    p_vertices[0].diffuse = D3DCOLOR_ARGB(255, 255, 255, 255);
    p_vertices[0].rhw     = 1.0f;
    p_vertices[0].tu      = 0.0f;
    p_vertices[0].tv      = 0.0f;
 
    p_vertices[1].x       = f_width;    // right
    p_vertices[1].y       = 0.0f;       // top
    p_vertices[1].z       = 0.0f;
    p_vertices[1].diffuse = D3DCOLOR_ARGB(255, 255, 255, 255);
    p_vertices[1].rhw     = 1.0f;
    p_vertices[1].tu      = 1.0f;
    p_vertices[1].tv      = 0.0f;
 
    p_vertices[2].x       = f_width;    // right
    p_vertices[2].y       = f_height;   // bottom
    p_vertices[2].z       = 0.0f;
    p_vertices[2].diffuse = D3DCOLOR_ARGB(255, 255, 255, 255);
    p_vertices[2].rhw     = 1.0f;
    p_vertices[2].tu      = 1.0f;
    p_vertices[2].tv      = 1.0f;
 
    p_vertices[3].x       = 0.0f;       // left
    p_vertices[3].y       = f_height;   // bottom
    p_vertices[3].z       = 0.0f;
    p_vertices[3].diffuse = D3DCOLOR_ARGB(255, 255, 255, 255);
    p_vertices[3].rhw     = 1.0f;
    p_vertices[3].tu      = 0.0f;
    p_vertices[3].tv      = 1.0f;
 
    hr= IDirect3DVertexBuffer9_Unlock(p_d3dvtc);
    if( FAILED(hr) )
    {
        TRACE(_T("IDirect3DVertexBuffer9_Unlock (hr=0x%0lX)\n"), hr);
        return;
    }

    // Begin the scene
    hr = IDirect3DDevice9_BeginScene(p_d3ddev);
    if( FAILED(hr) )
    {
        TRACE(_T("IDirect3DDevice9_BeginScene (hr=0x%0lX)\n"), hr);
        return;
    }

    // Setup our texture. Using textures introduces the texture stage states,
    // which govern how textures get blended together (in the case of multiple
    // textures) and lighting information. In this case, we are modulating
    // (blending) our texture with the diffuse color of the vertices.
    hr = IDirect3DDevice9_SetTexture(p_d3ddev, 0, (LPDIRECT3DBASETEXTURE9)p_d3dtex);
    if( FAILED(hr) )
    {
        TRACE(_T("IDirect3DDevice9_SetTexture (hr=0x%0lX)\n"), hr);
        IDirect3DDevice9_EndScene(p_d3ddev);
        return;
    }

    // Render the vertex buffer contents
    hr = IDirect3DDevice9_SetStreamSource(p_d3ddev, 0, p_d3dvtc, 0, sizeof(CUSTOMVERTEX));
    if( FAILED(hr) )
    {
        TRACE(_T("IDirect3DDevice9_SetStreamSource (hr=0x%0lX)\n"), hr);
        IDirect3DDevice9_EndScene(p_d3ddev);
        return;
    }
 
    // we use FVF instead of vertex shader
    hr = IDirect3DDevice9_SetVertexShader(p_d3ddev, NULL);
    if( FAILED(hr) )
    {
        TRACE(_T("IDirect3DDevice9_SetVertexShader (hr=0x%0lX)\n"), hr);
        IDirect3DDevice9_EndScene(p_d3ddev);
        return;
    }

    hr = IDirect3DDevice9_SetFVF(p_d3ddev, D3DFVF_CUSTOMVERTEX);
    if( FAILED(hr) )
    {
        TRACE(_T("IDirect3DDevice9_SetFVF (hr=0x%0lX)\n"), hr);
        IDirect3DDevice9_EndScene(p_d3ddev);
        return;
    }

    // draw rectangle
    hr = IDirect3DDevice9_DrawPrimitive(p_d3ddev, D3DPT_TRIANGLEFAN, 0, 2);
    if( FAILED(hr) )
    {
        TRACE(_T("IDirect3DDevice9_DrawPrimitive (hr=0x%0lX)\n"), hr);
        IDirect3DDevice9_EndScene(p_d3ddev);
        return;
    }

    // End the scene
    hr = IDirect3DDevice9_EndScene(p_d3ddev);
    if( FAILED(hr) )
    {
        TRACE(_T("IDirect3DDevice9_EndScene (hr=0x%0lX)\n"), hr);
        return;
    }
}
#else
void CDirect3D::Render(picture_t *p_pic) 
{
    LPDIRECT3DDEVICE9       p_d3ddev  = m_d3ddev;
    LPDIRECT3DSURFACE9      p_d3dsrc, p_d3ddest;
	UINT					iSwapChain, iSwapChains;
    HRESULT hr;

    // check if device is still available    
    hr = IDirect3DDevice9_TestCooperativeLevel(p_d3ddev);
    if( FAILED(hr) )
    {
        if( (D3DERR_DEVICENOTRESET != hr)
         || (SUCCESS != Direct3DVoutResetDevice(0, 0)) )
        {
            // device is not usable at present (lost device, out of video mem ?)
            return;
        }
    }

    /* */
	iSwapChains = IDirect3DDevice9_GetNumberOfSwapChains(p_d3ddev);
	if (0 == iSwapChains) {
		TRACE(_T("no swap chains to render\n"));
		return;
	}

    /*  retrieve picture surface */
    p_d3dsrc = (LPDIRECT3DSURFACE9)p_pic->p_sys;
    if( NULL == p_d3dsrc )
    {
        TRACE(_T("no surface to render ?\n"));
        return;
    }

	for (iSwapChain = 0; iSwapChain < iSwapChains; ++iSwapChain) {
		/* retrieve swap chain back buffer */
		hr = IDirect3DDevice9_GetBackBuffer(p_d3ddev, iSwapChain, 0, D3DBACKBUFFER_TYPE_MONO, &p_d3ddest);
		if (FAILED(hr)) {
			TRACE(_T("xxx:(hr=0x%0lX)\n"), hr);
		    continue;
		}
	}

    /* Copy picture surface into texture surface, color space conversion happens here */
    hr = IDirect3DDevice9_StretchRect(p_d3ddev, p_d3dsrc, NULL, p_d3ddest, NULL, D3DTEXF_NONE);
    IDirect3DSurface9_Release(p_d3ddest);
    if( FAILED(hr) )
    {
		TRACE(_T("xxx1:(hr=0x%0lX)\n"), hr);
        return;
    }
}
#endif

void CDirect3D::Display(picture_t *p_pic)
{
#ifdef USE_NEW3D
    // Present the back buffer contents to the display
    // stretching and filtering happens here
    HRESULT hr = IDirect3DDevice9_Present(m_d3ddev, 
		// check
                     &m_rectSrcClipped, 
                     /*&m_rectDestClipped*/NULL, NULL, NULL);
	if( FAILED(hr) ) {
        TRACE(_T("Display (hr=0x%0lX)\n"),hr);
	}

    /* if set, remove the black brush to avoid flickering in repaint operations */
    if( 0UL != GetClassLong( m_hvideownd, GCL_HBRBACKGROUND) )
    {
        SetClassLong( m_hvideownd, GCL_HBRBACKGROUND, 0UL);
        /*
        ** According to virtual dub blog, Vista has problems rendering 3D child windows
        ** created by a different thread than its parent. Try the workaround in
        ** http://www.virtualdub.org/blog/pivot/entry.php?id=149
        */
        if( m_got_vista_or_above )
        {
            SetWindowPos( m_hvideownd, 0, 0, 0, 0, 0,
                  SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);
        }
    }
#else
    HRESULT hr = IDirect3DDevice9_Present(m_d3ddev, NULL, NULL, NULL, NULL);
	if( FAILED(hr) ) {
        TRACE(_T("Display (hr=0x%0lX)\n"),hr);
	}
#endif
}

int CDirect3D::Lock(picture_t *p)
{
	return Direct3DLockSurface(p);
}

int CDirect3D::Unlock(picture_t *p)
{
	return Direct3DUnlockSurface(p);
}

/*****************************************************************************
 * DirectXUpdateRects: update clipping rectangles
 *****************************************************************************
 * This function is called when the window position or size are changed, and
 * its job is to update the source and destination RECTs used to display the
 * picture.
 *****************************************************************************/
void CDirect3D::DirectXUpdateRects( bool b_force )
{
    unsigned int i_width, i_height, i_x, i_y;

    RECT  rect;
    POINT point;

	/* Retrieve the window size */
    GetClientRect( m_hwnd, &rect );

		//TRACE(_T("DirectXUpdateRects:%dx%d\n"), rect.right - rect.left, rect.bottom - rect.top );
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
        SetWindowPos( m_hvideownd, 0/*HWND_TOP*/,
                      i_x, i_y, i_width, i_height, /*0*/
					  SWP_NOCOPYBITS|SWP_NOZORDER|SWP_ASYNCWINDOWPOS);

	/* Destination image position and dimensions */
    m_rectDest.left = point.x + i_x;
    m_rectDest.right = m_rectDest.left + i_width;
    m_rectDest.top = point.y + i_y;
    m_rectDest.bottom = m_rectDest.top + i_height;

	m_rectDestClipped = m_rectDest;

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
#if 0
    /* The destination coordinates need to be relative to the current
     * directdraw primary surface (display) */
    m_rectDestClipped.left -= m_rectDisplay.left;
    m_rectDestClipped.right -= m_rectDisplay.left;
    m_rectDestClipped.top -= m_rectDisplay.top;
    m_rectDestClipped.bottom -= m_rectDisplay.top;
#endif
    /* Signal the change in size/position */
    m_iChanges |= DX_POSITION_CHANGE;
}
/*****************************************************************************
 * Direct3DLockSurface: Lock surface and get picture data pointer
 *****************************************************************************
 * This function locks a surface and get the surface descriptor which amongst
 * other things has the pointer to the picture data.
 *****************************************************************************/
int CDirect3D::Direct3DLockSurface( picture_t *p_pic )
{
    HRESULT hr;
    D3DLOCKED_RECT d3drect;
	LPDIRECT3DSURFACE9 p_d3dsurf = (LPDIRECT3DSURFACE9)p_pic->p_sys;

    if( NULL == p_d3dsurf )
        return EGENERIC;

    /* Lock the surface to get a valid pointer to the picture buffer */
	// check D3DLOCK_DISCARD;
    hr = IDirect3DSurface9_LockRect(p_d3dsurf, &d3drect, NULL, 0/*D3DLOCK_DISCARD*/);
    if( FAILED(hr) )
    {
        TRACE(_T("Direct3DLockSurface (hr=0x%0lX)"), hr);
        return EGENERIC;
    }

    /* fill in buffer info in first plane */
    p_pic->p->p_pixels = (UINT8 *)d3drect.pBits;
    p_pic->p->i_pitch  = d3drect.Pitch;

    return SUCCESS;
}

/*****************************************************************************
 * Direct3DUnlockSurface: Unlock a surface locked by Direct3DLockSurface().
 *****************************************************************************/
int CDirect3D::Direct3DUnlockSurface( picture_t *p_pic )
{
    HRESULT hr;
	LPDIRECT3DSURFACE9 p_d3dsurf = (LPDIRECT3DSURFACE9)p_pic->p_sys;

    if( NULL == p_d3dsurf )
        return EGENERIC;

    /* Unlock the Surface */
    hr = IDirect3DSurface9_UnlockRect(p_d3dsurf);
    if( FAILED(hr) )
    {
        TRACE(_T("Direct3DUnlockSurface (hr=0x%0lX)"), hr);
        return EGENERIC;
    }
    return SUCCESS;
}

int CDirect3D::Direct3DEventThread()
{
    MSG msg;
	HMODULE hkernel32;

  TRACE(_T("------------------------------1\n"));
	/* Create a window for the video */
    /* Creating a window under Windows also initializes the thread's event
     * message queue */
    if( Direct3DCreateWindow() )
    {
        TRACE(_T("out of memory or wrong window handle\n"));
        m_bEventDead = true;
    }

  TRACE(_T("------------------------------20\n"));
	tme_thread_ready(m_pIpc);

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
            TRACE(_T("no support for SetThreadExecutionState()\n") );
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

    Direct3DCloseWindow();

	return 0;
}

struct win_data {
	HWND hWnd;
	int nIndex;
	LONG dwNewLong;
};

static DWORD WINAPI SetWindowLongPtrTimeoutThread(LPVOID lpParam)
{
	win_data *data = (win_data *)lpParam;
	HWND hWnd = data->hWnd;
	int nIndex = data->nIndex;
	LONG dwNewLong = data->dwNewLong;
	delete data;

	SetWindowLongPtr(hWnd, nIndex, dwNewLong);

	return 0;
}

bool SetWindowLongPtrTimeout(HWND hWnd, int nIndex, LONG dwNewLong, int timeout)
{
	win_data *data = new win_data;
	data->hWnd = hWnd;
	data->nIndex = nIndex;
	data->dwNewLong = dwNewLong;

	HANDLE threadHandle;
	threadHandle = CreateThread(0, 0, SetWindowLongPtrTimeoutThread, data, 0, NULL);
	if (WaitForSingleObject(threadHandle, timeout) == WAIT_TIMEOUT) {
		CloseHandle(threadHandle);
		TRACE(_T("====================a\n"));
		return false;
	}
	CloseHandle(threadHandle);
	return true;
}


/*****************************************************************************
 * Direct3DCreateWindow: create a window for the video.
 *****************************************************************************
 * Before creating a direct draw surface, we need to create a window in which
 * the video will be displayed. This window will also allow us to capture the
 * events.
 *****************************************************************************/
int CDirect3D::Direct3DCreateWindow()
{
    HINSTANCE  hInstance;
    RECT       rect_window;
    WNDCLASS   wc;                            /* window class components */
    HICON      tme_icon = NULL;
    TCHAR       tme_path[MAX_PATH+1];
    int        i_style, i_stylex;

    /* Get this module's instance */
    hInstance = GetModuleHandle(NULL);

	if (m_hparent) {
		// specify client area coordinate here
		m_iWindowX = 0;
		m_iWindowY = 0;
	}

    /* Get the Icon from the main app */
    if( GetModuleFileName( NULL, tme_path, MAX_PATH ) )
    {
		tme_icon = ::ExtractIcon( hInstance, tme_path, 0 );
    }

    /* Fill in the window class structure */
    wc.style         = CS_OWNDC | CS_DBLCLKS;          /* style: dbl click */
    wc.lpfnWndProc   = (WNDPROC)Direct3DEventProc;       /* event handler */
    wc.cbClsExtra    = 0;                         /* no extra class data */
    wc.cbWndExtra    = 0;                        /* no extra window data */
    wc.hInstance     = hInstance;                            /* instance */
    wc.hIcon         = tme_icon;                /* load the vlc big icon */
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);    /* default cursor */
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);  /* background color */
    wc.lpszMenuName  = NULL;                                  /* no menu */
    wc.lpszClassName = m_class_main;         /* use a special class */

    /* Register the window class */
    if( !RegisterClass(&wc) )
    {
        WNDCLASS wndclass;

        if( tme_icon ) DestroyIcon( tme_icon );

        /* Check why it failed. If it's because one already exists
         * then fine, otherwise return with an error. */
        if( !GetClassInfo( hInstance, m_class_main, &wndclass ) ) {
            TRACE(_T("Direct3DCreateWindow RegisterClass FAILED (err=%lu)\n"), GetLastError() );
            return EGENERIC;
        }
    }

    /* Register the video sub-window class */
    wc.lpszClassName = m_class_video;
	wc.hIcon = 0;
	//wc.hbrBackground = NULL;
    if( !RegisterClass(&wc) )
    {
        WNDCLASS wndclass;

        /* Check why it failed. If it's because one already exists
         * then fine, otherwise return with an error. */
        if( !GetClassInfo( hInstance, m_class_video, &wndclass ) ) {
            TRACE(_T("Direct3DCreateWindow RegisterClass FAILED (err=%lu)\n"), GetLastError() );
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
        i_style = /*WS_VISIBLE|*/WS_CLIPCHILDREN|WS_CHILD;
        i_stylex = 0;
    }

    m_iWindowStyle = i_style;

    //TRACE(_T("Creating Main window\n"));
    /* Create the window */
    m_hwnd =
        CreateWindowEx( WS_EX_NOPARENTNOTIFY | i_stylex,
                    m_class_main,               /* name of window class */
                    _T("(Direct3D Output)"),  /* window title */
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
        TRACE(_T("Direct3DCreateWindow create window FAILED (err=%lu)\n"), GetLastError() );
        return EGENERIC;
    }

    if( m_hparent )
    {
        LONG i_style;
		HWND hParentOwner = GetParent(m_hparent);

		if (hParentOwner != NULL) {
	        i_style = GetWindowLong( hParentOwner, GWL_STYLE );

			if( !(i_style & WS_CLIPCHILDREN) ) {
				SetWindowLong( hParentOwner, GWL_STYLE, i_style | WS_CLIPCHILDREN );
			}
		}

       /* We don't want the window owner to overwrite our client area */
        i_style = GetWindowLong( m_hparent, GWL_STYLE );

				if( !(i_style & WS_CLIPCHILDREN) ) {
            /* Hmmm, apparently this is a blocking call... */
            //SetWindowLong( m_hparent, GWL_STYLE, i_style | WS_CLIPCHILDREN );
					SetWindowLongPtrTimeout(m_hparent, GWL_STYLE, i_style | WS_CLIPCHILDREN, 200);
				}
	}

    /* Create video sub-window. This sub window will always exactly match
     * the size of the video, which allows us to use crazy overlay colorkeys
     * without having them shown outside of the video area. */
    m_hvideownd =
    CreateWindow( m_class_video, _T(""),   /* window class */
	// check
        WS_CHILD | WS_VISIBLE,                   /* window style */
        0, 0,
        m_render.i_width,         /* default width */
        m_render.i_height,        /* default height */
        m_hwnd,                    /* parent window */
        NULL, hInstance,
        (LPVOID)this );            /* send p_vout to WM_CREATE */

    if( !m_hvideownd )
		TRACE(_T("can't create video sub-window\n") );
    else
    	TRACE(_T("created video sub-window\n") );

    /* Now display the window */
    //ShowWindow( m_hwnd, SW_SHOW );

    return SUCCESS;
}

/*****************************************************************************
 * DirectXCloseWindow: close the window created by DirectXCreateWindow
 *****************************************************************************
 * This function returns all resources allocated by DirectXCreateWindow.
 *****************************************************************************/
void CDirect3D::Direct3DCloseWindow()
{
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
static long FAR PASCAL Direct3DEventProc( HWND hwnd, UINT message,
                                         WPARAM wParam, LPARAM lParam )
{
	CDirect3D *p_vout;

    if( message == WM_CREATE )
    {
        /* Store p_vout for future use */
        p_vout = (CDirect3D *)((CREATESTRUCT *)lParam)->lpCreateParams;
        SetWindowLongPtr( hwnd, GWLP_USERDATA, (LONG_PTR)p_vout );
        return TRUE;
    }
    else
    {
        p_vout = (CDirect3D *)GetWindowLongPtr( hwnd, GWLP_USERDATA );
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
#if 0
		switch (message) 
		{
			case WM_ERASEBKGND:
				return 1;
			case WM_PAINT:
				ValidateRect(hwnd, NULL);
			default:
				return (long)DefWindowProc(hwnd, message, wParam, lParam);
		}
#endif
	}

    switch( message )
    {
	case WM_NULL: /* Relay message received from CloseWindow to this thread's message queue */
		if (InSendMessage()) 
			ReplyMessage(TRUE); 
		return PostMessage(hwnd, WM_NULL, 0,0);

    case WM_WINDOWPOSCHANGED:
				TRACE(_T("WM_WINDOWPOSCHANGED\n"));
        p_vout->DirectXUpdateRects( true );
#if 0
		tme_mutex_lock( &p_vout->m_lockEvent );
        p_vout->m_bEventHasMoved = true;
		tme_mutex_unlock( &p_vout->m_lockEvent );
#endif
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
 * This function is called when the window position or size are changed, and
 * its job is to update the source and destination RECTs used to display the
 * picture.
 *****************************************************************************/
void CDirect3D::UpdateRects( bool is_forced )
{
    unsigned int i_width, i_height, i_x, i_y;

    RECT  rect;
    POINT point;

	/* Retrieve the window size */
    GetClientRect( m_hwnd, &rect );

    /* Retrieve the window position */
    point.x = point.y = 0;
    ClientToScreen( m_hwnd, &point );

    /* If nothing changed, we can return */
	bool has_moved;
	bool is_resized;

    EventThreadUpdateWindowPosition(&has_moved, &is_resized,
                                    point.x, point.y,
                                    rect.right, rect.bottom);

	//if (is_resized) {
        //vout_display_SendEventDisplaySize(vd, rect.right, rect.bottom, cfg->is_fullscreen);
    if (!is_forced && !has_moved && !is_resized )
        return;

	vout_PlacePicture( this, rect.right, rect.bottom,
                       &i_x, &i_y, &i_width, &i_height );

	if( m_hvideownd ) {
        SetWindowPos( m_hvideownd, HWND_TOP, i_x, i_y, i_width, i_height, 0 );
		// check
		/*
        SetWindowPos(m_hvideownd, 0,
                     i_x, i_y, i_width, i_height,
                     SWP_NOCOPYBITS|SWP_NOZORDER|SWP_ASYNCWINDOWPOS);*/
	}

	/* Destination image position and dimensions */
    m_rectDest.left = 0;
    m_rectDest.right = i_width;
    m_rectDest.top = 0;
    m_rectDest.bottom = i_height;

    /* UpdateOverlay directdraw function doesn't automatically clip to the
     * display size so we need to do it otherwise it will fail
     * It is also needed for d3d to avoid exceding our surface size */

    /* Clip the destination window */
    if( !IntersectRect( &m_rectDestClipped, &m_rectDest,
                        &m_rectDisplay ) )
    {
        SetRectEmpty( &m_rectSrcClipped );
        goto exit;
    }

#if 0
     TRACE(_T("DirectXUpdateRects image_dst_clipped coords: %i,%i,%i,%i\n"),
                     m_rectDestClipped.left, m_rectDestClipped.top,
                     m_rectDestClipped.right, m_rectDestClipped.bottom );
#endif

    /* the 2 following lines are to fix a bug when clicking on the desktop */
    if( (m_rectDestClipped.right - m_rectDestClipped.left)==0 ||
        (m_rectDestClipped.bottom - m_rectDestClipped.top)==0 )
    {
        SetRectEmpty( &m_rectSrcClipped );
        goto exit;
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

    /* Needed at least with YUV content */
    m_rectSrcClipped.left &= ~1;
    m_rectSrcClipped.right &= ~1;
    m_rectSrcClipped.top &= ~1;
    m_rectSrcClipped.bottom &= ~1;

#if 0
    _RPT4(_CRT_WARN, "DirectXUpdateRects image_src_clipped coords: %i,%i,%i,%i\n",
                     m_rectSrcClipped.left, m_rectSrcClipped.top,
                     m_rectSrcClipped.right, m_rectSrcClipped.bottom );
#endif

exit:
    /* Signal the change in size/position */
    m_iChanges |= DX_POSITION_CHANGE;
}

void CDirect3D::EventThreadUpdateWindowPosition(bool *pb_moved, bool *pb_resized,
                                      int x, int y, int w, int h)
{
    tme_mutex_lock( &m_lockEvent );
    *pb_moved   = x != m_iWindowX ||
                  y != m_iWindowY;
    *pb_resized = w != m_iWindowWidth ||
                  h != m_iWindowHeight;

    m_iWindowX      = x;
    m_iWindowY      = y;
    m_iWindowWidth  = w;
    m_iWindowHeight = h;
    tme_mutex_unlock( &m_lockEvent );
}

/**
 * It returns the format (closest to chroma) that can be converted to target */
const d3d_format_t *CDirect3D::Direct3DFindFormat(fourcc_t chroma, D3DFORMAT target)
{
    for (unsigned pass = 0; pass < 2; pass++) {
        const fourcc_t *list;

        if (pass == 0 && m_bAllowHwYuv && fourcc_IsYUV(chroma))
            list = fourcc_GetYUVFallback(chroma);
        else if (pass == 1)
            list = fourcc_GetRGBFallback(chroma);
        else
            continue;

        for (unsigned i = 0; list[i] != 0; i++) {
            for (unsigned j = 0; d3d_formats[j].name; j++) {
                const d3d_format_t *format = &d3d_formats[j];

                if (format->fourcc != list[i])
                    continue;

                TRACE(_T("trying surface pixel format: %s"),
                         format->name);
                if (!Direct3DCheckConversion(format->format, target)) {
                    TRACE(_T("selected surface pixel format is %s"),
                            format->name);
                    return format;
                }
            }
        }
    }

    return NULL;
}

int CDirect3D::Direct3DCheckConversion(D3DFORMAT src, D3DFORMAT dst)
{
    HRESULT hr;

    /* test whether device can create a surface of that format */
    hr = IDirect3D9_CheckDeviceFormat(m_d3dobj, D3DADAPTER_DEFAULT,
                                      D3DDEVTYPE_HAL, dst, 0,
                                      D3DRTYPE_SURFACE, src);
    if (SUCCEEDED(hr)) {
        /* test whether device can perform color-conversion
        ** from that format to target format
        */
        hr = IDirect3D9_CheckDeviceFormatConversion(m_d3dobj,
                                                    D3DADAPTER_DEFAULT,
                                                    D3DDEVTYPE_HAL,
                                                    src, dst);
    }
    if (!SUCCEEDED(hr)) {
        if (D3DERR_NOTAVAILABLE != hr)
            TRACE(_T("Could not query adapter supported formats. (hr=0x%lX)\n"), hr);
        return EGENERIC;
    }
    return SUCCESS;
}

/**
 * It creates the pool of picture (only 1).
 *
 * Each picture has an associated offscreen surface in video memory
 * depending on hardware capabilities the picture chroma will be as close
 * as possible to the orginal render chroma to reduce CPU conversion overhead
 * and delegate this work to video card GPU
 */
int CDirect3D::Direct3DCreatePool()
{
    LPDIRECT3DDEVICE9 d3ddev = m_d3ddev;

     // if vout is already running, use current chroma, otherwise choose from upstream
    int i_chroma = m_output.i_chroma ? m_output.i_chroma : m_render.i_chroma;

   //*fmt = m_fmtRender;

    /* Find the appropriate D3DFORMAT for the render chroma, the format will be the closest to
     * the requested chroma which is usable by the hardware in an offscreen surface, as they
     * typically support more formats than textures */
    const d3d_format_t *d3dfmt = Direct3DFindFormat(/*fmt->*/i_chroma, m_d3dpp.BackBufferFormat);
    if (!d3dfmt) {
        TRACE(_T("surface pixel format is not supported.\n"));
        return EGENERIC;
    }
    m_output.i_chroma = d3dfmt->fourcc;
    m_output.i_rmask  = d3dfmt->rmask;
    m_output.i_gmask  = d3dfmt->gmask;
    m_output.i_bmask  = d3dfmt->bmask;

    /* We create one picture.
     * It is useless to create more as we can't be used for direct rendering */

    /* Create a surface */
    LPDIRECT3DSURFACE9 surface;
    HRESULT hr = IDirect3DDevice9_CreateOffscreenPlainSurface(d3ddev,
                                                              /*fmt->*/m_render.i_width,
                                                              /*fmt->*/m_render.i_height,
                                                              d3dfmt->format,
                                                              D3DPOOL_DEFAULT,
                                                              &surface,
                                                              NULL);
    if (FAILED(hr)) {
        TRACE(_T("Failed to create picture surface. (hr=0x%lx)\n"), hr);
        return EGENERIC;
    }
    /* fill surface with black color */
    IDirect3DDevice9_ColorFill(d3ddev, surface, NULL, D3DCOLOR_ARGB(0xFF, 0, 0, 0));

    /* Create the associated picture */
    picture_resource_t *rsc = &m_resource;
    rsc->p_sys = (picture_sys_t *)malloc(sizeof(*rsc->p_sys));
	// check: free this
    if (!rsc->p_sys) {
        IDirect3DSurface9_Release(surface);
        return ENOMEM;
    }
    rsc->p_sys->surface = surface;
    rsc->p_sys->fallback = NULL;
    for (int i = 0; i < PICTURE_PLANE_MAX; i++) {
        rsc->p[i].p_pixels = NULL;
        rsc->p[i].i_pitch = 0;
        rsc->p[i].i_lines = /*fmt->*/m_render.i_height / (i > 0 ? 2 : 1);
    }

	picture_t *p_picture = &m_pPicture[0];
	if (picture_Setup(p_picture, m_output.i_chroma, m_render.i_width, m_render.i_height,
		m_fmtRender.i_sar_num, m_fmtRender.i_sar_den)) {
			IDirect3DSurface9_Release(surface);
			free(rsc->p_sys);
			rsc->p_sys = NULL;
			return EGENERIC;
	}
	p_picture->p_sys = rsc->p_sys;
	 
	for( int i = 0; i < p_picture->i_planes; i++ ) {
        p_picture->p[i].p_pixels = rsc->p[i].p_pixels;
        p_picture->p[i].i_lines  = rsc->p[i].i_lines;
        p_picture->p[i].i_pitch  = rsc->p[i].i_pitch;
    }

	p_picture->i_status = DESTROYED_PICTURE;
	p_picture->i_type = DIRECT_PICTURE;
	p_picture->b_slow = true;
	PP_OUTPUTPICTURE[0] = p_picture;
	I_OUTPUTPICTURES = 1;
#if 0
    picture_t *picture = picture_NewFromResource(fmt, rsc);
    if (!picture) {
        IDirect3DSurface9_Release(surface);
        free(rsc->p_sys);
        return VLC_ENOMEM;
    }

    /* Wrap it into a picture pool */
    picture_pool_configuration_t pool_cfg;
    memset(&pool_cfg, 0, sizeof(pool_cfg));
    pool_cfg.picture_count = 1;
    pool_cfg.picture       = &picture;
    pool_cfg.lock          = Direct3DLockSurface;
    pool_cfg.unlock        = Direct3DUnlockSurface;

    sys->pool = picture_pool_NewExtended(&pool_cfg);
    if (!sys->pool) {
        picture_Release(picture);
        IDirect3DSurface9_Release(surface);
        return VLC_ENOMEM;
    }
#endif
    return SUCCESS;
}

/**
 * It creates the picture and scene resources.
 */
int CDirect3D::Direct3DCreateResources()
{
    if (Direct3DCreatePool()) {
        TRACE(_T("Direct3D picture pool initialization failed"));
        return EGENERIC;
    }/*
    if (Direct3DCreateScene(vd)) {
        TRACE(_T("Direct3D scene initialization failed !"));
        return EGENERIC;
    }*/
    return SUCCESS;
}

D3DFORMAT CDirect3D::Direct3DVoutSelectFormat( D3DFORMAT target,
    const D3DFORMAT *formats, size_t count)
{
    LPDIRECT3D9 p_d3dobj = m_d3dobj;
    size_t c;

    for( c=0; c<count; ++c )
    {
        HRESULT hr;
        D3DFORMAT format = formats[c];
        /* test whether device can create a surface of that format */
        hr = IDirect3D9_CheckDeviceFormat(p_d3dobj, D3DADAPTER_DEFAULT,
                D3DDEVTYPE_HAL, target, 0, D3DRTYPE_SURFACE, format);
        if( SUCCEEDED(hr) )
        {
            /* test whether device can perform color-conversion
            ** from that format to target format
            */
            hr = IDirect3D9_CheckDeviceFormatConversion(p_d3dobj,
                    D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
                    format, target);
        }
        if( SUCCEEDED(hr) )
        {
            // found a compatible format
            switch( format )
            {
                case D3DFMT_UYVY:
                    TRACE(_T("selected surface pixel format is UYVY\n"));
                    break;
                case D3DFMT_YUY2:
                    TRACE(_T("selected surface pixel format is YUY2\n"));
                    break;
                case D3DFMT_X8R8G8B8:
                    TRACE(_T("selected surface pixel format is X8R8G8B8\n"));
                    break;
                case D3DFMT_A8R8G8B8:
                    TRACE(_T("selected surface pixel format is A8R8G8B8\n"));
                    break;
                case D3DFMT_R8G8B8:
                    TRACE(_T("selected surface pixel format is R8G8B8\n"));
                    break;
                case D3DFMT_R5G6B5:
                    TRACE(_T("selected surface pixel format is R5G6B5\n"));
                    break;
                case D3DFMT_X1R5G5B5:
                    TRACE(_T("selected surface pixel format is X1R5G5B5\n"));
                    break;
                default:
                    TRACE(_T("selected surface pixel format is 0x%0X\n"), format);
                    break;
            }
            return format;
        }
        else if( D3DERR_NOTAVAILABLE != hr )
        {
            TRACE(_T("Could not query adapter supported formats. (hr=0x%lX)"), hr);
            break;
        }
    }
    return D3DFMT_UNKNOWN;
}

D3DFORMAT CDirect3D::Direct3DVoutFindFormat(int i_chroma, D3DFORMAT target)
{
    if( m_bHwYuv )
    {
        switch( i_chroma )
        {
            case TME_FOURCC('U','Y','V','Y'):
            case TME_FOURCC('U','Y','N','V'):
            case TME_FOURCC('Y','4','2','2'):
            {
                static const D3DFORMAT formats[] =
                    { D3DFMT_UYVY, D3DFMT_YUY2, D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8, D3DFMT_R5G6B5, D3DFMT_X1R5G5B5 };
                return Direct3DVoutSelectFormat(target, formats, sizeof(formats)/sizeof(D3DFORMAT));
            }
            case TME_FOURCC('I','4','2','0'):
            case TME_FOURCC('I','4','2','2'):
            case TME_FOURCC('Y','V','1','2'):
            {
                /* typically 3D textures don't support planar format
                ** fallback to packed version and use CPU for the conversion
                */
                static const D3DFORMAT formats[] = 
                    { D3DFMT_YUY2, D3DFMT_UYVY, D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8, D3DFMT_R5G6B5, D3DFMT_X1R5G5B5 };
                return Direct3DVoutSelectFormat(target, formats, sizeof(formats)/sizeof(D3DFORMAT));
            }
            case TME_FOURCC('Y','U','Y','2'):
            case TME_FOURCC('Y','U','N','V'):
            {
                static const D3DFORMAT formats[] = 
                    { D3DFMT_YUY2, D3DFMT_UYVY, D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8, D3DFMT_R5G6B5, D3DFMT_X1R5G5B5 };
                return Direct3DVoutSelectFormat(target, formats, sizeof(formats)/sizeof(D3DFORMAT));
            }
        }
    }

    switch( i_chroma )
    {
        case TME_FOURCC('R', 'V', '1', '5'):
        {
            static const D3DFORMAT formats[] = 
                { D3DFMT_X1R5G5B5 };
            return Direct3DVoutSelectFormat(target, formats, sizeof(formats)/sizeof(D3DFORMAT));
        }
        case TME_FOURCC('R', 'V', '1', '6'):
        {
            static const D3DFORMAT formats[] = 
                { D3DFMT_R5G6B5 };
            return Direct3DVoutSelectFormat(target, formats, sizeof(formats)/sizeof(D3DFORMAT));
        }
        case TME_FOURCC('R', 'V', '2', '4'):
        {
            static const D3DFORMAT formats[] = 
                { D3DFMT_R8G8B8, D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8 };
            return Direct3DVoutSelectFormat(target, formats, sizeof(formats)/sizeof(D3DFORMAT));
        }
        case TME_FOURCC('R', 'V', '3', '2'):
        {
            static const D3DFORMAT formats[] = 
                { D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8 };
            return Direct3DVoutSelectFormat(target, formats, sizeof(formats)/sizeof(D3DFORMAT));
        }
        default:
        {
            /* use display default format */
            LPDIRECT3D9 p_d3dobj = m_d3dobj;
            D3DDISPLAYMODE d3ddm;

            HRESULT hr = IDirect3D9_GetAdapterDisplayMode(p_d3dobj, D3DADAPTER_DEFAULT, &d3ddm );
            if( SUCCEEDED(hr))
            {
                /*
                ** some professional cards could use some advanced pixel format as default, 
                ** make sure we stick with chromas that we can handle internally
                */
                switch( d3ddm.Format )
                {
                    case D3DFMT_X8R8G8B8:
                    case D3DFMT_A8R8G8B8:
                    case D3DFMT_R5G6B5:
                    case D3DFMT_X1R5G5B5:
                        TRACE(_T("defaulting to adapter pixel format\n"));
                        return Direct3DVoutSelectFormat(target, &d3ddm.Format, 1);
                    default:
                    {
                        /* if we fall here, that probably means that we need to render some YUV format */
                        static const D3DFORMAT formats[] = 
                            { D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8, D3DFMT_R5G6B5, D3DFMT_X1R5G5B5 };
                        TRACE(_T("defaulting to built-in pixel format"));
                        return Direct3DVoutSelectFormat(target, formats, sizeof(formats)/sizeof(D3DFORMAT));
                    }
                }
            }
        }
    }
    return D3DFMT_UNKNOWN;
}

/*****************************************************************************
 * Direct3DVoutCreatePictures: allocate a vector of identical pictures
 *****************************************************************************
 * Each picture has an associated offscreen surface in video memory
 * depending on hardware capabilities the picture chroma will be as close
 * as possible to the orginal render chroma to reduce CPU conversion overhead
 * and delegate this work to video card GPU
 *****************************************************************************/
int CDirect3D::Direct3DVoutCreatePictures( size_t i_num_pics )
{
    LPDIRECT3DDEVICE9       p_d3ddev  = m_d3ddev;
	D3DFORMAT               format;
    HRESULT hr;
    size_t c;
    // if vout is already running, use current chroma, otherwise choose from upstream
    int i_chroma = m_output.i_chroma ? m_output.i_chroma : m_render.i_chroma;

    I_OUTPUTPICTURES = 0;

    /*
    ** find the appropriate D3DFORMAT for the render chroma, the format will be the closest to
    ** the requested chroma which is usable by the hardware in an offscreen surface, as they
    ** typically support more formats than textures
    */ 
    format = Direct3DVoutFindFormat(i_chroma, m_bbFormat);
    if( SUCCESS != Direct3DVoutSetOutputFormat(format) )
    {
        TRACE(_T("surface pixel format is not supported.\n"));
        return EGENERIC;
    }

    for( c=0; c<i_num_pics; )
    {

        LPDIRECT3DSURFACE9 p_d3dsurf;
        picture_t *p_pic = m_pPicture+c; 

        hr = IDirect3DDevice9_CreateOffscreenPlainSurface(p_d3ddev, 
                m_render.i_width,
                m_render.i_height,
                format,
                D3DPOOL_DEFAULT,
                &p_d3dsurf,
                NULL);
        if( FAILED(hr) )
        {
            TRACE(_T("Failed to create picture surface. (hr=0x%lx)"), hr);
            Direct3DVoutReleasePictures();
            return EGENERIC;
        }

        /* fill surface with black color */
        IDirect3DDevice9_ColorFill(p_d3ddev, p_d3dsurf, NULL, D3DCOLOR_ARGB(0xFF, 0, 0, 0) );
        
        /* assign surface to internal structure */
        p_pic->p_sys = (picture_sys_t *)p_d3dsurf;

        /* Now that we've got our direct-buffer, we can finish filling in the
         * picture_t structures */
        switch( m_output.i_chroma )
        {
            case TME_FOURCC('R','G','B','2'):
                p_pic->p->i_lines = m_output.i_height;
                p_pic->p->i_visible_lines = m_output.i_height;
                p_pic->p->i_pixel_pitch = 1;
                p_pic->p->i_visible_pitch = m_output.i_width *
                    p_pic->p->i_pixel_pitch;
                p_pic->i_planes = 1;
            break;
            case TME_FOURCC('R','V','1','5'):
            case TME_FOURCC('R','V','1','6'):
                p_pic->p->i_lines = m_output.i_height;
                p_pic->p->i_visible_lines = m_output.i_height;
                p_pic->p->i_pixel_pitch = 2;
                p_pic->p->i_visible_pitch = m_output.i_width *
                    p_pic->p->i_pixel_pitch;
                p_pic->i_planes = 1;
            break;
            case TME_FOURCC('R','V','2','4'):
                p_pic->p->i_lines = m_output.i_height;
                p_pic->p->i_visible_lines = m_output.i_height;
                p_pic->p->i_pixel_pitch = 3;
                p_pic->p->i_visible_pitch = m_output.i_width *
                    p_pic->p->i_pixel_pitch;
                p_pic->i_planes = 1;
            break;
            case TME_FOURCC('R','V','3','2'):
                p_pic->p->i_lines = m_output.i_height;
                p_pic->p->i_visible_lines = m_output.i_height;
                p_pic->p->i_pixel_pitch = 4;
                p_pic->p->i_visible_pitch = m_output.i_width *
                    p_pic->p->i_pixel_pitch;
                p_pic->i_planes = 1;
                break;
            case TME_FOURCC('U','Y','V','Y'):
            case TME_FOURCC('Y','U','Y','2'):
                p_pic->p->i_lines = m_output.i_height;
                p_pic->p->i_visible_lines = m_output.i_height;
                p_pic->p->i_pixel_pitch = 2;
                p_pic->p->i_visible_pitch = m_output.i_width *
                    p_pic->p->i_pixel_pitch;
                p_pic->i_planes = 1;
                break;
            default:
                Direct3DVoutReleasePictures();
                return EGENERIC;
        }
        p_pic->i_status = DESTROYED_PICTURE;
        p_pic->i_type   = DIRECT_PICTURE;
        p_pic->b_slow   = true;
        //p_pic->pf_lock  = Direct3DVoutLockSurface;
        //p_pic->pf_unlock = Direct3DVoutUnlockSurface;
        PP_OUTPUTPICTURE[c] = p_pic;

        I_OUTPUTPICTURES = ++c;
    }

    TRACE(_T("%u Direct3D pictures created successfully\n"), c );

    return SUCCESS;
}

int CDirect3D::Direct3DVoutSetOutputFormat(D3DFORMAT format)
{
    switch( format )
    {
        case D3DFMT_YUY2:
            m_output.i_chroma = TME_FOURCC('Y', 'U', 'Y', '2');
            break;
        case D3DFMT_UYVY:
            m_output.i_chroma = TME_FOURCC('U', 'Y', 'V', 'Y');
            break;
        case D3DFMT_R8G8B8:
            m_output.i_chroma = TME_FOURCC('R', 'V', '2', '4');
            break;
        case D3DFMT_X8R8G8B8:
        case D3DFMT_A8R8G8B8:
			// check
            m_output.i_chroma = TME_FOURCC('R', 'V', '3', '2');
            m_output.i_rmask = 0x00ff0000;
            m_output.i_gmask = 0x0000ff00;
            m_output.i_bmask = 0x000000ff;
            break;
        case D3DFMT_R5G6B5:
 			// check
           m_output.i_chroma = TME_FOURCC('R', 'V', '1', '6');
            m_output.i_rmask = (0x1fL)<<11;
            m_output.i_gmask = (0x3fL)<<5;
            m_output.i_bmask = (0x1fL)<<0;
            break;
        case D3DFMT_X1R5G5B5:
			// check
            m_output.i_chroma = TME_FOURCC('R', 'V', '1', '5');
            m_output.i_rmask = (0x1fL)<<10;
            m_output.i_gmask = (0x1fL)<<5;
            m_output.i_bmask = (0x1fL)<<0;
            break;
        default:
            return EGENERIC;
    }
    return SUCCESS;
}

/*****************************************************************************
 * Direct3DVoutReleasePictures: destroy a picture vector 
 *****************************************************************************
 * release all video resources used for pictures
 *****************************************************************************/
void CDirect3D::Direct3DVoutReleasePictures()
{
    size_t i_num_pics = I_OUTPUTPICTURES;
    size_t c;
    for( c=0; c<i_num_pics; ++c )
    {
        picture_t *p_pic = m_pPicture+c; 
        if( p_pic->p_sys )
        {
            LPDIRECT3DSURFACE9 p_d3dsurf = (LPDIRECT3DSURFACE9)p_pic->p_sys;

            p_pic->p_sys = NULL;

            if( p_d3dsurf )
            {
                IDirect3DSurface9_Release(p_d3dsurf);
            }
        }
    }
    TRACE(_T("%u Direct3D pictures released.\n"), c);

    I_OUTPUTPICTURES = 0;
}
#ifdef USE_NEW3D
int CDirect3D::Direct3DVoutResetDevice()
{
    LPDIRECT3DDEVICE9       p_d3ddev = m_d3ddev;
    HRESULT hr;

	if (SUCCESS != Direct3DFillPresentationParameters())
		return EGENERIC;

	// release all D3D objects
    Direct3DVoutReleaseScene();
    Direct3DVoutReleasePictures();

    hr = IDirect3DDevice9_Reset(p_d3ddev, &m_d3dpp);
    if( SUCCEEDED(hr) )
    {
        // re-create them
        if ((SUCCESS != Direct3DVoutCreatePictures(1))
			|| (SUCCESS != Direct3DVoutCreateScene()))
        {
            return EGENERIC;
        }
    }
    else {
        TRACE(_T("Direct3DVoutResetDevice failed ! (hr=%08lX)"), hr);
        return EGENERIC;
    }
    return SUCCESS;
}
#else
int CDirect3D::Direct3DVoutResetDevice(UINT i_width, UINT i_height)
{
    LPDIRECT3DDEVICE9       p_d3ddev = m_d3ddev;
    D3DPRESENT_PARAMETERS   d3dpp;
    HRESULT hr;

	memcpy(&d3dpp, &m_d3dpp, sizeof(d3dpp));
	if (i_width)
		d3dpp.BackBufferWidth = i_width;
	if (i_height)
		d3dpp.BackBufferHeight = i_height;

    // release all D3D objects
    Direct3DVoutReleasePictures();

    hr = IDirect3DDevice9_Reset(p_d3ddev, &d3dpp);
    if( SUCCEEDED(hr) )
    {
        // re-create them
        if (SUCCESS == Direct3DVoutCreatePictures(1))
        {
			m_d3dpp.BackBufferWidth = i_width;
			m_d3dpp.BackBufferHeight = i_height;
            return SUCCESS;
        }
        return EGENERIC;
    }
    else {
        TRACE(_T("Direct3DVoutResetDevice failed ! (hr=%08lX)"), hr);
        return EGENERIC;
    }
    return SUCCESS;
}
#endif

/*****************************************************************************
 * Direct3DVoutCreateScene: allocate and initialize a 3D scene
 *****************************************************************************
 * for advanced blending/filtering a texture needs be used in a 3D scene.
 *****************************************************************************/
int CDirect3D::Direct3DVoutCreateScene()
{
    LPDIRECT3DDEVICE9       p_d3ddev  = m_d3ddev;
    LPDIRECT3DTEXTURE9      p_d3dtex;
    LPDIRECT3DVERTEXBUFFER9 p_d3dvtc;

    HRESULT hr;

    /*
    ** Create a texture for use when rendering a scene
    ** for performance reason, texture format is identical to backbuffer
    ** which would usually be a RGB format
    */
    hr = IDirect3DDevice9_CreateTexture(p_d3ddev, 
            m_render.i_width,
            m_render.i_height,
            1,
            D3DUSAGE_RENDERTARGET, 
            m_bbFormat,
            D3DPOOL_DEFAULT,
            &p_d3dtex, 
            NULL);
    if( FAILED(hr))
    {
        TRACE(_T("Failed to create texture. (hr=0x%lx)\n"), hr);
        return EGENERIC;
    }

    /*
    ** Create a vertex buffer for use when rendering scene
    */
    hr = IDirect3DDevice9_CreateVertexBuffer(p_d3ddev,
            sizeof(CUSTOMVERTEX)*4,
            D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,
            D3DFVF_CUSTOMVERTEX,
            D3DPOOL_DEFAULT,
            &p_d3dvtc,
            NULL);
    if( FAILED(hr) )
    {
        TRACE(_T("Failed to create vertex buffer. (hr=0x%lx)\n"), hr);
        IDirect3DTexture9_Release(p_d3dtex);
        return EGENERIC;
    }

    m_d3dtex = p_d3dtex;
    m_d3dvtc = p_d3dvtc;

    // Texture coordinates outside the range [0.0, 1.0] are set 
    // to the texture color at 0.0 or 1.0, respectively.
    IDirect3DDevice9_SetSamplerState(p_d3ddev, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    IDirect3DDevice9_SetSamplerState(p_d3ddev, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

    // Set linear filtering quality
    IDirect3DDevice9_SetSamplerState(p_d3ddev, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    IDirect3DDevice9_SetSamplerState(p_d3ddev, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

    // set maximum ambient light
    IDirect3DDevice9_SetRenderState(p_d3ddev, D3DRS_AMBIENT, D3DCOLOR_XRGB(255,255,255));

    // Turn off culling
    IDirect3DDevice9_SetRenderState(p_d3ddev, D3DRS_CULLMODE, D3DCULL_NONE);

    // Turn off the zbuffer
    IDirect3DDevice9_SetRenderState(p_d3ddev, D3DRS_ZENABLE, D3DZB_FALSE);

    // Turn off lights
    IDirect3DDevice9_SetRenderState(p_d3ddev, D3DRS_LIGHTING, FALSE);

    // Enable dithering
    IDirect3DDevice9_SetRenderState(p_d3ddev, D3DRS_DITHERENABLE, TRUE);

    // disable stencil
    IDirect3DDevice9_SetRenderState(p_d3ddev, D3DRS_STENCILENABLE, FALSE);

    // manage blending
    IDirect3DDevice9_SetRenderState(p_d3ddev, D3DRS_ALPHABLENDENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(p_d3ddev, D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
    IDirect3DDevice9_SetRenderState(p_d3ddev, D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
    IDirect3DDevice9_SetRenderState(p_d3ddev, D3DRS_ALPHATESTENABLE,TRUE);
    IDirect3DDevice9_SetRenderState(p_d3ddev, D3DRS_ALPHAREF, 0x10);
    IDirect3DDevice9_SetRenderState(p_d3ddev, D3DRS_ALPHAFUNC,D3DCMP_GREATER);

    // Set texture states
    IDirect3DDevice9_SetTextureStageState(p_d3ddev, 0, D3DTSS_COLOROP,D3DTOP_MODULATE);
    IDirect3DDevice9_SetTextureStageState(p_d3ddev, 0, D3DTSS_COLORARG1,D3DTA_TEXTURE);
    IDirect3DDevice9_SetTextureStageState(p_d3ddev, 0, D3DTSS_COLORARG2,D3DTA_DIFFUSE);

    // turn off alpha operation
    IDirect3DDevice9_SetTextureStageState(p_d3ddev, 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

    TRACE(_T("Direct3D scene created successfully\n"));

    return SUCCESS;
}

/*****************************************************************************
 * Direct3DVoutReleaseScene
 *****************************************************************************/
void CDirect3D::Direct3DVoutReleaseScene()
{
    LPDIRECT3DTEXTURE9      p_d3dtex = m_d3dtex;
    LPDIRECT3DVERTEXBUFFER9 p_d3dvtc = m_d3dvtc;

    if( p_d3dvtc )
    {
        IDirect3DVertexBuffer9_Release(p_d3dvtc);
        m_d3dvtc = NULL;
    }

    if( p_d3dtex )
    {
        IDirect3DTexture9_Release(p_d3dtex);
        m_d3dtex = NULL;
    }
    TRACE(_T("Direct3D scene released successfully\n"));
}

void CDirect3D::get_capability_for_osversion(void)
{
    OSVERSIONINFO winVer;
    winVer.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

    if( GetVersionEx(&winVer) )
    {
        if( winVer.dwMajorVersion > 5 )
        {
            /* Windows Vista or above, make this module the default */
			m_got_vista_or_above = true;
            return;
        }
    }
    /* Windows XP or lower, make sure this module isn't the default */
    m_got_vista_or_above = false;
}
