#ifndef MTIME_H
#define MTIME_H

typedef INT64				mtime_t;
#define I64C(x)         x##i64

/*****************************************************************************
 * LAST_MDATE: date which will never happen
 *****************************************************************************
 * This date can be used as a 'never' date, to mark missing events in a function
 * supposed to return a date, such as nothing to display in a function
 * returning the date of the first image to be displayed. It can be used in
 * comparaison with other values: all existing dates will be earlier.
 *****************************************************************************/
#define LAST_MDATE ((mtime_t)((UINT64)(-1)/2))

/*****************************************************************************
 * MSTRTIME_MAX_SIZE: maximum possible size of mstrtime
 *****************************************************************************
 * This values is the maximal possible size of the string returned by the
 * mstrtime() function, including '-' and the final '\0'. It should be used to
 * allocate the buffer.
 *****************************************************************************/
#define MSTRTIME_MAX_SIZE 22

/* Well, Duh? But it does clue us in that we are converting from
   millisecond quantity to a second quantity or vice versa.
*/
#define MILLISECONDS_PER_SEC 1000

#define msecstotimestr(psz_buffer, msecs) \
  secstotimestr( psz_buffer, (msecs / (int) MILLISECONDS_PER_SEC) )

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
char *mstrtime( char *psz_buffer, mtime_t date );
char *secstotimestr( char *psz_buffer, int i_seconds );
mtime_t mdate( void );
void mwait( mtime_t date );
void msleep( mtime_t delay );
//void date_Init( date_t *p_date, uint32_t i_divider_n, uint32_t i_divider_d );
//void date_Change( date_t *p_date, uint32_t i_divider_n, uint32_t i_divider_d );
//void date_Set( date_t *p_date, mtime_t i_new_date );
//mtime_t date_Get( const date_t *p_date );
//void date_Move( date_t *p_date, mtime_t i_difference );
//mtime_t date_Increment( date_t *p_date, uint32_t i_nb_samples );


/*****************************************************************************
 * date_t: date incrementation without long-term rounding errors
 *****************************************************************************/
struct date_t
{
    mtime_t  date;
    UINT32 i_divider_num;
    UINT32 i_divider_den;
    UINT32 i_remainder;
};

#endif