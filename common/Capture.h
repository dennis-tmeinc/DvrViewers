#pragma once

#include "../common/cstr.h"
#include "../common/cwin.h"
#include "../common/util.h"

// Capture dialog

class Capture : public Dialog
{
public:
	string m_picturename ;
	string m_filename ;
    Bitmap * m_img ;

    ~Capture(){
        if( m_img ) delete m_img ;
    }

protected:

	virtual void OnBnClickedButtonSavefile();
	virtual void OnBnClickedButtonPrint();
	virtual void OnPaint();
	virtual BOOL OnInitDialog();

    virtual int OnCommand( int Id, int Event ) {
        if( Id == IDC_BUTTON_SAVEFILE ) {
            OnBnClickedButtonSavefile();
            return TRUE ;
        }
        else if( Id == IDC_BUTTON_PRINT ) {
            OnBnClickedButtonPrint();
            return TRUE ;
        }
        return Dialog::OnCommand(Id,Event);
	}

	// dialog box procedures
	virtual int DlgProc(UINT message, WPARAM wParam, LPARAM lParam)
	{
		int res = FALSE ;
		switch (message)
		{
		case WM_INITDIALOG:
			res = OnInitDialog();
			break;

		case WM_PAINT:
			OnPaint();
            res = 1 ;
			break;

		case WM_COMMAND:
			res = OnCommand( LOWORD(wParam), HIWORD(wParam) );
			break;
		}
		return res;
	}

};
