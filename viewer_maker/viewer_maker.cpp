// viewer_maker.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include <Dbt.h>
#include <direct.h>
#include <process.h>

#include "viewer_maker.h"

#include "../common/cstr.h"
#include "../common/util.h"
#include "../common/tvsid.h"

#include "zip/unzip.h"

const char viewer[] = "secured_viewer.exe" ;

struct volumecheck_type {
	GUID volume_check_tag ;	
	char username[256] ;
	char password[256] ;
	DWORD VolumeSerialNumber ;
} ;

class ViewMaker : public Dialog {

public:
	ViewMaker() {
		CreateDlg(IDD_DIALOG_VIEWER_MAKER);
	}
	
protected:

	struct tvskey_data * tvskey ;
	int tvskeylen ;

	virtual int OnInitDialog()
    {
		// set main window ICON
		HICON hIcon = LoadIcon(ResInst(), MAKEINTRESOURCE(IDI_VIEWER_MAKER));
		SendMessage( m_hWnd, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)hIcon );
		SendMessage( m_hWnd, WM_SETICON, (WPARAM)ICON_SMALL, (LPARAM)hIcon );
		
		tvskeylen = 0 ;
		tvskey = tvs_readtvskey();
		if( tvskey != NULL && tvskey->size>1000 && tvskey->size < 20000 ) {
			tvskeylen = tvskey->size+sizeof(tvskey->checksum);
		}
		else {
			MessageBox( m_hWnd, _T("Please insert security usb key and restart this program."), _T("Error!"), MB_OK );
			EndDialog();
			return FALSE ;
		}

		SetDlgItemText( m_hWnd, IDC_EDIT_USERNAME, _T("admin") );
		SetDlgItemText( m_hWnd, IDC_EDIT_PASSWORD, _T("admin") );
		SendDlgItemMessage( m_hWnd, IDC_COMBO_TARGETDRIVE, CB_RESETCONTENT, (WPARAM)0, (LPARAM)0 );
		SendDlgItemMessage( m_hWnd, IDC_PROGRESS_MAKE, PBM_SETRANGE32, (WPARAM)0, (LPARAM)100 );

		scanDrives();

