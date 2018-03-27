// WebWin.h : Web base window
//

#ifndef __WEBWIN_H__
#define __WEBWIN_H__

#include <exdisp.h>		// Defines of stuff like IWebBrowser2. This is an include file with Visual C 6 and above
#include <mshtml.h>		// Defines of stuff like IHTMLDocument2. This is an include file with Visual C 6 and above
#include <mshtmhst.h>	// Defines of stuff like IDocHostUIHandler. This is an include file with Visual C 6 and above

#include <stdlib.h>  
#include <cwin.h>

extern IOleObject	* EmbedBrowser(HWND hwnd);
extern void  UnEmbedBrowser(IOleObject	* browserObject);

// external parameters
#ifndef __EXTERNEL_PARAM__
#define __EXTERNEL_PARAM__
struct external_param {
	DISPID dispIdMember;
	DISPPARAMS *pDispParams;
	VARIANT *pVarResult;
};
#endif	// __EXTERNEL_PARAM__

class WebWindow : public Window {
	static int ix, iy, iw, ih;

protected:

	IOleObject	* browserObject;

	float init_lat, init_lng;

	virtual LRESULT WndProc(UINT message, WPARAM wParam, LPARAM lParam) ;

public:

	WebWindow();

	~WebWindow() {
		if (m_hWnd) DestroyWindow(m_hWnd);
	}

	void show() {
		if(IsIconic(m_hWnd))
			ShowWindow(m_hWnd, SW_RESTORE);
		BringWindowToTop(m_hWnd);
	}

	void setInitloc(float lat, float lng) {
		init_lat = lat;
		init_lng = lng;
	}

	IWebBrowser2	*getWebBrowser2() {
		IWebBrowser2	*webBrowser2;

		if (browserObject && browserObject->QueryInterface(IID_IWebBrowser2, (void **)&webBrowser2) == S_OK) {
			return webBrowser2;
		}
		return NULL;
	}

	void DisplayPage(LPTSTR page);
	void DisplayHTML(LPTSTR html);
	int ExecScript(LPTSTR script);
};

#endif	// __WEBWIN_H__
