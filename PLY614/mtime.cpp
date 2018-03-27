#include "stdafx.h"
#include "mtime.h"
#include <stdio.h>
//#include <crtdbg.h>

/**
 * Return a date in a readable format
 *
 * This function converts a mtime date into a string.
 * psz_buffer should be a buffer long enough to store the formatted
 * date.
 * \param date to be converted
 * \param psz_buffer should be a buffer at least MSTRTIME_MAX_SIZE characters
 * \return psz_buffer is returned so this can be used as printf parameter.
 */
char *mstrtime( char *psz_buffer, mtime_t date )
{
    static mtime_t ll1000 = 1000, ll60 = 60, ll24 = 24;

    _snprintf( psz_buffer, MSTRTIME_MAX_SIZE, "%02d:%02d:%02d-%03d.%03d",
            (int) (date / (ll1000 * ll1000 * ll60 * ll60) % ll24),
             (int) (date / (ll1000 * ll1000 * ll60) % ll60),
             (int) (date / (ll1000 * ll1000) % ll60),
             (int) (date / ll1000 % ll1000),
             (int) (date % ll1000) );

    return( psz_buffer );
}

/**
 * Convert seconds to a time in the format h:mm:ss.
 *
 * This function is provided for any interface function which need to print a
 * time string in the format h:mm:ss
 * date.
 * \param secs  the date to be converted
 * \param psz_buffer should be a buffer at least MSTRTIME_MAX_SIZE characters
 * \return psz_buffer is returned so this can be used as printf parameter.
 */
char *secstotimestr( char *psz_buffer, int i_seconds )
{
	_snprintf( psz_buffer, MSTRTIME_MAX_SIZE, "%d:%2.2d:%2.2d",
              (int) (i_seconds / (60 *60)),
              (int) ((i_seconds / 60) % 60),
              (int) (i_seconds % 60) );

    return( psz_buffer );
}

/**
 * Return high precision date
 *
 */
mtime_t mdate( void )
{
    /* We don't need the real date, just the value of a high precision timer */
    static mtime_t freq = I64C(-1);
    mtime_t usec_time;

    if( freq == I64C(-1) )
    {
        /* Extract from the Tcl source code:
         * (http://www.cs.man.ac.uk/fellowsd-bin/TIP/7.html)
         *
         * Some hardware abstraction layers use the CPU clock
         * in place of the real-time clock as a performance counter
         * reference.  This results in:
         *    - inconsistent results among the processors on
         *      multi-processor systems.
         *    - unpredictable changes in performance counter frequency
         *      on "gearshift" processors such as Transmeta and
         *      SpeedStep.
         * There seems to be no way to test whether the performance
         * counter is reliable, but a useful heuristic is that
         * if its frequency is 1.193182 MHz or 3.579545 MHz, it's
         * derived from a colorburst crystal and is therefore
         * the RTC rather than the TSC.  If it's anything else, we
         * presume that the performance counter is unreliable.
         */

        freq = ( QueryPerformanceFrequency( (LARGE_INTEGER *)&freq ) &&
                 (freq == I64C(1193182) || freq == I64C(3579545) ) )
               ? freq : 0;
    }

    if( freq != 0 )
    {
        /* Microsecond resolution */
        QueryPerformanceCounter( (LARGE_INTEGER *)&usec_time );
        return ( usec_time * 1000000 ) / freq;
    }
    else
    {
        /* Fallback on GetTickCount() which has a milisecond resolution
         * (actually, best case is about 10 ms resolution)
         * GetTickCount() only returns a DWORD thus will wrap after
         * about 49.7 days so we try to detect the wrapping. */

        static CRITICAL_SECTION date_lock;
        static mtime_t i_previous_time = I64C(-1);
        static int i_wrap_counts = -1;

        if( i_wrap_counts == -1 )
        {
            /* Initialization */
            i_previous_time = I64C(1000) * GetTickCount();
            InitializeCriticalSection( &date_lock );
            i_wrap_counts = 0;
        }

        EnterCriticalSection( &date_lock );
        usec_time = I64C(1000) *
            (i_wrap_counts * I64C(0x100000000) + GetTickCount());
        if( i_previous_time > usec_time )
        {
            /* Counter wrapped */
            i_wrap_counts++;
            usec_time += I64C(0x100000000000);
        }
        i_previous_time = usec_time;
        LeaveCriticalSection( &date_lock );

        return usec_time;
    }

}

/**
 * Wait for a date
 *
 * This function uses select() and an system date function to wake up at a
 * precise date. It should be used for process synchronization. If current date
 * is posterior to wished date, the function returns immediately.
 * \param date The date to wake up at
 */
void mwait( mtime_t date )
{
    mtime_t usec_time, delay;

    usec_time = mdate();
    delay = date - usec_time;
    if( delay <= 0 )
    {
        return;
    }
    msleep( delay );
}

/**
 * More precise sleep()
 *
 * Portable usleep() function.
 * \param delay the amount of time to sleep
 */
void msleep( mtime_t delay )
{
	//_RPT1(_CRT_WARN, "msleep:%I64d\n", delay);
    Sleep( (int) (delay / 1000) );
}

/*
 * Date management (internal and external)
 */

/**
 * Initialize a date_t.
 *
 * \param date to initialize
 * \param divider (sample rate) numerator
 * \param divider (sample rate) denominator
 */

void date_Init( date_t *p_date, UINT32 i_divider_n, UINT32 i_divider_d )
{
    p_date->date = 0;
    p_date->i_divider_num = i_divider_n;
    p_date->i_divider_den = i_divider_d;
    p_date->i_remainder = 0;
}

/**
 * Change a date_t.
 *
 * \param date to change
 * \param divider (sample rate) numerator
 * \param divider (sample rate) denominator
 */

void date_Change( date_t *p_date, UINT32 i_divider_n, UINT32 i_divider_d )
{
    p_date->i_divider_num = i_divider_n;
    p_date->i_divider_den = i_divider_d;
}

/**
 * Set the date value of a date_t.
 *
 * \param date to set
 * \param date value
 */
void date_Set( date_t *p_date, mtime_t i_new_date )
{
    p_date->date = i_new_date;
    p_date->i_remainder = 0;
}

/**
 * Get the date of a date_t
 *
 * \param date to get
 * \return date value
 */
mtime_t date_Get( const date_t *p_date )
{
    return p_date->date;
}

/**
 * Move forwards or backwards the date of a date_t.
 *
 * \param date to move
 * \param difference value
 */
void date_Move( date_t *p_date, mtime_t i_difference )
{
    p_date->date += i_difference;
}

/**
 * Increment the date and return the result, taking into account
 * rounding errors.
 *
 * \param date to increment
 * \param incrementation in number of samples
 * \return date value
 */
mtime_t date_Increment( date_t *p_date, UINT32 i_nb_samples )
{
    mtime_t i_dividend = (mtime_t)i_nb_samples * 1000000;
    p_date->date += i_dividend / p_date->i_divider_num * p_date->i_divider_den;
    p_date->i_remainder += (int)(i_dividend % p_date->i_divider_num);

    if( p_date->i_remainder >= p_date->i_divider_num )
    {
        /* This is Bresenham algorithm. */
        p_date->date += p_date->i_divider_den;
        p_date->i_remainder -= p_date->i_divider_num;
    }

    return p_date->date;
}
