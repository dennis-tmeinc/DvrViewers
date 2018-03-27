// DVRDiskFix.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "DVRDiskFix.h"

#include "cwin.h"
#include "cstr.h"

#define MAX_LOADSTRING 100
#define DEVICE_UPDATE_DELAY	(1000)

static TCHAR disk_enum[]=_T("SYSTEM\\CurrentControlSet\\Services\\disk\\Enum") ;
static TCHAR disk_mounts[]=_T("SYSTEM\\MountedDevices") ;
static TCHAR disk_usbstor[] = _T("SYSTEM\\CurrentControlSet\\Enum\\" );

static string disk_strbuf;

// get available disk number
int DiskNum()
{
	HKEY key ;
	DWORD dwType ;
	DWORD cbData ;
	DWORD count=0 ;
	if( RegOpenKeyEx(HKEY_LOCAL_MACHINE, disk_enum, 0, KEY_READ, &key )==ERROR_SUCCESS ) {
		cbData = sizeof(count) ;
		RegQueryValueEx (key, _T("Count"), NULL, &dwType, (LPBYTE)&count, &cbData );
		RegCloseKey( key );
	}
	return (int)count ;
}

char * DiskSerialId(int disknum )
{
	int res = 0 ;
	DWORD dwType ;
	DWORD cbData ;
	HKEY key ;

	if( RegOpenKeyEx(HKEY_LOCAL_MACHINE, disk_enum, 0, KEY_READ, &key )==ERROR_SUCCESS ) {
		cbData = 512 ;
		if( RegQueryValueEx (key, 
			TMPSTR("%d", disknum),
			NULL, &dwType, (LPBYTE) disk_strbuf.tcssize( cbData ), &cbData ) == ERROR_SUCCESS ) {
			res = 1 ;
		}
		RegCloseKey( key );
	}
	if( res )	return (char *)disk_strbuf ;
	else		return NULL ;
}

// get disk friend name from ID ;
char * DiskFriendlyName(char * serialid)
{
	HKEY key ;
	int res = 0 ;
	string keyname ;
	string usbstor_enum = disk_usbstor ;
	keyname.printf("%s%s", (char *)usbstor_enum, (char *)serialid);
	
	if( RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyname, 0, KEY_READ, &key )==ERROR_SUCCESS ) {
		DWORD dwType, cbData ;
		cbData=256 ;
		if( RegQueryValueEx (key, (LPCTSTR)_T("FriendlyName"), NULL, &dwType, (LPBYTE)keyname.tcssize( cbData ), &cbData ) == ERROR_SUCCESS ) {
			disk_strbuf = keyname ;
			res = 1 ;
		}
		RegCloseKey( key );
	}
	if( res ) return (char *)disk_strbuf ;
	else      return NULL ;
}

// trim id into this form : Ven_SanDisk&Prod_Cruzer_Mini&Rev_0.1\20041100531daa107a09 
// use it for data encryption
char * DiskSerial(char * serialid)
{
	int res = 0;
	char * pkey = strstr( serialid, "Ven_" ) ;
    if( pkey ) {
		disk_strbuf = pkey ;
		char * s = strstr( disk_strbuf, "\\" ) ;
		 if( s ) {
			s = strstr( s, "&" ) ;
			if( s ) {
				*s = '\0' ;
			}
		}
	}
	else {
		disk_strbuf = serialid ;
	}
	return (char *)disk_strbuf ;
}

char * DiskVolume(char * serialid)
{
	int res = 0 ;
	int e ;
	string name ;
	DWORD  namelen ;
	string value ;
	DWORD  valuelen ;
	HKEY key ;

	if( serialid == NULL || strlen(serialid)<5 ) return NULL ;
	 
	// replay '\' with '#', for id compare
	string sid = serialid ;
	char * csid = (char *)sid ;
	for( e=0; e<strlen(csid); e++) {
		if( csid[e] == '\\' ) csid[e] = '#' ;
		else if( csid[e] == '/' ) csid[e] = '#' ;
	}

	if( RegOpenKeyEx(HKEY_LOCAL_MACHINE, disk_mounts, 0, KEY_READ, &key )==ERROR_SUCCESS ) {
		for( e=0; e<5000; e++) {
			namelen = 500 ;
			valuelen = 16384 ;
			BYTE * v = (BYTE *)value.strsize(valuelen+2);
			if( RegEnumValue( key, e, name.tcssize( namelen ), &namelen, NULL, NULL, (LPBYTE)v, &valuelen ) == ERROR_SUCCESS ) {
				// null terminate
				v[valuelen] = 0 ;
				v[valuelen+1] = 0 ;

				string volume ;
				volume = (wchar_t *) v ;
				
				if( strstr( (char*)name, "Volume") && strstr( (char *)volume, csid ) ) {		// found
					disk_strbuf = name ;
					res = 1 ;
					break;
				}
			}
			else {
				break;
			}
		}
		RegCloseKey( key );
	}
	if( res ) return (char *)disk_strbuf ;
	else      return NULL ;
}

