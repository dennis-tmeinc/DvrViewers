// PWManager.cpp : Defines the entry point for the application.
//
#include <stdafx.h>

#include <Windows.h>
#include <Dbt.h>
#include <Shlobj.h>
#include <Shellapi.h>
#include <winioctl.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include <string.h>

#include "..\common\cwin.h"
#include "..\common\cstr.h"
#include "..\common\cdir.h"
#include "..\common\util.h"
#include "DlgSrcId.h"
#include "DlgCleanTarget.h"
#include "DlgClTargetProgress.h"
#include "ManagerDlg.h"


#define TIMER_ARCHIVE  (101)

DWORD devicemap  ;
string targetdir="F:" ;
int    targetminspace=10 ;
int    keepsourceunlock=0 ;
static string pwsubkey("Software\\TME\\PW");

#define ARCHIVE_IDLE    (0)
#define ARCHIVE_START   (1)
#define ARCHIVE_CALSIZE (2)
#define ARCHIVE_RUN     (3)
#define ARCHIVE_COMPLETE (4)
#define ARCHIVE_CLEAN (5)

HANDLE OpenVolume(TCHAR cDriveLetter);
BOOL CloseVolume(HANDLE hVolume);
BOOL TryLockVolume(HANDLE hVolume);
BOOL LockVolume(HANDLE hVolume);
BOOL UnlockVolume(HANDLE hVolume);
BOOL FlushVolume(TCHAR cDriveLetter);
BOOL EjectVolume(TCHAR cDriveLetter);

static int archive_state ;      // 0: exit, 1: calculate space, 2: archiving
static long long totalarchivesize ;
static long long curarchivesize ;
static int archdisk ;           // current archiving or archived disk
static string arch_msg ;

int SaveCfg( char * name, char * value )
{
    HKEY  key = NULL ;
    if( RegCreateKeyEx( HKEY_LOCAL_MACHINE, pwsubkey, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL)!=ERROR_SUCCESS ) {
        if( RegCreateKeyEx( HKEY_CURRENT_USER, pwsubkey, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL)!=ERROR_SUCCESS ) {
            return 0 ;
        }
    }

    string sname, svalue ;
    sname = name ;
    svalue = value ;
    
    RegSetValueEx( key, sname, 0, REG_SZ, (BYTE*)(wchar_t *)svalue, sizeof(wchar_t)*(wcslen(svalue)+1));

    RegCloseKey( key );
    return 1 ;
}

int ReadCfg( char * name, char * value, int bsize )
{
    HKEY  key = NULL ;
    if( RegCreateKeyEx( HKEY_LOCAL_MACHINE, pwsubkey, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL)!=ERROR_SUCCESS ) {
        if( RegCreateKeyEx( HKEY_CURRENT_USER, pwsubkey, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL)!=ERROR_SUCCESS ) {
            return 0 ;
        }
    }

    DWORD type, rsize ;
    string sname, svalue ;
    sname = name ;
    svalue.wcssize( bsize+2 );
    rsize = sizeof(wchar_t)*bsize ;
    type = REG_SZ ;

    if( RegQueryValueEx( key, sname, 0, &type, (BYTE *)(wchar_t *)svalue, &rsize )==ERROR_SUCCESS ){
        strncpy( value, svalue, bsize );
        RegCloseKey( key );
        return 1;
    }
    else {
        RegCloseKey( key );
        return 0 ;
    }
}

class AboutDlg : public Dialog 
{
protected:
    virtual int OnInitDialog(){
        int   libversion[4] ;
        string version ;
        HRSRC hres = FindResource(NULL, MAKEINTRESOURCE(VS_VERSION_INFO), RT_VERSION);
        HGLOBAL hglo=LoadResource( NULL, hres );
        WORD * pver = (WORD *)LockResource( hglo );
        VS_FIXEDFILEINFO * pverinfo = (VS_FIXEDFILEINFO *) (pver+20);
        if( pverinfo->dwSignature ==  VS_FFI_SIGNATURE ) {
            libversion[0]=(pverinfo->dwProductVersionMS)/0x10000 ;
            libversion[1]=(pverinfo->dwProductVersionMS)%0x10000 ;
            libversion[2]=(pverinfo->dwProductVersionLS)/0x10000 ;
            libversion[3]=(pverinfo->dwProductVersionLS)%0x10000 ;
            sprintf(version.strsize(128), "Version %d.%d.%d.%d", libversion[0], libversion[1], libversion[2], libversion[3] );
            SetDlgItemText(m_hWnd, IDC_STATIC_VERSION, version );
        }
        return TRUE;
    }
};

void ManagerDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		AboutDlg dlgAbout;
		dlgAbout.DoModal(IDD_ABOUTBOX, m_hWnd);
	}
}

