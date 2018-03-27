#include "stdafx.h"
#include <stdio.h>
#include "trace.h"

#ifdef _DEBUG
void TRACE(LPCTSTR format, ...)
{
	va_list args;
	va_start(args, format);

	int length;
	TCHAR buffer[512];
	
	length = _vsntprintf(buffer, __countof(buffer), format, args);
	_ASSERT(length >= 0);

	if (length > 0)
	{
		OutputDebugString(buffer);
	}
	va_end(args);
}

#define WIDTH   16
#define DBUFSIZE 1024
int dump(unsigned char *s, int len)
{
    char buf[DBUFSIZE],lbuf[DBUFSIZE],rbuf[DBUFSIZE];
    BYTE *p;
    int line,i;

    p =(PBYTE)s;
    for(line = 1; len > 0; len -= WIDTH, line++) {
        memset(lbuf,0,DBUFSIZE);
        memset(rbuf,0,DBUFSIZE);
        for(i = 0; i < WIDTH && len > i; i++,p++) {
            sprintf(buf,"%02x ",(BYTE) *p);
            strcat(lbuf,buf);
            sprintf(buf,"%c",(!iscntrl(*p) && *p <= 0x7f) ? *p : '.');
            strcat(rbuf,buf);
        }
        _RPT4(_CRT_WARN, "%04x: %-*s    %s\n",line - 1, WIDTH * 3, lbuf, rbuf);
    }
    if(!(len%16))
		_RPT0(_CRT_WARN, "\n");
    return line;
}
#endif