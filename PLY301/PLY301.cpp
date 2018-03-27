// PLY301.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

#include "ply301.h"

#include <objidl.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib" )
using namespace Gdiplus;

#include "dvrharddrivestream.h"
#include "dvrpreviewstream.h"

// maximum devices supported

struct dvr_device g_dvr[MAX_DVR_DEVICE] ;	// array of DVR devices have been found
int    g_dvr_number ;						// number of DVR devices
int		g_playerversion[4] ;

int player_init()
{
	HMODULE hmod ;
	UINT uLen ;
	g_dvr_number = 0 ;
    VS_FIXEDFILEINFO * pverinfo ;
	if( GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		(LPCTSTR)&player_init, &hmod ) ) {
		HRSRC hres = FindResource(hmod, MAKEINTRESOURCE(VS_VERSION_INFO), RT_VERSION);
        HGLOBAL hglo=LoadResource( hmod, hres );
        WORD * pver = (WORD *)LockResource( hglo );
        if( VerQueryValue( pver, _T("\\"), (LPVOID *)&pverinfo, &uLen ) ) {
            if( pverinfo->dwSignature == VS_FFI_SIGNATURE) {
                g_playerversion[0]=(pverinfo->dwProductVersionMS)/0x10000 ;
                g_playerversion[1]=(pverinfo->dwProductVersionMS)%0x10000 ;
                g_playerversion[2]=(pverinfo->dwProductVersionLS)/0x10000 ;
                g_playerversion[3]=(pverinfo->dwProductVersionLS)%0x10000 ;
            }
        }
	}
	if( dvrplayer::keydata ) {
		delete dvrplayer::keydata ;
	}
	dvrplayer::keydata = NULL ;

	static ULONG_PTR gdiplusToken;
	GdiplusStartupInput gdiplusStartupInput;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	return 1;
}

int player_deinit()
{
	if( dvrplayer::keydata ) {
		delete dvrplayer::keydata ;
	}
	dvrplayer::keydata = NULL ;
	g_dvr_number = 0 ;
	return 0;
}

extern int net_finddevice();
extern int net_polldevice();
extern int net_detectdevice( char * ipaddr );

// find DVR devices
// flags - bit0: DVR server, bit1: local hard drive, bit2: Smart Server
//         flags = 0  for continue previous no finished finddevice()
// return total number of devices found. 0 for no device, <0 for error
int finddevice(int flags)
{
	int res ;
	if( flags==0 ) {
		if( net_polldevice() ) {
			return g_dvr_number ;
		}
		else {
			return DVR_ERROR_BUSY ;
		}
	}

	// start a new search
	g_dvr_number = 0 ;

	if( flags&DVR_DEVICE_DVR ) {		// bit 0, DVR server
		res = net_finddevice();
		if( res<0 ) {
			return res ;
		}
	}

	if( flags&DVR_DEVICE_HARDDRIVE ) {
		res = harddrive_finddevice() ;
		if( res<0 ) {
			return res ;
		}
	}

	if( flags&DVR_DEVICE_DVR ) {		// bit 0, DVR server
		return finddevice(0);
	}
	else {
		return g_dvr_number ;
	}
}

int detectdevice(char *ipname)
{
	net_detectdevice( ipname );
	return 0 ;
}

int polldevice()
{
	return g_dvr_number;
}

int initsmartserver( HPLAYER handle )
{
	return DVR_ERROR;
}

HPLAYER openremote(char * netname, int opentype ) 
{
	dvrplayer * player ;
	player = new dvrplayer ;
	if( player->openremote( netname, opentype ) ) {
		return (HPLAYER) player ;
	}
	delete player ;
	return NULL ;
}

int scanharddrive(char * drives, char **servernames, int maxcount)
{
	return DVR_ERROR;
}

int check_dvrplayer(dvrplayer * player)
{
	if( player==NULL )
		return 0 ;
	return 1 ;
}

HPLAYER openharddrive( char * path ) 
{
	dvrplayer * player ;
	player = new dvrplayer ;
	if( player->openharddrive( path ) ) {
		return (HPLAYER) player ;
	}
	delete player ;
	return NULL ;
}

HPLAYER openlocalserver( char * servername ) 
{
	dvrplayer * player ;
	player = new dvrplayer ;
	if( player->openlocalserver( servername ) ) {
		return (HPLAYER) player ;
	}
	delete player ;
	return NULL ;
}

HPLAYER openfile( char * dvrfilename )
{
	dvrplayer * player ;
	player = new dvrplayer ;
	if( player->openfile( dvrfilename ) ) {
		return (HPLAYER) player ;
	}
	delete player ;
	return NULL;
}

HPLAYER opendevice(int index, int opentype ) 
{
	if( index<g_dvr_number ) {
		if( g_dvr[index].type==DVR_DEVICE_TYPE_DVR ) {
			return openremote(g_dvr[index].name, opentype);
		}
		else if( g_dvr[index].type==DVR_DEVICE_TYPE_LOCALSERVER ) {
			return openlocalserver(g_dvr[index].name);
		}
		else if( g_dvr[index].type==DVR_DEVICE_TYPE_HARDDRIVE ) {
			return openharddrive(g_dvr[index].name);
		}
	}
	return NULL ;
}

