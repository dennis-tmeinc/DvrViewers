#ifndef _VIDEO_OUTPUT_H_
#define _VIDEO_OUTPUT_H_

#include "tmeviewdef.h"
#include "tme_thread.h"
#include "es.h"
#include "video.h"
#include "osd.h"
#include "chroma.h"
#include "subpicture.h"

#ifndef MAX_BLUR_AREA
#define MAX_BLUR_AREA   (10)

// blur area definition
//   coordinates are base on 256x256 picture
struct blur_area {
    int x;
    int y;
    int w;
    int h;
    int radius;
    int shape;
};

#endif

#define VOUT_MAX_WIDTH                  4096

class CVideoOutput
{
public:
	CVideoOutput(tme_object_t *pIpc, HWND hwnd, video_format_t *p_fmt, int iPtsDelay, bool bResizeParent);
	virtual ~CVideoOutput();
//protected:
	int StartThread(); // should be called after the constructor of the derived class
	virtual int OpenVideo() = 0;
	virtual int Init() = 0;
	virtual void End() = 0;
	virtual int Manage() = 0;
	virtual void Render(picture_t *) = 0;
	virtual void Display(picture_t *) = 0;
	virtual int Lock(picture_t *) = 0;
	virtual int Unlock(picture_t *) = 0;
	virtual void CloseVideo() = 0;
	virtual void ShowVideoWindow(int nCmdShow) = 0;

    HWND                 m_hparent;             /* Handle of the parent window */

	bool m_bDropLate;
	bool m_bResizeParent;
	bool m_bDie;
	bool m_bError;
	HANDLE m_hThread;
	tme_object_t *m_pIpc;
    tme_mutex_t         m_pictureLock;                 /**< picture heap lock */
    tme_mutex_t         m_subpictureLock;           /**< subpicture heap lock */
    tme_mutex_t         m_changeLock;                 /**< thread change lock */
    //vout_sys_t *        p_sys;                     /**< system output method */

    UINT16            m_iChanges;          /**< changes made to the thread.
                                                      \see \ref vout_changes */
    float               m_fGamma;                                  /**< gamma */
    bool          m_bGrayscale;         /**< color or grayscale display */
    bool          m_bInfo;            /**< print additional information */
    bool          m_bInterface;                   /**< render interface */
    bool          m_bScale;                  /**< allow picture scaling */
    bool          m_bFullscreen;         /**< toogle fullscreen display */
    UINT32            m_renderTime;           /**< last picture render time */
    unsigned int        m_iWindowWidth;              /**< video window width */
    unsigned int        m_iWindowHeight;            /**< video window height */
    unsigned int        m_iAlignment;          /**< video alignment in window */
    unsigned int        m_iParNum;           /**< monitor pixel aspect-ratio */
    unsigned int        m_iParDen;           /**< monitor pixel aspect-ratio */

    //intf_thread_t       *p_parent_intf;   /**< parent interface for embedded vout (if any) */

    unsigned long      m_cFpsSamples;                         /**< picture counts */
    mtime_t       m_pFpsSample[VOUT_FPS_SAMPLES];     /**< FPS samples dates */

    int                 m_iHeapSize;                          /**< heap size */
    picture_heap_t      m_render;                       /**< rendered pictures */
    picture_heap_t      m_output;                          /**< direct buffers */
    bool          m_bDirect;            /**< rendered are like direct ? */
    vout_chroma_t       m_chroma;                      /**< translation tables */
	int m_iChromaType;

    video_format_t      m_fmtRender;      /* render format (from the decoder) */
    video_format_t      m_fmtIn;            /* input (modified render) format */
    video_format_t      m_fmtOut;     /* output format (for the video output) */

    /* Picture heap */
    picture_t           m_pPicture[2*VOUT_MAX_PICTURES+1];      /**< pictures */

    /* Subpicture unit */
    CSubPicture            *m_pSpu;

    /* Statistics */
    unsigned long          c_loops;
    unsigned long          c_pictures, c_late_pictures;
    mtime_t          display_jitter;    /**< average deviation from the PTS */
    unsigned long          c_jitter_samples;  /**< number of samples used
                                           for the calculation of the
                                           jitter  */
    /** delay created by internal caching */
    int                 m_iPtsDelay;

    /* Filter chain */
    char *m_pszFilterChain;
    bool m_bFilterChange;

    /* blur area support */
    struct blur_area m_blur_area[MAX_BLUR_AREA] ;
    int    m_ba_number ;
    void   blurPicture(picture_t * pic) ;
    // these variable moved here (from CVideoOutput::RunThread() ) to support blur feature (dennis)
    int             i_idle_loops;    /* loops without displaying a picture */

    /* Misc */
    bool       m_bSnapshot;     /**< take one snapshot on the next loop */

	picture_t * m_pSnapshotpic ;	// A Snapshot picture. (dennisc)
	int GetSnapShotPic(PBYTE, int, int);

	void InitWindowSize();
	void RunThread();
	int InitThread();
	void ErrorThread();
	void EndThread();
	void ActivateChroma();
	void DeactivateChroma();
};

#define I_OUTPUTPICTURES m_output.i_pictures
#define PP_OUTPUTPICTURE m_output.pp_picture
#define I_RENDERPICTURES m_render.i_pictures
#define PP_RENDERPICTURE m_render.pp_picture


#endif