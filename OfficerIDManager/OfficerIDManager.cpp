// OfficerIDManager.cpp : Defines the entry point for the application.
//

#include "stdafx.h"

#include <stdio.h>

#include "..\common\cwin.h"
#include "..\common\cstr.h"

#include "OfficerIDManager.h"


char * log_file(string & logfile)
{
    string appname ;
    int len ;
    GetModuleFileName(NULL, appname.tcssize(260), 259 );
	len = strlen((char*)appname) ;
	strcpy( ((char*)appname)+(len-3), "log");
	logfile = appname ;
	return (char *)logfile ;
}

int log_write(char * l)
{
	string logfilen ;
	FILE * logfile = fopen( log_file( logfilen ), "a" );
	if( logfile ) {
		time_t t = time(NULL);
		fputs(ctime(&t), logfile);
		fputs(": ", logfile);
		fputs(l,logfile);
		fputs("\r\n",logfile);
		fclose( logfile );
	}
	return 0 ;
}

void log_clear()
{
	string logfilen ;
	FILE * logfile = fopen( log_file( logfilen ), "w" );
	if( logfile ) {
		fclose( logfile );
	}
	return ;
}

char * log_get( string & log )
{
	string logfilen ;
	FILE * logfile = fopen( log_file( logfilen ), "r" );
	if( logfile ) {
		fseek(logfile, 0, SEEK_END );
		int l = ftell(logfile);
		fseek(logfile, 0, SEEK_SET );
		l = fread(  log.strsize(l+2), 1, l, logfile );
		((char *)log)[l] = 0 ;
		fclose( logfile );
	}
	return (char *)log ;
}

#define MAX_LOADSTRING 100
#define TIMER_DEVICE (5)

// Global Variables:
HINSTANCE hInst;								// current instance
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR    lpCmdLine,
                     int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	HACCEL hAccelTable;
	
	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_OFFICERIDMANAGER, szWindowClass, MAX_LOADSTRING);

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDI_PWKEY));

    OfficerIdManagerDlg PWManDlg ;

	log_write( "Application started." );

	PWManDlg.DoModal( IDD_DIALOG_OFFICERIDMANAGER  );
	
	log_write( "Application quit.\r\n" );

	return 0 ;
}