#include <WinIoCtl.h>

// return size of partition in Gbytes
int DiskIsPartition(char * serialid)
{
	int parted = 0 ;
	int res ;
	int idx = 0 ;
	DWORD dwSize = 0 ;
	string volume ;
	volume = DiskVolume(serialid) ;
	if( strlen( volume ) > 2 ) {
		HANDLE hdisk = CreateFile( volume, GENERIC_READ, FILE_SHARE_WRITE|FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL ) ;
		if( hdisk != INVALID_HANDLE_VALUE ) {
			VOLUME_DISK_EXTENTS diskExtents;
			memset( &diskExtents, 0, sizeof(diskExtents) );
			DWORD dwSize;
			res = DeviceIoControl(
				hdisk,
				IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
				NULL,
				0,
				(LPVOID) &diskExtents,
				(DWORD) sizeof(diskExtents),
				(LPDWORD) &dwSize,
				NULL);
			CloseHandle(hdisk);

			if( res && diskExtents.NumberOfDiskExtents > 0 ) {
				int diskindex = diskExtents.Extents[0].DiskNumber ;
				if( diskExtents.Extents[0].StartingOffset.QuadPart > 0 ) {
					parted = 1 ;
				}
			}
		}
	}
	return parted ;
}

// get disk \\.\PhysicalDrive{x} x number
int DiskPhysicalDrive(char * serialid)
{
	int res ;
	int idx = 0 ;
	DWORD dwSize = 0 ;
	string volume ;
	volume = DiskVolume(serialid) ;
	if( strlen( volume ) > 2 ) {
		HANDLE hdisk = CreateFile( volume, GENERIC_READ, FILE_SHARE_WRITE|FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL ) ;
		if( hdisk != INVALID_HANDLE_VALUE ) {
			VOLUME_DISK_EXTENTS diskExtents;
			memset( &diskExtents, 0, sizeof(diskExtents) );
			DWORD dwSize;
			res = DeviceIoControl(
				hdisk,
				IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
				NULL,
				0,
				(LPVOID) &diskExtents,
				(DWORD) sizeof(diskExtents),
				(LPDWORD) &dwSize,
				NULL);
			CloseHandle(hdisk);

			if( res && diskExtents.NumberOfDiskExtents > 0 ) {
				idx = diskExtents.Extents[0].DiskNumber ;
			}
		}
	}
	return idx ;
}

// check if disk is mounted?
int DiskMounted( char * diskid )
{
	int mnt = 0 ;
	HANDLE hdisk ;
	DWORD dwSize ;
	string volume = DiskVolume(diskid) ;
	hdisk = CreateFile( volume, GENERIC_WRITE|GENERIC_READ, FILE_SHARE_WRITE|FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL ) ;
	if( hdisk != INVALID_HANDLE_VALUE ) {
		mnt = DeviceIoControl( hdisk, FSCTL_IS_VOLUME_MOUNTED, NULL, 0, NULL, 0, &dwSize, NULL);
		CloseHandle( hdisk );
	}
	return mnt ;
}

// Unmount disk
int DiskUnmount( char * diskid )
{
	int mnt = 0 ;
	HANDLE hdisk ;
	DWORD dwSize ;
	string volume = DiskVolume(diskid) ;
	hdisk = CreateFile( volume, GENERIC_WRITE|GENERIC_READ, FILE_SHARE_WRITE|FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL ) ;
	if( hdisk != INVALID_HANDLE_VALUE ) {
		int x ;
		for(x=0;x<20;x++) {
			if( DeviceIoControl( hdisk, FSCTL_IS_VOLUME_MOUNTED, NULL, 0, NULL, 0, &dwSize, NULL) ) {
				DeviceIoControl( hdisk, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &dwSize, NULL );
			}
			else {
				break;
			}
			Sleep(100);
		}
		CloseHandle( hdisk );
	}
	return mnt ;
}

