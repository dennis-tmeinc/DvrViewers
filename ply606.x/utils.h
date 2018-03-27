#ifndef _UTILS_H_
#define _UTILS_H_

#include <winbase.h>
#include <winnt.h>
#include <time.h>
#include "ply606.h"
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
#endif