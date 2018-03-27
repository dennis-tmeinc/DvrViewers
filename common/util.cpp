// util.cpp : implementation file
//

#include <stdafx.h>

#define _WINSOCK2_H

#pragma comment (lib, "Ws2_32.lib")

// Windows Header Files:
#include <Commdlg.h>
#include <winsock2.h>
#include <winioctl.h>
#include <ws2tcpip.h>
#include <Shellapi.h>

// C RunTime Header Files
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <string.h>
#include <direct.h>

#include "util.h"
#include "cwin.h"
#include "cdir.h"

// global functions

void dbgout( char * format, ... ) 
{
    string dbgstr ;
    va_list ap ;
    va_start( ap, format );
    vsnprintf(dbgstr.strsize(1024), 1024, format, ap );
    OutputDebugString(dbgstr);
    va_end( ap );
}

static char subkey[] = "Software\\TME\\" ;
int reg_save( char * appname, char * name, char * value )
{
	string pwsubkey(subkey);
	pwsubkey += appname ;
    HKEY  key = NULL ;
    if( RegCreateKeyEx( HKEY_CURRENT_USER, pwsubkey, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL)!=ERROR_SUCCESS ) {
        return 0 ;
    }

	string sname = name;
	string svalue = value;
    
	if (value == NULL || strlen(value) == 0) {
		RegDeleteValue(key, sname);
	}
	else {
		RegSetValueEx(key, sname, 0, REG_SZ, (BYTE*)(wchar_t *)svalue, sizeof(wchar_t)*(wcslen(svalue) + 1));
	}

    RegCloseKey( key );
    return 1 ;
}

// save integer
int reg_save(char * appname, char * name, int value)
{
	if (value == -1) {
		return reg_save(appname, name, (char *)NULL);
	}
	else {
		return reg_save(appname, name, (char *)TMPSTR("%d", value) );
	}
}

int reg_read( char * appname, char * name, string &value )
{
	string pwsubkey(subkey);
	pwsubkey += appname ;
    HKEY  key = NULL ;
    if( RegCreateKeyEx( HKEY_CURRENT_USER, pwsubkey, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL)!=ERROR_SUCCESS ) {
        return 0 ;
    }

    DWORD type, rsize ;
    string sname, svalue ;
    sname = name ;
    svalue.wcssize( 200 );
    rsize = sizeof(wchar_t)*200 ;
    type = REG_SZ ;

    if( RegQueryValueEx( key, sname, 0, &type, (BYTE *)(wchar_t *)svalue, &rsize )==ERROR_SUCCESS ){
		value = svalue ;
        RegCloseKey( key );
        return 1;
    }
    else {
        RegCloseKey( key );
        return 0 ;
    }
}

// read integer value, for error return -1 ;
int reg_read( char * appname, char * name )
{
	int r ;
	string ivalue ;
	if( reg_read( appname, name, ivalue ) ) {
		if( sscanf( ivalue, "%d", &r )==1 ) {
			return r ;
		}
	}
	return -1 ;
}

static char _appname[64] = "";

// get application name
char * getappname()
{
	if( _appname[0] == 0 ) {
		char * pappname ;
		string tpath ;
		GetModuleFileName(NULL, tpath.tcssize(512), 511 );
		pappname = (char *)tpath ;
		char * slash = strrchr( pappname, '\\' );
		if( slash ) {
			pappname = slash+1 ;
		}
		char * dot = strrchr( pappname, '.' );
		if( dot!=NULL ) {
			*dot = 0 ;
		}
		strncpy( _appname, pappname, 64 );
	}
	return _appname ;
}

// over loaded registrey functions
int reg_save( char * name, char * value )
{
	return reg_save(getappname(), name, value );
}

// save integer
int reg_save( char * name, int value )
{
	return reg_save(getappname(), name, value );
}

int reg_read( char * name, string &value )
{
	return reg_read(getappname(), name, value );
}

// read integer value, for error return -1 ;
int reg_read( char * name )
{
	int r ;
	string ivalue ;
	if( reg_read( name, ivalue ) ) {
		if( sscanf( (char *)ivalue, "%d", &r )==1 ) {
			return r ;
		}
	}
	return -1 ;
}

