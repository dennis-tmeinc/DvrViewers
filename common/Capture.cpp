// Capture.cpp : implementation file
//

#include <stdafx.h>

#include <Windows.h>
#include <commdlg.h>

#include "../common/cwin.h"
#include "dvrclient.h"
#include "Capture.h"

// -------------
// Capture Dialog Event Handler

BOOL Capture::OnInitDialog()
{
    m_img = Bitmap::FromFile((LPCTSTR)m_picturename) ;
    return TRUE;  // return TRUE unless you set the focus to a control
}

void Capture::OnPaint()
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(m_hWnd, &ps);
    Graphics g(hdc);

	RECT rect ;
	if( m_img ) {
        GetClientRect(GetDlgItem(m_hWnd, IDC_STATIC_PIC), &rect);
        MapWindowPoints( GetDlgItem(m_hWnd, IDC_STATIC_PIC), m_hWnd, (LPPOINT)&rect, 2 );

		int cx, cy , draw_h, draw_w;
		cx = (rect.right + rect.left) / 2;
		cy = (rect.bottom + rect.top) / 2;
		draw_w = (rect.right - rect.left);

		int w, h;
		w = m_img->GetWidth();
		h = m_img->GetHeight();
		if (h<700) {
			h = w * 3 / 4;
		}

		draw_h = draw_w * h / w;
		if (draw_h > (rect.bottom - rect.top)) {
			draw_h = (rect.bottom - rect.top);
			draw_w = draw_h * w / h;
		}
		g.DrawImage(m_img, Rect(cx - draw_w / 2, cy - draw_h / 2, draw_w, draw_h));
        //g.DrawImage(m_img, Rect(rect.left, rect.top, (rect.right-rect.left), (rect.bottom-rect.top)));
	}

    EndPaint(m_hWnd, &ps);
}


void Capture::OnBnClickedButtonSavefile()
{
    if( m_img ){

        string  filename ;
        OPENFILENAME ofn ;
        memset(&ofn, 0, sizeof(ofn));
        ofn.lStructSize = sizeof (OPENFILENAME) ;
        ofn.nMaxFile = 512 ;
        ofn.lpstrFile = filename.tcssize(ofn.nMaxFile) ;
        _tcscpy( ofn.lpstrFile, m_filename );
        ofn.lpstrFilter=_T("JPG files (*.jpg)\0*.JPG\0BMP files (*.bmp)\0*.BMP\0PNG files (*.png)\0*.PNG\0TIFF files (*.tif)\0*.TIF\0All files\0*.*\0\0");
        ofn.Flags=0;
        ofn.nFilterIndex=1;
        ofn.hInstance = ResInst();
        ofn.lpstrDefExt=_T("JPG");
        ofn.hwndOwner = m_hWnd ;
        ofn.Flags= OFN_HIDEREADONLY |OFN_OVERWRITEPROMPT ;
        if( GetSaveFileName(&ofn) ) {
#ifdef BIGSNAPSHOT		
			int w, h ;
			w = m_img->GetWidth();
			h = m_img->GetHeight();
			if( h<600 ) {
				w=800 ;
				h=600 ;
			}
            Bitmap bigsnapshot(w,h);
            Graphics g(&bigsnapshot );
            g.DrawImage(m_img, Rect(0, 0, w, h), 0, 0, m_img->GetWidth(), m_img->GetHeight(), UnitPixel);
			savebitmap( filename, &bigsnapshot );
#else
            savebitmap( filename, m_img );
#endif
        }
    }
}

void Capture::OnBnClickedButtonPrint()
{
   if( m_img==NULL ) 
	   return ;

   DOCINFO docInfo;
   ZeroMemory(&docInfo, sizeof(docInfo));
   docInfo.cbSize = sizeof(docInfo);
   docInfo.lpszDocName = _T("CAPTURED PICTURE");
   
   // Create a PRINTDLG structure, and initialize the appropriate fields.
   PRINTDLG printDlg;
   ZeroMemory(&printDlg, sizeof(printDlg));
   printDlg.lStructSize = sizeof(printDlg);
   printDlg.Flags = PD_RETURNDC;
   printDlg.hwndOwner=this->m_hWnd;

   // Display a print dialog box.
   if(PrintDlg(&printDlg))
   {
      StartDoc(printDlg.hDC, &docInfo);
      StartPage(printDlg.hDC);
      Graphics* graphics = new Graphics(printDlg.hDC);

	  int w, h;
	  w = m_img->GetWidth();
	  h = m_img->GetHeight();
	  if (h<600) {
		  w = 800;
		  h = 600;
	  }

      int hsize = GetDeviceCaps(printDlg.hDC, HORZRES);  // get hsize
	  int vsize = GetDeviceCaps(printDlg.hDC, VERTRES);  // get vsize
      int dpix = (int)graphics->GetDpiX();
      int dpiy = (int)graphics->GetDpiY();
	  int sizex = hsize*6/8 ;
	  int sizey = sizex*h/w ;
      int left =  (hsize-sizex)/2;
	  int top =  2*dpiy ;

      graphics->SetPageUnit(UnitPixel);

      Font        font(printDlg.hDC);
      PointF      point(left, top-dpiy);
      SolidBrush  solidBrush(Color(255, 0, 0, 0));
      string title ;
      title.printf("%s captured image", APPNAME);
      graphics->DrawString(title, -1, &font, point, &solidBrush);
      graphics->DrawImage(m_img, Rect(left, top, sizex, sizey));
            
      delete graphics;
      EndPage(printDlg.hDC);
      EndDoc(printDlg.hDC); 
   }
   else {
   	   MessageBox(m_hWnd, _T("Fail to open printer!\n"), NULL, MB_OK );
   }
   if(printDlg.hDevMode) 
      GlobalFree(printDlg.hDevMode);
   if(printDlg.hDevNames) 
      GlobalFree(printDlg.hDevNames);
   if(printDlg.hDC)
      DeleteDC(printDlg.hDC);

}