int player_close(HPLAYER handle)
{
	dvrplayer * player ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		delete player ;
	}
	return 0 ;
}

int getplayerinfo(HPLAYER handle, struct player_info * pplayerinfo )
{
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( pplayerinfo->size>=sizeof(struct player_info) ) {
		pplayerinfo->playerversion[0]=g_playerversion[0];
		pplayerinfo->playerversion[1]=g_playerversion[1];
		pplayerinfo->playerversion[2]=g_playerversion[2];
		pplayerinfo->playerversion[3]=g_playerversion[3];
	}
	if( check_dvrplayer( player ) ) {
		return player->getplayerinfo(pplayerinfo);
	}
	else if( player==NULL ) {
		pplayerinfo->features = PLYFEATURE_REALTIME | PLYFEATURE_PLAYBACK | PLYFEATURE_REMOTE  |PLYFEATURE_HARDDRIVE ;
		return 0 ;
	}
	return DVR_ERROR;
}

int setuserpassword( HPLAYER handle, char * username, void * password, int passwordsize )
{
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		return player->setuserpassword( username, password, passwordsize );
	}
	return DVR_ERROR;
}

int setvideokey( HPLAYER handle, void * key, int keysize )
{
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		return player->setvideokey( key, keysize );
	}
	return DVR_ERROR;
}

int getchannelinfo(HPLAYER handle, int channel, struct channel_info * pchannelinfo )
{
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		return player->getchannelinfo( channel, pchannelinfo );
	}
	return DVR_ERROR;
}

int attach(HPLAYER handle, int channel, HWND hwnd )
{
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		return player->attach( channel, hwnd );
	}
	return DVR_ERROR;
}

int detach(HPLAYER handle)
{
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		return player->detach();
	}
	return DVR_ERROR;
}

int detachwindow(HPLAYER handle, HWND hwnd )
{
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		return player->detachwindow( hwnd );
	}
	return DVR_ERROR;
}

int play(HPLAYER handle)
{
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		return player->play();
	}
	return DVR_ERROR;
}

int audioon(HPLAYER handle, int channel, int audioon)
{
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		return player->audioon(channel,audioon );
	}
	return DVR_ERROR;
}

int audioonwindow(HPLAYER handle, HWND hwnd, int audioon)
{
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		return player->audioonwindow(hwnd,audioon );
	}
	return DVR_ERROR;
}

int pause(HPLAYER handle)
{
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		return player->pause();
	}
	return DVR_ERROR;
}

int stop(HPLAYER handle)
{
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		return player->stop();
	}
	return DVR_ERROR;
}

int refresh(HPLAYER handle, int channel)
{
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		return player->refresh(channel);
	}
	return DVR_ERROR;
}

int fastforward( HPLAYER handle, int speed )
{
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		return player->fastforward(speed);
	}
	return DVR_ERROR;
}

int slowforward( HPLAYER handle, int speed )
{
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		return player->slowforward(speed);
	}
	return DVR_ERROR;
}

int oneframeforward( HPLAYER handle )
{
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		return player->oneframeforward();
	}
	return DVR_ERROR;
}

int getdecframes( HPLAYER handle )
{
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		return player->getdecframes();
	}
	return 0;
}

int backward( HPLAYER handle, int speed )
{
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		return player->backward(speed);
	}
	return DVR_ERROR;
}

int capture( HPLAYER handle, int channel, char * capturefilename )
{
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		return player->capture(channel, capturefilename);
	}
	return DVR_ERROR;
}

int seek( HPLAYER handle, struct dvrtime * dvrt )
{
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		return player->seek(dvrt);
	}
	return DVR_ERROR;
}

int getcurrenttime( HPLAYER handle, struct dvrtime * dvrt )
{
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		return player->getcurrenttime(dvrt);
	}
	return DVR_ERROR;
}

int getcliptimeinfo( HPLAYER handle, int channel, struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize)
{
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		return player->getcliptimeinfo(channel, begintime, endtime, tinfo, tinfosize);
	}
	return DVR_ERROR;
}

int getclipdayinfo( HPLAYER handle, int channel, dvrtime * daytime ) 
{
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		return player->getclipdayinfo(channel, daytime);
	}
	return DVR_ERROR;
}

int getlockfiletimeinfo( HPLAYER handle, int channel, struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize)
{
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		return player->getlockfiletimeinfo(channel, begintime, endtime, tinfo, tinfosize);
	}
	return DVR_ERROR;
}

int geteventtimelist( HPLAYER handle, struct dvrtime * date, int eventnum, int * elist, int elistsize)
{
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		return player->geteventtimelist( date, eventnum, elist, elistsize );
	}
	return DVR_ERROR;
}

int savedvrfile( HPLAYER handle, struct dvrtime * begintime, struct dvrtime * endtime, char * duppath, int flags, struct dup_state * dupstate)
{
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		return player->savedvrfile(begintime, endtime, duppath, flags, dupstate);
	}
	return DVR_ERROR;
}

