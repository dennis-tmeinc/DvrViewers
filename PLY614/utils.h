#ifndef _UTILS_H_
#define _UTILS_H_

#include <winbase.h>
#include <winnt.h>
#include <time.h>
#include "ply614.h"
//#include "filetree.h"

/* for GetCurrentModule() */
#if _MSC_VER >= 1300	// for VC 7.0
// from ATL 7.0 sources
#ifndef _delayimp_h
extern "C" IMAGE_DOS_HEADER __ImageBase;
#endif
#endif
HMODULE GetCurrentModule();
void printDvrtime(struct dvrtime *t);
void copyDvrTimeToSystemTime(struct dvrtime *dvrt, LPSYSTEMTIME pst);
void copySystemTimeToDvrTime(struct dvrtime *dvrt, LPSYSTEMTIME pst);
int isDvrTimeZero(struct dvrtime *dvrt);
int compareDvrTime(struct dvrtime *t1, struct dvrtime *t2);
#if 0
int compareDvrTimeWithFileEndTime(struct fileInfo *fi, struct dvrtime *t, INT64 *offset);
int getFileEndTime(struct fileInfo *fi, struct dvrtime *t);
bool fileInfoLess(struct fileInfo* &a, struct fileInfo* &b);
bool fileInfoLessA(struct fileInfo* a, struct fileInfo* b);
bool fileInfoRefLess(struct fileInfo &a, struct fileInfo &b);
bool fileInfoRefLessA(struct fileInfo a, struct fileInfo b);
void printFile(struct fileInfo *fi);
#endif
void UnixTimeToFileTime(time_t t, LPFILETIME pft);
void MicroTimeToFileTime(INT64 t, LPFILETIME pft);
INT64 GetMicroTimeFromDvrTime(struct dvrtime *t);

static inline UINT16 GetWLE( void const * _p )
{
    UINT8 * p = (UINT8 *)_p;
    return ( ((UINT16)p[1] << 8) | p[0] );
}
static inline UINT32 GetDWLE( void const * _p )
{
    UINT8 * p = (UINT8 *)_p;
    return ( ((UINT32)p[3] << 24) | ((UINT32)p[2] << 16)
              | ((UINT32)p[1] << 8) | p[0] );
}
static inline UINT64 GetQWLE( void const * _p )
{
    UINT8 * p = (UINT8 *)_p;
    return ( ((UINT64)p[7] << 56) | ((UINT64)p[6] << 48)
              | ((UINT64)p[5] << 40) | ((UINT64)p[4] << 32)
              | ((UINT64)p[3] << 24) | ((UINT64)p[2] << 16)
              | ((UINT64)p[1] << 8) | p[0] );
}

#define SetWLE( p, v ) _SetWLE( (UINT8*)p, v)
static inline void _SetWLE( UINT8 *p, UINT16 i_dw )
{
    p[1] = ( i_dw >>  8 )&0xff;
    p[0] = ( i_dw       )&0xff;
}

#define SetDWLE( p, v ) _SetDWLE( (UINT8*)p, v)
static inline void _SetDWLE( UINT8 *p, UINT32 i_dw )
{
    p[3] = ( i_dw >> 24 )&0xff;
    p[2] = ( i_dw >> 16 )&0xff;
    p[1] = ( i_dw >>  8 )&0xff;
    p[0] = ( i_dw       )&0xff;
}

static inline UINT16 U16_AT( void const * _p )
{
    UINT8 * p = (UINT8 *)_p;
    return ( ((UINT16)p[0] << 8) | p[1] );
}
static inline UINT32 U32_AT( void const * _p )
{
    UINT8 * p = (UINT8 *)_p;
    return ( ((UINT32)p[0] << 24) | ((UINT32)p[1] << 16)
              | ((UINT32)p[2] << 8) | p[3] );
}
static inline UINT64 U64_AT( void const * _p )
{
    UINT8 * p = (UINT8 *)_p;
    return ( ((UINT64)p[0] << 56) | ((UINT64)p[1] << 48)
              | ((UINT64)p[2] << 40) | ((UINT64)p[3] << 32)
              | ((UINT64)p[4] << 24) | ((UINT64)p[5] << 16)
              | ((UINT64)p[6] << 8) | p[7] );
}

#define GetWBE( p )     U16_AT( p )
#define GetDWBE( p )    U32_AT( p )
#define GetQWBE( p )    U64_AT( p )
#endif