int OfficerIdManagerDlg::OnInitDialog()
{

   	HICON hIcon = LoadIcon(ResInst(), MAKEINTRESOURCE(IDR_MAINFRAME));
    ::SendMessageA(m_hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
	::SendMessageA(m_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

	SetTimer( m_hWnd, TIMER_DEVICE, 200, 0 );

	return 1 ;
}

// BlowFish encryptions
#include "../common/blowfish.h" 

static BLOWFISH_CTX ctx ;

// init encryption key, salt is 64 bytes random data (include header 'OFID')
void DiskDataEncryptionInit( char * diskid, char * salt )
{
	int len = strlen(diskid);
	unsigned char key[512] ;
	memset( key, 0, 512 );
	memcpy( key, salt, 64 );
	memcpy( key+64, diskid, len );
	Blowfish_Init(&ctx, key, 64+len ) ;
}

// len must be blocks of 64bits (8 bytes)
int DiskDataEncrypt( char * data, int len )
{
	int i ;
	unsigned long * x = (unsigned long *) data ;
	len /= 2*sizeof(unsigned long) ;
	for( i=0; i<len; i++ ) {
		Blowfish_Encrypt( &ctx, x, x+1 );
		x+=2 ;
	}
	return 1 ;
}

int DiskDataDecrypt( char * data, int len )
{
	int i ;
	unsigned long * x = (unsigned long *) data ;
	len /= 2*sizeof(unsigned long) ;
	for( i=0; i<len; i++ ) {
		Blowfish_Decrypt( &ctx, x, x+1 );
		x+=2 ;
	}
	return 1 ;
}

//RC4 encryption 
#include "../common/crypt.h" 

struct RC4_seed rc4seed ;

// init encryption key, salt is 64 bytes random data (include header 'OFID')
void DiskDataEncryptionInit_rc4( char * diskid, char * salt )
{
	unsigned char k[256] ;
	key_256( diskid, k );
	RC4_KSA( &rc4seed, k );
	RC4_KSA_A( &rc4seed, NULL, 213 );
	RC4_KSA_A( &rc4seed, (unsigned char *)salt, 64 );
	RC4_KSA_A( &rc4seed, NULL, 731 );
}

// len must be blocks of 64bits (8 bytes)
void DiskDataEncrypt_rc4( char * data, int len )
{
	RC4_crypt( (unsigned char *)data, len, &rc4seed ) ;
}

void DiskDataDecrypt_rc4( char * data, int len )
{
	RC4_crypt( (unsigned char *)data, len, &rc4seed ) ;
}


#include <Wincrypt.h>

void DiskDataRandom( void * buf, int buflen )
{
	HCRYPTPROV hProvider ;
	if (CryptAcquireContextW(&hProvider, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT)){
		DWORD dwLength = buflen ;
		CryptGenRandom(hProvider, dwLength, (BYTE *)buf) ;
		CryptReleaseContext(hProvider, 0);
	}
}

static TCHAR disk_enum[]=_T("SYSTEM\\CurrentControlSet\\Services\\disk\\Enum") ;
static TCHAR disk_mounts[]=_T("SYSTEM\\MountedDevices") ;
static TCHAR disk_usbstor[] = _T("SYSTEM\\CurrentControlSet\\Enum\\" );
static string disk_strbuf ;	// string buffer used by disk utilites

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
		string diskn ;
		sprintf( diskn.strsize(50), "%d", disknum );
		cbData = 512 ;
		if( RegQueryValueEx (key, (LPCTSTR)diskn, NULL, &dwType, (LPBYTE) disk_strbuf.tcssize( cbData ), &cbData ) == ERROR_SUCCESS ) {
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
	sprintf( keyname.strsize(500), "%s%s", (char *)usbstor_enum, (char *)serialid ); 
	
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
	string valuename ;
	DWORD  valuenamelen ;
	char   valuebuf[1000] ;
	DWORD  valuebuflen ;
	HKEY key ;
	 
	if( strncmp( serialid, "USBSTOR", 7 )!=0 ) {
		// only check usb key device
		return NULL ;
	}

	// replay '\' with '#', for id compare
	string sid = serialid ;
	char * csid = (char *)sid ;
	for( e=0; e<strlen(csid); e++) {
		if( csid[e] == '\\' ) csid[e] = '#' ;
	}

	if( RegOpenKeyEx(HKEY_LOCAL_MACHINE, disk_mounts, 0, KEY_READ, &key )==ERROR_SUCCESS ) {
		for( e=0; e<5000; e++) {
			valuenamelen = 998 ;
			valuebuflen = 998 ;
			if( RegEnumValue( key, e, valuename.tcssize( 1000 ), &valuenamelen, NULL, NULL, (LPBYTE)valuebuf, &valuebuflen ) == ERROR_SUCCESS ) {
				// null terminate
				valuebuf[valuebuflen] = 0 ;
				valuebuf[valuebuflen+1] = 0 ;
				string volume ;
				volume = ( wchar_t * )valuebuf ;
				
				if( strstr( volume, "HDD_FW" ) ) {
					res = 0 ;
				}

				if( strstr( valuename, "\\Volume" ) && strstr( volume, csid ) ) {		// found
					disk_strbuf = valuename ;
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

// get disk \\.\PhysicalDrive{x} x number
int DiskPhysicalDrive(char * serialid)
{
	int res ;
	int idx = 0 ;
	DWORD dwSize = 0 ;
	string volume ;
	volume = DiskVolume(serialid) ;
	if( strlen( volume ) > 0 ) {
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
				string device ;
				sprintf( device.strsize(200), "\\\\.\\PhysicalDrive%d", diskindex );	
				hdisk = CreateFile( device, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_WRITE|FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL ) ;

				DWORD losize ;
				losize = SetFilePointer( hdisk, 1024*1024*92, NULL, FILE_BEGIN );
				if( losize == 1024*1024*92 ) {
					char buf[1024] ;
					ReadFile( hdisk, buf, 1024, &dwSize, NULL );
					if( dwSize==1024 ) {
						idx = diskindex ;
					}
				}

				GET_LENGTH_INFORMATION gli ;
				DeviceIoControl( hdisk, IOCTL_DISK_GET_LENGTH_INFO, NULL, 0, (LPVOID) &gli, sizeof(gli), &dwSize, NULL ) ;

				CloseHandle( hdisk );
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


// md5 checksum
void md5_checksum( char * checksum, unsigned char * data, unsigned int datalen ) ;
// pre-picked ID data location, in unit of MB
static DWORD IdOffset[8] = { 2, 220, 382, 674, 952, 1142, 1688, 2392 } ;

int DiskReadId( int disk, officerid_t * id )
{
	DWORD dwSize ;
	int res = 0 ;
	string diskid = DiskSerialId( disk ) ;
	int physicalidx = DiskPhysicalDrive( diskid );
	if( physicalidx>0 ) {
		string physicaldrive ;
		sprintf( physicaldrive.strsize(40), "\\\\.\\PhysicalDrive%d", physicalidx );	

		HANDLE hdisk = CreateFile( physicaldrive, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_WRITE|FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL ) ;
		if( hdisk != INVALID_HANDLE_VALUE ) {
			int s ;
			for( s=0; s<8; s++ ) {
				DWORD offset = IdOffset[s] * 1024 * 1024 ;
				SetFilePointer( hdisk, offset, NULL, FILE_BEGIN );
				char buf[4096] ;
				officerid_header * xhead = (officerid_header *)buf ;

				dwSize = 0 ;
				ReadFile( hdisk, buf, 4096, &dwSize, NULL );
				if( dwSize==4096 ) {
					if( strncmp( xhead->tag, "OFID", 4 )==0 && xhead->si<4000 && xhead->si>96 ) {
						// decrypt
						if( xhead->enctype == 1 ) {
							string serial = DiskSerial( diskid );
							DiskDataEncryptionInit( serial, xhead->salt ) ;
							DiskDataDecrypt( (char *)xhead->data, xhead->si ) ;
						}
						else if( xhead->enctype == 2 ) {
							string serial = DiskSerial( diskid );
							DiskDataEncryptionInit_rc4( serial, xhead->salt ) ;
							DiskDataDecrypt_rc4( (char *)xhead->data, xhead->si ) ;
						}

						// generate checksum
						char md5checksum[20] ;
						md5_checksum( md5checksum, (unsigned char *)xhead->data, xhead->si );

						if( memcmp(xhead->checksum , md5checksum, 16)==0 ) {		// checksum match?
							memcpy( id, xhead->data, sizeof(*id) );
							res = 1 ;
							break ;
						}
					}
				}
				else {
					break ;
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


int DiskWriteId( int disk, officerid_t * id )
{
	DWORD dwSize ;
	HANDLE hdisk ;
	int res = 0 ;
	string diskid = DiskSerialId( disk ) ;
	int physicalidx = DiskPhysicalDrive( diskid );
	if( physicalidx>0 ) {
		// umount disk
		DiskUnmount( diskid ) ;

		string physicaldrive ;
		sprintf( physicaldrive.strsize(40), "\\\\.\\PhysicalDrive%d", physicalidx );	

		hdisk = CreateFile( physicaldrive, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_WRITE|FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL ) ;
		if( hdisk != INVALID_HANDLE_VALUE ) {
			char buf[4096] ;
			officerid_header * xhead = (officerid_header *)buf ;
			memset( xhead, 0, sizeof(*xhead) );
			memcpy( xhead->tag,"OFID",4);
			xhead->enctype = 2 ;				// use rc4 
			DiskDataRandom( xhead->salt, sizeof( xhead->salt) );
			xhead->si = sizeof( *id );
			memcpy( xhead->data, id, sizeof(*id) );

			// generate checksum
			md5_checksum( xhead->checksum, (unsigned char *)xhead->data, xhead->si );

			if( xhead->enctype == 1 ) {	// blowfish
				// encrypt
				string serial = DiskSerial( diskid );
				DiskDataEncryptionInit( serial, xhead->salt ) ;
				DiskDataEncrypt( (char *)xhead->data, xhead->si ) ;
			}
			else if( xhead->enctype == 2 ) {	// rc4
				// encrypt
				string serial = DiskSerial( diskid );
				DiskDataEncryptionInit_rc4( serial, xhead->salt ) ;
				DiskDataEncrypt_rc4( (char *)xhead->data, xhead->si ) ;
			}

			int s ;
			for( s=0; s<8; s++ ) {
				DWORD offset = IdOffset[s] * 1024 * 1024 ;
				SetFilePointer( hdisk, offset, NULL, FILE_BEGIN );
				DWORD dwSize = 0 ;
				WriteFile( hdisk, buf, 4096, &dwSize, NULL );
				if( dwSize==4096 ) {
					res = 1 ;
				}
			}

			if( res ) {
				// write MBR, this would make usb stick not accessble by windows explorer
				memset(buf, 0, sizeof(buf));

				// to erase file system
				for( s=0; s<4; s++ ) {
					SetFilePointer( hdisk, 2048*512+s*4096, NULL, FILE_BEGIN );
					dwSize = 0 ;
					WriteFile( hdisk, buf, 4096, &dwSize, NULL );
				}

				memcpy( buf+0x1be, &mbr_1be, 0x10 );
				// end of mbr
				buf[0x1fe] = 0x55 ;	
				buf[0x1ff] = 0xaa ;
				SetFilePointer( hdisk, 0, NULL, FILE_BEGIN );
				dwSize = 0 ;
				WriteFile( hdisk, buf, 512, &dwSize, NULL );
			}

			CloseHandle( hdisk );
		}
	}
	return res ;
}

int DiskEraseId( int disk )
{
	DWORD dwSize ;
	HANDLE hdisk ;
	int res = 0 ;
	string diskid = DiskSerialId( disk ) ;
	int physicalidx = DiskPhysicalDrive( diskid );
	if( physicalidx>0 ) {
		// umount disk
		DiskUnmount( diskid ) ;

		string physicaldrive ;
		sprintf( physicaldrive.strsize(40), "\\\\.\\PhysicalDrive%d", physicalidx );	

		hdisk = CreateFile( physicaldrive, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_WRITE|FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL ) ;
		if( hdisk != INVALID_HANDLE_VALUE ) {
			char buf[4096] ;
			memset(buf, 0, sizeof(buf));
			int s ;
			for( s=0; s<8; s++ ) {
				DWORD offset = IdOffset[s] * 1024 * 1024 ;
				SetFilePointer( hdisk, offset, NULL, FILE_BEGIN );
				DWORD dwSize = 0 ;
				WriteFile( hdisk, buf, 4096, &dwSize, NULL );
				if( dwSize>0 ) {
					res = 1 ;
				}
			}

			if( res ) {
				// write empty MBR
				SetFilePointer( hdisk, 0, NULL, FILE_BEGIN );
				dwSize = 0 ;
				WriteFile( hdisk, buf, 512, &dwSize, NULL );
			}

			CloseHandle( hdisk );
		}
	}
	return res ;
}

const char emptyid[] = "   na   " ;
void OfficerIdManagerDlg::OnTimer(UINT_PTR nIDEvent)
{
	if( nIDEvent == TIMER_DEVICE ) {
		KillTimer( m_hWnd, nIDEvent );

		// reset list box
		SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DISK), LB_RESETCONTENT, 0,0 ) ;
		// to list all USB devices
		int ndisk = DiskNum() ;
		for( int d = 0 ; d<ndisk; d++ ) {
			string serialid ;
			serialid = DiskSerialId(d) ;
			if( strlen(serialid)>0 ) {
				int didx = DiskPhysicalDrive(serialid) ;
				if( didx>0 ) {
					string friendlyname = DiskFriendlyName(serialid ) ;
					if( strlen(friendlyname)>0 ) {
						string offid ;
						officerid_t id ;
						if( DiskReadId( d, &id ) ) {
							offid = id.officerid ;
						}
						else {
							offid = emptyid ;
						}
						string str ;
						sprintf( str.strsize(500), "id(%s) : %s", (char *)offid, (char *)friendlyname ) ;
						int item = SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DISK),LB_ADDSTRING, 0, (LPARAM)(LPCTSTR)str ) ;

						string logstr ;
						sprintf( logstr.strsize(500), "Detected USB key: \'%s\'", str );
						log_write( logstr );

						if( item>=0 && item!=LB_ERR ) {

							char * key = DiskSerial(serialid) ;

							// set id of the item
							SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DISK), LB_SETITEMDATA , (WPARAM)item, (LPARAM)d ) ;

						}
					}
				}
			}
		}

	}
}

void OfficerIdManagerDlg::OnDeviceChange( UINT nEventType, DWORD dwData )
{
	// to refresh USB key list
	SetTimer( m_hWnd, TIMER_DEVICE, 1000, 0 );
}

int OfficerIdEditDlg::OnInitDialog()
{
	// set edit field
	string v ;
	v = officerid.officerid ;
	SetDlgItemText( m_hWnd, IDC_EDIT_OFFICER_ID, v );
	SendDlgItemMessage( m_hWnd, IDC_EDIT_OFFICER_ID, EM_SETLIMITTEXT, (WPARAM)8, 0 );

	v = officerid.firstname ;
	SetDlgItemText( m_hWnd, IDC_EDIT_FIRSTNAME, v );
	SendDlgItemMessage( m_hWnd, IDC_EDIT_FIRSTNAME, EM_SETLIMITTEXT, (WPARAM)30, 0 );

	v = officerid.lastname ;
	SetDlgItemText( m_hWnd, IDC_EDIT_LASTNAME, v );
	SendDlgItemMessage( m_hWnd, IDC_EDIT_LASTNAME, EM_SETLIMITTEXT, (WPARAM)30, 0 );

	v = officerid.department ;
	SetDlgItemText( m_hWnd, IDC_EDIT_DEPARTMENT, v );
	SendDlgItemMessage( m_hWnd, IDC_EDIT_DEPARTMENT, EM_SETLIMITTEXT, (WPARAM)60, 0 );

	v = officerid.badgenumber ;
	SetDlgItemText( m_hWnd, IDC_EDIT_BADGE, v );
	SendDlgItemMessage( m_hWnd, IDC_EDIT_BADGE, EM_SETLIMITTEXT, (WPARAM)30, 0 );

	return TRUE;
}

int OfficerIdEditDlg::OnOK()
{
	// retrive edit field
	string v ;
	GetDlgItemText( m_hWnd, IDC_EDIT_OFFICER_ID, v.tcssize(32), 31 );
	strcpy( officerid.officerid, v );
	GetDlgItemText( m_hWnd, IDC_EDIT_FIRSTNAME, v.tcssize(32), 31 );
	strcpy( officerid.firstname, v );
	GetDlgItemText( m_hWnd, IDC_EDIT_LASTNAME, v.tcssize(32), 31 );
	strcpy( officerid.lastname, v );
	GetDlgItemText( m_hWnd, IDC_EDIT_DEPARTMENT, v.tcssize(32), 31 );
	strcpy( officerid.department, v );
	GetDlgItemText( m_hWnd, IDC_EDIT_BADGE, v.tcssize(32), 31 );
	strcpy( officerid.badgenumber, v );

	return EndDialog(IDOK);
}

const LPCTSTR wipedata_message = _T("WARNING! This disk is being used by OS and may contain valid files, to continue would wipe out all data on this disk and make it not accessables by OS!") ;

void OfficerIdManagerDlg::OnBnCreate()
{	
	int item ;
	int sel = SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DISK), LB_GETCURSEL, 0, 0 ) ;
	if( sel<0 || sel==LB_ERR ) {
		MessageBox(m_hWnd, _T("Please Select A USB Key"), NULL, MB_OK);
		return ;
	}
	string itemstr ;
	SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DISK),LB_GETTEXT, (WPARAM)sel, (LPARAM)itemstr.tcssize(300) ) ;
	item = SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DISK), LB_GETITEMDATA, (WPARAM)sel, 0 ) ;

	officerid_t oid ;
	if( DiskReadId( item, &oid ) ) {
		return ;
	}

	string diskid = DiskSerialId( item ) ;
	if( DiskMounted( diskid ) ) {
		// add warning for mounted disk
		if( MessageBox( m_hWnd, wipedata_message, _T("Erase Disk?"), MB_OKCANCEL )!=IDOK ) {
			return ;
		}
		// diskplay wait cursor
		ShowCursor(1);
		HCURSOR	oldcursor = SetCursor(LoadCursor(NULL,IDC_WAIT));
		DiskUnmount( diskid ) ;
		if( oldcursor ) SetCursor(oldcursor);
		ShowCursor(0);
	}

    OfficerIdEditDlg editdlg ;
	memset( &(editdlg.officerid), 0, sizeof(editdlg.officerid) );
	//DiskReadId( item, &(editdlg.officerid) ) ;

	if( editdlg.DoModal(IDD_DIALOG_OFFICERIDKEY, m_hWnd) == IDOK ) {

		// diskplay wait cursor
		ShowCursor(1);
		HCURSOR	oldcursor = SetCursor(LoadCursor(NULL,IDC_WAIT));

		DiskWriteId( item, &(editdlg.officerid) ) ;

		char * tail = strstr((char *)itemstr, ") : ");
		string nitemstr = (char *)itemstr ;
		if( tail ) {
			sprintf( nitemstr.strsize(300), "id(%s%s", editdlg.officerid.officerid, tail );
		}

		// change item string 
		SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DISK), LB_DELETESTRING, (WPARAM)sel, (LPARAM)0 ) ;
		sel = SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DISK),LB_ADDSTRING, 0, (LPARAM)(LPCTSTR)nitemstr ) ;
		// set id of the item
		SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DISK), LB_SETITEMDATA , (WPARAM)sel, (LPARAM)item ) ;

		// log writing
		string logstr ;
		sprintf( logstr.strsize(500), "Create disk : %s", (char *)nitemstr );
		log_write( logstr );

		if( oldcursor ) SetCursor(oldcursor);
		ShowCursor(0);

	}
}