void * option_open()
{
	char * ext ;
	string app_path ;
    GetModuleFileName(NULL, app_path.tcssize(512), 511 );
	ext = strrchr( (char *)app_path , '.' ) ;
	if( ext != NULL ) {
		*ext = 0 ;
	}
	app_path += ".option" ;
	FILE * of = fopen( ((char *)app_path), "r" );
	return (void *)of ;
}

void option_close( void * op )
{
	fclose( (FILE *)op );
}

string * option_get( void * op, char * key )
{
	char * li ;
	char * eq ;
	string line ;
	string lkey ;
	while( fgets(line.strsize(1024), 1024, (FILE *)op) ) {
		li = (char *)line ;
		eq = strchr( li, '=' );
		if( eq ) {
			*eq=0 ;
			lkey = li ;
			lkey.trim();
			if( strcmp( lkey, key )==0 ) {
				string * r = new string ;
				*r = eq+1 ;
				r->trim();
				return r ;
			}
		}
	}
	return NULL ;
}

string * option_getvalue( char * key )
{
	string * res = NULL ;
	void * ofile = option_open();
	if( ofile != NULL ) {
		res = option_get( ofile, key ) ;
		option_close( ofile );
	}
	return res ;
}

int time_compare(struct dvrtime & t1, struct dvrtime &t2)
{
	if( t1.year == t2.year ) {
		if( t1.month == t2.month ) {
			if( t1.day == t2.day) {
				if( t1.hour == t2.hour ) {
					if( t1.min == t2.min ) {
						if( t1.second == t2.second ) {
							return t1.millisecond - t2.millisecond ;
						}
						return t1.second - t2.second ;
					}
					return t1.min - t2.min ;
				}
				return t1.hour - t2.hour ;
			}
			return t1.day - t2.day ;
		}
		return t1.month - t2.month ;
	}
	return t1.year - t2.year ;
}

struct dvrtime time_now()
{
	struct dvrtime now ;
	SYSTEMTIME st ;
	GetLocalTime( &st );
	time_timedvr( &st, &now );
	return now ;
}

struct dvrtime * time_timedvr(SYSTEMTIME * psystime, struct dvrtime * dt)
{
    dt->year = psystime->wYear ;
    dt->month = psystime->wMonth ;
    dt->day = psystime->wDay ;
    dt->hour = psystime->wHour ;
    dt->min = psystime->wMinute ;
    dt->second = psystime->wSecond ;
    dt->millisecond = psystime->wMilliseconds ;
    dt->tz = 0 ;
	return dt ;
}

SYSTEMTIME * time_dvrtime(struct dvrtime * dt, SYSTEMTIME * psystime)
{
    psystime->wYear = dt->year ;
    psystime->wMonth = dt->month ;
    psystime->wDay = dt->day ;
    psystime->wHour = dt->hour ;
    psystime->wMinute = dt->min ;
    psystime->wSecond = dt->second ;
    psystime->wMilliseconds = dt->millisecond ;
    psystime->wDayOfWeek = 0 ;
	return psystime ;
}

// return difference in seconds (t1-t2)
int operator- ( struct dvrtime &t1, struct dvrtime &t2)
{
	LARGE_INTEGER lt1, lt2;
	SYSTEMTIME st ;
	FILETIME ft;
	time_dvrtime(&t1,&st);
	SystemTimeToFileTime(&st,&ft);
	lt1.LowPart = ft.dwLowDateTime ;
	lt1.HighPart = ft.dwHighDateTime ;
	time_dvrtime(&t2,&st);
	SystemTimeToFileTime(&st,&ft);
	lt2.LowPart = ft.dwLowDateTime ;
	lt2.HighPart = ft.dwHighDateTime ;
	return (int)( (lt1.QuadPart-lt2.QuadPart)/10000000 ) ;
}

// return seconds differce
void time_add(struct dvrtime &t, int seconds)
{
	FILETIME ft ;
	LARGE_INTEGER lt;
	SYSTEMTIME st ;
	time_dvrtime(&t,&st);
	SystemTimeToFileTime(&st,&ft);
	lt.LowPart = ft.dwLowDateTime ;
	lt.HighPart = ft.dwHighDateTime ;
	lt.QuadPart+=((INT64)seconds) * 10000000 ;
	ft.dwLowDateTime = lt.LowPart ;
	ft.dwHighDateTime = lt.HighPart ;
	FileTimeToSystemTime(&ft, &st);
	time_timedvr(&st,&t);
}