int ManagerDlg::OnInitDialog()
{
    // IDM_ABOUTBOX must be in the system command range.
    HMENU hSysMenu = ::GetSystemMenu(m_hWnd, FALSE);
	if (hSysMenu != NULL)
	{
		string strAboutMenu="About PWManager";
		::AppendMenu(hSysMenu,MF_SEPARATOR, 0, NULL);
		::AppendMenu(hSysMenu,MF_STRING, IDM_ABOUTBOX, strAboutMenu);
	}


    // supress no disk error message, when calling FindFirstFile() on empty drive
    SetErrorMode(SEM_FAILCRITICALERRORS);
    string value ;

   	HICON hIcon = LoadIcon(ResInst(), MAKEINTRESOURCE(IDI_PWMANAGER));
    ::SendMessageA(m_hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
	::SendMessageA(m_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

    if( ReadCfg( "target", value.strsize(1024), 1024) ) {
        targetdir=value ;
    }
    SetDlgItemText( m_hWnd, IDC_EDIT_PWDATADIR, targetdir );

    if( ReadCfg( "keepsourceunlock", value.strsize(1024), 1024 ) ) {
        sscanf(value, "%d", &keepsourceunlock);
    }
    else {
        keepsourceunlock = 0 ;
    }

    // initialize device map
    devicemap = GetLogicalDrives();

    if( ReadCfg( "targetminspace", value.strsize(1024), 1024 ) ) {
        sscanf(value, "%d", &targetminspace);
    }
    else {
        targetminspace = 10 ;
    }
    SetDlgItemInt( m_hWnd, IDC_EDIT_FREESPACE, targetminspace, TRUE );

    archive_state = ARCHIVE_IDLE ;
    archdisk = 0 ;

    m_checktargetspace = 1 ;

    SetTimer(m_hWnd, TIMER_ARCHIVE, 1000, NULL );

    return TRUE ;
}

void ManagerDlg::OnDeviceChange( UINT nEventType, DWORD dwData )
{
    if( nEventType == DBT_DEVICEARRIVAL || nEventType == DBT_DEVICEREMOVECOMPLETE || nEventType == DBT_DEVNODES_CHANGED ) {
        SetTimer(m_hWnd, TIMER_ARCHIVE, 2000, NULL ) ;
	}
    return ;
}

void ManagerDlg::OnBnClickedButtonClean()
{

}

void ManagerDlg::OnEnKillfocusEditFreespace()
{
    int minspace ;
    minspace = GetDlgItemInt(m_hWnd, IDC_EDIT_FREESPACE, NULL, FALSE );
    if( minspace != targetminspace ) {
        targetminspace = minspace ;
        string tmsp ;
        sprintf( tmsp.strsize(30), "%d", minspace );
        SaveCfg( "targetminspace", tmsp );
        UpdateTargetFreeSpace();
    }
}

// update target disk free space
void ManagerDlg::UpdateTargetFreeSpace()
{
    string msg ;
    ULARGE_INTEGER FreeBytesAvailable;
    ULARGE_INTEGER TotalNumberOfBytes;
    if( GetDiskFreeSpaceEx( targetdir, &FreeBytesAvailable, &TotalNumberOfBytes, NULL )) 
    {
        // show free space
        double freespace = ((double)FreeBytesAvailable.QuadPart)/(double)(1024*1024*1024);
        sprintf(msg.strsize(128), "%.2f GB", freespace );
        SetDlgItemText(m_hWnd, IDC_STATIC_FREESPACE, msg );
        // show total space
        double totalspace = ((double)TotalNumberOfBytes.QuadPart)/(double)(1024*1024*1024);
        sprintf(msg.strsize(128), "%.2f GB", totalspace );
        SetDlgItemText(m_hWnd, IDC_STATIC_TOTALSPACE, msg );

        if( m_checktargetspace && ((int)freespace) < targetminspace ) {       // space too low
            OnBnClickedButtonCleanTarget();
        }
    }
    else {
        sprintf(msg.strsize(128), "(Not available)" );
        SetDlgItemText(m_hWnd, IDC_STATIC_FREESPACE, msg );
        SetDlgItemText(m_hWnd, IDC_STATIC_TOTALSPACE, msg );
    }
}

void ManagerDlg::OnBnClickedButtonBrowseDataDir()
{
    string displayname ;
    BROWSEINFO browsinfo ;
	LPCITEMIDLIST pidl ;
	memset( &browsinfo, 0, sizeof(browsinfo) );
	browsinfo.hwndOwner = m_hWnd ;
	browsinfo.pszDisplayName = displayname.tcssize(1024) ;
	browsinfo.lpszTitle=_T("Select directory where to copy video files");
	browsinfo.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI ;
	pidl=SHBrowseForFolder(&browsinfo);
	if( pidl ) {
        string foldername ;
        if( SHGetPathFromIDList( pidl, foldername.tcssize(1024)) ) {
            SetDlgItemText( m_hWnd, IDC_EDIT_PWDATADIR, foldername );
            char * pfn = (char *)foldername ;
            int l = strlen( pfn ) ;
            if( l>1 && pfn[l-1]=='\\' ) {
                pfn[l-1]='\0' ;
            }
            targetdir=pfn ;
            SaveCfg("target", targetdir);

            // also update target disk free space
            UpdateTargetFreeSpace();

		}
		CoTaskMemFree((LPVOID)pidl);
	}
}



static int archivefile(char * sourcefile, char * targetfile )
{
    int    rsize ;
    FILE * tfile ;
    FILE * sfile ;
    string tempfile ;
    sfile=fopen( sourcefile, "rb" );
    if(sfile==NULL) {
        return -1 ;
    }
    fseek( sfile, 0, SEEK_END ) ;
    rsize = ftell( sfile );

    tfile=fopen( targetfile, "rb" ) ;
    if( tfile ) {
        fseek( tfile, 0, SEEK_END );
        if( ftell( tfile )>=rsize ) {
            fclose( tfile ) ;
            fclose( sfile ) ;
            return 2 ;
        }
        fclose(tfile);
        remove(targetfile);
    }

    if( archive_state==ARCHIVE_CALSIZE ) {                        // only to calculate archive size
        lock();
        totalarchivesize += (long long) rsize ;
        unlock();
        fclose( sfile );
        return 1;
    }
    else if( archive_state==ARCHIVE_RUN ) {

        rewind(sfile);

        sprintf( tempfile.strsize(strlen(targetfile)+10), "%s.part", (char *)targetfile ) ;
        tfile=fopen( tempfile, "wb" ) ;
        if( tfile ) {
            // do the copy 
            int  ioerror = 0 ;
            char * buf = new char [0x100000] ;
            do {
                rsize = fread( buf, 1, 0x100000, sfile );
                if( ferror( sfile ) )      {
                    ioerror=1 ;
                    break;
                }
                if( rsize>0 ) {
                    fwrite( buf, 1, rsize, tfile );
                    if( ferror( tfile ) )      {
                        ioerror=2 ;
                        break;
                    }
                    lock();
                    curarchivesize += (long long)rsize ;
                    unlock();
                }
            } while( rsize>0 ) ;
            delete [] buf ;
            fclose( tfile );
            fclose( sfile );
            if( ioerror==0 ) {
                rename( tempfile, targetfile );
                // rename source file?
                tempfile = sourcefile ;
                char * t ;
                t = strstr( tempfile, "_L_" ) ;
                if( t ) {
                    t[1]='N' ;
                    if( !keepsourceunlock ) {
                        rename( sourcefile, tempfile ) ;
                    }
                }
                else {
                    t = strstr( tempfile, "_L.0" ) ;
                    if( t ) {
                        t[1] = 'N' ;
                        if( !keepsourceunlock ) {
                            rename( sourcefile, tempfile );
                        }
                    }
                }
                return 1 ;
            }
            else {
                remove( tempfile );
                return 0 ;
            }
        }
        else {
            fclose( sfile );
            return 0 ;
        }
    }
    return 0 ;
}

static void ArchiveDir(char * sourdir, char * targdir)
{
    string target ;
    if( stricmp( sourdir, targdir )==0 ) {
        return ;        // of cause
    }

    dirfind dir(targdir);
    if( !dir.find() ) {
        dir.close();
        mkdir(targdir);
    }
    dir.open( sourdir ) ;
    while( dir.find() ) {
        if( dir.isdir() ) {
            // arch dir 
            sprintf( target.strsize( strlen(targdir)+strlen(dir.filename())+2 ), 
                    "%s\\%s", targdir, (char *)dir.filename() );
            ArchiveDir( dir.filepath(), target  );
        }
        else if( dir.isfile() ) {
            if( strstr( dir.filename(), "_N_" ) == NULL && strstr( dir.filename(), "_N.001")==NULL ) {  // not a locked file
                // archive file ;
                lock() ;
                arch_msg = dir.filename();
                unlock() ;
                sprintf( target.strsize( strlen(targdir)+strlen(dir.filename())+2 ), 
                    "%s\\%s", targdir, (char *)dir.filename() );
                archivefile( dir.filepath(), target );
            }
        }
    }
}


static void ArchiveHarddrive()
{
    char dirpath[4] ;

    sprintf( dirpath, "%c:\\", archdisk );
    dirfind rootdir(dirpath, "_*_");
    if( rootdir.find() ) {
        if( rootdir.isdir() ) {
            // First to calculate file size to copy
            totalarchivesize = 0 ;
            archive_state = ARCHIVE_CALSIZE ;
            rootdir.open( dirpath ) ;
            while( rootdir.find() ) {
                if( rootdir.isdir() ) {
                    string tardir ;
                    sprintf( tardir.strsize(strlen(targetdir)+strlen(rootdir.filename())+2),
                        "%s\\%s", (char *)targetdir, (char *)rootdir.filename() );
                    ArchiveDir( rootdir.filepath(), tardir ) ;
                }
            }

            curarchivesize = 0 ;
            archive_state = ARCHIVE_RUN ;
            rootdir.open( dirpath ) ;
            while( rootdir.find() ) {
                if( rootdir.isdir() ) {
                    string tardir ;
                    sprintf( tardir.strsize(strlen(targetdir)+strlen(rootdir.filename())+2),
                        "%s\\%s", (char *)targetdir, (char *)rootdir.filename() );
                    ArchiveDir( rootdir.filepath(), tardir ) ;
                }
            }
        }
    }
    archive_state = ARCHIVE_COMPLETE ;
}

HANDLE hArchThread ;

DWORD WINAPI ArchThread( LPVOID lpParam ) 
{ 
    SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_IDLE );      // run in idle
    ArchiveHarddrive();
    return 0; 
}

// return true if archiving started or disk archive stat changed
int ManagerDlg::DiskChange()
{
    int res = 1 ;
    int drive ;
	DWORD dmap ;
    DWORD xmap ;
    DWORD xbit ;
    string tvskeyfilename ;
    FILE * ftvskey ;

    if (hArchThread != NULL ) {             // previous task not finish
        return 1 ;
    }

    // archiving thread is not running
	dmap = GetLogicalDrives();
    xmap = dmap^devicemap ;
    if( xmap == 0 ) {
        return 0 ;
    }

    for( xbit=1, drive='A'; drive<='Z'; drive++, xbit<<=1 ) {
        if( (xmap&xbit)==0 ) continue ;
        devicemap ^= xbit ;
        
        // don't over write to target disk (in case target disk is also removable).
        if( drive == toupper( *(char *)targetdir ) ) {
            m_checktargetspace = 1 ;        // going to check target again
            continue ; 
        }

        // remove of disk
        if( (dmap&xbit)==0 ) {
            if( drive == archdisk ) {           // remove current disk ?
                archdisk = 0 ;
                archive_state = ARCHIVE_IDLE ;
            }
            continue ;
        }

        // detect tvs disk space, if 0, skip it
        string sourcedisk ;
        sprintf( sourcedisk.strsize(10), "%c:\\", drive );
        // update source disk free space
        ULARGE_INTEGER FreeBytesAvailable;
        ULARGE_INTEGER TotalNumberOfBytes;
        if( GetDiskFreeSpaceEx( sourcedisk, &FreeBytesAvailable, &TotalNumberOfBytes, NULL )) 
        {
            if( ((double)TotalNumberOfBytes.QuadPart)/(double)(1024*1024) < 10.0 ) {
                continue ;
            }
        }
        else {
            // unable to read totol bytes, don't do 
            continue ;
        }

        // detect tvs key disk, and skip it
        sprintf( tvskeyfilename.strsize(30), "%c:\\tvs\\tvskey.dat", drive );
        ftvskey = fopen( tvskeyfilename, "r" ) ;
        if( ftvskey != NULL ) {
            fclose( ftvskey );
            continue ;
        }

        // found a potential DVR disk, create archiving thread to archive this drive
        archdisk = drive ;
        archive_state = ARCHIVE_START ;
        hArchThread = CreateThread( NULL, 0, ArchThread, NULL, 0, NULL );
        break ;
    }

    return 1 ;

}

void ManagerDlg::updsrcdiskspace()
{
    string msg1 ;
    string msg2 ;
    if( archdisk ) {
        string sourcedisk ;
        sprintf( sourcedisk.strsize(10), "%c:\\", archdisk );
        // update source disk free space
        ULARGE_INTEGER FreeBytesAvailable;
        ULARGE_INTEGER TotalNumberOfBytes;
        if( GetDiskFreeSpaceEx( sourcedisk, &FreeBytesAvailable, &TotalNumberOfBytes, NULL )) 
        {
            // show free space
            sprintf( msg1.strsize(1024), "Disk %c: completed, totalsize: %.2f GB freespace: %.2f GB archived: %.2f GB",
                archdisk,
                ((double)TotalNumberOfBytes.QuadPart)/(double)(1024*1024*1024),
                ((double)FreeBytesAvailable.QuadPart)/(double)(1024*1024*1024),
                ((double)curarchivesize)/(double)(1024*1024*1024)
                );

            EnableWindow(GetDlgItem(m_hWnd, IDC_BUTTON_SETUPID ),TRUE);
            EnableWindow(GetDlgItem(m_hWnd, IDC_BUTTON_SRCCLEAN ),TRUE);
            EnableWindow(GetDlgItem(m_hWnd, IDC_BUTTON_EJECT ),TRUE);
            SetDlgItemText(m_hWnd, IDC_TXT_SOURCEMSG, msg1 );

            // if disk ID can be read, show ID
            string pwidFile ;
            string pwidSection ;
            string pwidKey ;
            string pwidDefValue ;
            string pwidValue ;
            sprintf( pwidFile.strsize(20), "%c:\\%s", archdisk, "pwid.conf" );
            pwidSection = "system" ;
            pwidKey = "id1" ;
            pwidDefValue = "" ;

            GetPrivateProfileString( pwidSection, pwidKey, pwidDefValue, pwidValue.tcssize(256), 256, pwidFile );
            if( strlen( pwidValue ) > 0 ) {
                sprintf(msg2.strsize(200), "Disk ID: %s", (char *)pwidValue );
            }
            SetDlgItemText(m_hWnd, IDC_TXT_SOURCEMSG2, msg2 );
        }
    }
}

void ManagerDlg::OnTimer(UINT_PTR nIDEvent)
{
    string msg1 ;
    string msg2 ;
    int totalm ;
    int archm ;
    int percent ;

    KillTimer( m_hWnd, TIMER_ARCHIVE );
    switch ( archive_state ) {
        case ARCHIVE_COMPLETE :

            // close archive thread
            if (hArchThread != NULL ) {
                WaitForSingleObject(hArchThread, 10000 );
                CloseHandle( hArchThread ) ;
                hArchThread = NULL ;

                if( archdisk && curarchivesize>1024 ) {
                    OnBnClickedButtonSrcclean();
                }

            }

            updsrcdiskspace();

            ShowWindow( GetDlgItem(m_hWnd, IDC_PROGRESS_SOURCE ), SW_HIDE ) ;

            // update target disk free space
            UpdateTargetFreeSpace();
            if( DiskChange() ) {
                SetTimer( m_hWnd, TIMER_ARCHIVE, 500, NULL );
            }

            break;

        case ARCHIVE_IDLE :
            KillTimer( m_hWnd, TIMER_ARCHIVE );

            SetDlgItemText(m_hWnd, IDC_TXT_SOURCEMSG, _T("Please Insert Disk") );
            SetDlgItemText(m_hWnd, IDC_TXT_SOURCEMSG2, _T("") );

            EnableWindow(GetDlgItem(m_hWnd, IDC_BUTTON_SETUPID ),FALSE);
            EnableWindow(GetDlgItem(m_hWnd, IDC_BUTTON_SRCCLEAN ),FALSE);
            EnableWindow(GetDlgItem(m_hWnd, IDC_BUTTON_EJECT ),FALSE);
            ShowWindow( GetDlgItem(m_hWnd, IDC_PROGRESS_SOURCE ), SW_HIDE ) ;
            
            // update target disk free space
            UpdateTargetFreeSpace();

            if( DiskChange() ) {
                m_checktargetspace = 1 ;        // going to check target again
                SetTimer( m_hWnd, TIMER_ARCHIVE, 500, NULL );
            }
            break;

        case ARCHIVE_CALSIZE :
            // update target disk free space
            UpdateTargetFreeSpace();
            lock();
            sprintf( msg1.strsize(500), "Calculate size : %s", (char *)arch_msg );
            totalm = (int)(totalarchivesize/1000000);
            sprintf(msg2.strsize(500), "Total archive size: %d Mb", totalm );
            unlock();
            SetDlgItemText(m_hWnd, IDC_TXT_SOURCEMSG, msg1 );
            SetDlgItemText(m_hWnd, IDC_TXT_SOURCEMSG2, msg2 );

            EnableWindow(GetDlgItem(m_hWnd, IDC_BUTTON_SETUPID ),FALSE);
            EnableWindow(GetDlgItem(m_hWnd, IDC_BUTTON_SRCCLEAN ),FALSE);
            EnableWindow(GetDlgItem(m_hWnd, IDC_BUTTON_EJECT ),FALSE);
            ShowWindow( GetDlgItem(m_hWnd, IDC_PROGRESS_SOURCE ), SW_SHOW ) ;
            SendDlgItemMessage( m_hWnd, IDC_PROGRESS_SOURCE, PBM_SETPOS, (WPARAM)0, 0 );

            SetTimer( m_hWnd, TIMER_ARCHIVE, 500, NULL );

            break;

        case ARCHIVE_RUN :
            // update target disk free space
            UpdateTargetFreeSpace();
            ShowWindow( GetDlgItem(m_hWnd, IDC_PROGRESS_SOURCE ), SW_SHOW ) ;

            // show src disk space
            lock();
            totalm = (int)(totalarchivesize/1000000);
            archm = (int)(curarchivesize/1000000);
            sprintf( msg1.strsize(500), "Archiving : %s", (char *)arch_msg );
            sprintf(msg2.strsize(500), "Total size: %d Mb, Remain %d Mb", totalm, (totalm-archm) );
            unlock();
            if( totalm<=0 ) totalm = 1 ;
            percent = archm*100/totalm ;
            if( percent>100 ) 
                percent = 100 ;

            SetDlgItemText(m_hWnd, IDC_TXT_SOURCEMSG, msg1 );
            SetDlgItemText(m_hWnd, IDC_TXT_SOURCEMSG2, msg2 );

            EnableWindow(GetDlgItem(m_hWnd, IDC_BUTTON_SETUPID ),FALSE);
            EnableWindow(GetDlgItem(m_hWnd, IDC_BUTTON_SRCCLEAN ),FALSE);
            EnableWindow(GetDlgItem(m_hWnd, IDC_BUTTON_EJECT ),FALSE);
            ShowWindow( GetDlgItem(m_hWnd, IDC_PROGRESS_SOURCE ), SW_SHOW ) ;
            SendDlgItemMessage( m_hWnd, IDC_PROGRESS_SOURCE, PBM_SETPOS, (WPARAM)percent, 0 );

            SetTimer( m_hWnd, TIMER_ARCHIVE, 500, NULL );

            break ;

        case ARCHIVE_CLEAN :
            // update target disk free space
            UpdateTargetFreeSpace();

            // show src disk space
            if( archdisk>0 ) {
                FlushVolume(archdisk);
                updsrcdiskspace();
            }
            SetTimer( m_hWnd, TIMER_ARCHIVE, 500, NULL );
            break ;
    }
}


void ManagerDlg::OnBnClickedButtonSetupId()
{
    DlgSrcId SetIdDlg ;
    string pwidFile ;
    string pwidSection ;
    string pwidKey ;
    string pwidDefValue ;
    string pwidValue ;

    if( archdisk == 0 ) {
        return ;
    }

    sprintf( pwidFile.strsize(20), "%c:\\%s", archdisk, "pwid.conf" );
    pwidSection = "system" ;
    pwidKey = "id1" ;
    pwidDefValue = "" ;

    GetPrivateProfileString( pwidSection, pwidKey, pwidDefValue, pwidValue.tcssize(256), 256, pwidFile );
    if( strlen( pwidValue ) > 0 ) {
        SetIdDlg.m_curId=pwidValue ;
        SetIdDlg.m_newId=pwidValue ;
    }
    else {
        SetIdDlg.m_curId="(Not available)" ;
        SetIdDlg.m_newId="" ;
    }
    if( SetIdDlg.DoModal( IDD_DIALOG_SETID, m_hWnd ) == IDOK && strlen(SetIdDlg.m_newId)>0  ) {
        FILE * fidfile ;
        fidfile = fopen( pwidFile, "wb" );
        if( fidfile ) {
            fprintf( fidfile, "#CONF\n[system]\nid1=%s\n", (char *)(SetIdDlg.m_newId) );
            fclose( fidfile );
        }
    }
}

int timeout=30;
int nCleanDiskRun ;

int f264timelen(const char *filename, struct dvrtime *dvrt)
{
    int n ;
    int len=0 ;
    memset( dvrt, 0, sizeof(*dvrt));
    n=sscanf( &filename[5], "%04d%02d%02d%02d%02d%02d_%d", 
             &(dvrt->year),
             &(dvrt->month),
             &(dvrt->day),
             &(dvrt->hour),
             &(dvrt->min),
             &(dvrt->second),
             &len );
    if (n!=7) {
        return 0 ;
    }
    else {
        return len ;
    }
}

DWORD WINAPI CleanDiskThreadFunction( LPVOID lpParam ) 
{ 

    DlgClTargetProgress * dlgcp = (DlgClTargetProgress *)lpParam ;

    struct dvrtime dvrtnow ;
    SYSTEMTIME now ;
    GetSystemTime(&now);
    time_timedvr( &now, &dvrtnow );

    string msg ;

    SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_IDLE );      // run in idle

    dirfind rootdir(dlgcp->m_rootdir, "_*_");
    while( !dlgcp->m_complete && rootdir.find() && rootdir.isdir() ) {
        dirfind datedir( rootdir.filepath(), "20??????" ) ;
        while( !dlgcp->m_complete && datedir.find() && datedir.isdir() ) {
            dirfind chdir( datedir.filepath(), "CH??" );
            while( !dlgcp->m_complete && chdir.find() && chdir.isdir() ) {
                dirfind videodir( chdir.filepath(), "CH*" );
                while( !dlgcp->m_complete && videodir.find() && videodir.isfile() ) {
                    struct dvrtime dvrt ;
                    int len = f264timelen ( videodir.filename(), &dvrt );
                    if( len>0 ) {
                        if( ( dvrtnow - dvrt )>( dlgcp->m_lifttime*24*60*60 ) ) {
                            sprintf(msg.strsize(100), "deleting %s", (char *)videodir.filename() );
                            lock();
                            dlgcp->m_msg = msg ;
                            dlgcp->m_update = 1 ;
                            unlock();
                            remove( videodir.filepath() );
                        }
                    }
                    else {
                        sprintf(msg.strsize(100), "deleting %s", (char *)videodir.filename() );
                        lock();
                        dlgcp->m_msg = msg ;
                        dlgcp->m_update = 1 ;
                        unlock();
                        remove( videodir.filepath() );
                    }
                }
                videodir.close();
                // try remove empty directory
                rmdir( chdir.filepath() );
            }
            chdir.close();
            // try remove empty directory
            rmdir( datedir.filepath() );
        }
        datedir.close();
        // try remove empty directory
        rmdir( rootdir.filepath() );
    }

    dlgcp->m_complete = 1 ;

    return 0; 
}

