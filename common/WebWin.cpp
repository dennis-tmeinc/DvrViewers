// WebWin.cpp : Web base window
//

#include <stdafx.h>

#include <exdisp.h>		// Defines of stuff like IWebBrowser2. This is an include file with Visual C 6 and above
#include <mshtml.h>		// Defines of stuff like IHTMLDocument2. This is an include file with Visual C 6 and above
#include <mshtmhst.h>	// Defines of stuff like IDocHostUIHandler. This is an include file with Visual C 6 and above

#include <stdlib.h>  
#include <cwin.h>

#include "dvrclient.h"
#include "WebWin.h"

// initial web win position
int WebWindow::ix = CW_USEDEFAULT;
int WebWindow::iy = 0;
int WebWindow::iw = 600;
int WebWindow::ih = 600;

LRESULT WebWindow::WndProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT res = FALSE;

	struct external_param * pext;

	LPOLESTR rgszName;
	switch (message)
	{

	case WM_MOVE:
		// save windows position for next time shows up
		if( !IsZoomed(m_hWnd) && !IsIconic(m_hWnd) ) {
			RECT r;
			GetWindowRect(m_hWnd, &r);
			ix = r.left;
			iy = r.top;
		}
		break;

	case WM_SIZE:
		// Resize the browser object to fit the window
		IWebBrowser2	*webBrowser2;
		if ((webBrowser2 = getWebBrowser2()) != NULL && lParam != 0 ) {
			webBrowser2->put_Width(LOWORD(lParam));
			webBrowser2->put_Height(HIWORD(lParam));
			webBrowser2->Release();

			// save windows position for next time shows up
			if (!IsZoomed(m_hWnd) && !IsIconic(m_hWnd)) {
				RECT r;
				GetWindowRect(m_hWnd, &r);
				iw = r.right - r.left;
				ih = r.bottom - r.top;
			}
		}
		break;

	case WM_DESTROY:
		// Detach the browser object from this window, and free resources.
		// UnEmbedBrowserObject(m_hWnd);
		if (browserObject) {
			UnEmbedBrowser(browserObject);
			browserObject = NULL;
		}
		res = FALSE;
		break;

	case (WM_APP + 100):				// external GetIDsOfNames
		rgszName = (LPOLESTR)wParam;
		if (lstrcmpi(rgszName, L"init_lat") == 0) {
			res = 101;
		}
		else if (lstrcmpi(rgszName, L"init_lng") == 0) {
			res = 102;
		}
		break;


	case (WM_APP + 101):		//	external method
		pext = (struct external_param *)wParam;


		break;

	case (WM_APP + 102) :		// external get
		pext = (struct external_param *)wParam;

		if (pext->dispIdMember == 101) {
			pext->pVarResult->vt = VT_R4;
			pext->pVarResult->fltVal = (FLOAT)init_lat;
			res = TRUE;
		}
		else if (pext->dispIdMember == 102) {
			pext->pVarResult->vt = VT_R4;
			pext->pVarResult->fltVal = (FLOAT)init_lng;
			res = TRUE;
		}
		break;

	case (WM_APP + 103):		// external put
		pext = (struct external_param *)wParam;

		break;

	default:
		res = Window::WndProc(message, wParam, lParam);
	}
	return res;
}


WebWindow::WebWindow() {
	HWND hwnd;
	browserObject = NULL;
	if ((hwnd = CreateWindowEx(0, Window::WinClass(), _T("Map"), WS_OVERLAPPEDWINDOW,
		ix, iy,
		iw, ih,
		NULL, NULL, AppInst(), 0)))
	{
		// set main window ICON
		HICON hIcon = LoadIcon(ResInst(), MAKEINTRESOURCE(IDR_MAINFRAME));
		SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
		SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

		init_lat = init_lng = 0.0;

		attach(hwnd);
		ShowWindowAsync(hwnd, SW_SHOW);
		browserObject = EmbedBrowser(m_hWnd);
	}
}

void WebWindow::DisplayPage(LPTSTR page) 
{
	IWebBrowser2	*webBrowser2;
	if ((webBrowser2 = getWebBrowser2()) != NULL) {
		VARIANT			myURL;
		VariantInit(&myURL);
		myURL.vt = VT_BSTR;
		myURL.bstrVal = SysAllocString((WCHAR *)page);

		// Call the Navigate2() function to actually display the page.
		webBrowser2->Navigate2(&myURL, 0, 0, 0, 0);

		// Free any resources (including the BSTR we allocated above).
		VariantClear(&myURL);

		webBrowser2->Release();
	}
}

void WebWindow::DisplayHTML(LPTSTR html) 
{
	// DisplayHTMLStr(m_hWnd, html);
}

int WebWindow::ExecScript(LPTSTR script) {
	IWebBrowser2	*webBrowser2;
	LPDISPATCH		lpDispatch;
	IHTMLDocument2	*htmlDoc2;
	IHTMLWindow2	*htmlWindow;
	int res = 0;

	if ((webBrowser2 = getWebBrowser2()) != NULL) {
		READYSTATE rdy;
		if (webBrowser2->get_ReadyState(&rdy) == S_OK) {
			if (rdy >= READYSTATE_COMPLETE) {
				// First, we need the IDispatch object
				if (webBrowser2->get_Document(&lpDispatch) == S_OK) {
					// Get the IHTMLDocument2 object embedded within the IDispatch object
					if (lpDispatch->QueryInterface(IID_IHTMLDocument2, (void **)&htmlDoc2) == S_OK) {
						if (htmlDoc2->get_parentWindow(&htmlWindow) == S_OK) {
							VARIANT var;
							VariantInit(&var);
							BSTR x_script = SysAllocString(script);
							htmlWindow->execScript(x_script, L"JavaScript", &var);
							htmlWindow->Release();
							SysFreeString(x_script);
							VariantClear(&var);
							res = 1;
						}
						htmlDoc2->Release();
					}
					lpDispatch->Release();
				}
			}
		}
		webBrowser2->Release();
	}
	return res;
}

