#pragma once

#include "../common/cwin.h"
#include "../common/cstr.h"

// CEnterDVRPassword dialog

class DlgEnterDVRPassword : public Dialog
{
public:
// Dialog Data
    string m_password1 ;
    string m_password2 ;

protected:
    int OnOK()
    {
        TCHAR pass[512] ;
        GetDlgItemText(m_hWnd, IDC_EDIT_PASSWORD1, pass, 512 );
        m_password1=pass ;
        GetDlgItemText(m_hWnd, IDC_EDIT_PASSWORD2, pass, 512 );
        m_password2=pass ;
        return EndDialog( IDOK );
    }
};