void ManagerDlg::OnBnClickedButtonCleanTarget()
{
   DlgCleanTarget dlgct ;
   if( dlgct.DoModal( IDD_DIALOG_CLEANTARGET, m_hWnd )==IDOK ) {
       // to clean target files
       DlgClTargetProgress  dlgcp ;

       dlgcp.m_lifttime = dlgct.m_filelifeday ;
       dlgcp.m_rootdir = targetdir;

       HANDLE hCleanDiskThread ;

       hCleanDiskThread = CreateThread( NULL, 0, CleanDiskThreadFunction, (LPVOID) &dlgcp, 0, NULL );
       dlgcp.DoModal( IDD_DIALOG_CLRTARGETPROGESS, m_hWnd );

       // to stop cleaning thread
        dlgcp.m_complete = 1 ;

       // wait cleaning thread
        WaitForSingleObject(hCleanDiskThread, 10000 ) ;
        CloseHandle( hCleanDiskThread ) ;
   }
   m_checktargetspace = 0 ;
}

int DlgCleanTarget::OnInitDialog()
{
    char value[512] ;
    m_filelifeday = 365 ;
    if( ReadCfg( "filelife", value, 512) ) {
        sscanf( value, "%d", &m_filelifeday );
    }
    else {
        m_filelifeday = 365 ;
    }
    SetDlgItemInt(m_hWnd, IDC_EDIT_FILELIFEDAYS, (UINT)m_filelifeday, FALSE );
    return TRUE ;
}

