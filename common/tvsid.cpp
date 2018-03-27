// tvsid.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"

#include <Windows.h>
#include <shellapi.h>

#include "stdio.h"
#include "crypt.h"

#include "../common/cwin.h"
#include "../common/cstr.h"
#include "../common/tvsid.h"
#include "../common/util.h"

static TCHAR disk_enum[]=_T("SYSTEM\\CurrentControlSet\\Services\\disk\\Enum") ;

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

int DiskSerialId(int disknum, char * serialid, int idlen )
{
	DWORD dwType ;
	DWORD cbData ;
	HKEY key ;
	DWORD count=0 ;
	string diskid ;
    string sid ;
	if( RegOpenKeyEx(HKEY_LOCAL_MACHINE, disk_enum, 0, KEY_READ, &key )==ERROR_SUCCESS ) {
		sprintf( sid.strsize(50), "%d", disknum );
		cbData = 256 ;
		if( RegQueryValueEx (key, (LPCTSTR)sid, NULL, &dwType, (LPBYTE)(LPWSTR)diskid.tcssize( cbData ), &cbData ) != ERROR_SUCCESS ) {
			cbData=0 ;
		}
		RegCloseKey( key );
		if( cbData>0 ) {
            strncpy( serialid, diskid, idlen );
            return strlen( serialid );
		}
	}
	return 0 ;
}

static char tvsdata [8000] ;
struct tvskey_data * tvskeydata ;
char * powerid = 
	"IDE\\DiskWDC_WD5000AAKS-65V0A0___________________05.01D05\\4&2a33c906&0&0.0.0" ;
//	"IDE\\DiskWDC_WD10EZEX-22RKKA0____________________80.00A80\\4&2a33c906&0&0.0.0" ;

struct tvskey_data * tvs_readtvskey()
{
	char md5checksum[16] ;
	int checksize ;
	char serialid[512] ;
	struct RC4_seed rc4seed ;
	unsigned char k256[256] ;
	FILE * keyfile ;
	char * tvskey1=NULL ;
	char * tvskey2=NULL ;
    char * pkey ;
	int fsize=0 ;
	int disk ;
	int i;
	struct tvskey_data * tvskey_data ;
    int res=0 ;
	int keysize = sizeof(tvsdata) ;

	DWORD serialno ;
	DWORD drives = GetLogicalDrives();
	for( i='D' ; i<='Z' ; i++ ) {
		if( ( drives & (1<<(i-'A')) ) == 0 ) continue ;
       	string keyfilename ;
		sprintf(keyfilename.strsize(50),"%c:\\", i) ;

		UINT drivetype = GetDriveType( keyfilename ) ;
		if( drivetype != DRIVE_REMOVABLE && drivetype != DRIVE_FIXED ) 
			continue ;
		if( GetVolumeInformation( keyfilename, NULL, 0, &serialno, NULL, NULL, NULL, 0 ) == 0 ) 
			continue ;
		sprintf(keyfilename.strsize(50),"%c:\\TVS\\TVSKEY.DAT", i) ;
		keyfile = fopen( keyfilename, "rb" );
		if( keyfile ) {
			fseek( keyfile, 0, SEEK_END ) ;
			fsize = ftell( keyfile );
			if(fsize>=sizeof(struct tvskey_data)-512 && fsize<keysize) {
				fseek( keyfile, 0, SEEK_SET ) ;
				tvskey1 = new char [fsize] ;
				tvskey2 = new char [fsize] ;
				fread( tvskey1, 1, fsize, keyfile ) ;
				fclose( keyfile );
				break ;
			}
			else {
				fclose( keyfile );
                fsize=0 ;
			}
		}
	}

