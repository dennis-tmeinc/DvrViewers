#MinGW makefile for TMEID
#
#	remove #include <stdafx.h> line if compiler failed!!!
#

CFLAGS = -I. -I../common -D_UNICODE -DUNICODE -D__MINGW__ -DWINVER=0x0501 -D_WIN32_IE=0x600
LFLAGS = -lgdiplus -lcomctl32 -lversion -lole32 -luuid -lnetapi32 -lwinmm -lws2_32 -loleaut32 -static-libstdc++ -static-libgcc

SOURCES = tmeid.cpp diskid.cpp

HEADERS = resource.h ../common/cstr.h

tmeid.dll: $(SOURCES) $(HEADERS) resource.o
	g++ -g -shared $(CFLAGS) -o tmeid.dll $(SOURCES) resource.o $(LFLAGS)
	cp tmeid.dll ../Debug/
	cp tmeid.dll ../Release/

resource.o : tmeid.rc
	windres tmeid.rc resource.o

clean :
	rm tmeid.dll *.o
