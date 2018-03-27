#include "stdafx.h"
#include <stdio.h>
#include "utils.h"


HMODULE GetCurrentModule()
{
#if _MSC_VER < 1300	// ealier than .NET compiler (VC 6.0)
	// see http://www.dotnet247.com/247reference/msgs/13/65259.aspx
	MEMORY_BASIC_INFORMATION mbi;
	static int dummy;
	VirtualQuery(&dummy, &mbi, sizeof(mbi));

	return reinterpret_cast<HMODULE>(mbi.AllocationBase);
#else
	return reinterpret_cast<HMODULE>(&__ImageBase);
#endif
}

void printDvrtime(struct dvrtime *t)
{
	char  buf[512];

	sprintf_s(buf, sizeof(buf),
		"%04d%02d%02d-%02d:%02d:%02d.%03d",
				  t->year,
				  t->month,
				  t->day,
				  t->hour,
				  t->min,
				  t->second,
				  t->millisecond);
	_RPT1(_CRT_WARN, "%s\n", buf);
}

void copyDvrTimeToSystemTime(struct dvrtime *dvrt, LPSYSTEMTIME pst)
{
	pst->wYear = dvrt->year;
	pst->wMonth = dvrt->month;
	pst->wDay = dvrt->day;
	pst->wHour = dvrt->hour;
	pst->wMinute = dvrt->min;
	pst->wSecond = dvrt->second;
	pst->wMilliseconds = dvrt->millisecond;
}

void copySystemTimeToDvrTime(struct dvrtime *dvrt, LPSYSTEMTIME pst)
{
	dvrt->year = pst->wYear;
	dvrt->month = pst->wMonth;
	dvrt->day = pst->wDay;
	dvrt->hour = pst->wHour;
	dvrt->min = pst->wMinute;
	dvrt->second = pst->wSecond;
	dvrt->millisecond = pst->wMilliseconds;
}

int isDvrTimeZero(struct dvrtime *dvrt)
{
	if (!dvrt->year && !dvrt->month && !dvrt->day &&
		!dvrt->hour && !dvrt->min && !dvrt->second && !dvrt->millisecond)
		return 1;
	else
		return 0;
}

/*
 * return:
 *    -1: t1 earlier than t2
 *     0: t1 == t2
 *     1: t1 later than t2
 */
int compareDvrTime(struct dvrtime *t1, struct dvrtime *t2)
{
	SYSTEMTIME st1, st2;
	FILETIME ft1, ft2;

	copyDvrTimeToSystemTime(t1, &st1);
	SystemTimeToFileTime(&st1, &ft1);

	copyDvrTimeToSystemTime(t2, &st2);
	SystemTimeToFileTime(&st2, &ft2);

	return CompareFileTime(&ft1, &ft2);
}

#if 0
int getFileEndTime(struct fileInfo *fi, struct dvrtime *t)
{
	SYSTEMTIME stStart, stEnd;
	FILETIME ftStart, ftEnd;
	ULARGE_INTEGER uliStart, uliEnd;

	if (fi == NULL)
		return 1;

	copyDvrTimeToSystemTime(&fi->time, &stStart);
	if (!SystemTimeToFileTime(&stStart, &ftStart))
		return 1; // error

	// calculate file end time
	CopyMemory(&uliStart, &ftStart, sizeof(FILETIME));
	uliEnd.QuadPart = uliStart.QuadPart + (fi->len * 10000LL); // millisec -> 100's nanosec
	CopyMemory(&ftEnd, &uliEnd, sizeof(FILETIME));
	if (!FileTimeToSystemTime(&ftEnd, &stEnd))
		return 1; // error

	copySystemTimeToDvrTime(t, &stEnd);

	return 0;
}

/*
 * compareDvrTimeWithFileEndTime
 *    compares dvrtime t with fileInfo time.
 *    when the file ends later than dvrtime t,
 *    offset of the dvrtime t from the fileInfo start time
 *    can be retrieved with offset.
 * return:
 *    -1: fileEndtime earlier than t
 *     0: fileEndtime == t
 *     1: fileEndtime later than t
 * parameter:
 *   offset: pointer to offset which will receive offset value in microsec
 */
int compareDvrTimeWithFileEndTime(struct fileInfo *fi, struct dvrtime *t, INT64 *offset)
{
	LONG ret;
	SYSTEMTIME stStart, st2;
	FILETIME ftStart, ftEnd, ft2;
	ULARGE_INTEGER uliStart, uliEnd, uli2;

	copyDvrTimeToSystemTime(&fi->time, &stStart);
	SystemTimeToFileTime(&stStart, &ftStart);
	// calculate file end time
	CopyMemory(&uliStart, &ftStart, sizeof(FILETIME));
	/* substract 1 sec, as we can have some timing errors, and we don't want to play a file for just 1 sec. */
	uliEnd.QuadPart = uliStart.QuadPart + (fi->len * 10000LL) - 10000000LL; // millisec -> 100's nanosec
	CopyMemory(&ftEnd, &uliEnd, sizeof(FILETIME));
	// debug: FileTimeToSystemTime(&ftEnd, &st2);

	copyDvrTimeToSystemTime(t, &st2); // time to compare (where to start playing, usually)
	SystemTimeToFileTime(&st2, &ft2); 

	ret = CompareFileTime(&ftEnd, &ft2);
	if (ret > 0) {
		/* calculate the difference in time */
		CopyMemory(&uli2, &ft2, sizeof(FILETIME));
		/* check the diff beween file end time and the compare time */
		if ((uli2.QuadPart - uliEnd.QuadPart) < 20000000LL) {
			/* diff is less than 2 sec,
			 * let's don't bother to play the file for so little time,
			 * and the length is not so accurate.
			 */
			*offset = 0;
			return 0;
		}
		*offset = (uli2.QuadPart - uliStart.QuadPart) / 10LL; // 100's nanosec -> usec
	}

	return ret;
}

