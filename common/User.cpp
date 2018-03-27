// User.cpp : implementation file
//
#include "stdafx.h"

#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <time.h>
#include <process.h>
#include <direct.h>

#include "dvrclient.h"
#include "dvrclientdlg.h"
#include "User.h"
#include "UserNew.h"
#include "ChangePassword.h"

// User management

#define DLL_API __declspec(dllimport)

extern "C" {
	DLL_API int SetStorage(char * storage);
	DLL_API int DeleteID(char * id);
	DLL_API int ListID(int index, char * id);
	DLL_API int AddID(char * id, DWORD * key, int idtype);
	DLL_API int CheckID(char * id, DWORD * key);
	DLL_API void PasswordToKey(char * password, DWORD *key);
	DLL_API void Encode(char * v, int size);
	DLL_API void Decode(char * v, int size);
}

void user_setStorage()
{
	SetStorage(APPNAME);
}

int user_checkpassword( char * username, char * password )
{
	DWORD key[4] ;
	PasswordToKey( password, key );
	return CheckID( username, key );
}

// Add new user, or change password of existed user
int user_adduser( char * username, char * password, int newusertype )
{
	DWORD key[4] ;
	if( g_usertype == IDADMIN || strcmp( username, g_username ) == 0 ){
		PasswordToKey( password, key );
		return AddID( username, key, newusertype );
	}
	else {
		return FALSE ;
	}
}

// delete user
int user_deleteuser( char * username ) 
{
	if( strcmp( username, g_username )==0 )
		return FALSE ;
	if( strcmp( username, "admin")==0 ) 
		return FALSE ;
	if( g_usertype == IDADMIN ){
		return DeleteID(username);
	}
	else
		return FALSE ;
}

// return usertype, 0: empty slot, -1: error(eof)
int user_getuser(int index,  string & listuser)
{
	char id[256] ;
	int idtype ;
	if( g_usertype == IDADMIN ) {
		idtype = ListID( index, id );
		if( idtype>0 ) {
            listuser = id ;
		}
		return idtype ;
	}
	if( g_usertype == IDUSER && index==0 ) {
		idtype = g_usertype ;
        listuser = g_username ;
		return idtype ;
	}
	return -1 ;
}

static char * user_GenNewPassword(char * pbuf, int psize)
{
	int i;
	for( i=0; i<psize-1; i++ ) {
		unsigned int r;
		rand_s(&r);
		r &= 0x0fffffff;
		pbuf[i] = 'a'+r%26 ;
	}
	pbuf[i] = 0;
	return pbuf;
}

static void user_Getcode( char * password, char * code )
{
	DWORD ecode[4] = { 0,0,0,0 };

	memcpy(ecode, password, 8);
	Encode((char *)ecode, 8);
	memcpy(code, ecode, 8);

	// reverse it for testing or recovery password
	Decode((char *)ecode, 8);

	memcpy(ecode, password, 8);
	Decode((char *)ecode, 8);
	memcpy(code+8, ecode, 8);

	// reverse it for testing or recovery password
	Encode((char *)ecode, 8);

}

// Login dialog

int LoginDlg::OnInitDialog()
{
    m_retry = 20 ;
    HICON hicon = LoadIcon(ResInst(), MAKEINTRESOURCE(IDR_MAINFRAME) );
    SendMessage(m_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hicon );        // set ICON
    int i ;

	user_setStorage();

    g_usertype = IDADMIN ;      // temperary admin
    for( i=0; i<MAXUSER; i++ ) {
        string listuser ;
        int idtype = user_getuser( i,  listuser);
        if( idtype < 0 ) break;				// error(eof)
        else if(idtype>0 ) {
            SendMessage( GetDlgItem(m_hWnd, IDC_USERNAME),  CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)listuser );
        }
		// else {} // empty slot
    }

    int count= SendMessage( GetDlgItem(m_hWnd, IDC_USERNAME),  CB_GETCOUNT, 0, 0 );
    if( count<=0 || count==CB_ERR ) {		// no user, ask for contact TME
		char password[8];
		user_GenNewPassword( password, 8 );
		char code[16] ;
		user_adduser("admin", password, IDADMIN);

        user_Getcode(password, code);
        DWORD * dwcode = (DWORD *)code ;
        CallTmeDlg calltme ;
		calltme.m_code.printf("%08X - %08X - %08X - %08X", dwcode[0], dwcode[1], dwcode[2], dwcode[3]);
        if( calltme.DoModal(IDD_DIALOG_CALLTME, this->m_hWnd ) == IDOK ) {
            i = SendMessage( GetDlgItem(m_hWnd, IDC_USERNAME),  CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)_T("admin") );
            SendMessage( GetDlgItem(m_hWnd, IDC_USERNAME),  CB_SETCURSEL, i, 0 );
        }
        else {
            EndDialog(IDCANCEL);
        }
    }
    g_usertype = 0 ;					// no usertype 

    return TRUE;
}