		makeComplete = 0 ;
        return TRUE;
    }

	int makeComplete ;

	void makeViewer() 
	{

		volumecheck_type volumecheck = {
			// {7745A171-3D20-4A15-AF81-8DBF66F30302}
			{ 0x7745a171, 0x3d20, 0x4a15, { 0xaf, 0x81, 0x8d, 0xbf, 0x66, 0xf3, 0x3, 0x2 } },
			"",
			"",
			0
		} ;

		char * vc = (char *) & (volumecheck) ;
		int    vs = sizeof( GUID ) ;

		ShowWindow( GetDlgItem(m_hWnd, IDC_PROGRESS_MAKE), SW_SHOW);
		EnableWindow( m_hWnd, FALSE );

		string edittxt ;

		GetDlgItemText( m_hWnd, IDC_EDIT_USERNAME, edittxt.tcssize(128), 120 );
		key_256( edittxt, (unsigned char *)(volumecheck.username));
		GetDlgItemText( m_hWnd, IDC_EDIT_PASSWORD, edittxt.tcssize(128), 120 );
		key_256( edittxt, (unsigned char *)(volumecheck.password));

		// volume serial no
		GetDlgItemText( m_hWnd, IDC_COMBO_TARGETDRIVE, edittxt.tcssize(128), 120 );
		DWORD VolumeSerialNumber ;
		GetVolumeInformation( edittxt, NULL, 0, &VolumeSerialNumber, NULL, NULL, NULL, 0 );
		volumecheck.VolumeSerialNumber = VolumeSerialNumber ;

		string outputfile ;
		outputfile = edittxt ;
		outputfile += "autorun.inf" ;
		FILE * inff = fopen( outputfile, "w" );
		if( inff ) {
			sprintf( outputfile.strsize(200), "[autorun]\nopen=\\viewer\\%s\nicon=\\viewer\\%s\n", viewer, viewer ) ;
			fwrite( (char *)outputfile, 1, strlen((char *)outputfile), inff );
			fclose( inff );
		}

		string targetDir ;
		targetDir = edittxt ;
		targetDir += "viewer" ;
		mkdir( targetDir );
		targetDir += "\\" ;

		string viewzip ;
		viewzip="secureview.zip" ;
		getfilepath(viewzip);
		HZIP hzip = OpenZip(viewzip, 0);
		if( hzip ) {
			ZIPENTRY ze ;
			int totalsize = 0 ;
			for ( int i=0; i<1000; i++ ) {
				ze.unc_size = 0 ; 
				ZRESULT r = GetZipItem( hzip, i, &ze );
				totalsize += ze.unc_size ;
			}

			makeComplete = 5 ;
			SendDlgItemMessage( m_hWnd, IDC_PROGRESS_MAKE, PBM_SETPOS, (WPARAM)makeComplete, (LPARAM)0 );

			int totalmake = 0 ;

			for ( int i=0; i<1000; i++ ) {
				ze.unc_size = -1 ; 
				ZRESULT r = GetZipItem( hzip, i, &ze );
				if( r==ZR_OK && ze.unc_size > 0 ) {
					outputfile = viewer ; 
					if( _tcsicmp( ze.name, outputfile ) == 0 ) {
						// the viewer exe
						char * buf = new char [ze.unc_size+20] ;
						if( UnzipItem( hzip, i, buf, ze.unc_size ) == ZR_OK ) {
							// the viewer exe
							int s = 0 ;
							for( s=0; s < (ze.unc_size - vs) ; s++ ) {
								if( memcmp( buf+s, vc, vs)==0 ) {
									memcpy( buf+s, vc, sizeof(volumecheck) );
									break ;
								}
							}
							outputfile = targetDir ;
							outputfile += ze.name ;
							FILE * f = fopen( outputfile, "wb" );
							if( f ) {
								fwrite( buf, 1, ze.unc_size, f );
								fclose( f );
							}
						}
						delete buf ;
					}
					else {
						outputfile = targetDir ;
						outputfile += ze.name ;
						UnzipItem( hzip, i, outputfile ); 
					}

					totalmake += ze.unc_size;
					makeComplete =  (int)(100.0 * totalmake/ totalsize );
					if( makeComplete>95 ) makeComplete = 95 ;
					SendDlgItemMessage( m_hWnd, IDC_PROGRESS_MAKE, PBM_SETPOS, (WPARAM)makeComplete, (LPARAM)0 );

				}
				else 
					break;
			}

			CloseZip( hzip );

			if( tvskey != NULL && tvskeylen>0 ) {
				char * skey = new char [tvskeylen] ;
				struct tvskey_data * tkey = (struct tvskey_data *)skey ;

				memcpy( skey, tvskey, tvskeylen );
				
				extern void tvs_keymake( struct tvskey_data * tvskey_data ) ;
				// remake tvskey data with no passwords
				memset( tkey->mfpassword, 0, sizeof(tkey->mfpassword) );
				memset( tkey->plpassword, 0, sizeof(tkey->plpassword) );
				memset( tkey->inpassword, 0, sizeof(tkey->inpassword) );

				tvs_keymake( tkey );

				// over write tvskey.dat file
				outputfile = targetDir ;
				outputfile += "tvskey.dat" ;
				FILE * f = fopen( outputfile, "wb" );
				if( f ) {
					fwrite( skey, 1, tvskeylen, f );
					fclose( f );
				}

				delete skey ;
			}
		}

	
		makeComplete = 100 ;

		ShowWindow( GetDlgItem(m_hWnd, IDC_PROGRESS_MAKE), SW_HIDE);

		EnableWindow( m_hWnd, TRUE );

	}
	
	static void __cdecl makeThreadProc( void * pWin ) 
	{
		((ViewMaker *) pWin)->makeViewer() ;
		return ;
	}

	int OnMake()
    {
		int i;

		if( makeComplete >0 && makeComplete<100 ) {
			return TRUE ;
		}

		makeComplete = 1 ;
		_beginthread(makeThreadProc, 0, (void *)this);

        return TRUE ;
    }

	void scanDrives() {
		int disks = 0 ;
		TCHAR drives[256] ;
		TCHAR * pdrv = drives ;
		DWORD l = GetLogicalDriveStrings( 254, drives );
		SendDlgItemMessage( m_hWnd, IDC_COMBO_TARGETDRIVE, CB_RESETCONTENT, (WPARAM)0, (LPARAM)0 );
		while( l>1 ) {
			UINT drivetype = GetDriveType( pdrv ) ;
			DWORD serialno ;
			if( ( drivetype == DRIVE_REMOVABLE || drivetype == DRIVE_FIXED ) &&
				GetVolumeInformation( pdrv, NULL, 0, &serialno, NULL, NULL, NULL, 0 ) != 0 ) { 
				SendDlgItemMessage( m_hWnd, IDC_COMBO_TARGETDRIVE, CB_ADDSTRING, (WPARAM)0, (LPARAM)pdrv );
				disks++ ;
			}
			int xl = _tcslen(pdrv);
			pdrv += xl+1 ;
			l -= xl+1 ;
		}
		l = SendDlgItemMessage( m_hWnd, IDC_COMBO_TARGETDRIVE, CB_SETCURSEL, (WPARAM)(disks-1), (LPARAM)0 );
	}

	BOOL OnDeviceChange( WPARAM wParam, LPARAM lParam ) {
		if( wParam == DBT_DEVNODES_CHANGED ) {
			// to rescan drive
			scanDrives();
		}
		return TRUE ;
	}

    virtual int OnCancel()
    {
        return EndDialog(IDCANCEL);
    }

    virtual int OnCommand( int Id, int Event ) 
	{
        switch (Id)
        {
		case IDC_BUTTON_MAKE :
			return OnMake();

        case IDCANCEL:
            return OnCancel();
        }
        return FALSE;
    }

	// dialog box procedures
	virtual int DlgProc(UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch( message ) {
			case WM_DEVICECHANGE:
				OnDeviceChange( wParam, lParam );
				break;

			default:
				return Dialog::DlgProc( message, wParam, lParam );
		}
		return FALSE ;
	}

};


int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// main dialog
	ViewMaker viewmaker ;
	viewmaker.DoModal();
	return 0;
}