bool fileInfoLess(struct fileInfo* &a, struct fileInfo* &b)
{
	LONG ret;
	SYSTEMTIME st1, st2;
	FILETIME ft1, ft2;

	copyDvrTimeToSystemTime(&a->time, &st1);
	SystemTimeToFileTime(&st1, &ft1);

	copyDvrTimeToSystemTime(&b->time, &st2);
	SystemTimeToFileTime(&st2, &ft2);

	ret = CompareFileTime(&ft1, &ft2);

	if (ret == 0) { // same time
		ret = a->len - b->len; 
		if (ret == 0) { // same length
			ret = a->type - b->type;
			if (ret == 0) { // same type, this won't happen, as we added only "*.mp4" files
				ret = a->ch - b->ch;
				if (ret == 0) { // same channel
					ret = a->ext - b->ext;
				}
			}
		}
	}

	return (ret < 0);
}

bool fileInfoLessA(struct fileInfo* a, struct fileInfo* b)
{
	return fileInfoLess(a, b);
}

bool fileInfoRefLess(struct fileInfo &a, struct fileInfo &b)
{
	LONG ret;
	SYSTEMTIME st1, st2;
	FILETIME ft1, ft2;

	copyDvrTimeToSystemTime(&a.time, &st1);
	SystemTimeToFileTime(&st1, &ft1);

	copyDvrTimeToSystemTime(&b.time, &st2);
	SystemTimeToFileTime(&st2, &ft2);

	ret = CompareFileTime(&ft1, &ft2);

	if (ret == 0) { // same time
		ret = a.len - b.len; 
		if (ret == 0) { // same length
			ret = a.type - b.type;
			if (ret == 0) { // same type
				ret = a.ch - b.ch;
				if (ret == 0) { // same channel
					ret = a.ext - b.ext; // same ext won't happen, as we added only "*.mp4" files
				}
			}
		}
	}

	return (ret < 0);
}

bool fileInfoRefLessA(struct fileInfo a, struct fileInfo b)
{
	return fileInfoRefLess(a, b);
}
void printFile(struct fileInfo *fi)
{
	char  buf[512];

	sprintf_s(buf, sizeof(buf),
		"printFile:CH%02d_%04d%02d%02d%02d%02d%02d%03d_%u_%c.mp4(size:%d,offset:%d)",
				  fi->ch,
				  fi->time.year,
				  fi->time.month,
				  fi->time.day,
				  fi->time.hour,
				  fi->time.min,
				  fi->time.second,
				  fi->time.millisecond,
				  fi->len,
				  fi->type,
				  fi->filesize,
				  fi->fileoffset);
	_RPT1(_CRT_WARN, "%s\n", buf);

}
#endif
void UnixTimeToFileTime(time_t t, LPFILETIME pft)
{
	LONGLONG ll;

	ll = Int32x32To64(t, 10000000) + 116444736000000000;
	pft->dwLowDateTime = (DWORD)ll;
	pft->dwHighDateTime = ll >> 32;
}

// t in unix time * 1000000
void MicroTimeToFileTime(INT64 t, LPFILETIME pft)
{
	LONGLONG ll;

	ll = t * 10 + 116444736000000000;
	pft->dwLowDateTime = (DWORD)ll;
	pft->dwHighDateTime = ll >> 32;
}

INT64 GetMicroTimeFromDvrTime(struct dvrtime *t)
{
	SYSTEMTIME st;
	FILETIME ft;
	ULARGE_INTEGER uli;
	INT64 result;

	_RPT0(_CRT_WARN, "GetMicroTimeFromDvrTime:");
#ifdef _DEBUG
	printDvrtime(t);
#endif
	copyDvrTimeToSystemTime(t, &st);
	SystemTimeToFileTime(&st, &ft);
	CopyMemory(&uli, &ft, sizeof(FILETIME));

	result = (INT64)((uli.QuadPart - 116444736000000000ULL) / 10ULL);
	_RPT1(_CRT_WARN, "%I64d\n", result);

	// verification for debugging
	MicroTimeToFileTime(result, &ft);
	FileTimeToSystemTime(&ft, &st);
	struct dvrtime dt;
	copySystemTimeToDvrTime(&dt, &st);
#ifdef _DEBUG
	printDvrtime(&dt);
#endif
	return result;
}

