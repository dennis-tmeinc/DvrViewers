#pragma once

#include "../common/cwin.h"
#include "../common/cstr.h"
#include "decoder.h"

// Capture dialog (4 screen capture)
// 
class Capture4 : public Dialog
{
public:
	Capture4();
	~Capture4();

// Dialog Data
//	enum { IDD = IDD_DIALOG_CAPTURE };

	int     m_picturenumber ;
	struct tvs_info * ptvs_info ;
	string m_picturename[4] ;
	string m_cameraname[4] ;
	struct dvrtime m_capturetime ;
	string  m_filename ;

protected:
	virtual void OnBnClickedButtonSavefile();
	virtual void OnBnClickedButtonPrint();
	virtual void OnBnClickedCheckCam1();
	virtual void OnBnClickedCheckCam2();
	virtual void OnBnClickedCheckCam3();
	virtual void OnBnClickedCheckCam4();
	virtual void OnPaint();
	virtual int OnInitDialog();

	Bitmap  * m_img ;
	void	MakeImage();
	int		GetCheckedCameras();

	virtual int OnCommand( int Id, int Event ) {
		switch(Id) {
		case IDC_BUTTON_SAVEFILE: OnBnClickedButtonSavefile(); break;
		case IDC_BUTTON_PRINT: OnBnClickedButtonPrint(); break;
		case IDC_CHECK_CAM1: OnBnClickedCheckCam1(); break;
		case IDC_CHECK_CAM2: OnBnClickedCheckCam2(); break;
		case IDC_CHECK_CAM3: OnBnClickedCheckCam3(); break;
		case IDC_CHECK_CAM4: OnBnClickedCheckCam4(); break;
		default:
			return Dialog::OnCommand(Id, Event);
		}
		return TRUE ;
	}

	virtual int DlgProc(UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
		case WM_PAINT:
			OnPaint();
			break;
		default:
			return Dialog::DlgProc(message, wParam, lParam);
		}
		return TRUE;
	}

};