	if( fsize==0 ) {
        // check local key file
        string keyfilename="TVSKEY.DAT" ;
        if( getfilepath(keyfilename) ) {
            keyfile = fopen( keyfilename, "rb" );
            if( keyfile ) {
                fseek( keyfile, 0, SEEK_END ) ;
                fsize = ftell( keyfile );
                if(fsize>=sizeof(struct tvskey_data)-512 && fsize<keysize) {
                    fseek( keyfile, 0, SEEK_SET ) ;
                    tvskey1 = new char [fsize] ;
                    tvskey2 = new char [fsize] ;
                    fread( tvskey1, 1, fsize, keyfile ) ;
                }
                else {
                    fsize=0 ;
                }
                fclose( keyfile );
            }
        }
	}

    if( fsize==0 ) {
        return 0 ;
    }

	int testing=0 ;
    LPWSTR *szArglist;
    int nArgs;
    szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);    

	disk = DiskNum();
	for(i=-1; i<disk; i++) {
		memcpy( tvskey2, tvskey1, fsize );
		serialid[0]=0 ;
		if( i<0 ) {
			strcpy( serialid, powerid );
		}
		else {
			DiskSerialId(i, serialid, sizeof(serialid) );
		}
		if( strlen( serialid ) > 0 ) {
            pkey = strstr( serialid, "Ven_" ) ;
            if( pkey==NULL ) {
                pkey=serialid ;
            }
            if( pkey ) {
				int l = strlen( pkey ) ;
				if( pkey[l-2]=='&' ) {
					pkey[l-2]=0 ;
				}
				key_256( pkey, k256 ) ;
				RC4_KSA( &rc4seed, k256 );
				RC4_crypt( (unsigned char *)tvskey2, fsize, &rc4seed ) ;
				tvskey_data = (struct tvskey_data *) tvskey2 ;

                // Make a new key
                if( testing==1 ) {
                    keyfile = fopen( "ntvskey.dat", "wb" );
                    if( keyfile ) {
                        tvskey_data = (struct tvskey_data *) tvsdata ;
                        fsize=sizeof(struct tvskey_data)+100 ;
                        memset( tvsdata, 0, fsize );
                        tvskey_data->size = fsize-sizeof(tvskey_data->checksum) ;
                        tvskey_data->version = 100 ;
                        strcpy( tvskey_data->tag, "TVS" );
                        strcpy( tvskey_data->usbid, "MF9001" );
                        strcpy( tvskey_data->usbkeyserialid, &serialid[13] );
                        strcpy( tvskey_data->manufacturerid, "MF9001" );
                        tvskey_data->counter=999 ;
                        tvskey_data->keyinfo_start= sizeof(struct tvskey_data) ;
                        tvskey_data->keyinfo_size=100 ;
                        sprintf( tvsdata+sizeof(struct tvskey_data), 
                            "City: Toronto\nProvince: Ontario\n Country: Canada\n" );
                        md5_checksum( tvskey_data->checksum, (unsigned char *)(&(tvskey_data->size)), tvskey_data->size );
                        key_256( tvskey_data->usbkeyserialid, k256 ) ;
                        RC4_KSA( &rc4seed, k256 );
                        RC4_crypt( (unsigned char *)tvsdata, fsize, &rc4seed ) ;
                        fwrite( tvsdata, 1, fsize, keyfile ) ;

                        fclose( keyfile );
                    }
                }

                // repair checksum
				if( testing==2 ) {
					memset( tvskey_data->checksum, 0, sizeof(tvskey_data->checksum) );
					md5_checksum( md5checksum, (unsigned char *)(&(tvskey_data->size)), tvskey_data->size );
					memcpy( tvskey_data->checksum, md5checksum, sizeof(md5checksum));
					keyfile = fopen( "ntvskey.dat", "wb" );
 					if( keyfile ) {
    					RC4_KSA( &rc4seed, k256 );
	 					RC4_crypt( (unsigned char *)tvskey2, fsize, &rc4seed ) ;
						fwrite( tvskey2, 1, fsize, keyfile ) ;
						fclose( keyfile );
					}
				}

                // output video key in c64 format, (to be used in dvr system)
				if( testing==3 ) {
					int bin2c64(unsigned char * bin, int binsize, char * c64 );
					char c64code[512] ;
					bin2c64( (unsigned char *)tvskey_data->videokey, 256, c64code );
					keyfile = fopen( "videocode", "w" );
					if( keyfile ) {
						fprintf( keyfile, "%s", c64code );
						fclose( keyfile ) ;
					}
				} 

				tvskey_data = (struct tvskey_data *) tvskey2 ;
				checksize = tvskey_data->size ;
				if( checksize<0 || checksize>(fsize-(int)sizeof(tvskey_data->checksum)) ) {
					checksize=0;
				}
				md5_checksum( md5checksum, (unsigned char *)(&(tvskey_data->size)), checksize );
                if( memcmp( md5checksum, tvskey_data->checksum, 16 )==0 &&
                    strcmp( tvskey_data->usbkeyserialid, pkey )== 0 ) 
                {
                    // key file verified

                    // generate tvsid file for update TVS box for Production
                    if( testing == 5 ||
                        ( nArgs>1 && szArglist!=NULL && nArgs>1 && wcscmp(szArglist[1],L"-mid")==0 ) )
                    {
                        FILE * idfile ;
                        string fname("MFID.DAT");
                        if( nArgs>2 ) {
                            fname=szArglist[2] ;
                        }
                        idfile = fopen( fname, "wb" );
                        if( idfile ) {
                            // fill serialid with random data
                            int n ;
                            srand(GetTickCount());
                            for( n=0; n<256; ) {
                                BYTE b = (BYTE) rand();
                                if( b!=0 ) {
                                    tvskey_data->usbkeyserialid[n] = b ;
                                    n++ ;
                                }
                            }
                            tvskey_data->usbkeyserialid[n]=0 ;
                            // write key
                            fwrite( tvskey_data->usbkeyserialid, 1, 256, idfile );

                            key_256( tvskey_data->usbkeyserialid, k256 ) ;
                            RC4_KSA( &rc4seed, k256 );
                            // calculate checksum
                            md5_checksum( tvskey_data->checksum, (unsigned char *)(&(tvskey_data->size)), tvskey_data->size );
                            // encrypt
                            RC4_crypt( (unsigned char *)tvskey_data, fsize, &rc4seed ) ;
                            // write data
                            fwrite( (unsigned char *)tvskey_data, 1, fsize, idfile );
                            fclose( idfile );
                        }
                        _exit(1) ;
                    }

                    // duplicate key file to hard disk
                    if( testing == 6 ||
                        ( nArgs>1 && szArglist!=NULL && nArgs>1 && wcscmp(szArglist[1],L"-dupkey")==0 ) )
                    {
                        FILE * idfile ;
                        string fname("TVSKEY.DAT");
                        getfilepath(fname);
                        idfile = fopen( fname, "wb" );
                        if( idfile ) {
                            int ndisk=0 ;
							for( ndisk=0; ndisk<10; ndisk++) 
                            if( DiskSerialId(ndisk, serialid, sizeof(serialid) ) ) {
								pkey = strstr( serialid, "Ven_" ) ;
								if( pkey==NULL ) {
									pkey=serialid ;
								}
                                int l = strlen( pkey ) ;
                                if( pkey[l-2]=='&' ) {
                                    pkey[l-2]=0 ;
                                }
                                // duplicate key
                                strncpy( tvskey_data->usbkeyserialid, pkey, sizeof(tvskey_data->usbkeyserialid) );
                                key_256( pkey, k256 ) ;
                                RC4_KSA( &rc4seed, k256 );
                                // calculate checksum
                                md5_checksum( tvskey_data->checksum, (unsigned char *)(&(tvskey_data->size)), tvskey_data->size );
                                // encrypt
                                RC4_crypt( (unsigned char *)tvskey_data, fsize, &rc4seed ) ;
                                fwrite( (unsigned char *)tvskey_data, 1, fsize, idfile );
								break;
                            }
                            fclose( idfile );
                        }
                        _exit(1) ;
                    }

					// write unencrypted key info to disk, for Android program
                    if( testing == 7 ) {
                        FILE * idfile ;
                        string fname("M.DAT");
                        getfilepath(fname);
                        idfile = fopen( fname, "wb" );
                        if( idfile ) {
                            fwrite( (unsigned char *)tvskey_data, 1, fsize, idfile );
                            fclose( idfile );
                        }
                    }

					// write no password key file
                    if( testing == 8 ) {
                        FILE * idfile ;
                        string fname("TVSKEY.DAT.nopass");
                        getfilepath(fname);
                        idfile = fopen( fname, "wb" );
                        if( idfile ) {
                            // duplicate key
							pkey=powerid ;
                            strncpy( tvskey_data->usbkeyserialid, pkey, sizeof(tvskey_data->usbkeyserialid) );

							// erase passwords
							memset( tvskey_data->mfpassword, 0, sizeof( tvskey_data->mfpassword ) );
							memset( tvskey_data->plpassword, 0, sizeof( tvskey_data->plpassword ) );

                            key_256( pkey, k256 ) ;
                            RC4_KSA( &rc4seed, k256 );
                            // calculate checksum
                            md5_checksum( tvskey_data->checksum, (unsigned char *)(&(tvskey_data->size)), tvskey_data->size );
                            // encrypt
                            RC4_crypt( (unsigned char *)tvskey_data, fsize, &rc4seed ) ;
                            fwrite( (unsigned char *)tvskey_data, 1, fsize, idfile );
                            fclose( idfile );
                        }
                    }

                    // Free memory allocated for CommandLineToArgvW arguments.
                    LocalFree(szArglist);

  					if( fsize>0 ) {
					    memcpy( tvsdata, tvskey2, fsize );
                        res=fsize;
                        break;
					}
				}
			}
		}
		else {
			break;
		}
	}

	delete tvskey1 ;
	delete tvskey2 ;

	if( res> 0 ) {
		tvskeydata = (struct tvskey_data *)tvsdata;
	}
	else {
		tvskeydata = NULL ;
	}
	return tvskeydata ;

}

