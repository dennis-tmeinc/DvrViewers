#pragma once

#include "resource.h"

#include "cstr.h"
#include "cwin.h"

// UserPass dialog

class VolumePass : public Dialog
{
public:
	string m_title;
	string m_message;
    string m_username;
	string m_password;

protected:

    virtual int OnInitDialog()
	{
		SetWindowText( m_hWnd, m_title );
        SetDlgItemText(m_hWnd, IDC_STATIC_MESSAGE, m_message );
		return TRUE;
	}

    virtual int OnOK()
    {
        TCHAR buf[512] ;
        GetDlgItemText(m_hWnd, IDC_EDIT_USERNAME, buf, 512 );
        m_username = buf ;
        GetDlgItemText(m_hWnd, IDC_EDIT_PASSWORD, buf, 512 );
        m_password = buf ;
        return EndDialog(IDOK);
    }

};
