#pragma once

#include "../common/cstr.h"
#include "../common/cwin.h"

// New User dialog

class UserNew : public Dialog
{
public:
    string m_username;
    string m_password;
    string m_password2;
    BOOL m_usertype;

protected:

    virtual int OnInitDialog()
	{
        CheckRadioButton( m_hWnd, IDC_RADIO_USER, IDC_RADIO_ADMIN, IDC_RADIO_USER );
		return TRUE;
	}

    virtual int OnOK()
    {
        TCHAR buf[512] ;
        GetDlgItemText(m_hWnd, IDC_EDIT_USERNAME, buf, 512 );
        m_username = buf ;
        GetDlgItemText(m_hWnd, IDC_EDIT_PASSWORD, buf, 512 );
        m_password = buf ;        
        GetDlgItemText(m_hWnd, IDC_EDIT_PASSWORD2, buf, 512 );
        m_password2 = buf ;
        m_usertype = IsDlgButtonChecked(m_hWnd, IDC_RADIO_ADMIN ) ;
        return EndDialog(IDOK);
    }

    virtual void OnEnChangeEdit() {
        TCHAR buf[512] ;
        GetDlgItemText(m_hWnd, IDC_EDIT_USERNAME, buf, 512 );
        m_username = buf ;
        GetDlgItemText(m_hWnd, IDC_EDIT_PASSWORD, buf, 512 );
        m_password = buf ;        
        GetDlgItemText(m_hWnd, IDC_EDIT_PASSWORD2, buf, 512 );
        m_password2 = buf ;
        if( strlen( m_username )==0 || 
            ( strcmp( m_password, m_password2 )!=0 ) )
        {
                EnableWindow( GetDlgItem(m_hWnd, IDOK), 0 );
        }
        else {
                EnableWindow( GetDlgItem(m_hWnd, IDOK), 1 );
        }
    }

    virtual int OnCommand( int Id, int Event ) {
        if( ( Id == IDC_EDIT_USERNAME || Id == IDC_EDIT_PASSWORD || Id == IDC_EDIT_PASSWORD2 ) &&
            Event == EN_CHANGE ) 
        {
            OnEnChangeEdit();
            return TRUE ;
        }
		return Dialog::OnCommand( Id, Event ) ;
	}

};

