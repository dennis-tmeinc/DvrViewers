#include "stdafx.h"
#include <crtdbg.h>
#include <process.h>
#include "tme_thread.h"
#include <stdio.h>
#include "trace.h"

int tme_mutex_init(tme_object_t *p_this, tme_mutex_t *p_mutex) {
    /* We use mutexes on WinNT/2K/XP because we can use the SignalObjectAndWait
     * function and have a 100% correct vlc_cond_wait() implementation.
     * As this function is not available on Win9x, we can use the faster
     * CriticalSections */
    if( p_this->SignalObjectAndWait &&
        !p_this->b_fast_mutex )
    {
        /* We are running on NT/2K/XP, we can use SignalObjectAndWait */
        p_mutex->mutex = CreateMutex( 0, FALSE, 0 );
        return ( p_mutex->mutex != NULL ? 0 : 1 );
    }
    else
    {
        p_mutex->mutex = NULL;
        InitializeCriticalSection( &p_mutex->csection );
        return 0;
    }
}

int tme_mutex_destroy( tme_mutex_t *p_mutex ) {
    if( p_mutex->mutex )
    {
        CloseHandle( p_mutex->mutex );
    }
    else
    {
        DeleteCriticalSection( &p_mutex->csection );
    }
    return 0;
}

int tme_cond_init(tme_object_t *p_this, tme_cond_t *p_condvar) {
    /* Initialize counter */
    p_condvar->i_waiting_threads = 0;

    /* Misc init */
    p_condvar->i_win9x_cv = p_this->i_win9x_cv;
    p_condvar->SignalObjectAndWait = p_this->SignalObjectAndWait;

    if( (p_condvar->SignalObjectAndWait && !p_this->b_fast_mutex)
        || p_condvar->i_win9x_cv == 0 )
    {
        /* Create an auto-reset event. */
        p_condvar->event = CreateEvent( NULL,   /* no security */
                                        FALSE,  /* auto-reset event */
                                        FALSE,  /* start non-signaled */
                                        NULL ); /* unnamed */

        p_condvar->semaphore = NULL;
        return !p_condvar->event;
    }
    else
    {
        p_condvar->semaphore = CreateSemaphore( NULL,       /* no security */
                                                0,          /* initial count */
                                                0x7fffffff, /* max count */
                                                NULL );     /* unnamed */

        if( p_condvar->i_win9x_cv == 1 )
            /* Create a manual-reset event initially signaled. */
            p_condvar->event = CreateEvent( NULL, TRUE, TRUE, NULL );
        else
            /* Create a auto-reset event. */
            p_condvar->event = CreateEvent( NULL, FALSE, FALSE, NULL );

        InitializeCriticalSection( &p_condvar->csection );

        return !p_condvar->semaphore || !p_condvar->event;
    }
}

int tme_cond_destroy( tme_cond_t *p_condvar ) {
    if( !p_condvar->semaphore )
        return !CloseHandle( p_condvar->event );
    else
        return !CloseHandle( p_condvar->event )
          || !CloseHandle( p_condvar->semaphore );

    if( p_condvar->semaphore != NULL )
		DeleteCriticalSection( &p_condvar->csection );
	return 0;
}

int tme_thread_init(tme_object_t *p_this) {
    /* Dynamically get the address of SignalObjectAndWait */
    if( GetVersion() < 0x80000000 )
    {
        HINSTANCE hInstLib;

        /* We are running on NT/2K/XP, we can use SignalObjectAndWait */
        hInstLib = LoadLibrary( _T("kernel32") );
        if( hInstLib )
        {
            p_this->SignalObjectAndWait =
                    (SIGNALOBJECTANDWAIT)GetProcAddress( hInstLib,
                                                     "SignalObjectAndWait" );
			if (p_this->SignalObjectAndWait != NULL) {
				//_RPT0(_CRT_WARN, "Using SignalObjectAndWait\n");
			}
        }
    }
    else
    {
        p_this->SignalObjectAndWait = NULL;
    }

    p_this->b_fast_mutex = 0;
    p_this->i_win9x_cv = 0;

	tme_mutex_init(p_this, &p_this->object_lock);
	tme_cond_init(p_this, &p_this->object_wait);

	return 0;
}	