// Read Volume's first sector
int DiskReadVolume(char * diskid , char * buf, int size )
{
	int res = 0 ;
	HANDLE hdisk ;
	DWORD dwSize ;
	string volume = DiskVolume(diskid) ;
	hdisk = CreateFile( volume, GENERIC_WRITE|GENERIC_READ, FILE_SHARE_WRITE|FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL ) ;
	if( hdisk != INVALID_HANDLE_VALUE ) {
		if( ReadFile( hdisk, buf, (DWORD)size, &dwSize, NULL ) ) {
			res = (int)dwSize ;
		}
		CloseHandle( hdisk );
	}
	return res ;
}

int DiskReadMBR( char * diskid, char * mbr, int size )
{
	DWORD dwSize ;
	int res = 0 ;
	int physicalidx = DiskPhysicalDrive( diskid );
	if( physicalidx>0 ) {
		HANDLE hdisk = CreateFile( 
			TMPSTR("\\\\.\\PhysicalDrive%d", physicalidx),
			GENERIC_READ|GENERIC_WRITE, FILE_SHARE_WRITE|FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL ) ;
		if( hdisk != INVALID_HANDLE_VALUE ) {
			if( ReadFile( hdisk, mbr, (DWORD)size, &dwSize, NULL ) ) {
				res = (int)dwSize ;
			}
			CloseHandle( hdisk );
		}
	}
	return res ;
}

int DiskWriteMBR( char * diskid, char * mbr, int size )
{
	DWORD dwSize ;
	int res = 0 ;
	int physicalidx = DiskPhysicalDrive( diskid );
	if( physicalidx>0 ) {
		HANDLE hdisk = CreateFile( 
			TMPSTR("\\\\.\\PhysicalDrive%d", physicalidx),
			GENERIC_READ|GENERIC_WRITE, FILE_SHARE_WRITE|FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL ) ;
		if( hdisk != INVALID_HANDLE_VALUE ) {
			if( WriteFile( hdisk, mbr, (DWORD)size, &dwSize, NULL ) ) {
				if( dwSize == (DWORD)size ) {
					res = 1 ;
				}
			}
			CloseHandle( hdisk );
		}
	}
	return res ;
}

// partition table (an empty partition)
struct mbr_pentry {
	DWORD chs_start ;
	DWORD chs_end ;
	DWORD lba_start ;
	DWORD lba_len ; 
} mbr_1be = { 
	0x00030200,		// first byte 0 for nonactive partition
	0x08030200,		// first byte 0 for id
	2048,			// start from 2048 sector
	0x80000 } ;		// 256M

int fat32check( char * bootrecord )
{
	return (
		bootrecord[0x0b] == (char)0 &&
		bootrecord[0x0c] == (char)2 &&
		bootrecord[0x15] == (char) 0xf8 &&
		memcmp( bootrecord + 0x52, "FAT32", 5 )==0 &&
		bootrecord[0x1fe] == (char)0x55 &&
		bootrecord[0x1ff] == (char)0xaa &&
		memcmp( bootrecord + 0x200, "RRaA", 4 )==0 &&
		memcmp( bootrecord + 0x3e4, "rrAa", 4 )==0 &&
		bootrecord[0x3fe] == (char)0x55 &&
		bootrecord[0x3ff] == (char)0xaa ) ;
}

class DiskFixDialog : public Dialog {

protected:
	
	char mbr[1024] ;

	void OnTimer()
	{
		KillTimer(m_hWnd, 101);

        HWND hwndList = GetDlgItem(m_hWnd, IDC_LIST_DEV );  
		SendMessage(hwndList, LB_RESETCONTENT, 0, 0 );
	
		// read disks
		int disk ;
		int res ;
		int disknum = DiskNum() ;

		// skip first disk, that should be the system disk
		for( disk=1 ; disk < disknum; disk++ ) {

			string diskid = DiskSerialId(disk);
			int  pnum = DiskPhysicalDrive(diskid) ;
			if( pnum>0 ) {

				DiskReadVolume(diskid, mbr, 1024);

				if( DiskIsPartition(diskid) ) {
					res = DiskReadMBR( diskid, mbr, 1024 ) ;

					// minimum fat32 boot sector check
					if( res == 1024 &&  fat32check( mbr ) )
					{

						// detected a possible malfunctioning disk
						string friendlyname = DiskFriendlyName(diskid);
						int idx ;
						if( strlen( friendlyname )>0 ) {
							idx = SendMessage(hwndList, LB_ADDSTRING, 0, 
								(LPARAM)(TCHAR *)friendlyname); 
						}
						else {
							idx = SendMessage(hwndList, LB_ADDSTRING, 0, 
								(LPARAM)(TCHAR *)diskid); 
						}
						if( idx >= 0 ) {
							SendMessage(hwndList, LB_SETITEMDATA, (WPARAM)idx, (LPARAM)disk);
						}
					}
				}
			}
		}

	}