int DlgCleanTarget::OnOK()
{
    string filelife ;
    m_filelifeday = GetDlgItemInt(m_hWnd, IDC_EDIT_FILELIFEDAYS, NULL, FALSE );
    sprintf( filelife.strsize(50), "%d", m_filelifeday );
    SaveCfg(  "filelife", filelife );
    EndDialog(IDOK);
	return TRUE ;
}

int DlgCleanSource::OnInitDialog()
{
    string v ;
    if( ReadCfg( "deletesrcnfileonly", v.strsize(500), 500) ) {
        sscanf( v, "%d", &m_nfileonly );
    }
    else {
        m_nfileonly = 1 ;
    }
    CheckDlgButton( m_hWnd, IDC_CHECK_NFILEONLY, m_nfileonly );
    sprintf( v.strsize(500), "To clean disk %c:", m_drive );
    SetDlgItemText(m_hWnd, IDC_TXT_CLEANDISK, v );
    SetTimer(m_hWnd, 1, 120000, NULL );
    return TRUE ;
}

void DlgCleanSource::OnTimer()
{
    EndDialog( IDCANCEL );
}

int DlgCleanSource::OnOK()
{
    if( IsDlgButtonChecked( m_hWnd, IDC_CHECK_NFILEONLY ) ) {
        m_nfileonly = 1 ;
    }
    else {
        m_nfileonly = 0 ;
    }
    string delnfonly ;
    sprintf( delnfonly.strsize(10), "%d", m_nfileonly );
    SaveCfg(  "deletesrcnfileonly", delnfonly );
    EndDialog(IDOK);
	return TRUE ;
}

