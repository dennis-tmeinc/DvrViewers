// PWManager.cpp : Defines the entry point for the application.
//

#include <Windows.h>

#include "..\common\cwin.h"
#include "..\common\cstr.h"
#include "PWManager.h"
#include "ManagerDlg.h"

static CRITICAL_SECTION criticalsection ;					// Critical section used for threads synchronization
void lock()   
{
    EnterCriticalSection(&criticalsection); 
}

void unlock()
{
    LeaveCriticalSection(&criticalsection); 
}

int WINAPI WinMain(HINSTANCE ,
                     HINSTANCE ,
                     LPSTR    ,
                     int      )
{
    ManagerDlg PWManDlg ;

    // initial lock
    InitializeCriticalSectionAndSpinCount(&criticalsection, 1000 );

    PWManDlg.DoModal( IDD_DIALOG_PWMANAGER );

    // Release lock
    DeleteCriticalSection(&criticalsection);

	return 0 ;
}