    void OnDeviceChange( UINT nEventType, DWORD dwData )
	{
		// timer to reset device list
		SetTimer(m_hWnd, 101, DEVICE_UPDATE_DELAY, NULL );
	}

	int OnRefresh()
	{
		// timer to reset device list
		SetTimer(m_hWnd, 101, 20, NULL );
		return TRUE ;
	}
	
	int WarningMessage() 
	{
		int res = MessageBox(m_hWnd, _T("Please make sure this is the disk you want to fix, this operation would change key data structures of the disk and may possibly make the disk unusable! Click OK to continue."), _T("Stop"), MB_OKCANCEL|MB_ICONWARNING);
		return res == IDOK ;
	}

	int UnplugMessage()
	{
		MessageBox(m_hWnd, _T("You may needed to remove this disk and plug it in again!"), _T("Remove The Disk"), MB_OK|MB_ICONINFORMATION);
		return 0;
	}

	int OnFix()
	{
		HWND hwndList = GetDlgItem(m_hWnd, IDC_LIST_DEV );  
		int sel = SendMessage(hwndList, LB_GETCURSEL, 0, 0 ) ;
		if( sel >= 0 ) {
			// to fix this disk
			int disk = (int)SendMessage(hwndList, LB_GETITEMDATA, (WPARAM)sel, 0 ) ;
			string diskid = DiskSerialId(disk);
			int  pnum = DiskPhysicalDrive(diskid) ;
			if( pnum>0 ) {
				if( DiskIsPartition(diskid) ) {
					int res = DiskReadMBR( diskid, mbr, 1024 ) ;

					// minimum fat32 boot sector check
					if( res == 1024 &&  fat32check( mbr ) )
					{
						// 
						if( WarningMessage() ) {
							memset( &(mbr[0x1be]), 0, 64 );
							DiskWriteMBR( diskid, mbr, 512 );
							if( DiskMounted( diskid ) ) {
								DiskUnmount( diskid ) ;
							}
							OnRefresh();
							UnplugMessage();
						}
					}
				}
			}

		}

		return TRUE ;
	}

	int OnAbout()
	{
		Dialog about ;
		about.DoModal(IDD_ABOUTBOX, m_hWnd);
		return TRUE ;
	}

	virtual int OnCommand( int Id, int Event ) 
	{
        switch (Id)
        {
        case IDC_BUTTON_REFRESH:
			return OnRefresh();

        case IDC_BUTTON_FIX:
			return OnFix();

		case IDM_ABOUT:
			return OnAbout();

		default:
			return Dialog::OnCommand( Id, Event ) ;
        }
        return FALSE;
    }

    // dialog box procedures
    virtual int DlgProc(UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
		case WM_DEVICECHANGE:
			OnDeviceChange( wParam, lParam );
			break;
		
		case WM_TIMER:
			OnTimer();
			break;

		case WM_SYSCOMMAND:
			if ((wParam & 0xFFFF) == IDM_ABOUT)
			{
				return OnAbout();
			}
			break ;

		default :
			return Dialog::DlgProc(message, wParam, lParam);

        }
        return FALSE ;
    }

public:

	DiskFixDialog() {
		CreateDlg( IDD_DIALOG_DISKFIX );
		// Initialize global strings
		TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
		LoadString(Window::ResInst(), IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
		SetWindowText( m_hWnd, szTitle );
		HICON hicon = LoadIcon( Window::ResInst(), MAKEINTRESOURCE(IDI_DVRDISKFIX) );
		::SendMessageA(m_hWnd, WM_SETICON, ICON_BIG, (LPARAM)hicon);
		::SendMessageA(m_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hicon);

		// add about menu
		HMENU hSysMenu = ::GetSystemMenu(m_hWnd, FALSE);
		if (hSysMenu != NULL)
		{
			string strAboutMenu;
			sprintf( strAboutMenu.strsize(200), "&About DvrDiskFix") ;
			// ::AppendMenu(hSysMenu,MF_SEPARATOR, 0, NULL);
			// ::AppendMenu(hSysMenu,MF_STRING, IDM_ABOUT, strAboutMenu);
		}


		// timer to reset device list
		SetTimer(m_hWnd, 101, DEVICE_UPDATE_DELAY, NULL );

	}

};


int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	DiskFixDialog sdfixdlg ;
	sdfixdlg.DoModal();

	return 0;
}