void OfficerIdManagerDlg::OnBnEdit()
{
	int item ;
	int sel = SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DISK), LB_GETCURSEL, 0, 0 ) ;
	if( sel<0 || sel==LB_ERR ) {
		MessageBox(m_hWnd, _T("Please Select A USB Key"), NULL, MB_OK);
		return ;
	}
	string itemstr ;
	SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DISK),LB_GETTEXT, (WPARAM)sel, (LPARAM)itemstr.tcssize(300) ) ;
	item = SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DISK), LB_GETITEMDATA, (WPARAM)sel, 0 ) ;

	string diskid = DiskSerialId( item ) ;
	if( DiskMounted( diskid ) ) {
		// add warning for mounted disk
		if( MessageBox( m_hWnd, wipedata_message, _T("Erase Disk?"), MB_OKCANCEL )!=IDOK ) {
			return ;
		}
		// diskplay wait cursor
		ShowCursor(1);
		HCURSOR	oldcursor = SetCursor(LoadCursor(NULL,IDC_WAIT));
		DiskUnmount( diskid ) ;
		if( oldcursor ) SetCursor(oldcursor);
		ShowCursor(0);
	}

    OfficerIdEditDlg editdlg ;
	memset( &(editdlg.officerid), 0, sizeof(editdlg.officerid) );
	DiskReadId( item, &(editdlg.officerid) ) ;

	if( editdlg.DoModal(IDD_DIALOG_OFFICERIDKEY, m_hWnd) == IDOK ) {

		// diskplay wait cursor
		ShowCursor(1);
		HCURSOR	oldcursor = SetCursor(LoadCursor(NULL,IDC_WAIT));

		// log writing
		string logstr ;
		sprintf( logstr.strsize(500), "Edit disk : %s", (char *)itemstr );
		log_write( logstr );

		DiskWriteId( item, &(editdlg.officerid) ) ;

		char * tail = strstr((char *)itemstr, ") : ");
		string nitemstr = (char *)itemstr ;
		if( tail ) {
			sprintf( nitemstr.strsize(300), "id(%s%s", editdlg.officerid.officerid, tail );
		}

		// change item string 
		SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DISK), LB_DELETESTRING, (WPARAM)sel, (LPARAM)0 ) ;
		sel = SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DISK),LB_ADDSTRING, 0, (LPARAM)(LPCTSTR)nitemstr ) ;
		// set id of the item
		SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DISK), LB_SETITEMDATA , (WPARAM)sel, (LPARAM)item ) ;

		// log writing
		sprintf( logstr.strsize(500), "Save disk : %s", (char *)nitemstr );
		log_write( logstr );

		if( oldcursor ) SetCursor(oldcursor);
		ShowCursor(0);

	}
}