HANDLE tme_thread_create(tme_object_t *p_this, char *psz_name, 
						 unsigned (WINAPI *func ) ( void * ), void *p_data,
                         int i_priority, bool b_wait) {
	HANDLE hThread;
    unsigned threadID;

	tme_mutex_lock(&p_this->object_lock);
    /* When using the MSVCRT C library you have to use the _beginthreadex
     * function instead of CreateThread, otherwise you'll end up with
     * memory leaks and the signal functions not working (see Microsoft
     * Knowledge Base, article 104641) */
    hThread = (HANDLE)_beginthreadex( NULL, 0, func,
                                        p_data, 0, &threadID );

    if( hThread && i_priority )
    {
        if( !SetThreadPriority(hThread, i_priority) )
        {
            _RPT0(_CRT_WARN, "couldn't set a faster priority\n" );
            i_priority = 0;
        }
    }

    if( b_wait )
    {
        _RPT1(_CRT_WARN, "waiting for thread (%s) completion\n", psz_name );
        tme_cond_wait( &p_this->object_wait, &p_this->object_lock );
    }
    _RPT3(_CRT_WARN, "thread %u (%s) created at priority %d\n",
                 (unsigned int)hThread, psz_name, i_priority);

    tme_mutex_unlock( &p_this->object_lock );

	return hThread;
}

void tme_thread_ready( tme_object_t *p_this )
{
    tme_mutex_lock( &p_this->object_lock );
    tme_cond_signal( &p_this->object_wait );
    tme_mutex_unlock( &p_this->object_lock );
}

void tme_thread_join(HANDLE hThread)
{
    int i_ret = 0;

    HMODULE hmodule;
    BOOL (WINAPI *OurGetThreadTimes)( HANDLE, FILETIME*, FILETIME*,
                                      FILETIME*, FILETIME* );
    FILETIME create_ft, exit_ft, kernel_ft, user_ft;
    INT64 real_time, kernel_time, user_time;

	TRACE(_T("thread_join:%u\n"), hThread);
    WaitForSingleObject( hThread, INFINITE );

    hmodule = GetModuleHandle( _T("KERNEL32") );
    OurGetThreadTimes = (BOOL (WINAPI*)( HANDLE, FILETIME*, FILETIME*,
                                         FILETIME*, FILETIME* ))
        GetProcAddress( hmodule, "GetThreadTimes" );

    if( OurGetThreadTimes &&
        OurGetThreadTimes( hThread,
                           &create_ft, &exit_ft, &kernel_ft, &user_ft ) )
    {
        real_time =
          ((((INT64)exit_ft.dwHighDateTime)<<32)| exit_ft.dwLowDateTime) -
          ((((INT64)create_ft.dwHighDateTime)<<32)| create_ft.dwLowDateTime);
        real_time /= 10;

        kernel_time =
          ((((INT64)kernel_ft.dwHighDateTime)<<32)|
           kernel_ft.dwLowDateTime) / 10;

        user_time =
          ((((INT64)user_ft.dwHighDateTime)<<32)|
           user_ft.dwLowDateTime) / 10;

		char buf[512];
        sprintf( buf, "thread times: real %I64dm%fs, kernel %I64dm%fs, user %I64dm%fs",
                 real_time/60/1000000,
                 (double)((real_time%(60*1000000))/1000000.0),
                 kernel_time/60/1000000,
                 (double)((kernel_time%(60*1000000))/1000000.0),
                 user_time/60/1000000,
                 (double)((user_time%(60*1000000))/1000000.0) );
		_RPT1(_CRT_WARN, "%s\n", buf);
    }
    CloseHandle( hThread );

    if( i_ret )
    {
       _RPT1(_CRT_WARN, "thread_join(%u) failed\n",
                         (unsigned int)hThread);
    }
    else
    {
        _RPT1(_CRT_WARN, "thread %u joined\n",
                         (unsigned int)hThread);
    }
}