// delete dir tree 
void shdeletedir( char *dirname, HWND hwnd )
{
    SHFILEOPSTRUCT  shfo ;
    string fname ;
    sprintf(fname.strsize(strlen(dirname)+2), "%s*", dirname );
    memset( &shfo, 0, sizeof(shfo));
    shfo.hwnd = hwnd ;
    shfo.wFunc = FO_DELETE ;
    LPTSTR lpfrom = fname ;
    int l = wcslen( lpfrom ) ;
    lpfrom[l-1] = 0 ;  // make double nul terminate char
    shfo.pFrom = lpfrom ;
    shfo.pTo = NULL ;
    shfo.fFlags = FOF_NOCONFIRMATION ;
    shfo.fAnyOperationsAborted = 0;
    shfo.hNameMappings = NULL ;
    shfo.lpszProgressTitle = _T("Deleting Folder");

    SHFileOperation( &shfo );
}

void shdeletefile( char * filename, HWND hwnd )
{
    SHFILEOPSTRUCT  shfo ;
    string fname ;
    sprintf(fname.strsize(strlen(filename)+2), "%s*", filename );
    memset( &shfo, 0, sizeof(shfo));
    shfo.hwnd = hwnd ;
    shfo.wFunc = FO_DELETE ;
    LPTSTR lpfrom = fname ;
    int l = wcslen( lpfrom ) ;
    lpfrom[l-1] = 0 ;  // make double nul terminate char
    shfo.pFrom = lpfrom ;
    shfo.pTo = NULL ;
    shfo.fFlags = FOF_NOCONFIRMATION | FOF_SILENT ;
    shfo.fAnyOperationsAborted = 0;
    shfo.hNameMappings = NULL ;
    shfo.lpszProgressTitle = _T("Deleting File");

    SHFileOperation( &shfo );
}