void OfficerIdManagerDlg::OnBnFormat()
{
	int sel = SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DISK), LB_GETCURSEL, 0, 0 ) ;
	if( sel<0 || sel==LB_ERR ) {
		MessageBox(m_hWnd, _T("Please Select A USB Key"), NULL, MB_OK);
		return ;
	}
	string itemstr ;
	SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DISK),LB_GETTEXT, (WPARAM)sel, (LPARAM)itemstr.tcssize(300) ) ;
	int item = SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DISK), LB_GETITEMDATA, (WPARAM)sel, 0 ) ;

	string diskid = DiskSerialId( item ) ;
	if( DiskMounted( diskid ) ) {
		// add warning for mounted disk
		if( MessageBox( m_hWnd, wipedata_message, _T("Erase Disk?"), MB_OKCANCEL )!=IDOK ) {
			return ;
		}
		// diskplay wait cursor
		ShowCursor(1);
		HCURSOR	oldcursor = SetCursor(LoadCursor(NULL,IDC_WAIT));
		DiskUnmount( diskid ) ;
		if( oldcursor ) SetCursor(oldcursor);
		ShowCursor(0);
	}

	// diskplay wait cursor
	ShowCursor(1);
	HCURSOR	oldcursor = SetCursor(LoadCursor(NULL,IDC_WAIT));

	officerid_t oid ;
	if( !DiskReadId( item, &oid ) ) {
		memset( &oid, 0, sizeof(oid) );

		// log formating
		string logstr ;
		sprintf( logstr.strsize(500), "Format disk : %s", (char *)itemstr );
		log_write( logstr );

	}
	else {
	}
	DiskWriteId( item, &oid ) ;

	if( oldcursor ) SetCursor(oldcursor);
	ShowCursor(0);

}

