// ply614.cpp : Defines the initialization routines for the DLL.
//

#include "stdafx.h"
#include "ply614.h"
#include "dvrharddrivestream.h"
#include "dvrpreviewstream.h"
#include "tme_thread.h"

#ifdef _DEBUG //for debug builds only
#define new DEBUG_NEW
#endif

#pragma comment(lib, "PlayCtrl.lib")
#pragma comment(lib, "version.lib")

//
//TODO: If this DLL is dynamically linked against the MFC DLLs,
//		any functions exported from this DLL which call into
//		MFC must have the AFX_MANAGE_STATE macro added at the
//		very beginning of the function.
//
//		For example:
//
//		extern "C" BOOL PASCAL EXPORT ExportedFunction()
//		{
//			AFX_MANAGE_STATE(AfxGetStaticModuleState());
//			// normal function body here
//		}
//
//		It is very important that this macro appear in each
//		function, prior to any calls into MFC.  This means that
//		it must appear as the first statement within the 
//		function, even before any object variable declarations
//		as their constructors may generate calls into the MFC
//		DLL.
//
//		Please see MFC Technical Notes 33 and 58 for additional
//		details.
//

#if 0 // from Visual Studio templete
// Cply614App

BEGIN_MESSAGE_MAP(Cply614App, CWinApp)
END_MESSAGE_MAP()


// Cply614App construction

Cply614App::Cply614App()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}


// The one and only Cply614App object

Cply614App theApp;


// Cply614App initialization

BOOL Cply614App::InitInstance()
{
	CWinApp::InitInstance();

	return TRUE;
}
#endif

struct dvr_device g_dvr[MAX_DVR_DEVICE] ;	// array of DVR devices have been found
int    g_dvr_number ;						// number of DVR devices

int		g_playerversion[4] ;
tme_object_t g_ipc;

// need this so  AfxGetApp() would not return NULL. which may crash the app.
//CWinApp player_app ;
int player_init()
{
	HMODULE hmod ;
	g_dvr_number = 0 ;

	TCHAR path[MAX_PATH];
	DWORD dummy, viSize;
	if( GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		(LPCTSTR)&player_init, &hmod ) ) {
			int ret = GetModuleFileName(hmod, path, MAX_PATH);
			if (ret) {
				viSize = GetFileVersionInfoSize(path, &dummy);
				if (viSize) {
					LPBYTE lpVersionInfo = new BYTE[viSize];
					GetFileVersionInfo(path, 0, viSize, lpVersionInfo);
					UINT uLen;
					VS_FIXEDFILEINFO *lpFfi;
					ret = VerQueryValue(lpVersionInfo, _T("\\"), (LPVOID *)&lpFfi, &uLen);
					DWORD dwFileVersionMS = lpFfi->dwFileVersionMS;
					DWORD dwFileVersionLS = lpFfi->dwFileVersionLS;
					delete [] lpVersionInfo;
					g_playerversion[0] = HIWORD(dwFileVersionMS);
					g_playerversion[1] = LOWORD(dwFileVersionMS);
					g_playerversion[2] = HIWORD(dwFileVersionLS);
					g_playerversion[3] = LOWORD(dwFileVersionLS);
				}
			}
	}
	tme_thread_init(&g_ipc);

	return 7;
}


int player_deinit()
{
	for (int i = 0; i < MAX_DVR_DEVICE; i++) {
		if (g_dvr[i].name == NULL)
			break;
		delete [] g_dvr[i].name;
	}

	g_dvr_number = 0 ;
	tme_mutex_destroy(&(g_ipc.object_lock));
	tme_cond_destroy(&(g_ipc.object_wait));
	return 0;
}

extern int net_finddevice();
extern int net_polldevice();

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

int fastforward( HPLAYER handle, int speed )
{
	if (speed > 3)
		return DVR_ERROR;
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		return player->fastforward(speed);
	}
	return DVR_ERROR;
}

int slowforward( HPLAYER handle, int speed )
{
	//if (speed > 4)
	//	return DVR_ERROR;
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

int savedvrfile( HPLAYER handle, struct dvrtime * begintime, struct dvrtime * endtime, char * duppath, int flags, struct dup_state * dupstate)
{
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		return player->savedvrfile(begintime, endtime, duppath, flags, dupstate);
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

int detectdevice(char *ipname)
{
	net_detectdevice(ipname);
	return 0;
}

int pwkeypad(HPLAYER handle, int keycode, int press) {
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		if (player->pwkeypad(keycode, press) ) {
            return DVR_ERROR_NONE ;
        }
	}
	return DVR_ERROR;
}

int getlocation(HPLAYER handle, char *locationbuffer, int buffersize)
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

int geteventtimelist( HPLAYER handle, struct dvrtime * date, int eventnum, int * elist, int elistsize)
{
	dvrplayer * player  ;
	player = (dvrplayer *)handle ;
	if( check_dvrplayer( player ) ) {
		return player->geteventtimelist( date, eventnum, elist, elistsize );
	}
	return DVR_ERROR;
}