void deletefile( char * filename )
{
    string fname=filename ;
    DeleteFile(fname);
}

void CleanSrcDir( char * dir, int nfileonly, DlgClTargetProgress * dlgcp )
{
    int delcount=0 ;
    dirfind dirf( dir ) ;
    string msg ;
    while( ! dlgcp->m_complete && dirf.find() ) {
        if( dirf.isdir() ) {
            CleanSrcDir( dirf.filepath(), nfileonly, dlgcp) ;
            RemoveDirectory( dirf.filepath() );
        }
        else {
            if( stricmp( dirf.filename(), "pwid.conf" )==0 ) {  // don't delete pwid file
                continue ;
            }
            else if( nfileonly ) {
                if( strstr( dirf.filename(), "_N_" ) ) {        // archived file
                    sprintf(msg.strsize(1024), "Deleting %s", (char *)dirf.filename() );
                    lock();
                    dlgcp->m_msg = msg ;
                    dlgcp->m_update = 1 ;
                    unlock();
                    deletefile( dirf.filepath() );
                    // shdeletefile(  dirf.filepath(), dlgcp->getHWND() );
                    delcount++;
                }
            }
            else {
                sprintf(msg.strsize(1024), "Deleting %s", (char *)dirf.filename() );
                lock();
                dlgcp->m_msg = msg ;
                dlgcp->m_update = 1 ;
                unlock();
                deletefile( dirf.filepath() );
                // shdeletefile(  dirf.filepath(), dlgcp->getHWND() );
                delcount++;
            }
            if( delcount>8 && strstr( dirf.filename(), ".26")!=NULL ) {
                FlushVolume(archdisk);
                delcount=0 ;
            }
        }
    }
    dirf.close();
    FlushVolume(archdisk);
    delcount=0 ;
}

