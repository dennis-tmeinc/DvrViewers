#pragma once

#include "../common/cstr.h"
#include "../common/cwin.h"

#include "CallTmeDlg.h"

int user_checkpassword( char * username, char * password );
int user_adduser( char * username, char * password, int newusertype );
int user_deleteuser( char * username ) ;
int user_getuser(int index,  string & listuser);
void user_Getcode( char * password, char * code );

// Login dialog

class LoginDlg : public Dialog
{
public:
	enum { IDD = IDD_DIALOG_LOGIN };

	int m_usertype ;
    int m_retry ;

	LoginDlg( HWND hParent) {
		CreateDlg(IDD_DIALOG_LOGIN, hParent);
	}

protected:
    virtual int OnInitDialog();
    virtual int OnOK();
    virtual void OnCbnEditchangeUsername() ;
    virtual int OnCommand( int Id, int Event ) 
    {
        if( Id == IDC_USERNAME && Event == CBN_EDITCHANGE ) 
        {
            OnCbnEditchangeUsername();
            return TRUE ;
        }
        return Dialog::OnCommand( Id, Event ) ;
    }
};

// User dialog
class UserDlg : public Dialog
{
public:
// Dialog Data
	enum { IDD = IDD_DIALOG_USER };

protected:
    virtual int OnInitDialog();
    virtual void OnBnClickedButtonNewuser();
	virtual void OnBnClickedButtonDeluser();
	virtual void OnBnClickedButtonChangepassword();
    virtual int OnCommand( int Id, int Event ) 
    {
        switch (Id) {
        case IDC_BUTTON_NEWUSER :
            OnBnClickedButtonNewuser();
            break;
        case IDC_BUTTON_DELUSER :
            OnBnClickedButtonDeluser();
            break;
        case IDC_BUTTON_CHANGEPASSWORD :
            OnBnClickedButtonChangepassword();
            break;
        }
        return Dialog::OnCommand( Id, Event ) ;
    }
};
