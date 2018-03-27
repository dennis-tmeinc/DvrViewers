#ifndef _DIRECTX_H_
#define _DIRECTX_H_

#include <objbase.h> // include this before ddraw.h to avoid linking with dxguid.lib
#include <initguid.h> //include this before ddraw.h to avoid linking with dxguid.lib
#include <ddraw.h>
#include "video_output.h"
#include "direct_common.h"

#define MODULE_NAME_IS_vout_directx

class CDirectX : public CVideoOutput
{
public:
	CDirectX(HWND hwnd, video_format_t *p_fmt, int iPtsDelay, bool bResizeParent, bool bUseOverlay = true);
	virtual ~CDirectX();

    HWND                 m_hvideownd;        /* Handle of the video sub-window */
    HWND                 m_hwnd;                  /* Handle of the main window */
    HWND                 m_hfswnd;          /* Handle of the fullscreen window */
    WNDPROC              m_pfWndproc;             /* Window handling callback */

	/* Multi-monitor support */
    GUID                 *m_pDisplayDriver;
    HMONITOR             (WINAPI* m_MonitorFromWindow)( HWND, DWORD );
    BOOL                 (WINAPI * m_GetMonitorInfo)( HMONITOR, LPMONITORINFO );
    HMONITOR             m_hmonitor;          /* handle of the current monitor */

	virtual int OpenVideo();
	virtual int Init();
	virtual void End();
	virtual int Manage();
	virtual void Render(picture_t *);
	virtual void Display(picture_t *);
	virtual int Lock(picture_t *);
	virtual int Unlock(picture_t *);

	int DirectXEventThread();
	void DirectXUpdateRects( bool b_force );
	virtual void CloseVideo();
	virtual void ShowVideoWindow(int nCmdShow);

private:
	tme_object_t m_ipc;

	// user configurable variables
	bool m_bHwYuvConf, m_bUseSysmemConf, m_b3BufferingConf, m_bWallpaperConf;

	HANDLE	m_hEventThread;
	bool	m_bEventDead, m_bEventDie;
	bool m_bDisplayInitialized;


    /* size of the display */
    RECT         m_rectDisplay;
    int          i_display_depth;

    /* size of the overall window (including black bands) */
    RECT         m_rectParent;

    /* Window position and size */
    int          m_iWindowX;
    int          m_iWindowY;
    int          m_iWindowWidth;
    int          m_iWindowHeight;
    int          m_iWindowStyle;

    volatile UINT16 m_iChanges;        /* changes made to the video display */

    /* Mouse */
    volatile bool m_bCursorHidden;
    volatile mtime_t    m_iLastmoved;

    /* Misc */
    bool      m_bOnTopChange;

    bool      m_bWallpaper;

    /* screensaver system settings to be restored when vout is closed */
    UINT m_iSpiLowpowertimeout;
    UINT m_iSpiPowerofftimeout;
    UINT m_iSpiScreensavetimeout;

    /* Coordinates of src and dest images (used when blitting to display) */
    RECT         m_rectSrc;
    RECT         m_rectSrcClipped;
    RECT         m_rectDest;
    RECT         m_rectDestClipped;

    bool   m_bHwYuv;    /* Should we use hardware YUV->RGB conversions */

	/* Overlay alignment restrictions */
    int          m_iAlignSrcBoundary;
    int          m_iAlignSrcSize;
    int          m_iAlignDestBoundary;
    int          m_iAlignDestSize;

    bool   m_bUsingOverlay;         /* Are we using an overlay surface */
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

    tme_mutex_t		m_lock;

	int NewPictureVec( picture_t *p_pic, int i_num_pics );
	void FreePictureVec( picture_t *p_pic, int i_num_pics );
	int DirectXCreateWindow();
	void DirectXCloseWindow();
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
	int DirectXLockSurface( picture_t *p_pic );
	int DirectXUnlockSurface( picture_t *p_pic );
	int UpdatePictureStruct( picture_t *p_pic, int i_chroma );


};


#endif