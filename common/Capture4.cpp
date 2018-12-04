// Capture.cpp : implementation file
//

#include "stdafx.h"
#include <CommDlg.h>
#include "dvrclient.h"
#include "../common/util.h"
#include "Capture4.h"

#define TVS_SNAPSHOT2
// #define TVS_SNAPSHOT

// Capture4 dialog

extern tvskey_data * tvskeydata ;
Capture4::Capture4()
{
	m_img=NULL ;
}

Capture4::~Capture4()
{
	if( m_img ) {
		delete m_img ;
	}
}

int Capture4::OnInitDialog()
{

#ifdef TVS_SNAPSHOT

	RECT rect ;
	HDC snapshotdc=0;
	CImage snapshot;
	snapshot.Load((LPCTSTR)m_picturename[0]);

	m_img.Create(800, 800, 32 );
	snapshotdc = m_img.GetDC();
	if( snapshotdc ) {
		HFONT hFont, hOldFont; 

		// Retrieve a handle to the variable stock font.  

		hFont = CreateFont(30,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY, DEFAULT_PITCH ,TEXT("Tahoma"));
		if( hFont == NULL ) {
			hFont = (HFONT)GetStockObject(SYSTEM_FONT); 
		}

		// Select the variable stock font into the specified device context. 
		hOldFont = (HFONT)SelectObject(snapshotdc, hFont) ;
	    // Display the text string.  


		rect.left = 0 ;
		rect.right = m_img.GetWidth() ;
		rect.top = 0 ;
		rect.bottom = m_img.GetHeight() ;
		::FillRect(snapshotdc, &rect, (HBRUSH) (COLOR_WINDOW+1));
		
		SetStretchBltMode(snapshotdc, HALFTONE);
		snapshot.Draw( snapshotdc, 0, 0, 800, 600 );

		TCHAR txtbuf[256] ;
		int  y, yinc ;
		int  l ;
		y = 610 ;
		yinc = 30 ;

		l = wsprintf(txtbuf, _T("Date/Time : %02d/%02d/%04d %02d:%02d:%02d"),
			m_capturetime.month, 
			m_capturetime.day, 
			m_capturetime.year, 
			m_capturetime.hour, 
			m_capturetime.min, 
			m_capturetime.second ) ; 
		::TextOut( snapshotdc, 50, y, txtbuf, l );
		y+=yinc ;

		if( strcmp( ptvs_info->productid, "TVS" )==0 ) {
			l = wsprintf(txtbuf, _T("Medallion / VIN # : %hs"), ptvs_info->medallion );
			::TextOut( snapshotdc, 50, y, txtbuf, l );
			y+=yinc ;

			l = wsprintf(txtbuf, _T("License # : %hs"), ptvs_info->lincenseplate );
			::TextOut( snapshotdc, 50, y, txtbuf, l );
			y+=yinc ;

			l = wsprintf(txtbuf, _T("Controller serial # : %hs"), ptvs_info->ivcs );
			::TextOut( snapshotdc, 50, y, txtbuf, l );
			y+=yinc ;
		}

		l = wsprintf(txtbuf, _T("Camera serial # : %s"), (LPCTSTR)(m_cameraname) );
		::TextOut( snapshotdc, 50, y, txtbuf, l );
		y+=yinc ;

		// check installer shop name
		char * shopinfo ;
		char * shopname ;
		shopinfo = (char *) &(tvskeydata->size) ;
		shopinfo += tvskeydata->keyinfo_start ;
		shopname = strstr( shopinfo, "Shop Name" );
		if( shopname ) {
			char shopnameline[128] ;
			int ii ;
			// extract shop name from key file
			for(ii = 0 ; ii<127; ii++ ) {
				if( shopname[ii] == '\r' || shopname[ii] == '\n' || shopname[ii]==0) {
					break;
				}
				else {
					shopnameline[ii] = shopname[ii] ;
				}
			}
			shopnameline[ii]=0 ;
			l = wsprintf(txtbuf, _T("%hs"), shopnameline );
			::TextOut( snapshotdc, 50, y, txtbuf, l );
			y+=yinc ;
		}

		SelectObject(snapshotdc, hOldFont); 
		DeleteObject( hFont );
		m_img.ReleaseDC();
	}

#endif	// TVS_SNAPSHOT

#ifdef TVS_SNAPSHOT2

	HDC snapshotdc=0;

	if( this->m_picturenumber > 1 ) {
		ShowWindow(GetDlgItem(m_hWnd,IDC_STATIC_SELCAM),SW_SHOW);
		ShowWindow(GetDlgItem(m_hWnd, IDC_CHECK_CAM1),SW_SHOW);
		ShowWindow(GetDlgItem(m_hWnd, IDC_CHECK_CAM2),SW_SHOW);
		if (m_picturenumber > 2) {
			ShowWindow(GetDlgItem(m_hWnd, IDC_CHECK_CAM3), SW_SHOW);
			if (m_picturenumber > 3) {
				ShowWindow(GetDlgItem(m_hWnd, IDC_CHECK_CAM4), SW_SHOW);
			}
		}
	}

	if( strlen(this->m_picturename[0])>2 ) {
		CheckDlgButton(m_hWnd, IDC_CHECK_CAM1, BST_CHECKED );
	}
	if( strlen(this->m_picturename[1])>2 ) {
		CheckDlgButton(m_hWnd, IDC_CHECK_CAM2, BST_CHECKED );
	}
	if( strlen(this->m_picturename[2])>2 ) {
		CheckDlgButton(m_hWnd, IDC_CHECK_CAM3, BST_CHECKED );
	}
	if( strlen(this->m_picturename[3])>2 ) {
		CheckDlgButton(m_hWnd, IDC_CHECK_CAM4, BST_CHECKED );
	}

	MakeImage();

#endif	// TVS_SNAPSHOT2

#ifdef NOT_TVS
	m_img.Load((LPCTSTR)m_picturename[0]);
#endif // NOT_TV

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void	Capture4::MakeImage()
{
#ifdef TVS_SNAPSHOT2

	RECT rect ;
	int pic1, pic2, pic3, pic4 ;
	int nPictures = 0;
	int pictures[4];
	int i, camIdx;
	int xpos ;
	const int maxcam = 4;

	for (i = 0; i < maxcam; i++) {
		pictures[i] = 0; // 0 means not available
	}

	pic1 = (IsDlgButtonChecked(m_hWnd, IDC_CHECK_CAM1)==BST_CHECKED);
	if (pic1) {
		pictures[nPictures++] = 1;
	}
	pic2 = (IsDlgButtonChecked(m_hWnd, IDC_CHECK_CAM2)==BST_CHECKED);
	if (pic2) {
		pictures[nPictures++] = 2;
	}
	pic3 = (IsDlgButtonChecked(m_hWnd, IDC_CHECK_CAM3)==BST_CHECKED);
	if (pic3) {
		pictures[nPictures++] = 3;
	}
	pic4 = (IsDlgButtonChecked(m_hWnd, IDC_CHECK_CAM4)==BST_CHECKED);
	if (pic4) {
		pictures[nPictures++] = 4;
	}

	if( m_img ) {
		delete m_img ;
		m_img = NULL ;
	}

	m_img = new Bitmap(1408, 1006);
	Graphics * g=new Graphics(m_img);
	g->Clear(Color(255,255,255,255));

	FontFamily  fontFamily(_T("Tahoma"));
	Font        font(&fontFamily, 30, FontStyleRegular, UnitPixel);
	SolidBrush  solidBrush(Color(255, 0, 0, 0));

	rect.left = 0 ;
	rect.right = m_img->GetWidth() ;
	rect.top = 0 ;
	rect.bottom = m_img->GetHeight() ;

	int h ;
	int w ;
	int draw_h;
	int draw_w;
	int cx = 704;
	int cy = 0 ;

	Bitmap * img ;
	if (nPictures == 1) {
		camIdx = pictures[0] - 1; /* get the camera index number to draw */
		img = Bitmap::FromFile((LPCTSTR)m_picturename[camIdx]);
		if( img ) {
			cx = 704;					// center x
			h = img->GetHeight();
			w = img->GetWidth();
			draw_h = 528 + 264;
			if (h >= 700) {		// use original picture ratial
				draw_w = draw_h * w / h;
			}
			else {
				draw_w = draw_h * 4 / 3;
			}

			// g->DrawImage(img, 352-176, 0, 704+352, 528+264 );
			g->DrawImage(img, cx - draw_w/2, cy, draw_w, draw_h);
			delete img ;
		}
	} else if (nPictures == 2) {
		draw_w = 704;			// half page 
		draw_h = 704 * 3 / 4;	// defalut 4x3 ratial

		camIdx = pictures[0] - 1; /* get the 1st camera index number to draw */
		img = Bitmap::FromFile((LPCTSTR)m_picturename[camIdx]);
		if( img ) {
			h = img->GetHeight();
			w = img->GetWidth();
			if (h >= 700) {
				draw_h = draw_w * h / w;
			}
			else {
				draw_h = draw_w * 3 / 4;
			}
			g->DrawImage(img, cx-draw_w, 0, draw_w, draw_h );
			delete img ;
		}

		camIdx = pictures[1] - 1; /* get the 2nd camera index number to draw */
		img = Bitmap::FromFile((LPCTSTR)m_picturename[camIdx]);
		if( img ) {
			h = img->GetHeight();
			w = img->GetWidth();
			if (h >= 700) {
				draw_h = draw_w * h / w;
			}
			else {
				draw_h = draw_w * 3 / 4;
			}
			g->DrawImage(img, cx, 0, draw_w, draw_h);
			delete img ;
		}

	} else if (nPictures >= 3) {
		draw_h = 432;				// single picture height
		for (i = 0; i < nPictures; i++) {
			if (pictures[i] == 1) {
				camIdx = pictures[i] - 1; /* get the 1st camera index number to draw */
				img = Bitmap::FromFile((LPCTSTR)m_picturename[camIdx]);
				if( img ) {
					h = img->GetHeight();
					w = img->GetWidth();
					if (h >= 700) {
						draw_w = draw_h * w / h;
						if (draw_w > cx) {
							draw_w = cx;
							draw_h = draw_w * h / w;
						}
					}
					else {
						draw_w = draw_h * 4 / 3;
					}
					g->DrawImage(img, cx-draw_w, 0, draw_w, draw_h );
					if (draw_h > cy)
						cy = draw_h;
					delete img ;
				}
			} else if (pictures[i] == 2) {
				camIdx = pictures[i] - 1; /* get the 2nd camera index number to draw */
				img = Bitmap::FromFile((LPCTSTR)m_picturename[camIdx]);
				if( img ) {
					h = img->GetHeight();
					w = img->GetWidth();
					if (h >= 700) {
						draw_w = draw_h * w / h;
						if (draw_w > cx) {
							draw_w = cx;
							draw_h = draw_w * h / w;
						}
					}
					else {
						draw_w = draw_h * 4 / 3;
					}
					g->DrawImage(img, cx, 0, draw_w, draw_h);
					if( draw_h > cy )
						cy = draw_h;
					delete img ;
				}
			} else if (pictures[i] == 3) {		
				camIdx = pictures[i] - 1; /* get the 3rd camera index number to draw */
				img = Bitmap::FromFile((LPCTSTR)m_picturename[camIdx]);
				if( img ) {
					h = img->GetHeight();
					w = img->GetWidth();
					if (h >= 700) {
						draw_w = draw_h * w / h;
						if (draw_w > cx) {
							draw_w = cx;
							draw_h = draw_w * h / w;
						}
					}
					else {
						draw_w = draw_h * 4 / 3;
					}
					g->DrawImage(img, cx-draw_w, cy, draw_w, draw_h);
					delete img ;
				}
			} else if (pictures[i] == 4) {		
				camIdx = pictures[i] - 1; /* get the 4th camera index number to draw */
				img = Bitmap::FromFile((LPCTSTR)m_picturename[camIdx]);
				if( img ) {
					h = img->GetHeight();
					w = img->GetWidth();
					if (h >= 700) {
						draw_w = draw_h * w / h;
						if (draw_w > cx) {
							draw_w = cx;
							draw_h = draw_w * h / w;
						}
					}
					else {
						draw_w = draw_h * 4 / 3;
					}
					g->DrawImage(img, cx, cy, draw_w, draw_h);
					delete img ;
				}
			}
		}
	}

	string txtbuf ;
	int  y, yinc ;
	int  l ;

	if (nPictures == 1) {
		xpos = 380 ;
		y = 810 ;
	} else {
		xpos = 140 ;
		if (nPictures == 2) {
			y = 620 ;
		} else {
			y = 880 ;
		}
	}

	yinc = 28 ;
	yinc = 28 ;

	l = sprintf(txtbuf.strsize(256), "Date/Time : %02d/%02d/%04d %02d:%02d:%02d",
		m_capturetime.month, 
		m_capturetime.day, 
		m_capturetime.year, 
		m_capturetime.hour, 
		m_capturetime.min, 
		m_capturetime.second ) ; 
	g->DrawString(txtbuf, -1, &font, PointF(xpos, y), &solidBrush);

	if( strcmp( ptvs_info->productid, "TVS" )==0 ) {
		l = sprintf(txtbuf.strsize(200), "Medallion / VIN # : %s", ptvs_info->medallion );
		g->DrawString(txtbuf, -1, &font, PointF(xpos+500, y), &solidBrush);
		y+=yinc ;

		l = sprintf(txtbuf.strsize(200), "License # : %s", ptvs_info->lincenseplate );
		g->DrawString(txtbuf, -1, &font, PointF(xpos, y), &solidBrush);
		//y+=yinc ;

		l = sprintf(txtbuf.strsize(200), "Controller serial # : %s", ptvs_info->ivcs );
		g->DrawString(txtbuf, -1, &font, PointF(xpos+500, y), &solidBrush);
	}
	y+=yinc ;

	bool linefeed;
	for (i = 0; i < nPictures; i++) {
		linefeed = false;
		camIdx = pictures[i] - 1; /* get the camera index number to draw */
		l = sprintf(txtbuf.strsize(200), "Camera %d serial # : %s", pictures[i], (LPCSTR)(m_cameraname[camIdx]) );
		g->DrawString(txtbuf, -1, &font, PointF(xpos + ((i % 2) ? 500 : 0), y), &solidBrush);
		if (i % 2) {
			linefeed = true;
			y+=yinc ;
		}
	}

	if (!linefeed) y+=yinc ;

	// check installer shop name
	char * shopinfo ;
	char * shopname ;
	shopinfo = (char *) &(tvskeydata->size) ;
	shopinfo += tvskeydata->keyinfo_start ;
	shopname = strstr( shopinfo, "Shop Name" );
	if( shopname ) {
		char shopnameline[128] ;
		int ii ;
		// extract shop name from key file
		for(ii = 0 ; ii<127; ii++ ) {
			if( shopname[ii] == '\r' || shopname[ii] == '\n' || shopname[ii]==0) {
				break;
			}
			else {
				shopnameline[ii] = shopname[ii] ;
			}
		}
		shopnameline[ii]=0 ;
		txtbuf=shopnameline ;
		g->DrawString(txtbuf, -1, &font, PointF(xpos, y), &solidBrush);
		y+=yinc ;
	}


	delete g ;

#endif	// TVS_SNAPSHOT2

	InvalidateRect(m_hWnd, NULL, FALSE);
}


// Capture4 message handlers

void Capture4::OnBnClickedButtonSavefile()
{
	if( m_img ){
		string  filename ;
		OPENFILENAME ofn ;
		memset(&ofn, 0, sizeof(ofn));
		ofn.lStructSize = sizeof (OPENFILENAME) ;
		ofn.nMaxFile = 512 ;
		ofn.lpstrFile = filename.tcssize(ofn.nMaxFile) ;
        _tcscpy( ofn.lpstrFile, m_filename);
		ofn.lpstrFilter=_T("JPG files (*.jpg)\0*.JPG\0BMP files (*.bmp)\0*.BMP\0PNG files (*.png)\0*.PNG\0All files\0*.*\0\0");
		ofn.Flags=0;
		ofn.nFilterIndex=1;
		ofn.hInstance = ResInst();
		ofn.lpstrDefExt=_T("JPG");
		ofn.hwndOwner = m_hWnd ;
		ofn.Flags= OFN_HIDEREADONLY |OFN_OVERWRITEPROMPT ;
		if( GetSaveFileName(&ofn) ) {
			savebitmap( filename, m_img );
		}
	}
}

void Capture4::OnBnClickedButtonPrint()
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


		int hsize = GetDeviceCaps(printDlg.hDC, HORZRES);  // get hsize
		int vsize = GetDeviceCaps(printDlg.hDC, VERTRES);  // get vsize
		int dpix = (int)graphics->GetDpiX();
		int dpiy = (int)graphics->GetDpiY();
		int sizex = hsize*5/8 ;
		int sizey = sizex*3/4 ;
		int left =  (hsize-sizex)/2;
		int top =  2*dpiy ;
		graphics->SetPageUnit(Unit::UnitPixel);

		Gdiplus::Font        font(printDlg.hDC);
		PointF      point((float)left, (float)(top-dpiy));
		SolidBrush  solidBrush(Color(255, 0, 0, 0));
		string title ;
		sprintf(title.strsize(80), "%s captured image", APPNAME);
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

void Capture4::OnPaint()
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(m_hWnd, &ps);
	Graphics g(hdc);

	RECT rect;
	if( m_img ) {
		GetClientRect(GetDlgItem(m_hWnd, IDC_STATIC_PIC), &rect);
		MapWindowPoints( GetDlgItem(m_hWnd, IDC_STATIC_PIC), m_hWnd, (LPPOINT)&rect, 2 );
		g.DrawImage(m_img, Rect(rect.left, rect.top, (rect.right-rect.left), (rect.bottom-rect.top)));
	}

	EndPaint(m_hWnd, &ps);
}