int LoginDlg::OnOK()
{
	string username;
	string password;
    GetDlgItemText(m_hWnd, IDC_USERNAME, username.tcssize(500), 500);
    GetDlgItemText(m_hWnd, IDC_PASSWORD, password.tcssize(500), 500);
    m_usertype = user_checkpassword(username, password);
    if( m_usertype>0 ) {
        g_username= username;
        EndDialog(IDOK);
    }
    else
    {
        if( --m_retry>0 ) {
            SetDlgItemText(m_hWnd, IDC_PASSWORD, _T(""));
            if( MessageBox(m_hWnd, _T("Username/Password wrong, try again!"), NULL, MB_RETRYCANCEL )
                == IDCANCEL ) {
                    EndDialog(IDCANCEL);
            }
        }
        else {
            MessageBox(m_hWnd, _T("Username/Password error, application Quit!"), _T("Error"), MB_OK);
            EndDialog(IDCANCEL);
        }
    }
	return TRUE ;
}

void LoginDlg::OnCbnEditchangeUsername() 
{
    SendMessage(GetDlgItem(m_hWnd, IDC_USERNAME ), CB_SHOWDROPDOWN , TRUE, 0 );
}


// UserDlg
int UserDlg::OnInitDialog()
{
    int i;
    if( g_usertype != IDADMIN ) {
        ShowWindow( GetDlgItem(m_hWnd, IDC_BUTTON_DELUSER), SW_HIDE );
        ShowWindow( GetDlgItem(m_hWnd, IDC_BUTTON_NEWUSER), SW_HIDE );
    }

    for( i=0; i<MAXUSER; i++ ) {
        string listuser ;
        int idtype = user_getuser( i,  listuser);
		if (idtype > 0) {
			SendMessage(GetDlgItem(m_hWnd, IDC_LIST_USER), LB_ADDSTRING, 0, (LPARAM)(LPCTSTR)listuser);
		}
        else if( idtype < 0 ) break;				// error(eof)
        // else {continue}							// empty slot
    }
    SendMessage( GetDlgItem(m_hWnd, IDC_LIST_USER),  LB_SETCURSEL, 0, 0 );
    return TRUE ;
}

void UserDlg::OnBnClickedButtonNewuser()
{
	UserNew usernew ;
	int usertype ;
    if( usernew.DoModal(IDD_DIALOG_USERNEW, m_hWnd)==IDOK && (strlen(usernew.m_username)!=0 ) ) {
		if( usernew.m_usertype==0 ) {
			usertype = IDUSER ;
		}
		else {
			usertype = IDADMIN ;
		}
		if( user_adduser( usernew.m_username, usernew.m_password, usertype ) ) {
            SendMessage( GetDlgItem(m_hWnd, IDC_LIST_USER),  LB_ADDSTRING, 0, (LPARAM)(LPCTSTR)usernew.m_username );
		}
	}
}

void UserDlg::OnBnClickedButtonDeluser()
{
    TCHAR buf[512] ;
    int sel = SendMessage( GetDlgItem(m_hWnd, IDC_LIST_USER),  LB_GETCURSEL, 0, 0 );
	if( sel==LB_ERR) 
		return ;
	string seluser ;
    SendMessage( GetDlgItem(m_hWnd, IDC_LIST_USER),  LB_GETTEXT, sel, (LPARAM)buf );
    seluser=buf ;
	if( strcmp( seluser, "admin")==0 ) {
		MessageBox(m_hWnd, _T("Can't delete admin!"), _T("Error!"), MB_OK );
		return ;
	}
	if( strcmp( seluser, g_username )==0 ) {
		MessageBox(m_hWnd, _T("Can't delete current user!"), _T("Error!"), MB_OK );
		return;
	}
    string delstr ;
	delstr.printf("Delete user: %s ?", (LPCSTR)seluser);
	if( MessageBox(m_hWnd, delstr, _T("Confirm?"), MB_YESNO)==IDYES ){
		if( user_deleteuser( seluser ) ) {
            SendMessage( GetDlgItem(m_hWnd, IDC_LIST_USER),  LB_DELETESTRING, sel, 0 );
		}
	}
}

void UserDlg::OnBnClickedButtonChangepassword()
{
    TCHAR buf[512] ;
    int sel = SendMessage( GetDlgItem(m_hWnd, IDC_LIST_USER),  LB_GETCURSEL, 0, 0 );
	if( sel==LB_ERR) 
		return ;
	string seluser ;
    SendMessage( GetDlgItem(m_hWnd, IDC_LIST_USER),  LB_GETTEXT, sel, (LPARAM)buf );
    seluser=buf ;
	ChangePassword changepasswd ;
	changepasswd.m_username = (LPCTSTR)seluser ; 
	if( changepasswd.DoModal(IDD_DIALOG_CHANGEPASSWORD, this->m_hWnd)==IDOK ) {
		if( _tcscmp( changepasswd.m_confirmpassword,changepasswd.m_newpassword )!=0 ) {
			MessageBox(m_hWnd, _T("New password incorrect, please try again."), _T("Error!"), MB_OK);
			return ;
		}
		int idtype = user_checkpassword( seluser, changepasswd.m_oldpassword ) ;
		if( idtype > 0 ) {
			user_adduser( seluser, changepasswd.m_newpassword, idtype );
		}
		else {
			MessageBox(m_hWnd, _T("Old password incorrect, please try again."), _T("Error!"), MB_OK);
			return ;
		}
	}
}

