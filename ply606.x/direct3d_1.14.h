#ifndef _DIRECT3D_H_
#define _DIRECT3D_H_

//#include <objbase.h> // include this before ddraw.h to avoid linking with dxguid.lib
//#include <initguid.h> //include this before ddraw.h to avoid linking with dxguid.lib
//#include <ddraw.h>
#include <d3d9.h>
#include "video_output.h"
#include "direct_common.h"

typedef struct
{
    const TCHAR   *name;
    D3DFORMAT    format;    /* D3D format */
    fourcc_t fourcc;    /* fourcc */
    DWORD     rmask;
    DWORD     gmask;
    DWORD     bmask;
} d3d_format_t;

class CDirect3D : public CVideoOutput
{
public:
	CDirect3D(HWND hwnd, video_format_t *p_fmt, int iPtsDelay, bool bResizeParent);
	virtual ~CDirect3D();
	virtual int OpenVideo();

	int Direct3DEventThread();
    HWND                 m_hvideownd;        /* Handle of the video sub-window */
    tme_mutex_t		m_lockEvent;
	bool m_bEventHasMoved;

private:
	int Direct3DCreate();
	int Direct3DOpen();
	void Direct3DDestroy();
	void Direct3DClose();
	int Direct3DFillPresentationParameters();
	int Direct3DCreateWindow();
	void Direct3DCloseWindow();
	const d3d_format_t *Direct3DFindFormat(fourcc_t chroma, D3DFORMAT target);
	int Direct3DCheckConversion(D3DFORMAT src, D3DFORMAT dst);
	int Direct3DCreatePool();
	int Direct3DCreateResources();
	int Direct3DLockSurface( picture_t *p_pic );
	int Direct3DUnlockSurface( picture_t *p_pic );
	void EventThreadUpdateWindowPosition(bool *pb_moved, bool *pb_resized,
                                      int x, int y, int w, int h);
	void UpdateRects( bool is_force );

	virtual int Init();
	virtual void End();
	virtual int Manage();
	virtual void Render(picture_t *);
	virtual void Display(picture_t *);
	virtual int Lock(picture_t *);
	virtual int Unlock(picture_t *);
	virtual void CloseVideo();
	virtual void ShowVideoWindow(int nCmdShow);

	tme_object_t m_ipc;
    tme_mutex_t		m_lock;

	HANDLE	m_hEventThread;
	bool	m_bEventDead, m_bEventDie;

	HWND                 m_hwnd;                  /* Handle of the main window */
    HWND                 m_hfswnd;          /* Handle of the fullscreen window */
	bool m_bResetDevice, m_bAllowHwYuv;
	bool m_bIsFirstDisplay, m_bIsOnTop;

	/* size of the display */
    RECT         m_rectDisplay;
    int          m_iDisplayDepth;

    /* size of the overall window (including black bands) */
    RECT         m_rectParent;
    /* Coordinates of src and dest images (used when blitting to display) */
    RECT         m_rectSrc;
    RECT         m_rectSrcClipped;
    RECT         m_rectDest;
    RECT         m_rectDestClipped;


    volatile UINT16 m_iChanges;        /* changes made to the video display */

	/* Mouse */
    volatile bool m_bCursorHidden;
    volatile mtime_t    m_iLastmoved;
	picture_resource_t m_resource;

	/* ------ event thread -----*/
    TCHAR m_class_main[256];
    TCHAR m_class_video[256];
    /* Window position and size */
    int          m_iWindowX;
    int          m_iWindowY;
    int          m_iWindowWidth;
    int          m_iWindowHeight;
    int          m_iWindowStyle;

	HINSTANCE m_hd3d9_dll;
	LPDIRECT3D9 m_d3dobj;
	LPDIRECT3DDEVICE9 m_d3ddev;
	D3DPRESENT_PARAMETERS m_d3dpp;
#if 0
    WNDPROC              m_pfWndproc;             /* Window handling callback */

	/* Multi-monitor support */
    GUID                 *m_pDisplayDriver;
    HMONITOR             (WINAPI* m_MonitorFromWindow)( HWND, DWORD );
    BOOL                 (WINAPI * m_GetMonitorInfo)( HMONITOR, LPMONITORINFO );
    HMONITOR             m_hmonitor;          /* handle of the current monitor */


	void DirectXUpdateRects( bool b_force );

private:

	// user configurable variables
	bool m_bHwYuvConf, m_bUseSysmemConf, m_b3BufferingConf, m_bWallpaperConf;

	bool m_bDisplayInitialized;




    /* Misc */
    bool      m_bOnTopChange;

    bool      m_bWallpaper;

    /* screensaver system settings to be restored when vout is closed */
    UINT m_iSpiLowpowertimeout;
    UINT m_iSpiPowerofftimeout;
    UINT m_iSpiScreensavetimeout;


    bool   m_bHwYuv;    /* Should we use hardware YUV->RGB conversions */

	/* Overlay alignment restrictions */
    int          m_iAlignSrcBoundary;
    int          m_iAlignSrcSize;
    int          m_iAlignDestBoundary;
    int          m_iAlignDestSize;

    bool   m_bUseSysmem;   /* Should we use system memory for surfaces */
    bool   m_b3BufOverlay;   /* Should we use triple buffered overlays */

    /* DDraw capabilities */
    int          b_caps_overlay_clipping;

    UINT32          m_iRgbColorkey;      /* colorkey in RGB used by the overlay */
    UINT32          m_iColorkey;                 /* colorkey used by the overlay */

    COLORREF        color_bkg;
    COLORREF        color_bkgtxt;

    LPDIRECTDRAW7        m_pDDObject;                    /* DirectDraw object */
    LPDIRECTDRAWSURFACE7 m_pDisplay;                        /* Display device */
    LPDIRECTDRAWSURFACE7 m_pCurrentSurface;   /* surface currently displayed */
    LPDIRECTDRAWCLIPPER  m_pClipper;             /* clipper used for blitting */

    HINSTANCE            m_hDDrawDll;       /* handle of the opened ddraw dll */


	int NewPictureVec( picture_t *p_pic, int i_num_pics );
	void FreePictureVec( picture_t *p_pic, int i_num_pics );
	int DirectXInitDDraw();
	void DirectXGetDDrawCaps();
	int DirectXCreateDisplay();
	int DirectXCreateClipper();
	DWORD DirectXFindColorkey( UINT32 *pi_color );
	int DirectXUpdateOverlay();
	void DirectXCloseDisplay();
	void DirectXCloseDDraw();
	void DirectXCloseSurface( LPDIRECTDRAWSURFACE7 p_surface );
	int DirectXCreateSurface( LPDIRECTDRAWSURFACE7 *pp_surface_final,
                                 int i_chroma, int b_overlay,
                                 int i_backbuffers );
	int UpdatePictureStruct( picture_t *p_pic, int i_chroma );
#endif


};


#endif