DWORD WINAPI CleanSrcDiskThreadFunction( LPVOID lpParam ) 
{ 
    string msg ;

    DlgClTargetProgress * dlgcp = (DlgClTargetProgress *)lpParam ;
    SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_IDLE );      // run in idle

    CleanSrcDir( dlgcp->m_rootdir, dlgcp->m_nfileonly, dlgcp ) ;

    dlgcp->m_complete = 1 ;
    return 0; 
}

void ManagerDlg::OnBnClickedButtonSrcclean()
{
   if( archdisk==0 ) {
       return ;
   }
      
   DlgCleanSource dlgcs ;
   dlgcs.m_drive = archdisk ;

   if( dlgcs.DoModal( IDD_DIALOG_CLSRC, m_hWnd )==IDOK ) {
       // to clean target files
       DlgClTargetProgress  dlgcp ;
       dlgcp.m_title = "Clean Source Disk" ;
       dlgcp.m_nfileonly = dlgcs.m_nfileonly ;
       sprintf( dlgcp.m_rootdir.strsize(10), "%c:\\", archdisk );
       HANDLE hCleanDiskThread ;
       hCleanDiskThread = CreateThread( NULL, 0, CleanSrcDiskThreadFunction, (LPVOID) &dlgcp, 0, NULL );

       // set archiving state
       archive_state=ARCHIVE_CLEAN ;
       SetTimer( m_hWnd, TIMER_ARCHIVE, 500, NULL );

       dlgcp.DoModal( IDD_DIALOG_CLRTARGETPROGESS, m_hWnd );

       // to stop cleaning thread
       dlgcp.m_complete = 1 ;

       WaitCursor wc ;
       // wait cleaning thread
       WaitForSingleObject(hCleanDiskThread, 10000 ) ;
       CloseHandle( hCleanDiskThread ) ;

       FlushVolume(archdisk);
       archive_state=ARCHIVE_COMPLETE ;

       updsrcdiskspace();
   }
}

void ManagerDlg::OnBnClickedButtonSrcEject()
{
    TCHAR disk ;
    string msg ;
    BOOL saveremove ;
    WaitCursor * waitcursor = new WaitCursor ;
    disk = (TCHAR)archdisk ;
    saveremove = EjectVolume( disk );
    delete waitcursor ;
    if( saveremove ) {
        archdisk = 0 ;
        archive_state = ARCHIVE_IDLE ;
        sprintf(msg.strsize(100), "Safe to remove disk %c:", (char)disk );
        MessageBoxW( m_hWnd, msg, _T("Message"), MB_OK );
        SetTimer(m_hWnd, TIMER_ARCHIVE, 500, NULL);
    }
    else {
        sprintf(msg.strsize(100), "Failed to eject disk %c: , try again later.", (char)disk );
        MessageBoxW( m_hWnd, msg, NULL, MB_OK|MB_ICONERROR );
    }
}


