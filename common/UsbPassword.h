#pragma once

#include "../common/cstr.h"
#include "../common/cwin.h"

#ifndef __USBPASSWORD_H__
#define __USBPASSWORD_H__

// Enter USB key password dialog       
class UsbPasswordDlg : public Dialog
{
public:
    int m_retry ;
	char * m_key;
    UsbPasswordDlg() {
        m_retry = 10 ;
    }
protected:

	virtual int OnOK()
    {
        unsigned char k256[256] ;

		string pass ;
		GetDlgItemText( m_hWnd, IDC_EDIT_PASSWORD, pass.tcssize(512), 500 );

		key_256( pass, k256 ) ;
        if( memcmp( k256, m_key, 256 )==0 ) {
            EndDialog(IDOK );
        }
        else {
            if( --m_retry>0 ) {
                if( MessageBox(m_hWnd, _T("Wrong password, please try again!"), NULL, MB_RETRYCANCEL)==IDRETRY ) {
					SetDlgItemText( m_hWnd, IDC_EDIT_PASSWORD, _T("") );
                    return TRUE ;
                }
            }
            EndDialog(IDCANCEL );
        }
		return TRUE ;
    }
	virtual int OnInitDialog()
	{
		HICON hIcon = LoadIcon(ResInst(), MAKEINTRESOURCE(IDR_MAINFRAME));
		::SendMessageA(m_hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
		::SendMessageA(m_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

		unsigned char k256[258] ;
		memset( k256, 0, sizeof(k256) ) ;
		if( memcmp( k256, m_key, 64 )==0 ) {
			EndDialog(IDOK );
		}

        key_256( "", k256 ) ;
		if( memcmp( k256, m_key, 256 )==0 ) {
			EndDialog(IDOK );
		}

		return TRUE ;
	}
};

#endif	// __USBPASSWORD_H__