int Capture4::GetCheckedCameras()
{
	int nCam = 0;

	if( IsDlgButtonChecked(m_hWnd, IDC_CHECK_CAM1)==BST_CHECKED) {
		nCam++;
	}
	if( IsDlgButtonChecked(m_hWnd, IDC_CHECK_CAM2)==BST_CHECKED) {
		nCam++;
	}
	if( IsDlgButtonChecked(m_hWnd, IDC_CHECK_CAM3)==BST_CHECKED) {
		nCam++;
	}
	if( IsDlgButtonChecked(m_hWnd, IDC_CHECK_CAM4)==BST_CHECKED) {
		nCam++;
	}
	return nCam;
}

void Capture4::OnBnClickedCheckCam1()
{
	if( GetCheckedCameras() == 0) 
	{
		CheckDlgButton(m_hWnd, IDC_CHECK_CAM2, BST_CHECKED );
	}
	MakeImage();
}

void Capture4::OnBnClickedCheckCam2()
{
	if( GetCheckedCameras() == 0) 
	{
		CheckDlgButton(m_hWnd, IDC_CHECK_CAM1, BST_CHECKED );
	}
	MakeImage();
}

void Capture4::OnBnClickedCheckCam3()
{
	if( GetCheckedCameras() == 0) 
	{
		CheckDlgButton(m_hWnd, IDC_CHECK_CAM1, BST_CHECKED );
	}
	MakeImage();
}

void Capture4::OnBnClickedCheckCam4()
{
	if( GetCheckedCameras() == 0) 
	{
		CheckDlgButton(m_hWnd, IDC_CHECK_CAM1, BST_CHECKED );
	}
	MakeImage();
}
