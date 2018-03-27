/*
  Copyright (c) 2008 Toronto MicroElectronics Inc
 
  --  CWin.h  --

   A simple windows wrapper 

   By Dennis Chen, 2008

*/

#ifndef __CWIN_H__
#define __CWIN_H__

#include <sdkddkver.h>

// my default win versions
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#ifndef WINVER
#define WINVER _WIN32_WINNT
#endif

#ifndef _WIN32_IE
#define _WIN32_IE       0x0A00
#endif

#include <windows.h>
#include <Commctrl.h>
#include <tchar.h>

// to enable visual styles
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define USE_GDIPLUS

// Use GDIPlus
#ifdef USE_GDIPLUS
#pragma comment(lib, "gdiplus.lib" )
#pragma comment(lib, "Comctl32.lib" )
#include <objidl.h>
#include <gdiplus.h>
using namespace Gdiplus;
#endif

// Property to get windows handle
#define PROP_WIN	( MAKEINTATOM(8) )

// define last window message received
#define WM_LAST_MSG (WM_NCDESTROY)

// mouse position parameter
#define MOUSE_POSX(l)                        ((int)(short)LOWORD(l))
#define MOUSE_POSY(l)                        ((int)(short)HIWORD(l))

// window subclassing
class Window {

private:
	static class _app {

#ifdef USE_GDIPLUS
		ULONG_PTR gdiplusToken;
#endif

	public:
		_app() {
#ifdef USE_GDIPLUS
			GdiplusStartupInput gdiplusStartupInput;
			GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
#endif

#ifdef	ICC_STANDARD_CLASSES
			INITCOMMONCONTROLSEX iccex;
			iccex.dwSize = sizeof(iccex);
			iccex.dwICC = ICC_WIN95_CLASSES |
				ICC_DATE_CLASSES |
				ICC_STANDARD_CLASSES;
			InitCommonControlsEx(&iccex);
#else
			InitCommonControls();
#endif
		}
		~_app() {
#ifdef USE_GDIPLUS
			GdiplusShutdown(gdiplusToken);
#endif
		}

	};

	static LRESULT CALLBACK sWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        Window * pwin = getWindow( hwnd ) ;
		if( pwin != NULL ) {
			LRESULT res = pwin->WndProc(uMsg, wParam, lParam);
            if( uMsg == WM_LAST_MSG ) {			// The last message sent to a window
                pwin->detach();
            }
			return res ;
        }
        else {
            return ::DefWindowProc( hwnd, uMsg, wParam, lParam );
        }
    }

public:

    // public static help functions

	// get Window from handle
    static inline Window * getWindow( HWND hWnd )
    {
		Window * pwin = (Window *) GetProp( hWnd, PROP_WIN );
		return (pwin && pwin->m_hWnd == hWnd) ? pwin : NULL;
    }

	// Get application instance
    static inline HINSTANCE AppInst()
    {
		return GetModuleHandle(NULL);
    }

    // Get Module Instance contain current resource ( Used for resources in DLL )
    static inline HINSTANCE ResInst()
    {
        HMODULE hMod ;
        GetModuleHandleEx(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCTSTR)&Window::sWindowProc,
            &hMod) ;
        return hMod ;
    }

    // Register/retrieve standard window class
    static inline LPCTSTR WinClass( HBRUSH bgcolor = NULL, UINT style=(CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS) )
    {
		ATOM hClass ;
        TCHAR wclassname[40] ;
        WNDCLASSEX wcex;
		wsprintf( wclassname, TEXT("S%xB%p"), (UINT)style, (void *)bgcolor );
        ZeroMemory(&wcex, sizeof(wcex));
        wcex.cbSize = sizeof(WNDCLASSEX);
		hClass = (ATOM)GetClassInfoEx( AppInst(), wclassname, &wcex );
        if( hClass == NULL ) {
			wcex.cbSize = sizeof(WNDCLASSEX);
			wcex.hInstance = AppInst();
            wcex.style          = style ;
			wcex.lpfnWndProc = ::DefWindowProc;
			wcex.hIcon			= NULL ;
			wcex.hIconSm        = NULL ;
		    wcex.hCursor		= LoadCursor(NULL, IDC_ARROW) ;   
            //wcex.hbrBackground	= (HBRUSH)(COLOR_BACKGROUND+1) ;
			wcex.hbrBackground	= (HBRUSH)bgcolor ;
            wcex.lpszClassName	= wclassname ;
			hClass = RegisterClassEx(&wcex) ;
        }
		return (LPCTSTR)hClass ;
    }

protected:
    HWND	m_hWnd ;		// window handle
	WNDPROC m_savedWndProc;

    LRESULT DefWndProc(UINT uMsg, WPARAM wParam, LPARAM lParam) {
        return CallWindowProc(m_savedWndProc, m_hWnd, uMsg, wParam, lParam );
    }

    virtual LRESULT WndProc(UINT uMsg, WPARAM wParam, LPARAM lParam) {
        return DefWndProc(uMsg, wParam, lParam);
    }

	// called after window been attached (can be serve as OnCreate)
    virtual void OnAttach() {} ;

	// called after window been dettached, may do some final cleaning here or even delete the object it self
    virtual void OnDetach() {} ;

	// run message loop
	virtual BOOL PreProcessMessage(MSG *pmsg){
		return FALSE;
	}