static char _apppath[256] = "";
// convert appfile name to full directory filename located in app directory
char * getapppath()
{
	if( _apppath[0] == 0 ) {
		char * pappname ;
		string mpath ;
		GetModuleFileName(NULL, mpath.tcssize(500), 500 );
		pappname = (char *)mpath ;
		char * slash = strrchr( pappname, '\\' );
		if( slash ) {
			*slash = 0 ;
		}
		strncpy( _apppath, pappname, 256 );
	}
	return _apppath ;
}

// convert appfile name to full directory filename located in app directory
int getfilepath(string & appfile)
{
	string apppath;
	apppath.printf("%s\\%s", getapppath(), (char *)appfile) ;
	appfile = apppath;
	return 1 ;
}

static DWORD portable_tag[2] = {
	PORTABLE_FILE_TAG1,
	PORTABLE_FILE_TAG2
};

// duplicate portable copy of files
int copyportablefiles(char * targetfile, char * password)
{
	string targetfolder ;
	string listfile ;

	// find folder name form targetfile
	char * slash ;
	targetfolder = targetfile ;
	slash = strrchr(targetfolder, '\\');
	if( slash!=NULL ) {
		FILE * fdfile ;

		// put target file name to fname
		string fname = (slash+1) ;
		*(slash+1) = 0 ;

		// create portable autoplay executable file folder
		mkdir(TMPSTR("%splayer", (char *)targetfolder));

		// create portable autoplay file
		fdfile = fopen(TMPSTR("%splayer\\portableplay", (char *)targetfolder), "w" );
		if( fdfile != NULL ) {
			fprintf(fdfile, "%s", (char *)fname);
			fclose(fdfile);
		}

		string app_path;
		GetModuleFileName(NULL, app_path.tcssize(512), 511);
		string _app_path_clone = (char *)app_path;
		char * app_dir = (char *)_app_path_clone;
		char * slash = strrchr(app_dir, '\\');
		*slash = 0;
		char * app_file = slash + 1;

		// 		CopyFile(app_path, TMPSTR("%splayer\\%s", (char *)targetfolder, app_file), TRUE);

		int r = 0;	// rsize ;
		FILE * fapp = fopen(app_path, "rb");
		if (fapp) {
			fseek(fapp, 0, SEEK_END);
			r = ftell(fapp);
			char * fbuf = new char[r];
			fseek(fapp, 0, SEEK_SET);
			r = fread(fbuf, 1, r, fapp);
			int x;
			DWORD * dbuf = (DWORD *)fbuf;
			for (x = 0; x < r / sizeof(DWORD); x++) {
				if (dbuf[x] == PORTABLE_FILE_TAG1 && dbuf[x + 1] == PORTABLE_FILE_TAG2) {
					dbuf[x + 1] = PORTABLE_FILE_TAG3;
					break;
				}
			}
			fclose(fapp);
			fapp = fopen(TMPSTR("%splayer\\PortableViewer.exe", (char *)targetfolder), "wb");
			if (fapp) {
				fwrite(fbuf, 1, r, fapp);
				fclose(fapp);
			}
			delete fbuf;
		}

		dirfind dir(app_dir, "*.dll");
		while (dir.find())
		{
			CopyFile(dir.filepath(), TMPSTR("%splayer\\%s", (char *)targetfolder, (char *)dir.filename()), TRUE);
		}
		dir.close();

		// to copy necessary files other than .exe and .dll
		listfile = "portablelist" ;
		getfilepath(listfile);
		FILE * flistfile = fopen( listfile, "r");
		if( flistfile ) {
			while( fgets(fname.strsize(500), 499, flistfile ) ) {
				fname.trim();
				string dfile;
				dfile.printf("%splayer\\%s", (char *)targetfolder, (char *)fname );
				getfilepath(fname);
				if( strcmp(fname, dfile )==0 ) {
					break;
				}
				CopyFile(fname,dfile,TRUE);
			}
			fclose( flistfile );
		}
		
//		CreateLink(  TMPSTR(".\\player\\PortableViewer.exe"), TMPSTR("%sPortableViewer.lnk", (char *)targetfolder), TMPSTR("Portable Dvr Viewer"));

		// create shortcut lnk
		/*
		sprintf(sfile.strsize(600), "%splayer\\PortableViewer.exe", (char *)targetfolder );
		slash = (char *) sfile ;
		if( slash[1]==':' ) {
			slash +=2 ;
		}
		sprintf(dfile.strsize(600), "%splayer.lnk", (char *)targetfolder );
		CreateLink( slash, dfile, "Player");
		*/
	}
	return 0;
}

