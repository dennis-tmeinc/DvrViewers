#ifndef _VOUT_MEM_H_
#define _VOUT_MEM_H_

#include "video_output.h"

class CVoutMem : public CVideoOutput
{
public:
	CVoutMem(tme_object_t *pIpc, video_format_t *p_fmt, int iPtsDelay);
	virtual ~CVoutMem();
	virtual int OpenVideo();

private:
	virtual int Init();
	virtual void End();
	virtual int Manage();
	virtual void Render(picture_t *);
	virtual void Display(picture_t *);
	virtual int Lock(picture_t *);
	virtual int Unlock(picture_t *);
	virtual void CloseVideo();
	virtual void ShowVideoWindow(int nCmdShow);

	//tme_object_t m_ipc;

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


     bool   m_bHwYuv;    /* Should we use hardware YUV->RGB conversions */
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

	bool m_got_vista_or_above;
};


#endif