public:
	Window() :
	  m_savedWndProc(NULL),	m_hWnd(NULL)
		{
			static _app app;
		}

	virtual ~Window() {
		// if a window is attached, destroy the window.
		if( m_hWnd ) {
			::DestroyWindow( m_hWnd );
		}
	}

    HWND getHWND() {
        return m_hWnd ;
    }

	operator HWND () {
		return m_hWnd ;
	}

    // subclass window
    virtual void attach(HWND hWnd) {
		detach();
		SetProp( hWnd, PROP_WIN, (HANDLE)this);
		m_hWnd = hWnd;
		m_savedWndProc = (WNDPROC)SetWindowLongPtr( hWnd, GWLP_WNDPROC, (LONG_PTR)&Window::sWindowProc );
        OnAttach();
    }

    virtual void detach() {
		if (m_savedWndProc) {
			SetWindowLongPtr(m_hWnd, GWLP_WNDPROC, (LONG_PTR)m_savedWndProc);
			m_savedWndProc = NULL;
		}
		if( m_hWnd ) {
            RemoveProp(m_hWnd, PROP_WIN);
            m_hWnd = NULL;
			// the window is detached from this object
			// now it is safe to call "delete this" ;
			OnDetach();
        }
    }

	// return true if this is a dialog
	virtual int isDialog() {
		return FALSE ;
	}

	// Message loop, for both window and dialog
	virtual int run() {
        MSG msg;
		msg.wParam = 0;
        // Main message loop
		while( m_hWnd && GetMessage(&msg, NULL, 0, 0) )
        {
			if( !PreProcessMessage(&msg) ){
				if( isDialog() ) {
					if( msg.message == WM_NULL &&
						msg.hwnd == m_hWnd &&
						GetActiveWindow()!=m_hWnd )			// EndDialog() called,  this window is deactivated
					{
						break;
					}
					if (IsDialogMessage(m_hWnd, &msg)) {
						continue;
					}
				}
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		return msg.wParam ;
	}
};

class Dialog : public Window {
private:

	static INT_PTR CALLBACK sDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
		Dialog * dlg = (Dialog *)getWindow(hwndDlg);
		if (dlg == NULL) {
			if (uMsg == WM_INITDIALOG && lParam != NULL) {
				dlg = (Dialog *)lParam;
				dlg->attach(hwndDlg);
			}
			else {
				return FALSE;
			}
		}
		if (dlg->isDialog())
			return dlg->DlgProc(uMsg, wParam, lParam);
		else
			return FALSE;
	}

protected:

	virtual int OnInitDialog()
    {
        return TRUE;
    }

    virtual int OnOK()
    {
        return EndDialog(IDOK);
	}

    virtual int OnCancel()
    {
        return EndDialog(IDCANCEL);
    }

    virtual int OnCommand( int Id, int Event ) 
	{
        switch (Id)
        {
        case IDOK:
			return OnOK();

        case IDCANCEL:
            return OnCancel();
        }
        return FALSE;
    }

    virtual int OnNotify( LPNMHDR pnmhdr ) {
        return FALSE ;
    }

    // dialog box procedures
    virtual int DlgProc(UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_INITDIALOG:
            return OnInitDialog();

        case WM_NOTIFY:
            return OnNotify( (LPNMHDR)lParam ) ;

        case WM_COMMAND:
            return OnCommand( LOWORD(wParam), HIWORD(wParam) );

        }
        return FALSE ;
    }

public:

	// return true if this is a dialog
	virtual int isDialog() {
		return TRUE ;
	}

	// Create (modeless) dialog
    HWND CreateDlg(int IdDialog, HWND hWndParent = NULL)
    {
        return CreateDialogParam(ResInst(), MAKEINTRESOURCE(IdDialog), hWndParent, Dialog::sDialogProc, (LPARAM)this ) ;
    }

	int DoModal() {
		int dlgret = IDCANCEL ;
		if( m_hWnd ) {
			HWND hOwner = ::GetWindow( m_hWnd, GW_OWNER );
			// Disable parent window
			if( hOwner ){		
				EnableWindow( hOwner, FALSE );
			}
			// show dialog
			ShowWindowAsync( m_hWnd, SW_SHOW ); 

			// run message loop
			dlgret = run();

			if (m_hWnd) {
				::DestroyWindow(m_hWnd);
			}

			// Re-enable parent window
			if( hOwner && IsWindow(hOwner) ) {
				EnableWindow( hOwner, TRUE ) ;
			}
		}

		return dlgret ;
	}

	// Modal Dialog
	int DoModal(int IdDialog, HWND hWndParent = NULL ) {
		CreateDlg(IdDialog, hWndParent);
		return DoModal();
	}

	// Direct api Modal dialog
	int DoModal2(int IdDialog, HWND hWndParent = NULL ) {
        return (int)DialogBoxParam(ResInst(), MAKEINTRESOURCE(IdDialog), hWndParent, Dialog::sDialogProc, (LPARAM)this);
    }
	
	// End Dialog
    BOOL EndDialog( int result = IDCANCEL ) {
		// send extra NULL MSG with dialog result
		PostMessage( m_hWnd, WM_NULL, (WPARAM)result, (LPARAM)-1 );
		return ::EndDialog( m_hWnd, result );
	}

};