int isportableapp()
{
	return portable_tag[0] == PORTABLE_FILE_TAG1 && portable_tag[1] == PORTABLE_FILE_TAG3;
}

/*
// load an image to CImage
int	loadimg( CImage * pimg, LPCTSTR resource )
{
	HRSRC  hres = ::FindResource(AppInst(),  resource, _T("CUSTOM") );
	if( hres == NULL ) {
		return  0;
	}
	HGLOBAL hresmem = ::LoadResource( AppInst(), hres );
	if( hresmem== NULL ) {
		return 0;
	}
	LPVOID pres =LockResource( hresmem );
	DWORD sizeres = SizeofResource(AppInst(), hres );
	MemoryStream memoryStream( pres, sizeres) ;
	if( !pimg->IsNull() ) {
		pimg->Destroy();
	}
	HRESULT result= pimg->Load(&memoryStream);

	// Image with alpha channel need to be pre-multiplied, so image can works on GDI api, (alphablend, etc)
	if( result==S_OK && pimg->IsDIBSection() && pimg->IsTransparencySupported() ) {
		int width = pimg->GetWidth();
		int height = pimg->GetHeight();
		int pitch = pimg->GetPitch();
		int abspitch = pitch ;
		if( abspitch<0 ) {
			abspitch=-abspitch;
		}
		if( abspitch>=4*width && pimg->GetBPP()==32) {			// 4 bytes per pixel ( with alpha )
			int line ;
			unsigned char * bits = (unsigned char *)pimg->GetBits();
			if( bits!=NULL ) {
				for( line=0; line<height; line++ ) {
					int i ;
					for( i=0; i<abspitch; i+=4) {
						bits[i+0] = ((unsigned)bits[i+0]) * ((unsigned)bits[i+3]) / 255 ;
						bits[i+1] = ((unsigned)bits[i+1]) * ((unsigned)bits[i+3]) / 255 ;
						bits[i+2] = ((unsigned)bits[i+2]) * ((unsigned)bits[i+3]) / 255 ;
					}
					bits+=pitch ;
				}
			}
		}
	}
	return (result==S_OK);
}
*/

struct sockad {
    union {
	    sockaddr addr;
        sockaddr_storage addrst ;
    };
	int    len;
};

// convert computer name (string) to sockad ( cp name example: 192.168.42.227 or 192.168.42.227:15111 )
// return 0 for success, -1 fail
static int net_sockaddr(struct sockad * sad, char * cpname, int port)
{
	int result = -1 ;
	char ipaddr[256] ;
	char servname[128] ;
	struct addrinfo * res ;
	struct addrinfo hint ;
	char * portonname ;
	memset(&hint, 0, sizeof(hint));
	hint.ai_family = PF_INET ;
	hint.ai_socktype = SOCK_STREAM ;
	strncpy( ipaddr, cpname, 255 );
	ipaddr[255]=0 ;
	portonname = strchr(ipaddr, ':');
	if( portonname ) {
		strncpy( servname, portonname+1, 120 );
		*portonname=0 ;
	}
	else {
		sprintf(servname, "%d", port);
	}
	if( getaddrinfo( ipaddr, servname, &hint, &res )==0 ) {
		if( res->ai_socktype == SOCK_STREAM ) {
			sad->len = res->ai_addrlen ;
			memcpy(&(sad->addr), res->ai_addr, sad->len );
			result = 0 ;
		}
		freeaddrinfo(res);
	}
	return result ;
}

