#MinGW makefile for PWManager
#
#
#

WINFLAGS = -I. -I../common -D_UNICODE -DUNICODE -D__MINGW__ -DWINVER=0x0501 -D_WIN32_IE=0x600
CFLAGS = $(WINFLAGS) -s -O2 -mwindows
LFLAGS = -lgdiplus -lcomctl32 -lversion -lole32 -luuid -lnetapi32 -lwinmm -lws2_32 -loleaut32 -static-libstdc++ -static-libgcc

TARGET = PWManager

SOURCES = $(TARGET).cpp \
		  ManagerDlg.cpp \
		  ../common/util.cpp

HEADERS = PWManager.h \
		  ManagerDlg.h \
		  DlgClTargetProgress.h \
		  DlgCleanTarget.h \
		  DlgSrcId.h \
		  resource.h \
		  ../common/cwin.h \
		  ../common/cdir.h \
		  ../common/cstr.h \
		  ../common/util.h

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))
		
$(TARGET).exe : $(OBJS) $(TARGET)_res.o
	g++ $(CFLAGS) -o $@ $^ ../Debug/tmeid.dll /c/windows/system32/urlmon.dll $(LFLAGS)
	cp $@ ../Debug/
	cp $@ ../Release/

$(TARGET)_res.o : $(TARGET).rc
	windres $(WINFLAGS) $(TARGET).rc $@

%.o : %.cpp $(HEADERS)
	g++ $(CFLAGS) -c $< -o $@
	
clean :
	rm -f $(TARGET).exe $(OBJS) *.o