#ifdef APP_DEMO

// 
//	Demo codes for application with a very simple main window
//
// a hello world window example
class SimpleMainWin : public Window
{

#ifdef	IDC_APP
        HACCEL hAccelTable ;
#endif

protected:
    virtual LRESULT OnPaint()
    {
        PAINTSTRUCT ps;
        HDC hdc;
        hdc = BeginPaint(m_hWnd, &ps);
#ifdef USE_GDIPLUS
        Gdiplus::Graphics * g = new Graphics (hdc);
        Gdiplus::Font        font(FontFamily::GenericSansSerif(), 24, FontStyleRegular, UnitPixel);
        Gdiplus::PointF      pointF(30.0f, 10.0f);
        Gdiplus::SolidBrush  solidBrush(Color(255, 0, 0, 255));
        g->DrawString(TEXT("Hello world!"), -1, &font, pointF, &solidBrush);
        delete g ;
#else
        // painting codes here.
        TextOut( hdc, 10, 10, TEXT("Hello world!"), 12);
#endif  // USE_GDIPLUS
        EndPaint(m_hWnd, &ps);
        return 0 ;
    }

    virtual LRESULT OnCommand( int Id, int Event ) {
        switch (Id)
        {
#ifdef IDM_ABOUT
#ifdef IDD_ABOUTBOX
		case IDM_ABOUT:
            // Open about dialog
            {
                Dialog dlg;
                dlg.DoModal(IDD_ABOUTBOX, m_hWnd);
            }
            break;
#endif	// IDD_ABOUTBOX
#endif	// IDM_ABOUT
#ifdef	IDM_EXIT
        case IDM_EXIT:
            PostMessage(m_hWnd, WM_CLOSE, 0, 0);
            break;
#endif
        default:
            return FALSE ;
        }
        return TRUE ;
    }

    virtual LRESULT OnNotify( LPNMHDR pnmhdr ) {
        return FALSE ;
    }

    virtual LRESULT WndProc(UINT message, WPARAM wParam, LPARAM lParam) {
        LRESULT res = FALSE ;
        switch (message)
        {
        case WM_COMMAND:
            res = OnCommand( LOWORD(wParam), HIWORD(wParam) ) ;
            break ;
        case WM_NOTIFY:
            res = OnNotify( (LPNMHDR)lParam ) ;
            break ;
        case WM_PAINT:
            res = OnPaint();
            break;
        default:
            res = DefWndProc(message, wParam, lParam);
        }
        return res ;
    }

#ifdef	IDC_APP
	virtual BOOL PreProcessMessage(MSG *pmsg)
	{
		return TranslateAccelerator(m_hWnd, hAccelTable, pmsg );
	}
#endif

public:
    SimpleMainWin()
    {
        HWND hwnd ;
        HMENU hMenu = NULL ;
        TCHAR title[256] = TEXT("Simple Window") ;
#ifdef	IDC_APP
        LoadString(ResInst(), IDC_APP, title, 256 );
        hAccelTable = LoadAccelerators(ResInst(), MAKEINTRESOURCE(IDC_APP));
        hMenu = LoadMenu( ResInst(), MAKEINTRESOURCE(IDC_APP));
#endif
		hwnd = CreateWindowEx(
            0,						// no extended styles
            WinClass( (HBRUSH)(COLOR_WINDOW+1) ),  // class name
            title,					// window name
            WS_OVERLAPPEDWINDOW,	// overlapped window
            CW_USEDEFAULT,          // default horizontal position
            CW_USEDEFAULT,          // default vertical position
            CW_USEDEFAULT,          // default width
            CW_USEDEFAULT,          // default height
            (HWND) NULL,            // no parent or owner window
            (HMENU) hMenu,          // menu
            AppInst(),			    // instance handle
            NULL);                  // no window creation data
        if( hwnd ) {
            attach( hwnd );
#ifdef	IDC_APP
			HICON hicon = LoadIcon( ResInst(), MAKEINTRESOURCE(IDC_APP));
			if( hicon ) {
				SendMessage(hwnd, WM_SETICON, ICON_SMALL,(LPARAM)hicon );
				SendMessage(hwnd, WM_SETICON, ICON_BIG,  (LPARAM)hicon );
			}
#endif
			ShowWindow(hwnd, SW_SHOWNORMAL);
        }
    }
};

// a hello world demo WinMain
inline int simpleWinMain()
{
    SimpleMainWin mainWin ;
    return mainWin.run();
}

#endif		// APP_DEMO

#endif		// define __CWIN_H__
