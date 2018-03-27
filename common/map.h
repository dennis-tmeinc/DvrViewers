#pragma once

#include <Windows.h>
#include <shellapi.h>
#include <commdlg.h>
#include <Shlobj.h>
#include <mshtml.h>
#include <mshtmhst.h>

#include "../common/cstr.h"
#include "../common/cwin.h"
#include "../common/util.h"

#include "decoder.h"

#define TIMER_MOUSEWHEEL    (5)

class Map : public Window
{
protected:

    int m_zoomlevel ;
    int m_wheeldir ;
    float lati[5] ;
    float longi[5] ;
    Image * mapimg ;

public:
    Map( Window * parent ){
        HRESULT hr ;

        static IHTMLWindow2 * pWindow=NULL ;

        if( pWindow ) {
            VARIANT_BOOL vb ;
            hr=pWindow->get_closed(&vb);
            if( vb==VARIANT_TRUE ) {
                pWindow->Release();
            }
            else {
                VARIANT var;
                var.vt=VT_EMPTY ;
                hr=pWindow->execScript(SysAllocString(_T("setvalue(123)")),SysAllocString(_T("JavaScript")), &var);
                if(hr==S_OK) {
                    return ;
                }
                else {
                    pWindow->Release();
                }
            }
        }

        if (SUCCEEDED(OleInitialize(NULL)))
        {

            HINSTANCE hinstMSHTML = LoadLibrary(TEXT("MSHTML.DLL"));

            if (hinstMSHTML == NULL)
            {
                //Error loading module -- fail as securely as possible
                return;
            }

            SHOWMODELESSHTMLDIALOGFN* pfnShowModelessHTMLDialog;
            pfnShowModelessHTMLDialog =
                (SHOWMODELESSHTMLDIALOGFN*)GetProcAddress(hinstMSHTML,
                "ShowModelessHTMLDialog");
            if (pfnShowModelessHTMLDialog)
            {
                string url ;
                string exefilename;

                GetModuleFileName(AppInst(), exefilename.tcssize(512), 512); 
                url.printf("res://%s/HTML_RESOURCE", (char *)exefilename );

                IMoniker *pURLMoniker;
                BSTR bstrURL =
                    SysAllocString(url);
                CreateURLMoniker(NULL, bstrURL, &pURLMoniker);

                if (pURLMoniker)
                {
                    DWORD dwFlags = HTMLDLG_MODELESS | HTMLDLG_VERIFY;
                    hr=(*pfnShowModelessHTMLDialog)(parent->getHWND(), pURLMoniker, NULL,
                        NULL, &pWindow);

                    pURLMoniker->Release();
                }

                SysFreeString(bstrURL);
            }

            FreeLibrary(hinstMSHTML);

        }
/*
        int i;
        m_zoomlevel=14 ;
        for(i=0;i<5;i++) {
            lati[i]=longi[i]=0.0 ;
        }
        m_wheeldir=0 ;

        HWND hWnd = FindWindow(CWinClass(),_T("MAP"));
        if( hWnd ) {
            BringWindowToTop(hWnd);
        }
        else {
            m_hWnd = CreateWindowEx( 0, 
                CWinClass(),
                _T("MAP"),
                WS_POPUPWINDOW|WS_CAPTION,
                CW_USEDEFAULT,          // default horizontal position  
                CW_USEDEFAULT,          // default vertical position    
                512,                 // width                
                512,                 // height    
                parent->getHWND(), 
                NULL, 
                AppInst(), 
                NULL );
            attach(m_hWnd);
            mapimg=NULL ;
            ShowWindow(m_hWnd, SW_SHOW);
        }

*/
    }

    void setlocation( float slati, float slongi ) {
        int i;
        for(i=4;i>0;i--) {
            lati[i]=lati[i-1] ;
            longi[i]=longi[i-1];
        }
        lati[0]=slati;
        longi[0]=slongi;
        newimg();
    }

protected:

