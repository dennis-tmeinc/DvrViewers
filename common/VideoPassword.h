#pragma once

#include "../common/cstr.h"
#include "../common/cwin.h"

class VideoPassword : public Dialog
{
public:
    string m_password ;
protected:
    virtual int OnOK()
    {
        TCHAR password[512] ;
        GetDlgItemText(m_hWnd, IDC_EDIT_PASSWORD, password, 512 );
        m_password = password ;
		return EndDialog(IDOK);
    }
};

