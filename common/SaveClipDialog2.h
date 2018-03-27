#pragma once

#include "../common/cstr.h"
#include "../common/cwin.h"

#include "resource.h"
#include "dvrclientDlg.h"

#ifdef IDD_DIALOG_MAKECLIP2

// Select Camera dialog
class SaveClipDialog2 : public Dialog
{
public:
	static string m_clippath ;
	string m_clipfilename;
	int    m_saveplayer;
	decoder * m_dec;

	int m_encrypted ;
	string m_channels;
	string m_password;
	
	SaveClipDialog2(decoder * dec) {
		m_dec = dec;
		m_saveplayer = 0;
		m_encrypted = 0;
	}

protected:

	virtual int OnInitDialog()
    {
		int ch;
		SetDlgItemText(m_hWnd, IDC_EDIT_CLIPFILENAME, m_clipfilename );
		m_saveplayer = 0 ;
		m_encrypted = 0;

		int totalch = m_dec->getchannel();
		if (totalch > 128) totalch = 128;

		for (ch = 0; ch < totalch; ch++ ) {
			channel_info ci;
			ci.size = sizeof(ci);
			ci.features = 0;
			ci.cameraname[0] = 0;
			if (m_dec->getchannelinfo(ch, &ci) >= 0) {
				if ((ci.features & 1) != 0) {
					if (ci.cameraname[0] == 0) {
						sprintf(ci.cameraname, "camera%d", ch + 1);
					}
					string cameraname;
					cameraname.printf("%s - %s", m_dec->getservername(), ci.cameraname);
					int idx = SendMessage(GetDlgItem(m_hWnd, IDC_LIST_CAMERALIST), LB_ADDSTRING, 0, (LPARAM)(LPCTSTR)cameraname);
					SendMessage(GetDlgItem(m_hWnd, IDC_LIST_CAMERALIST), LB_SETITEMDATA, idx, (LPARAM)ch);
				}
			}
			else {
				break;
			}
		}
		SendMessage(GetDlgItem(m_hWnd, IDC_LIST_CAMERALIST), LB_SETSEL, TRUE, (LPARAM)-1);

        return TRUE;
    }

    virtual int OnOK()
    {
		GetDlgItemText(m_hWnd, IDC_EDIT_CLIPFILENAME, m_clipfilename.tcssize(512), 510 ) ;
		m_saveplayer = IsDlgButtonChecked(m_hWnd, IDC_CHECK_CREATEPLAYER) == BST_CHECKED ;

		// get selected channels
		int i;
		int sels = 0;
		int icount = SendMessage(GetDlgItem(m_hWnd, IDC_LIST_CAMERALIST), LB_GETCOUNT, 0, 0);
		for (i = 0; i < icount; i++) {
			if (SendMessage(GetDlgItem(m_hWnd, IDC_LIST_CAMERALIST), LB_GETSEL, i, 0) > 0) {
				int ch = (int)SendMessage(GetDlgItem(m_hWnd, IDC_LIST_CAMERALIST), LB_GETITEMDATA, i, 0);
				if (sels == 0) {
					m_channels.printf("%d", ch);
				}
				else {
					m_channels += TMPSTR(",%d", ch);
				}
				sels++;
			}
		}
		if (sels == 0) {
			MessageBox(m_hWnd, _T("At least one camera should be selected!"), _T("Error"), MB_OK);
			return 1;
		}

		// get password
		m_encrypted = (IsDlgButtonChecked(m_hWnd, IDC_CHECK_ENABLE_ENC) == BST_CHECKED );
		if (m_encrypted) {
			string pass1;
			string pass2;
			GetDlgItemText(m_hWnd, IDC_EDIT_PASSWORD1, pass1.tcssize(520), 512);
			GetDlgItemText(m_hWnd, IDC_EDIT_PASSWORD2, pass2.tcssize(520), 512);
			if (pass1 == pass2) {
				m_password = pass1;
			}
			else {
				MessageBox(m_hWnd, _T("Passwords not match!"), _T("Error"), MB_OK);
				return 1;
			}
		}

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