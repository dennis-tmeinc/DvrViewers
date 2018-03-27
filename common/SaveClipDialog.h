#pragma once

#include "../common/cstr.h"
#include "../common/cwin.h"

#include "resource.h"
#include "dvrclientDlg.h"

#ifdef IDD_DIALOG_MAKECLIP

// Select Camera dialog
class SaveClipDialog : public Dialog
{
public:
    string m_clipfilename ;
	int    m_saveplayer ;
	static string m_clippath ;


protected:
	virtual int OnInitDialog()
    {
		SetDlgItemText(m_hWnd, IDC_EDIT_CLIPFILENAME, m_clipfilename );
		m_saveplayer = 0 ;
        return TRUE;
    }

    virtual int OnOK()
    {
		GetDlgItemText(m_hWnd, IDC_EDIT_CLIPFILENAME, m_clipfilename.tcssize(512), 510 ) ;
		m_saveplayer = IsDlgButtonChecked(m_hWnd, IDC_CHECK_CREATEPLAYER) == BST_CHECKED ;
        return EndDialog(IDOK);
	}

	int OnBrowseFileName() {

		string  filename ;
        OPENFILENAME ofn ;
        memset(&ofn, 0, sizeof(ofn));
        ofn.lStructSize = sizeof (OPENFILENAME) ;
        ofn.nMaxFile = 512 ;
        ofn.lpstrFile = filename.tcssize(ofn.nMaxFile) ;
        // set initialized file name (dvrname_date_time_lengh)
		GetDlgItemText(m_hWnd, IDC_EDIT_CLIPFILENAME, ofn.lpstrFile, 500 ) ;
        ofn.lpstrFilter=_T("DVR Files (*.DVR)\0*.DVR\0AVI Files (*.AVI)\0*.AVI\0MP4 Files (*.MP4)\0*.MP4\0All files\0*.*\0\0") ;  // all .avi support
        ofn.Flags=0;
        ofn.nFilterIndex=1;
        ofn.hInstance = AppInst();
        ofn.hwndOwner = m_hWnd ;
        ofn.lpstrDefExt=_T("DVR");
        ofn.Flags=OFN_OVERWRITEPROMPT ;
        if( GetSaveFileName(&ofn) ) {
			m_clipfilename = ofn.lpstrFile ;
			SetDlgItemText( m_hWnd, IDC_EDIT_CLIPFILENAME, m_clipfilename );
		}
		return TRUE ;
	}

    virtual int OnCommand( int Id, int Event ) 
	{
        switch (Id)
        {
        case IDOK:
			return OnOK();

        case IDCANCEL:
            return OnCancel();

		case IDC_BUTTON_BROWSEFILENAME:
			return OnBrowseFileName();
        }
        return FALSE;
    }

};

#endif