int savedvrfile2(HPLAYER handle, struct dvrtime * begintime, struct dvrtime * endtime, char * duppath, int flags, struct dup_state * dupstate, char * channels, char * password)
{
	dvrplayer * player;
	player = (dvrplayer *)handle;
	if (check_dvrplayer(player)) {
		return player->savedvrfile2(begintime, endtime, duppath, flags, dupstate, channels, password);
	}
	return DVR_ERROR;
}

int dupvideo(HPLAYER handle, struct dvrtime * begintime, struct dvrtime * endtime, char * duppath, int flags, struct dup_state * dupstate)
{
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		return player->dupvideo(begintime, endtime, duppath, flags, dupstate);
	}
	return DVR_ERROR;
}

int configureDVR(HPLAYER handle)
{
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		return player->configureDVR();
	}
	return DVR_ERROR;
}

int senddvrdata(HPLAYER handle, int protocol, void * sendbuf, int sendsize)
{
	dvrplayer * player  ;

	if( dvrplayer::keydata!=NULL ) {
		delete (char *) dvrplayer::keydata ;
		dvrplayer::keydata=NULL ;
	}
	if( sendsize > sizeof(struct key_data) && 
		sendsize < 8000 ) {
		dvrplayer::keydata = (struct key_data *)new char [sendsize] ;
		memcpy( dvrplayer::keydata, sendbuf, sendsize ) ;
	}

	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		return player->senddvrdata(protocol, sendbuf, sendsize);
	}
	return DVR_ERROR;
}

int recvdvrdata(HPLAYER handle, int protocol, void ** precvbuf, int * precvsize)
{
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		return player->recvdvrdata(protocol, precvbuf, precvsize);
	}
	return DVR_ERROR;
}

int freedvrdata(HPLAYER handle, void * dvrbuf)
{
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		return player->freedvrdata(dvrbuf);
	}
	return DVR_ERROR;
}

int pwkeypad(HPLAYER handle, int keycode, int press)
{
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		if (player->pwkeypad(keycode, press) ) {
            return DVR_ERROR_NONE ;
        }
	}
	return DVR_ERROR;
}

int getlocation(HPLAYER handle, char * locationbuffer, int buffersize)
{
    dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
        return player->getlocation(locationbuffer, buffersize);
	}
	return DVR_ERROR;
}

int setblurarea(HPLAYER handle, int channel, struct blur_area * ba, int banumber )
{
    dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
        return player->setblurarea(channel, ba, banumber );
	}
	return DVR_ERROR;
}

int clearblurarea(HPLAYER handle, int channel )
{
    dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
        return player->clearblurarea(channel);
	}
	return DVR_ERROR;
}

// showzoomin
//   Zoom in display with the given area 
// Parameter:
//	handle	-	player handle
//	channel -	player channel to set zoom in mode
//	za	-	zoom in area to display,  percentage value related to original video
//  Return:
//	0:		success
// 	-1:		function failed (not supporting zoom in feature)
// Note
//	the structure zoom_area define a rectangle area related to original video
//	 zoom_area members are floating point value in percentage of screen width or height (0-1)
//	top-left value of origina video  is (0,0), right-bottom value of original video is (1,1)
//	give zoom area value of (x=0, y=0,z=1) will do a full video screen zoom out
int showzoomin(HPLAYER handle, int channel, struct zoom_area * za)
{
	dvrplayer * player;
	player = (dvrplayer *)handle;
	if (check_dvrplayer(player)) {
		return player->showzoomin(channel, za);
	}
	return DVR_ERROR;
}

// draw a line on video screen
int drawline(HPLAYER handle, int channel, float x1, float y1, float x2, float y2)
{
	dvrplayer * player;
	player = (dvrplayer *)handle;
	if (check_dvrplayer(player)) {
		return player->drawline(channel, x1, y1, x2, y2);
	}
	return DVR_ERROR;
}

// clear all lines previously draw
int clearlines(HPLAYER handle, int channel)
{
	dvrplayer * player;
	player = (dvrplayer *)handle;
	if (check_dvrplayer(player)) {
		return player->clearlines(channel);
	}
	return DVR_ERROR;
}

int setdrawcolor(HPLAYER handle, int channel, COLORREF color)
{
	dvrplayer * player;
	player = (dvrplayer *)handle;
	if (check_dvrplayer(player)) {
		return player->setdrawcolor(channel,color);
	}
	return DVR_ERROR;
}

// draw a rectangel indicator
int drawrectangle(HPLAYER handle, int channel, float x, float y, float w, float h)
{
	// use draw line 
	clearlines(handle, channel);
	if (w > 0.001) {
		drawline(handle, channel, x, y, x + w, y);
		drawline(handle, channel, x + w, y, x + w, y + h);
		drawline(handle, channel, x + w, y + h, x, y + h);
		drawline(handle, channel, x, y + h, x, y);
	}
	return DVR_OK;
}

// select a focused decoder (window)
int selectfocus(HPLAYER handle, HWND hwnd)
{
	dvrplayer * player;
	player = (dvrplayer *)handle;
	if (check_dvrplayer(player)) {
		return player->selectfocus(hwnd);
	}
	return DVR_ERROR;
}
