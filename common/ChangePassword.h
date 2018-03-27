#pragma once

#include "../common/cstr.h"
#include "../common/cwin.h"

// ChangePassword dialog

class ChangePassword : public Dialog
{
public:
    string m_username;
	string m_oldpassword;
	string m_newpassword;
	string m_confirmpassword;

protected:

    virtual int OnInitDialog()
	{
        SetDlgItemText(m_hWnd, IDC_STATIC_USERNAME, m_username );
        SetDlgItemText(m_hWnd, IDC_EDIT_OLDPASSWORD, m_oldpassword );
		return TRUE;
	}

    virtual int OnOK()
    {
        TCHAR pass[512] ;
        GetDlgItemText(m_hWnd, IDC_EDIT_NEWPASSWORD, pass, 512 );
        m_newpassword = pass ;
        GetDlgItemText(m_hWnd, IDC_EDIT_CONFIIRMPASSWORD, pass, 512 );
        m_confirmpassword = pass ;
        GetDlgItemText(m_hWnd, IDC_EDIT_OLDPASSWORD, pass, 512 );
        m_oldpassword = pass ;
        return EndDialog(IDOK);
    }

    virtual void OnEnChangeEdit() {
        TCHAR confirmpassword[512], newpassword[512] ;
        GetDlgItemText(m_hWnd, IDC_EDIT_NEWPASSWORD, newpassword, 512 );
        GetDlgItemText(m_hWnd, IDC_EDIT_CONFIIRMPASSWORD, confirmpassword, 512 );
        if( _tcscmp(confirmpassword, newpassword)==0 ) {
            EnableWindow(GetDlgItem(m_hWnd, IDOK), 1 );
        }
        else {
            EnableWindow(GetDlgItem(m_hWnd, IDOK), 0 );
        }
	}

    virtual int OnCommand( int Id, int Event ) {
        if( ( Id == IDC_EDIT_NEWPASSWORD || Id == IDC_EDIT_CONFIIRMPASSWORD ) &&
            Event == EN_CHANGE ) 
        {
            OnEnChangeEdit();
            return TRUE ;
        }
		return Dialog::OnCommand( Id, Event ) ;
	}

};
