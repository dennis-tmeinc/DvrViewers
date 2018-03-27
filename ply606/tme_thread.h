#ifndef TME_THREAD_H
#define TME_THREAD_H

/* Define different priorities for WinNT/2K/XP and Win9x/Me */
#define IS_WINNT ( GetVersion() < 0x80000000 )

#define TME_THREAD_PRIORITY_LOW 0
#define TME_THREAD_PRIORITY_INPUT \
        (IS_WINNT ? THREAD_PRIORITY_ABOVE_NORMAL : 0)
#define TME_THREAD_PRIORITY_AUDIO \
        (IS_WINNT ? THREAD_PRIORITY_HIGHEST : 0)
#define TME_THREAD_PRIORITY_VIDEO \
        (IS_WINNT ? 0 : THREAD_PRIORITY_BELOW_NORMAL )
#define TME_THREAD_PRIORITY_OUTPUT \
        (IS_WINNT ? THREAD_PRIORITY_ABOVE_NORMAL : 0)
#define TME_THREAD_PRIORITY_HIGHEST \
        (IS_WINNT ? THREAD_PRIORITY_TIME_CRITICAL : 0)

typedef HANDLE tme_thread_t;
typedef BOOL (WINAPI *SIGNALOBJECTANDWAIT) ( HANDLE, HANDLE, DWORD, BOOL );
//typedef unsigned (WINAPI *PTHREAD_START) (void *);

typedef struct
{
    /* WinNT/2K/XP implementation */
    HANDLE              mutex;
    /* Win95/98/ME implementation */
    CRITICAL_SECTION    csection;
} tme_mutex_t;

typedef struct
{
    volatile int        i_waiting_threads;
    /* WinNT/2K/XP implementation */
    HANDLE              event;
    SIGNALOBJECTANDWAIT SignalObjectAndWait;
    /* Win95/98/ME implementation */
    HANDLE              semaphore;
    CRITICAL_SECTION    csection;
    int                 i_win9x_cv;
} tme_cond_t;

typedef struct {
    SIGNALOBJECTANDWAIT SignalObjectAndWait;
    bool				b_fast_mutex;
    int                 i_win9x_cv;
	tme_mutex_t		object_lock;
	tme_cond_t			object_wait;
} tme_object_t;

inline int tme_mutex_lock(tme_mutex_t *p_mutex) {
    if( p_mutex->mutex )
    {
        WaitForSingleObject( p_mutex->mutex, INFINITE );
    }
    else
    {
        EnterCriticalSection( &p_mutex->csection );
    }
    return 0;
}

inline int tme_mutex_unlock( tme_mutex_t *p_mutex ) {
    if( p_mutex->mutex )
    {
        ReleaseMutex( p_mutex->mutex );
    }
    else
    {
        LeaveCriticalSection( &p_mutex->csection );
    }
	return 0;
}

inline int tme_cond_signal( tme_cond_t *p_condvar ) {
    /* Release one waiting thread if one is available. */
    /* For this trick to work properly, the tme_cond_signal must be surrounded
     * by a mutex. This will prevent another thread from stealing the signal */
    if( !p_condvar->semaphore )
    {
        SetEvent( p_condvar->event );
    }
    else if( p_condvar->i_win9x_cv == 1 )
    {
        /* Wait for the gate to be open */
        WaitForSingleObject( p_condvar->event, INFINITE );

        if( p_condvar->i_waiting_threads )
        {
            /* Using a semaphore exposes us to a race condition. It is
             * possible for another thread to start waiting on the semaphore
             * just after we signaled it and thus steal the signal.
             * We have to prevent new threads from entering the cond_wait(). */
            ResetEvent( p_condvar->event );

            /* A semaphore is used here because Win9x doesn't have
             * SignalObjectAndWait() and thus a race condition exists
             * during the time we release the mutex and the time we start
             * waiting on the event (more precisely, the signal can sometimes
             * be missed by the waiting thread if we use PulseEvent()). */
            ReleaseSemaphore( p_condvar->semaphore, 1, 0 );
        }
    }
    else
    {
        if( p_condvar->i_waiting_threads )
        {
            ReleaseSemaphore( p_condvar->semaphore, 1, 0 );

            /* Wait for the last thread to be awakened */
            WaitForSingleObject( p_condvar->event, INFINITE );
        }
    }
	return 0;
}

inline int tme_cond_wait( tme_cond_t *p_condvar, tme_mutex_t *p_mutex ) {
    if( !p_condvar->semaphore )
    {
        /* Increase our wait count */
        p_condvar->i_waiting_threads++;

        if( p_mutex->mutex )
        {
            /* It is only possible to atomically release the mutex and
             * initiate the waiting on WinNT/2K/XP. Win9x doesn't have
             * SignalObjectAndWait(). */
            p_condvar->SignalObjectAndWait( p_mutex->mutex,
                                            p_condvar->event,
                                            INFINITE, FALSE );
        }
        else
        {
            LeaveCriticalSection( &p_mutex->csection );
            WaitForSingleObject( p_condvar->event, INFINITE );
        }

        p_condvar->i_waiting_threads--;
    }
    else if( p_condvar->i_win9x_cv == 1 )
    {
        int i_waiting_threads;

        /* Wait for the gate to be open */
        WaitForSingleObject( p_condvar->event, INFINITE );

        /* Increase our wait count */
        p_condvar->i_waiting_threads++;

        LeaveCriticalSection( &p_mutex->csection );
        WaitForSingleObject( p_condvar->semaphore, INFINITE );

        /* Decrement and test must be atomic */
        EnterCriticalSection( &p_condvar->csection );

        /* Decrease our wait count */
        i_waiting_threads = --p_condvar->i_waiting_threads;

        LeaveCriticalSection( &p_condvar->csection );

        /* Reopen the gate if we were the last waiting thread */
        if( !i_waiting_threads )
            SetEvent( p_condvar->event );
    }
    else
    {
        int i_waiting_threads;

        /* Increase our wait count */
        p_condvar->i_waiting_threads++;

        LeaveCriticalSection( &p_mutex->csection );
        WaitForSingleObject( p_condvar->semaphore, INFINITE );

        /* Decrement and test must be atomic */
        EnterCriticalSection( &p_condvar->csection );

        /* Decrease our wait count */
        i_waiting_threads = --p_condvar->i_waiting_threads;

        LeaveCriticalSection( &p_condvar->csection );

        /* Signal that the last waiting thread just went through */
        if( !i_waiting_threads )
            SetEvent( p_condvar->event );
    }

    /* Reacquire the mutex before returning. */
    tme_mutex_lock( p_mutex );

    return 0;
}

int tme_thread_init(tme_object_t *p_this);
int tme_mutex_init(tme_object_t *p_this, tme_mutex_t *p_mutex);
int tme_mutex_destroy( tme_mutex_t *p_mutex );
int tme_cond_init(tme_object_t *p_this, tme_cond_t *p_condvar);
int tme_cond_destroy( tme_cond_t *p_condvar );
HANDLE tme_thread_create(tme_object_t *p_this, char *psz_name, 
						 unsigned (WINAPI *func ) ( void * ), void *p_data,
                         int i_priority, bool b_wait);
void tme_thread_ready( tme_object_t *p_this );
void tme_thread_join(HANDLE hThread);

#endif