static SOCKET net_connect( char * dvrserver, int port=80 )
{
	SOCKET so ;
	struct sockad soad ;
	if( net_sockaddr( &soad, dvrserver, port )==0 ) {
		so=socket(AF_INET, SOCK_STREAM, 0);
		if( connect(so, &(soad.addr), soad.len )==0 ) {
			return so ;
		}
		closesocket(so);
	}
	return 0 ;
}

// return 1 if socket ready to read, 0 for time out
static int net_wait(SOCKET sock, int timeout)
{
	struct timeval tout ;
	fd_set set ;
	FD_ZERO(&set);
	FD_SET(sock, &set);
	tout.tv_sec=timeout/1000000 ;
	tout.tv_usec=timeout%1000000 ;
	return (select( (int)sock+1, &set, NULL, NULL, &tout))>0 ;
}

static void net_send( SOCKET sock, void * buf, int bufsize )
{
	int slen ;
	char * cbuf=(char *)buf ;
	while( bufsize>0 ) {
		slen=send(sock, cbuf, bufsize, 0);
		if( slen<0 ) {
            int nError=WSAGetLastError();
            if( nError!=WSAEWOULDBLOCK) {
                return ;
            }
        }
        else {
            bufsize-=slen ;
            cbuf+=slen ;
        }
	}
}

// recv network data until buffer filled. 
// return :
//      size of data read
static int net_recv( SOCKET sock, void * buf, int bufsize, int wait )
{
    int rsize = 0 ;
	int recvlen ;
	while( rsize<bufsize ) {
		if( net_wait(sock, wait)>0 ) {			// data available? 
			recvlen=recv(sock, ((char *)buf)+rsize, bufsize-rsize, 0 );
			if( recvlen>0 ) {
				rsize+=recvlen ;
			}
            else if ( recvlen==0 ) {
                // socket closed
                break;
            }
			else {
                int nError=WSAGetLastError();
                if( nError!=WSAEWOULDBLOCK ) {
                    break;
                }
                else {
                    Sleep(10);
                }
            }
		}
		else {
            break;
		}
	}
	return rsize;						
}

static int httpget( char * url, char * buffer, int buffersize)
{
    int rsize=0 ;
    SOCKET httpsocket ;
    // get host
    char * endofhost = strchr( url+7, '/' );
    if( endofhost ) {
        *endofhost=0 ;
        string host (url+7) ;
        *endofhost='/' ;
        httpsocket=net_connect( host );
        if( httpsocket ) {
			string req;
			req.printf("GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", endofhost, (char *)host);
            net_send(httpsocket, (char *)req, strlen((char *)req));
            rsize = net_recv(httpsocket, (void *)buffer, buffersize, 2000000 );
            closesocket(httpsocket);
        }
    }
    return rsize ;
}

// load an image, (original)
Bitmap * loadimage( LPCTSTR resource )
{
    Bitmap * img = NULL ;
    MemoryStream *memoryStream ;
    string res ;
	res = resource ;
    if( strncmp(res, "http://", 7 )==0 ) {
//      r2="http://maps.googleapis.com/maps/api/staticmap?center=43.641666,-79.672043&zoom=14&size=512x512&maptype=roadmap&markers=size:small|43.641666,-79.672043&markers=size:tiny|43.642666,-79.672043&markers=43.643666,-79.672043&sensor=false";
        char * httpbuffer ;
        char * httpimg ;
        httpbuffer = new char [2000000];
        int imgsize = httpget( res, httpbuffer, 2000000 );
        if( imgsize>10 ) {
            httpimg = strstr(httpbuffer, "\r\n\r\n");
            if( httpimg ) {
                httpimg+=4 ;
                imgsize-=httpimg-httpbuffer ;
                memoryStream=new MemoryStream( httpimg, imgsize, 1 );
                img = Bitmap::FromStream(memoryStream) ;
                memoryStream->Release();
            }
        }
        delete [] httpbuffer;
        return img ;
    }

    // ELSE

	HRSRC  hres = ::FindResource(Window::ResInst(),  resource, _T("CUSTOM") );
	if( hres == NULL ) {
		return  NULL;
	}
	HGLOBAL hresmem = ::LoadResource( Window::ResInst(), hres );
	if( hresmem== NULL ) {
		return NULL;
	}

	LPVOID pres =LockResource( hresmem );
	DWORD sizeres = SizeofResource(Window::ResInst(), hres );
	memoryStream=new MemoryStream( pres, sizeres, 1 ) ;
    img = Bitmap::FromStream(memoryStream) ;
    memoryStream->Release();
    if( img ) {
        if( img->GetLastStatus() == Ok ) {
            return img ;
        }
        else {
            delete img ;
            return NULL ;
        }
    }
    return NULL ;
}


