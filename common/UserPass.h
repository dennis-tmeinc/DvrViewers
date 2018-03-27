#pragma once

#include "../common/cstr.h"
#include "../common/cwin.h"

// UserPass dialog

class UserPass : public Dialog
{
public:
    string m_username;
	string m_password;
	string m_servername;

protected:

    virtual int OnInitDialog()
	{
        SetDlgItemText(m_hWnd, IDC_STATIC_SERVER, m_servername );
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