void OfficerIdManagerDlg::OnBnDelete()
{
	int sel = SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DISK), LB_GETCURSEL, 0, 0 ) ;
	if( sel<0 || sel==LB_ERR ) {
		MessageBox(m_hWnd, _T("Please Select A USB Key"), NULL, MB_OK);
		return ;
	}
	string itemstr ;
	SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DISK),LB_GETTEXT, (WPARAM)sel, (LPARAM)itemstr.tcssize(300) ) ;
	int item = SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DISK), LB_GETITEMDATA, (WPARAM)sel, 0 ) ;

	officerid_t oid ;
	if( DiskReadId( item, &oid ) ) {
		// add warning for mounted disk
		if( MessageBox( m_hWnd, _T("Are you sure you want to delete this Officer ID? "), _T("Delete Officer ID"), MB_YESNO )==IDYES ) {
			//unmount disk
			// diskplay wait cursor
			ShowCursor(1);
			HCURSOR	oldcursor = SetCursor(LoadCursor(NULL,IDC_WAIT));

			DiskEraseId( item ) ;

			char * tail = strstr((char *)itemstr, ") : ");
			string nitemstr = (char *)itemstr ;
			if( tail ) {
				sprintf( nitemstr.strsize(300), "id(%s%s", emptyid, tail );
			}

			// change item string 
			SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DISK), LB_DELETESTRING, (WPARAM)sel, (LPARAM)0 ) ;
			sel = SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DISK),LB_ADDSTRING, 0, (LPARAM)(LPCTSTR)nitemstr ) ;
			// set id of the item
			SendMessage( GetDlgItem(m_hWnd, IDC_LIST_DISK), LB_SETITEMDATA , (WPARAM)sel, (LPARAM)item ) ;

			// log formating
			string logstr ;
			sprintf( logstr.strsize(500), "Delete disk : %s", (char *)itemstr );
			log_write( logstr );

			if( oldcursor ) SetCursor(oldcursor);
			ShowCursor(0);

			return ;
		}
	}

}