// load bitmap, duplicated a new 32 bits bmp to make it strechable
Bitmap*	loadbitmap( LPCTSTR resource )
{
    Bitmap * img = loadimage(resource);
    if( img ) {
        BitmapData bitmapDataSource ;
        BitmapData bitmapDataDest ;
        Rect rect(0, 0,  img->GetWidth(), img->GetHeight());
        img->LockBits( &rect, ImageLockModeRead, PixelFormat32bppARGB, &bitmapDataSource );
        Bitmap * bmp = new Bitmap(bitmapDataSource.Width, bitmapDataSource.Height, PixelFormat32bppARGB );
        bitmapDataDest = bitmapDataSource ;
        bitmapDataDest.Reserved = NULL;
        bmp->LockBits( &rect, 
            ImageLockModeWrite|ImageLockModeUserInputBuf,
            PixelFormat32bppARGB,
            &bitmapDataDest);
        bmp->UnlockBits( &bitmapDataDest );
        img->UnlockBits( &bitmapDataSource );
        delete img ;
        return bmp ;
    }
    return NULL ;
}

int GetEncoderClsid(LPCTSTR filename, CLSID* pClsid)
{
   UINT  num = 0;          // number of image encoders
   UINT  size = 0;         // size of the image encoder array in bytes

   ImageCodecInfo* pImageCodecInfo = NULL;
   GetImageEncodersSize(&num, &size);
   if(size == 0)
      return -1;  // Failure

   int  ext = _tcsclen(filename);
   while( --ext>0 ) {
       if( filename[ext]=='.' ) break;
   }
   pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
   if(pImageCodecInfo == NULL)
      return -1;  // Failure

   GetImageEncoders(num, size, pImageCodecInfo);

   for(UINT j = 0; j < num; ++j)
   {
       if( _tcsstr( pImageCodecInfo[j].FilenameExtension, &filename[ext])!=NULL ) {
         *pClsid = pImageCodecInfo[j].Clsid;
         free(pImageCodecInfo);
         return j;  // Success
      }    
   }

   free(pImageCodecInfo);
   return -1;  // Failure
}

int savebitmap( LPCTSTR savefilename, Bitmap* pbmp  )
{
   // Save the altered image.
   CLSID pngClsid;
   if( GetEncoderClsid(savefilename, &pngClsid)>=0 ) {
    pbmp->Save(savefilename, &pngClsid, NULL);   
    return 1 ;
   }
   return 0;
}

// open html setup dialog
int openhtmldlg( char * servername, char * page )
{
	ShellExecute(NULL, _T("open"), 
		TMPSTR("http://%s%s", servername, page),
		NULL, NULL, SW_SHOWNORMAL );
	return 1;
}

#include <dlgs.h>

class opendialog : public Window 
{
protected:
	OPENFILENAME ofn ;

	static UINT_PTR CALLBACK openfolder_hookproc( HWND hdlg,  UINT uiMsg, WPARAM wParam, LPARAM lParam)
	{
		int res = 0 ;
		HWND hp ;		// parent dialog
		OFNOTIFY * lon ;
		hp = GetParent(hdlg) ;

		switch( uiMsg ) {
		case WM_INITDIALOG:
			((opendialog *)(((OPENFILENAME *)lParam)->lCustData))->attach(hp);
			break;
		case WM_NOTIFY:
			lon = (OFNOTIFY *)lParam ;
			switch (lon->hdr.code) {
			case CDN_FILEOK :
				res = 1 ;
				SetWindowLongPtr(hdlg, DWLP_MSGRESULT, 1 );
				break;
			
			case CDN_SELCHANGE :
				TCHAR displayname[MAX_PATH];
				lon->lpOFN->lpstrFile[0]=0 ;
				if( (int)SendMessage( hp, CDM_GETFILEPATH, MAX_PATH, (LPARAM)displayname ) > 0 ) {
					if( GetFileAttributes(displayname) & FILE_ATTRIBUTE_DIRECTORY ) {
						_tcsncpy( lon->lpOFN->lpstrFile, displayname, lon->lpOFN->nMaxFile );
					}
				}
				CommDlg_OpenSave_SetControlText(hp, edt1, lon->lpOFN->lpstrFile ) ;

				res = 1 ;
				break;
			}

			break;
		default:

			break;
		}
		return res;
	}