// remake key
void tvs_keymake( struct tvskey_data * tvskey )
{
	struct RC4_seed rc4seed ;
	unsigned char k256[256] ;

    strncpy( tvskey->usbkeyserialid, powerid, sizeof(tvskey->usbkeyserialid) );

    key_256( powerid, k256 ) ;
    RC4_KSA( &rc4seed, k256 );
    // calculate checksum
    md5_checksum( tvskey->checksum, (unsigned char *)(&(tvskey->size)), tvskey->size );
	int fsize = tvskey->size + sizeof(tvskey->checksum) ;
    // encrypt
    RC4_crypt( (unsigned char *)tvskey, fsize, &rc4seed ) ;
}

// check if key still available, 1: available, 0: removed
int tvs_checktvskey()
{
	char serialid[512] ;
	int disk ;
	int i;
    int l;
    char * pkey ;

	if( tvskeydata == NULL ) 
		return 0 ;

	disk = DiskNum();
    for(i=-1; i<disk; i++) {
		serialid[0]=0 ;
		if( i<0 ) {
           strcpy( serialid, powerid );		// power key
		}
		else {
			DiskSerialId(i, serialid, sizeof(serialid));
		}
        if( strlen(serialid)>0 ) {
            pkey = strstr( serialid, "Ven_" ) ;
            if( pkey==NULL ) {
                pkey=serialid ;
            }
            l = strlen( pkey ) ;
            if( pkey[l-2]=='&' ) {
                pkey[l-2]=0 ;
            }
            if( strcmp( tvskeydata->usbkeyserialid, pkey )==0 ) {
                return 1;
            }
        }
    }
	return 0 ;
}