    void newimg(){
        int l;
        string mapurl ;
        char * murl = mapurl.strsize(4096);
        sprintf( murl, "http://maps.googleapis.com/maps/api/staticmap?zoom=%d&size=512x512", m_zoomlevel );
        l=strlen(murl);
        murl+=l ;
        if( lati[0]>1.0 || lati[0]<-1.0 || longi[0]>1.0 || longi[0]<-1.0 ) {
            sprintf( murl, "&markers=size:small%%7C%.6f,%.6f", lati[0], longi[0] );
            l=strlen(murl);
            murl+=l ;
            if( lati[1]>1.0 || lati[1]<-1.0 || longi[1]>1.0 || longi[1]<-1.0 ) {
                sprintf( murl, "&path=color:0x0000c080%%7Cweight:5%%7C%.6f,%.6f%%7C%.6f,%.6f", lati[0], longi[0], lati[1], longi[1] );
                l=strlen(murl);
                murl+=l ;
            }
            if( lati[2]>1.0 || lati[2]<-1.0 || longi[2]>1.0 || longi[2]<-1.0 ) {
                sprintf( murl, "&path=color:0x0000c060%%7Cweight:4%%7C%.6f,%.6f%%7C%.6f,%.6f", lati[1], longi[1], lati[2], longi[2] );
                l=strlen(murl);
                murl+=l ;
                if( lati[3]>1.0 || lati[3]<-1.0 || longi[3]>1.0 || longi[3]<-1.0 ) {
                    sprintf( murl, "&path=color:0x0000c030%%7Cweight:3%%7C%.6f,%.6f%%7C%.6f,%.6f", lati[2], longi[2], lati[3], longi[3] );
                    l=strlen(murl);
                    murl+=l ;
                    if( lati[4]>1.0 || lati[4]<-1.0 || longi[4]>1.0 || longi[4]<-1.0 ) {
                        sprintf( murl, "&path=color:0x0000c010%%7Cweight:2%%7C%.6f,%.6f%%7C%.6f,%.6f", lati[3], longi[3], lati[4], longi[4] );
                        l=strlen(murl);
                        murl+=l ;
                    }
                }
            }
        }

        sprintf( murl, "&maptype=roadmap&sensor=false" );
        l=strlen(murl);
        murl+=l ;

        Image * nmap = loadbitmap( mapurl );
        if( nmap ) {
            if( nmap->GetHeight()>10 ) {
                if( mapimg ) {
                    delete mapimg ;
                }
                mapimg = nmap ;
                InvalidateRect(m_hWnd, NULL, FALSE );
            }
            else {
                delete nmap ;
            }
        }
    }

	virtual LRESULT OnPaint() {
        PAINTSTRUCT ps;
        HDC hdc;
        hdc = BeginPaint(m_hWnd, &ps);
        Graphics g(hdc);

        if( mapimg ) {
            RECT rd ;
            GetClientRect(m_hWnd, &rd);
            g.DrawImage( mapimg, 0, 0, rd.right,rd.bottom );
        }

        EndPaint(m_hWnd, &ps);
        return 0 ;
    }

    virtual LRESULT OnEraseBkgnd(HDC hdc){
        return 1;		// no need to erase background
    }

    virtual LRESULT OnLButtonDown(UINT nFlags, int x, int y ){
        return FALSE;
    }

    virtual LRESULT OnLButtonUp(UINT nFlags, int x, int y ){
        // http://maps.google.com/maps?q=37.670777,-122.437363&hl=en&t=m&z=10
        if( lati[0]>1.0 || lati[0]<-1.0 || longi[0]>1.0 || longi[0]<-1.0 ) {
    		ShellExecute(NULL, _T("open"), 
				TMPSTR("http://maps.google.com/maps?q=%.6f,%.6f&hl=en&t=m&z=%d", lati[0], longi[0], m_zoomlevel),
				NULL, NULL, SW_SHOWNORMAL );
        }
        return TRUE;
    }

    virtual LRESULT OnRButtonUp(UINT nFlags, int x, int y ){
        DestroyWindow(m_hWnd);
        return FALSE;
    }

    virtual LRESULT OnMouseMove(UINT nFlags,int x, int y ){
        return FALSE;
    }

    LRESULT OnMouseWheel( int wheel ) {
        m_wheeldir = wheel ;
        ::SetTimer(m_hWnd, TIMER_MOUSEWHEEL, 300, NULL);
        return FALSE ;
    }

    LRESULT OnTimer( int id ) {
        if( m_wheeldir>0 && m_zoomlevel<18 ) {
            m_zoomlevel++ ;
            newimg();
        }
        else if( m_wheeldir <0 && m_zoomlevel>5 ) {
            m_zoomlevel-- ;
            newimg();
        }
        m_wheeldir=0 ;
        return TRUE ;
    }

	virtual LRESULT WndProc(UINT message, WPARAM wParam, LPARAM lParam) {
		LRESULT res = FALSE ;
		switch (message)
		{
		case WM_PAINT:
			res = OnPaint();
			break;

		case WM_ERASEBKGND:
			res = OnEraseBkgnd((HDC)wParam);
			break;

		case WM_LBUTTONDOWN:
			res = OnLButtonDown(wParam, MOUSE_POSX(lParam), MOUSE_POSY(lParam) );
			break;

        case WM_LBUTTONUP:
			res = OnLButtonUp(wParam, MOUSE_POSX(lParam), MOUSE_POSY(lParam) );
			break;

        case WM_RBUTTONUP:
			res = OnRButtonUp(wParam, MOUSE_POSX(lParam), MOUSE_POSY(lParam) );
			break;

		case WM_MOUSEMOVE:
			res = OnMouseMove(wParam, MOUSE_POSX(lParam), MOUSE_POSY(lParam) );
			break;

#ifdef WM_MOUSEWHEEL     
		case WM_MOUSEWHEEL:
			res = OnMouseWheel((int)(signed short)HIWORD(wParam) );
			break;
#endif
        case WM_TIMER:
			res = OnTimer(wParam);
			break;

		default:
			res = DefWndProc(message, wParam, lParam);
		}
		return res ;
	}
    
};