// follow code are copied from MS library sample code
HANDLE OpenVolume(TCHAR cDriveLetter);
BOOL LockVolume(HANDLE hVolume);
BOOL DismountVolume(HANDLE hVolume);
BOOL PreventRemovalOfVolume(HANDLE hVolume, BOOL fPrevent);
BOOL AutoEjectVolume(HANDLE hVolume);
BOOL CloseVolume(HANDLE hVolume);

HANDLE OpenVolume(TCHAR cDriveLetter)
{
    HANDLE hVolume;
    UINT uDriveType;
    TCHAR szVolumeName[8];
    TCHAR szRootName[5];
    DWORD dwAccessFlags;

    wsprintf(szRootName, _T("%c:\\"), cDriveLetter);

    uDriveType = GetDriveType(szRootName);
    switch(uDriveType) {
        /*
        case DRIVE_REMOVABLE:
        dwAccessFlags = GENERIC_READ | GENERIC_WRITE;
        break;
        */
    case DRIVE_CDROM:
        dwAccessFlags = GENERIC_READ;
        break;
    default:
        dwAccessFlags = GENERIC_READ | GENERIC_WRITE;
        //           return INVALID_HANDLE_VALUE;
    }

    wsprintf(szVolumeName, _T("\\\\.\\%c:"), cDriveLetter);

    hVolume = CreateFile(   szVolumeName,
        dwAccessFlags,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL );
    return hVolume;
}

BOOL CloseVolume(HANDLE hVolume)
{
    return CloseHandle(hVolume);
}

#define LOCK_TIMEOUT        20000       // 20 Seconds
#define LOCK_RETRIES        20

BOOL TryLockVolume(HANDLE hVolume)
{
    DWORD dwBytesReturned;
    return DeviceIoControl(hVolume,
        FSCTL_LOCK_VOLUME,
        NULL, 0,
        NULL, 0,
        &dwBytesReturned,
        NULL);
}

BOOL LockVolume(HANDLE hVolume)
{
    DWORD dwSleepAmount;
    int nTryCount;

    dwSleepAmount = LOCK_TIMEOUT / LOCK_RETRIES;

    // Do this in a loop until a timeout period has expired
    for (nTryCount = 0; nTryCount < LOCK_RETRIES; nTryCount++) {
        if( TryLockVolume(hVolume) ) {
            return TRUE ;
        }
        Sleep(dwSleepAmount);
    }

    return FALSE;
}

BOOL UnlockVolume(HANDLE hVolume)
{
    DWORD dwBytesReturned;
    return DeviceIoControl(hVolume,
        FSCTL_UNLOCK_VOLUME,
        NULL, 0,
        NULL, 0,
        &dwBytesReturned,
        NULL);
}

BOOL DismountVolume(HANDLE hVolume)
{
    DWORD dwBytesReturned;

    return DeviceIoControl( hVolume,
        FSCTL_DISMOUNT_VOLUME,
        NULL, 0,
        NULL, 0,
        &dwBytesReturned,
        NULL);
}

BOOL PreventRemovalOfVolume(HANDLE hVolume, BOOL fPreventRemoval)
{
    DWORD dwBytesReturned;
    PREVENT_MEDIA_REMOVAL PMRBuffer;

    PMRBuffer.PreventMediaRemoval = fPreventRemoval;

    return DeviceIoControl( hVolume,
        IOCTL_STORAGE_MEDIA_REMOVAL,
        &PMRBuffer, sizeof(PREVENT_MEDIA_REMOVAL),
        NULL, 0,
        &dwBytesReturned,
        NULL);
}

BOOL AutoEjectVolume(HANDLE hVolume)
{
    DWORD dwBytesReturned;

    return DeviceIoControl( hVolume,
        IOCTL_STORAGE_EJECT_MEDIA,
        NULL, 0,
        NULL, 0,
        &dwBytesReturned,
        NULL);
}


// flush all cache to volume by lock&unlock the volume
BOOL FlushVolume(TCHAR cDriveLetter)
{
    HANDLE hVolume;

    // Open the volume.
    hVolume = OpenVolume(cDriveLetter);
    if (hVolume == INVALID_HANDLE_VALUE)
        return FALSE;

    FlushFileBuffers(hVolume);

    // Lock and dismount the volume.
    if (TryLockVolume(hVolume) ) {
        UnlockVolume(hVolume) ;
    }
    CloseVolume(hVolume);
    return TRUE ;
}

BOOL EjectVolume(TCHAR cDriveLetter)
{
    HANDLE hVolume;

    BOOL fRemoveSafely = FALSE;
    BOOL fAutoEject = FALSE;

    // Open the volume.
    hVolume = OpenVolume(cDriveLetter);
    if (hVolume == INVALID_HANDLE_VALUE)
        return FALSE;

    // Lock and dismount the volume.
    if (LockVolume(hVolume) && DismountVolume(hVolume)) {
        fRemoveSafely = TRUE;

        // Set prevent removal to false and eject the volume.
        if (PreventRemovalOfVolume(hVolume, FALSE) &&
            AutoEjectVolume(hVolume))
            fAutoEject = TRUE;
    }

    // Close the volume so other processes can use the drive.
    if (!CloseVolume(hVolume))
        return FALSE;

    return fRemoveSafely;
}
