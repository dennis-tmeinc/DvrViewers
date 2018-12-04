// gendvrid.cpp : Defines the entry point for the application.
//

#include "gendvrid.h"

#include <windows.h>
#include <Shellapi.h>

#include "stdio.h"
#include "stdlib.h"

#include "../common/cwin.h"
#include "../common/cstr.h"

/*
gendvrid usage:

gendvrid 53679372|[disk] [-s Storage] [-e] [-d|-u|-a] [username] [password]

option:
-e		erase all id
-d		delete specified id
-u		add a user
-a		add a administrator
*/

#define DLL_API __declspec(dllimport)

#define MAXUSER	1000
#define IDUSER	101
#define IDADMIN	102

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

const char keygenid[] = "d735179a-d47a-47f9-97b4-a400744ef034";

void AddUser(char * user, char * password, int type)
{
	DWORD key[8];
	PasswordToKey(password, key);
	AddID(user, key, type);
}

void DeleteUser( char * user )
{
    DeleteID( user );
}

void EraseAll() 
{
	DeleteID("*");;
}

int APIENTRY _tWinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPWSTR    lpCmdLine,
	int       nCmdShow)
{
    string user ;
    string password ;

    LPWSTR *szArglist;
    int nArgs=0;

    szArglist = CommandLineToArgvW(lpCmdLine, &nArgs);

    if( nArgs<2 )
        return 1 ;

    if( lstrcmp( szArglist[0], _T("53679372") ) != 0 ) {
        return 1 ;
    }

    for( int i=1; i<nArgs; i++ ) {
        if( szArglist[i][0]=='-' ) {
			if (szArglist[i][1] == 's') {
				if (nArgs > i + 1 && szArglist[i + 1][0] != '-') {
					i++;
					SetStorage(string(szArglist[i]));
				}
			}
			else if( szArglist[i][1]=='d' ) {
				if (nArgs > i + 1 && szArglist[i + 1][0] != '-') {
					i++;
					DeleteUser(string(szArglist[i]));
				}
            }
			else if (szArglist[i][1] == 'e') {
				EraseAll();
			}
			else if( szArglist[i][1]=='u' ) {
				if (nArgs > i + 1 && szArglist[i + 1][0] != '-') {
					user = szArglist[++i];
					if (nArgs > i + 1 && szArglist[i + 1][0] != '-') {
						password = szArglist[++i];
					}
					else {
						password = "";
					}
					AddUser(user, password, IDUSER);
				}
            }
            else if( szArglist[i][1]=='a' ) {
				if (nArgs > i + 1 && szArglist[i + 1][0] != '-') {
					user = szArglist[++i];
					if (nArgs > i + 1 && szArglist[i + 1][0] != '-') {
						password = szArglist[++i];
					}
					else {
						password = "";
					}
					AddUser(user, password, IDADMIN);
				}
            }
        }
    }

    return 0;
}
