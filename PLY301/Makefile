#MinGW makefile for PLY301.DLL
#
#	remove #include <stdafx.h> line if compiler failed!!!
#

WINFLAGS = -I. -I../common -D_UNICODE -DUNICODE -D__MINGW__ -DWINVER=0x0501 -D_WIN32_IE=0x600
CFLAGS = $(WINFLAGS) -s -O2 -shared
LFLAGS = -lgdiplus -lcomctl32 -lversion -lole32 -luuid -lnetapi32 -lwinmm -lws2_32 -lComdlg32 -loleaut32 -static-libstdc++ -static-libgcc

TARGET = ply301

SOURCES = ply301.cpp\
            dvrfilestream.cpp\
            dvrplaybackstream.cpp\
            dvrsetup.cpp\
            player.cpp\
            EnterDVRPassword.cpp\
            dvrharddrivestream.cpp\
            dvrplayer.cpp\
            h264filestream.cpp\
            h265filestream.cpp\
            dvrnet.cpp\
            dvrpreviewstream.cpp\
            avi_convertor/adpcm.cpp\
            avi_convertor/videoclip.cpp\
            avi_convertor/xvidenc.cpp\
            ../common/crypt.cpp

HEADERS = PLY301.h\
            dvrplayer.h\
            EnterDVRPassword.h\
            PlayM4.h\
            dvrnet.h\
            dvrpreviewstream.h\
            HikPlayM4.h\
            dvrfilestream.h\
            dvrplaybackstream.h\
            dvrsetup.h\
            player.h\
            dvrharddrivestream.h\
            h264filestream.h\
            resource.h\
            avi_convertor/adpcm.h\
            avi_convertor/inttypedef.h\
            avi_convertor/xvid.h\
            avi_convertor/dvrplayer.h\
            avi_convertor/videoclip.h\
            avi_convertor/xvidenc.h\
            ../common/crypt.h

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))
		
$(TARGET).dll : $(OBJS) $(TARGET)_res.o
	g++ $(CFLAGS) -o $@ $^ PlayCtrl.lib ../debug/xvidcore.dll $(LFLAGS)
	cp $@ ../Debug/
	cp $@ ../Release/

$(TARGET)_res.o : $(TARGET).rc
	windres $(WINFLAGS) $(TARGET).rc $@

%.o : %.cpp $(HEADERS)
	g++ $(CFLAGS) -c $< -o $@
	
clean :
	rm -f $(TARGET).dll $(OBJS) *.o
