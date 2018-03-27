#ifndef _TRACE_H_
#define _TRACE_H_

int dump(PBYTE s, int len);

#ifdef _DEBUG
void TRACE(LPCTSTR format, ...);
#define DUMP(buf, len) do { dump(buf, len); } while(0)
#else
#define TRACE
#define DUMP(buf, len)
#endif

#define __countof(array) (sizeof(array)/sizeof(array[0]))



#endif