	virtual void OnAttach() {
		SendMessage(m_hWnd, CDM_HIDECONTROL, chx1, 0);
		SendMessage(m_hWnd, CDM_HIDECONTROL, cmb1, 0);
		SendMessage(m_hWnd, CDM_HIDECONTROL, stc2, 0);
		CommDlg_OpenSave_SetControlText(m_hWnd, stc3, _T("Folder Name:"));
		SetWindowText(m_hWnd, _T("Open Directory"));
	}

public:
	string filename;

	int opendir(HWND hparent, char * dir)
	{
		TCHAR strFile[512];
		filename = dir;
		if (filename.isempty()) {
			strFile[0] = 0;
		}
		else {
			_tcscpy(strFile, filename);
		}
		memset(&ofn, 0, sizeof(ofn));
		ofn.lStructSize = sizeof (OPENFILENAME) ;
		ofn.nMaxFile = 512 ;
		ofn.lpstrFile = strFile;
		ofn.hwndOwner = hparent ;
		ofn.Flags = OFN_EXPLORER | OFN_ENABLEHOOK ;
		ofn.lpfnHook = openfolder_hookproc ;
		ofn.lpstrFilter=_T("No Files\0NotExistedFileName.NoExistExt\0\0");
		ofn.nFilterIndex=0;
		ofn.hInstance=Window::AppInst();
		ofn.lCustData=(LPARAM)this ;
		if (GetOpenFileName(&ofn)) {
			filename = strFile;
			return TRUE;
		}
		return FALSE;
	}

protected:
    virtual LRESULT WndProc(UINT uMsg, WPARAM wParam, LPARAM lParam) {
		switch( uMsg ) {
		case WM_COMMAND:
			if( LOWORD(wParam) == IDOK ) {
				if( _tcslen( ofn.lpstrFile ) > 0 ) {
					::EndDialog(m_hWnd, IDOK);
				}
				return 0 ;
			}
			break ;

		default:
			break ;
		}
		return DefWndProc(uMsg, wParam, lParam);
    }

};

int	openfolder(HWND hparent, string &folder )
{
	opendialog od;
	if (od.opendir(hparent, folder) == IDOK) {
		folder = od.filename;
		return IDOK;
	}
	else {
		return 0;
	}
}

#include "winnls.h"
#include "shobjidl.h"
#include "objbase.h"
#include "objidl.h"
#include "shlguid.h"

HRESULT CreateLink( LPCSTR PathObj, LPCSTR PathLink, LPCSTR Desc) 
{ 
    HRESULT hres; 
    IShellLink* psl; 
	string sObj, sLink, sDesc ;
	sObj = PathObj ;
	sLink = PathLink ;
	sDesc = Desc ;
 
    // Get a pointer to the IShellLink interface. It is assumed that CoInitialize
    // has already been called.
    hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&psl); 
    if (SUCCEEDED(hres)) 
    { 
        IPersistFile* ppf; 
 
        // Set the path to the shortcut target and add the description. 
        psl->SetPath(sObj); 
        psl->SetDescription(sDesc); 
 
        // Query IShellLink for the IPersistFile interface, used for saving the 
        // shortcut in persistent storage. 
        hres = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf); 
 
        if (SUCCEEDED(hres)) 
        {
            // Save the link by calling IPersistFile::Save. 
            hres = ppf->Save(sLink, TRUE); 
            ppf->Release(); 
        } 
        psl->Release(); 
    } 
    return hres; 

}