void OfficerIdManagerDlg::OnBnLog()
{
	OfficerIdLogDlg logdialog ;
	logdialog.DoModal( IDD_DIALOG_LOG, m_hWnd);
}


int OfficerIdLogDlg::OnInitDialog()
{
	RECT rce, rcp ;
	HWND htext = GetDlgItem( m_hWnd, IDC_EDIT_LOG );

	string log ;
	log_get( log ) ;

	// set log string
	SetDlgItemText( m_hWnd, IDC_EDIT_LOG, log );
	log="" ;	// release memory

	// init size
    GetWindowRect(htext, &rce); 
    GetClientRect(m_hWnd, &rcp);

	dw = ( rcp.right - rcp.left ) - (rce.right - rce.left );
	dh = ( rcp.bottom - rcp.top ) - (rce.bottom - rce.top );

	return 1 ;
}

void OfficerIdLogDlg::OnSize( int code, int w, int h)
{
	HWND htext = GetDlgItem( m_hWnd, IDC_EDIT_LOG );
	SetWindowPos(htext, HWND_TOP, 0, 0,
			 w-dw, h-dh, SWP_NOMOVE );

}

void OfficerIdLogDlg::OnBnClear()
{
	// clear log file
	log_clear();
	string log ;
	SetDlgItemText( m_hWnd, IDC_EDIT_LOG, log );
}
