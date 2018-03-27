
#include "stdafx.h"

#include <process.h>

#include "dvrclient.h"
#include "dvrclientdlg.h"
#include "decoder.h"
#include "VideoPassword.h"
#include "UserPass.h"

#include "cstr.h"
#include "../common/cdir.h"

decoder_library::decoder_library(LPCTSTR libname)
{

	busy=0 ;
	filedate = 0 ;
	libtype = 0 ;

	modulename = libname;
	hmod = LoadLibrary(modulename);

	player_init = (int (*)())getapi("player_init");
	player_deinit = (int (*)())getapi( "player_deinit");
	finddevice = (int (*)(int))getapi( "finddevice");
	detectdevice=(int (*)(char *))getapi( "detectdevice");
	polldevice = (int (*)())getapi( "polldevice");
	opendevice = (HPLAYER (*)(int,int))getapi( "opendevice");
	initsmartserver = (int (*)(HPLAYER))getapi( "initsmartserver");
	openremote = (HPLAYER (*)(char *, int))getapi( "openremote");
	openharddrive = (HPLAYER (*)(char *))getapi( "openharddrive");
	openfile = (HPLAYER (*)(char *))getapi( "openfile");
	close =(int (*)(HPLAYER)) getapi( "close");
	if( close==NULL ) {
		close=(int (*)(HPLAYER)) getapi( "player_close");
	}
	getplayerinfo = (int (*)(HPLAYER, player_info *))getapi( "getplayerinfo");
	setuserpassword = (int (*)(HPLAYER, char *, void *, int))getapi( "setuserpassword");
	setvideokey = (int (*)(HPLAYER, void *, int))getapi( "setvideokey");
	getchannelinfo = (int (*)(HPLAYER, int, channel_info *))getapi( "getchannelinfo");
	attach = (int (*)(HPLAYER, int, HWND))getapi( "attach");
	detach = (int (*)(HPLAYER))getapi( "detach");
	detachwindow = (int (*)(HPLAYER, HWND))getapi( "detachwindow");
	play = (int (*)(HPLAYER))getapi( "play");
	audioon = (int (*)(HPLAYER, int, int))getapi( "audioon");
	audioonwindow = (int (*)(HPLAYER, HWND, int))getapi( "audioonwindow");
	pause = (int (*)(HPLAYER))getapi( "pause");
	stop = (int (*)(HPLAYER))getapi( "stop");
	refresh = (int (*)(HPLAYER, int))getapi( "refresh");
	fastforward = (int (*)(HPLAYER, int))getapi( "fastforward");
	slowforward = (int (*)(HPLAYER, int))getapi( "slowforward");
	oneframeforward = (int (*)(HPLAYER))getapi( "oneframeforward");
	getdecframes = (int (*)(HPLAYER))getapi( "getdecframes");
	backward = (int (*)(HPLAYER, int))getapi( "backward");
	capture = (int (*)(HPLAYER, int, char *))getapi( "capture");
	capturewindow = (int (*)(HPLAYER, HWND, char *))getapi( "capturewindow");
	seek = (int (*)(HPLAYER, dvrtime *))getapi( "seek");
	getcurrenttime = (int (*)(HPLAYER, dvrtime *))getapi( "getcurrenttime");
	getdaylist = (int (*)(HPLAYER, int *, int))getapi( "getdaylist");
	getcliptimeinfo = (int (*)(HPLAYER, int, dvrtime *, dvrtime *,cliptimeinfo *, int))getapi( "getcliptimeinfo");
	getclipdayinfo = (int (*)(HPLAYER, int, dvrtime *))getapi( "getclipdayinfo");
	getlockfiletimeinfo = (int (*)(HPLAYER, int, dvrtime *, dvrtime *,cliptimeinfo *, int))getapi( "getlockfiletimeinfo");
	savedvrfile = (int (*)(HPLAYER, dvrtime *, dvrtime *, char *, int, dup_state *))getapi( "savedvrfile");
	dupvideo = (int (*)(HPLAYER, dvrtime *, dvrtime *, char *, int, dup_state *))getapi( "dupvideo");
	configureDVR = (int (*)(HPLAYER))getapi( "configureDVR");
	senddvrdata = (int (*)(HPLAYER, int, void *, int))getapi( "senddvrdata");
	recvdvrdata = (int (*)(HPLAYER, int, void **, int *))getapi( "recvdvrdata");
	freedvrdata = (int (*)(HPLAYER, void *))getapi( "freedvrdata");
	configurePlayer=(int (*)(HPLAYER))getapi( "configurePlayer");
	lockdata=(int (*)(HPLAYER, struct dvrtime * , struct dvrtime * ))getapi( "lockdata");
	unlockdata=(int (*)(HPLAYER, struct dvrtime * , struct dvrtime * ))getapi( "unlockdata");
	cleandata=(int (*)(HPLAYER, int, struct dvrtime * , struct dvrtime * ))getapi( "cleandata");
	pwkeypad=(int (*)(HPLAYER, int, int))getapi( "pwkeypad");
	minitrack_start=(int (*)(struct dvr_func *, HWND))getapi( "minitrack_start");
	minitrack_selectserver=(int (*)(void))getapi( "minitrack_selectserver");
	minitrack_stop=(int (*)(void))getapi( "minitrack_stop");
	minitrack_init_interface =(int (*)(struct dvr_func *))getapi( "minitrack_init_interface");
	getlocation =(int (*)(HPLAYER, char *, int))getapi( "getlocation");
    geteventtimelist=(int (*)( HPLAYER , struct dvrtime * , int , int * , int ))getapi( "geteventtimelist");
    setblurarea=(int (*)(HPLAYER, int, struct blur_area * , int))getapi( "setblurarea");
    clearblurarea=(int (*)(HPLAYER, int))getapi( "clearblurarea");
	showzoomin = (int(*)(HPLAYER handle, int channel, struct zoom_area * za))getapi( "showzoomin");
	drawrectangle = (int(*)(HPLAYER handle, int channel, float x, float y, float w, float h))getapi( "drawrectangle");
	selectfocus = (int(*)(HPLAYER handle, HWND hwnd))getapi( "selectfocus");
}


decoder_library::~decoder_library()
{
	if( hmod ) {
		if( player_deinit )
			player_deinit();
		FreeLibrary(hmod) ;
	}
}

decoder_library * decoder::library[MAXLIBRARY] ;
int decoder::num_library = 0 ;
device_list * decoder::devlist = NULL ;
decoder * g_decoder ;
string  decoder::remotehost ;

int decoder::initlibrary()
{
	int res ;
	if( num_library>0 ) {
		releaselibrary();
	}
	num_library=0;
	// find all player libraries
    dirfind dir(getapppath(), "ply*.dll") ;
    while( num_library<MAXLIBRARY && dir.findfile() ) {
        library[num_library]=new decoder_library((LPCTSTR)dir.filename());
		res = 0 ;
		if( library[num_library]->player_init ) {
			res = library[num_library]->player_init();
		}
		if( res> 0 ) {
			library[num_library]->libtype=res ;
//			if( finder.GetLastWriteTime(ftime) ) {
//				library[num_library]->filedate = ftime.GetYear()*10000+ftime.GetMonth()*100+ftime.GetDay() ;
//			}
			if( res==PLAYER_TYPE_MINITRACK ) {	// a minitrack library.
				g_minitracksupport=1 ;			// enable minitrack support.
			}
			num_library++;
		}
		else {
			delete library[num_library] ;
		}
	}
    g_decoder = new decoder [MAXDECODER] ;
    g_minitrack_init_interface();
	return num_library ;
}

void decoder::releaselibrary()
{
	int i; 
    stopfinddevice();
	if( g_decoder ) {
		for(i=0; i<MAXDECODER; i++ ) {
			g_decoder[i].close();
		}
		delete [] g_decoder ;
	}
	while( num_library>0 ) {
		delete library[--num_library] ;
	}
	g_minitracksupport = 0 ;		
	decoder::remotehost.clean();
}

// return total device found, or ERROR_BUSY for non-block operation
// parameter:
//     flags : searching flags. flags==0 to clean find list array.
int decoder::finddevice(int flags)
{
	int i;
	int dev ;
	int busy=0 ;
    
    if( devlist ) {
        delete devlist ;
    }
    devlist = new device_list ;

	for(i=0; i<num_library; i++ ){
		library[i]->busy = 0 ;
		if( library[i]->finddevice ) {
			dev=library[i]->finddevice(flags);
			if( dev > 0 ) {
				devlist->update(i,dev);
			}
			else if( dev==DVR_ERROR_BUSY ) {
				library[i]->busy = 1 ;
				busy = 1 ;
			}
		}
	}

	// added feature, search specified directories
    if( (flags & 2)!=0 ) {          // search hard drive
        // detect dvr files on specified in "dvrdirlist" file
        string dirlist("dvrdirlist") ;
        if( getfilepath( dirlist ) ) {
            FILE * flist = fopen( dirlist, "r" );
            if( flist ) {
                string directory ;
                while( fscanf(flist, "%s", directory.strsize(512))==1 ) {
                    dirfind dir(directory,"_*_") ;
                    while( dir.finddir() ) {
                        devlist->adddir( dir.filepath() );
                    }
                }
                fclose( flist );
            }
        }

        // detect dvr folders specified in registry Software\TME\PW
        string pwtargetdir ;
        if(reg_read( "target", pwtargetdir) ) {
            dirfind dir(pwtargetdir, "_*_") ;
            while( dir.finddir() ) {
                devlist->adddir( dir.filepath() );
            }
        }
    }

	if( busy ) {
		return DVR_ERROR_BUSY ;
	}
	else {
		return decoder::polldevice() ;
	}
}

void decoder::stopfinddevice()
{
    if( devlist ) {
        delete devlist ;
        devlist=NULL ;
    }
}

// return total device found, or ERROR_BUSY for non-block operation
// parameter:
//     flags : searching flags. flags==0 to clean find list array.
int decoder::detectdevice(char * ipaddr)
{
	int i;
	for(i=0; i<num_library; i++ ){
		if( library[i]->detectdevice ) {
			library[i]->detectdevice(ipaddr);
		}
	}
	return 0 ;
}

// return number of devices already found.
int decoder::polldevice(void)
{
    int i ;
    int dev ;
    if( devlist ) {
        for(i=0; i<num_library; i++ ){
            if( library[i]->busy && library[i]->finddevice ) {
                dev = library[i]->finddevice(0);
                if( dev == DVR_ERROR_BUSY && library[i]->polldevice ) {
                    dev = library[i]->polldevice() ;
                }
                else {
                    library[i]->busy = 0 ;
                }
                if( dev>0 && devlist ) {
                    devlist->update(i,dev);
                }
            }
        }
        return devlist->totaldev();
    }
    return 0;
}

int decoder::settvskey(struct tvskey_data * tvskey)
{
	int res = -1 ;
#ifdef APP_TVS
	int l ;
	for( l=0; l<num_library; l++ ) {
		if( library[l]->senddvrdata ) {
			library[l]->senddvrdata( NULL, PROTOCOL_TVSKEYDATA, (void *)tvskey, tvskey->size+sizeof(tvskey->checksum) );
			res = 0 ;
		}
	}
#endif
	return res ;
}


// open by find index
int decoder::open( int index, int opentype )
{
	close();

    if( devlist == NULL ) {
        return DVR_ERROR ;
    }

    if( devlist->isdir(index) ) {
        return openharddrive( devlist->getdir(index) );
    }

	m_library = devlist->findlib(index);		// general index has been replaced with index of found library
	if( m_library>=0 ) {
		if( library[m_library]->opendevice ) {
			m_hplayer = library[m_library]->opendevice(index, opentype) ;
		}
	}
	if( m_hplayer ) {
#ifdef APP_TVS
		tvskeycheck();
#endif
		m_playerinfo.size=sizeof(m_playerinfo) ;
		if( getplayerinfo( &m_playerinfo )<0 ) {
			m_playerinfo.servername[0]=0;
			m_playerinfo.total_channel=0 ;
		}
		else {
			if( m_playerinfo.videokeytype> 0 ) {
			// ask for video password
/*				res = -1;
				while( res<0 ) {
					CVideoPassword askpassword ;
					int n;
					if( askpassword.DoModal()==IDOK ) {
						char password[256] ;
						LPCTSTR pps = (LPCTSTR)(askpassword.m_password) ;
						for( n=0; n<255; n++ ) {
							if( pps[n]==0 ) break;
							password[n]=(char)pps[n] ;
						}
						password[n]=0;
						res = setvideokey(password, n+1);
					}
					else {
						break;
					}
				} 
*/
			}
		}
		return 1 ;
	}
	else {
		return DVR_ERROR ;
	}
}

int decoder::openremote(char * netname, int opentype ) 
{
	int i ;
	close();
	for( i=0; i<num_library; i++ ) {
		m_library = i ;
		memset(&m_playerinfo, 0, sizeof(m_playerinfo) );
		m_playerinfo.size=sizeof(m_playerinfo) ;
		library[m_library]->getplayerinfo(NULL, &m_playerinfo);
		if( (m_playerinfo.features & PLYFEATURE_REMOTE) &&
			library[m_library]->openremote ) {
			m_hplayer = library[m_library]->openremote(netname, opentype);
		}
		if( m_hplayer ) {
#ifdef APP_TVS
			tvskeycheck();
#endif
			m_playerinfo.size=sizeof(m_playerinfo) ;
			if( getplayerinfo( &m_playerinfo )<0 ) {
				m_playerinfo.servername[0]=0;
				m_playerinfo.total_channel=0 ;
			}
			else if( m_playerinfo.videokeytype> 0 ) {
				// ask for video password
/*				res = -1;
				while( res<0 ) {
					CVideoPassword askpassword ;
					int n;
					if( askpassword.DoModal()==IDOK ) {
						char password[256] ;
						LPCTSTR pps = (LPCTSTR)(askpassword.m_password) ;
						for( n=0; n<255; n++ ) {
							if( pps[n]==0 ) break;
							password[n]=(char)pps[n] ;
						}
						password[n]=0;
						res = setvideokey(password, n+1);
					}
					else {
						break;
					}
				} 
*/
			}
			m_displayname=netname ;
            remotehost=netname ;
			return 1 ;
		}
	}
	return DVR_ERROR ;
}

int decoder::openharddrive( char * path )
{
	int i ;
	close();
	for( i=0; i<num_library; i++ ) {
		m_library = i ;
		memset(&m_playerinfo, 0, sizeof(m_playerinfo) );
		m_playerinfo.size=sizeof(m_playerinfo) ;
		library[m_library]->getplayerinfo(NULL, &m_playerinfo);
		if( (m_playerinfo.features & PLYFEATURE_HARDDRIVE) &&
			library[m_library]->openharddrive ) {
			m_hplayer = library[m_library]->openharddrive(path);
		}
		if( m_hplayer ) {
#ifdef APP_TVS
			tvskeycheck();
#endif
			m_playerinfo.size=sizeof(m_playerinfo) ;
			if( getplayerinfo( &m_playerinfo )<0 ) {
				m_playerinfo.servername[0]=0;
				m_playerinfo.total_channel=0 ;
			}
			else if( m_playerinfo.videokeytype> 0 ) {
				// ask for video password
/*				res = -1;
				while( res<0 ) {
					CVideoPassword askpassword ;
					int n;
					if( askpassword.DoModal()==IDOK ) {
						char password[256] ;
						LPCTSTR pps = (LPCTSTR)(askpassword.m_password) ;
						for( n=0; n<255; n++ ) {
							if( pps[n]==0 ) break;
							password[n]=(char)pps[n] ;
						}
						password[n]=0;
						res = setvideokey(password, n+1);
					}
					else {
						break;
					}
				} 
*/
			}
			m_displayname=path;
			return 1 ;
		}
	}
	return DVR_ERROR ;
}

int decoder::openfile( char * dvrfilename ) 
{
	int i ;

	close();
	for( i=0; i<num_library; i++ ) {
		m_library = i ;
		if( library[m_library]->openfile ) {
			m_hplayer = library[m_library]->openfile(dvrfilename);
		}
		if( m_hplayer ) {
#ifdef APP_TVS
			tvskeycheck();
#endif
			m_playerinfo.size=sizeof(m_playerinfo) ;
			if( getplayerinfo( &m_playerinfo )<0 ) {
				m_playerinfo.servername[0]=0;
				m_playerinfo.total_channel=0 ;
			}
			else if( m_playerinfo.videokeytype> 0 ) {
				// ask for video password
/*				res = -1;
				while( res<0 ) {
					CVideoPassword askpassword ;
					int n;
					if( askpassword.DoModal()==IDOK ) {
						char password[256] ;
						LPCTSTR pps = (LPCTSTR)(askpassword.m_password) ;
						for( n=0; n<255; n++ ) {
							if( pps[n]==0 ) break;
							password[n]=(char)pps[n] ;
						}
						password[n]=0;
						res = setvideokey(password, n+1);
					}
					else {
						break;
					}
				} 
*/
			}
            if( strstr( dvrfilename, ".dpl" )==NULL ) {
			    m_displayname=dvrfilename ;
            }
			return 1 ;
		}
	}
	return DVR_ERROR ;
}

int decoder::close()
{
	if( m_hplayer!=NULL ) {
		if( library[m_library]->close ) {
			library[m_library]->close(m_hplayer);
		}
	}
	m_hplayer=NULL ;
	m_library=0;
    m_displayname.clean();
	memset(&m_playerinfo, 0, sizeof(m_playerinfo));
	m_attachnum=0 ;
	return 0 ;
}

int decoder::initsmartserver()
{
	if( m_hplayer!=NULL && library[m_library]->initsmartserver ) {
		return library[m_library]->initsmartserver(m_hplayer);
	}
	return DVR_ERROR ; 
}

int decoder::tvskeycheck()
{
	int res = DVR_ERROR ;
#ifdef APP_TVS
    if( library[m_library]->senddvrdata && tvskeydata!=NULL ) {
		res = senddvrdata( PROTOCOL_TVSKEYDATA, (void *)tvskeydata, tvskeydata->size+sizeof(tvskeydata->checksum) );
    }
#endif
	return res ;
}


int decoder::getplayerinfo(struct player_info * pplayerinfo )
{
	if( m_hplayer!=NULL && library[m_library]->getplayerinfo ) {
        return library[m_library]->getplayerinfo(m_hplayer, pplayerinfo) ;
    }
	else
		return DVR_ERROR ; 
}

int decoder::setuserpassword( char * username, void * password, int passwordsize )
{
	if( m_hplayer!=NULL && library[m_library]->setuserpassword ) {
		return library[m_library]->setuserpassword(m_hplayer, username, password, passwordsize);
	}
	return DVR_ERROR ; 
}

int decoder::setvideokey( void * key, int keysize )
{
	if( library[m_library]->setvideokey ) {
		return library[m_library]->setvideokey(m_hplayer, key, keysize);
	}
	else
		return DVR_ERROR ; 
}

int decoder::getchannelinfo( int channel, struct channel_info * pchannelinfo )
{
	if( m_hplayer!=NULL && pchannelinfo ) {
		int res = -1;
		pchannelinfo->size = sizeof(struct channel_info);
		if (library[m_library]->getchannelinfo != NULL) {
			res = library[m_library]->getchannelinfo(m_hplayer, channel, pchannelinfo);
		}
		if( res<0 && channel < m_playerinfo.total_channel ) {
			// getchannelinfo HACKS, PLYWISCH, PLY266 may not support this function correctly
			pchannelinfo->features = 1 ;
			pchannelinfo->status = 0 ;
			pchannelinfo->xres = 0 ;
			pchannelinfo->yres = 0 ;
			sprintf( pchannelinfo->cameraname, "camera %d", channel+1 );
			res = 1 ;
		}
		return res ;
	}
	else
		return DVR_ERROR ; 
}

int decoder::attach(int channel, HWND hwnd )
{
	int res=DVR_ERROR ;
#ifdef APP_TVS
	if( g_usertype==IDOPERATOR ) {      // operator can't open first two channel
        if( channel<2 ) {
            return res ;
        }
    }
#endif
	if( m_hplayer!=NULL && library[m_library]->attach ) {
        res = library[m_library]->attach(m_hplayer, channel, hwnd);
		if( res>=0 ) {
			m_attachnum++;
		}
	}
	return res ; 
}

int decoder::detach()
{
	if( m_hplayer!=NULL && library[m_library]->detach) {
		m_attachnum=0;
		return library[m_library]->detach(m_hplayer);
	}
	else
		return DVR_ERROR ; 
}

int decoder::detachwindow( HWND hwnd )
{
	if( m_hplayer!=NULL && library[m_library]->detachwindow ) {
		if( m_attachnum>0 ) 
			m_attachnum--;
		return library[m_library]->detachwindow(m_hplayer, hwnd);
	}
	else
		return DVR_ERROR ; 
}

int decoder::selectfocus(HWND hwnd)
{
	typedef int(* _f)(HPLAYER handle, HWND hwnd);
	if (m_hplayer != NULL && library[m_library]) {
		_f f = (_f)library[m_library]->getapi("selectfocus");
		if (f) {
			return f(m_hplayer, hwnd);
		}
	}
	return DVR_ERROR;
}

int decoder::play()
{
    int i;
    int retry ;
    int res = DVR_ERROR ;
    char * password ;
    if( m_hplayer!=NULL && library[m_library]->play ) {
        res = library[m_library]->play(m_hplayer);
        if( res == DVR_ERROR_FILEPASSWORD ) {			// need to provide password
            // try old password
            for( i=0; i<DECODER_PASSWORD_NUMBER; i++) {
                password = (char *)m_password[i] ;
                if(*password!=0 ) {
                    setvideokey(password, strlen(password)+1);
                    res = library[m_library]->play(m_hplayer);
                    if( res!=DVR_ERROR_FILEPASSWORD ) {
                        return res ;
                    }
                }
            }
            // ask for user input
            for( retry=0; retry<3; retry++ ) {
                VideoPassword askpassword ;
                if( askpassword.DoModal(IDD_DIALOG_VIDEOPASSWORD)==IDOK ) {
                    password = (char *)(askpassword.m_password) ;
                    setvideokey(password, strlen(password)+1);
                    res = library[m_library]->play(m_hplayer);
                    if( res == DVR_ERROR_FILEPASSWORD ) {       // password error
                        continue ;              // retry 3 times
                    }
                    else if( res == 0 ) {
                        // add working pasword into password list
                        for( i=0; i<DECODER_PASSWORD_NUMBER; i++) {
                            if( *(char *)m_password[i] == 0 ) {
                                m_password[i]=password ;
                                break;
                            }
                        }
                        if( i==DECODER_PASSWORD_NUMBER ) {
                            // no empty slot? pick one
                            i= ((unsigned char)*password) % DECODER_PASSWORD_NUMBER ;
                            m_password[i]=password ;
                        }
                    }
                    break;
                }
                else {
                    break;
                }
            }
        }
    }

    return res ; 
}

int decoder::audioon(int channel, int audioon)
{
	int res = DVR_ERROR ;
	if( m_hplayer!=NULL && library[m_library]->audioon ) {
		int i ;
		// audio on/off function on PLYWISCH.DLL may have little delay
		for(i=0; i<10; i++) {
			res = library[m_library]->audioon(m_hplayer, channel, audioon);
			if( res>=0 ) {
				break;
			}
			Sleep(100);
		}
	}
	return res ;
}

int decoder::audioonwindow(HWND hwnd, int audioon)
{
	if( m_hplayer!=NULL && library[m_library]->audioonwindow ) {
		return library[m_library]->audioonwindow(m_hplayer, hwnd, audioon);
	}
	else
		return DVR_ERROR ; 
}

int decoder::pause()
{
	if( m_hplayer!=NULL && library[m_library]->pause ) {
		return library[m_library]->pause(m_hplayer);
	}
	else
		return DVR_ERROR ; 
}

int decoder::stop()
{
	if( m_hplayer!=NULL && library[m_library]->stop ) {
		return library[m_library]->stop(m_hplayer);
	}
	else
		return DVR_ERROR ; 
}

int decoder::refresh(int channel) 
{
	if( m_hplayer!=NULL && library[m_library]->refresh ) {
		return library[m_library]->refresh(m_hplayer, channel);
	}
	else
		return DVR_ERROR ; 
}

int decoder::fastforward(int speed )
{
	if( m_hplayer!=NULL && library[m_library]->fastforward ) {
		return library[m_library]->fastforward(m_hplayer, speed);
	}
	else
		return DVR_ERROR ; 
}

int decoder::slowforward(int speed )
{
	if( m_hplayer!=NULL && library[m_library]->slowforward) {
		return library[m_library]->slowforward(m_hplayer, speed);
	}
	else
		return DVR_ERROR ; 
}

int decoder::oneframeforward()
{
	if( m_hplayer!=NULL && library[m_library]->oneframeforward) {
		return library[m_library]->oneframeforward(m_hplayer);
	}
	else
		return DVR_ERROR ; 
}

int decoder::getdecframes()
{
	if( m_hplayer!=NULL && library[m_library]->getdecframes) {
		return library[m_library]->getdecframes(m_hplayer);
	}
	else
		return DVR_ERROR ; 
}

int decoder::backward(int speed )
{
	if( m_hplayer!=NULL && library[m_library]->backward ) {
		return library[m_library]->backward(m_hplayer, speed);
	}
	else
		return DVR_ERROR ; 
}

int decoder::capture(int channel, char * capturefilename )
{
	if( m_hplayer!=NULL && library[m_library]->capture ) {
		return library[m_library]->capture(m_hplayer, channel, capturefilename );
	}
	else
		return DVR_ERROR ; 
}

int decoder::capturewindow(HWND hwnd, char * capturefilename )
{
	if( m_hplayer!=NULL && library[m_library]->capturewindow ) {
		return library[m_library]->capturewindow(m_hplayer, hwnd, capturefilename );
	}
	else
		return DVR_ERROR ; 
}

int decoder::seek(struct dvrtime * t )
{
    int i;
    int retry ;
    int res = DVR_ERROR ;
    char * password ;
    if( m_hplayer!=NULL && library[m_library]->seek ) {
        res = library[m_library]->seek(m_hplayer, t);
        if( res == DVR_ERROR_FILEPASSWORD ) {			// need to provide password
            // try old password
            for( i=0; i<DECODER_PASSWORD_NUMBER; i++) {
                password = (char *)m_password[i] ;
                if(*password!=0 ) {
                    setvideokey(password, strlen(password)+1);
                    res = library[m_library]->seek(m_hplayer, t);
                    if( res!=DVR_ERROR_FILEPASSWORD ) {
                        return res ;
                    }
                }
            }
            // ask for user input
            for( retry=0; retry<3; retry++ ) {
                VideoPassword askpassword ;
                if( askpassword.DoModal(IDD_DIALOG_VIDEOPASSWORD)==IDOK ) {
                    password = (char *)(askpassword.m_password) ;
                    setvideokey(password, strlen(password)+1);
                    res = library[m_library]->seek(m_hplayer, t);
                    if( res == DVR_ERROR_FILEPASSWORD ) {       // password error
                        continue ;              // retry 3 times
                    }
                    else if( res == 0 ) {
                        // add working pasword into password list
                        for( i=0; i<DECODER_PASSWORD_NUMBER; i++) {
                            if( *(char *)m_password[i] == 0 ) {
                                m_password[i]=password ;
                                break;
                            }
                        }
                        if( i==DECODER_PASSWORD_NUMBER ) {
                            // no empty slot? pick one
                            i= ((unsigned char)*password) % DECODER_PASSWORD_NUMBER ;
                            m_password[i]=password ;
                        }
                    }
                    break;
                }
                else {
                    break;
                }
            }
        }
    }

    return res ; 
}

int decoder::getcurrenttime(struct dvrtime * t )
{
	if( m_hplayer!=NULL && library[m_library]->getcurrenttime ) {
        return library[m_library]->getcurrenttime(m_hplayer, t);
    }
	else {
		return DVR_ERROR ;
    }
}

int decoder::getdaylist(int * daylist, int sizeoflist )
{
	if( m_hplayer!=NULL && library[m_library]->getdaylist ) {
        return library[m_library]->getdaylist(m_hplayer,daylist,sizeoflist);
    }
	else {
		return DVR_ERROR ;
    }
}

int decoder::getcliptimeinfo(int channel, struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize)
{
	if( m_hplayer!=NULL && library[m_library]->getcliptimeinfo ) {
		return library[m_library]->getcliptimeinfo(m_hplayer, channel, begintime, endtime, tinfo, tinfosize);
	}
	else
		return DVR_ERROR ; 
}

int decoder::getclipdayinfo(int channel, dvrtime * daytime ) 
{
	if( m_hplayer!=NULL && library[m_library]->getclipdayinfo ) {
		if( daytime->year<1980 || daytime->year>2099 )
			return 0;
		return library[m_library]->getclipdayinfo(m_hplayer, channel, daytime);
	}
	else
		return 0 ; 
}

int decoder::getlockfiletimeinfo(int channel, struct dvrtime * begintime, struct dvrtime * endtime, struct cliptimeinfo * tinfo, int tinfosize)
{
	if( m_hplayer!=NULL && library[m_library]->getlockfiletimeinfo ) {
		return library[m_library]->getlockfiletimeinfo(m_hplayer, channel, begintime, endtime, tinfo, tinfosize);
	}
	else
		return DVR_ERROR ; 
}

int decoder::geteventtimelist( struct dvrtime * date, int eventnum, int * elist, int elistsize)
{
    int res = DVR_ERROR ;
	if( m_hplayer!=NULL && library[m_library]->geteventtimelist ) {
        res = library[m_library]->geteventtimelist(m_hplayer, date, eventnum, elist, elistsize);
	}
    if( res==DVR_ERROR ) {
        // check on SMART SERVER
        int lib ;
        for( lib=0; lib<num_library ; lib++) {
            if( (library[lib]->libtype==PLAYER_TYPE_MINITRACK || library[lib]->libtype==PLAYER_TYPE_SMARTSERVER) &&
                library[lib]->geteventtimelist ) 
            {
                res = library[lib]->geteventtimelist(m_hplayer, date, eventnum, elist, elistsize);
                if( res>DVR_ERROR ) {
                    break;
                }
            }
        }
    }
    return res ;
}

int decoder::savedvrfile(struct dvrtime * begintime, struct dvrtime * endtime, char * duppath, int flags, struct dup_state * dupstate)
{
	if( m_hplayer!=NULL && library[m_library]->savedvrfile ) {
		return library[m_library]->savedvrfile(m_hplayer, begintime, endtime, duppath, flags, dupstate);
	}
	else
		return DVR_ERROR ; 
}

int decoder::savedvrfile2(struct dvrtime * begintime, struct dvrtime * endtime, char * duppath, int flags, struct dup_state * dupstate, char * channels, char * password )
{
	typedef int(*_f)(HPLAYER handle, struct dvrtime * begintime, struct dvrtime * endtime, char * duppath, int flags, struct dup_state * dupstate, char * channels, char * password);
	_f f = (_f)getapi("savedvrfile2");
	if (f) {
		return f(m_hplayer, begintime, endtime, duppath, flags, dupstate, channels, password);
	}
	else 
		return DVR_ERROR_NOAPI;
}


int decoder::dupvideo(struct dvrtime * begintime, struct dvrtime * endtime, char * duppath, int flags, struct dup_state * dupstate)
{
	if( m_hplayer!=NULL && library[m_library]->dupvideo ) {
		return library[m_library]->dupvideo(m_hplayer, begintime, endtime, duppath, flags, dupstate);
	}
	else
		return DVR_ERROR ; 
}

int decoder::configureDVR()
{

	extern HWND g_mainwnd;
	if( m_hplayer!=NULL && library[m_library]->configureDVR ) {
		return library[m_library]->configureDVR(m_hplayer);
	}
	else {
		return DVR_ERROR ; 
	}
}

int decoder::senddvrdata(int protocol, void * sendbuf, int sendsize)
{
	if( m_hplayer!=NULL && library[m_library]->senddvrdata ) {
        return library[m_library]->senddvrdata(m_hplayer, protocol, sendbuf, sendsize);
	}
	else
		return DVR_ERROR ; 
}

int decoder::recvdvrdata(int protocol, void ** precvbuf, int * precvsize)
{
	if( m_hplayer!=NULL && library[m_library]->recvdvrdata) {
		return library[m_library]->recvdvrdata(m_hplayer, protocol, precvbuf, precvsize);
	}
	else
		return DVR_ERROR ; 
}

int decoder::freedvrdata(void * dvrbuf)
{
	if( m_hplayer!=NULL && library[m_library]->freedvrdata) {
		return library[m_library]->freedvrdata(m_hplayer, dvrbuf);
	}
	else
		return DVR_ERROR ; 
}

// get recording state, 1: recording, 0: no recording
int decoder::getrecstate()
{
    int i;
    int rec = 0 ;

    unsigned char * recvbuf = NULL ;
    int  recvsize = 0 ;
    int res ;

    res = recvdvrdata( PROTOCOL_PW_GETSTATUS,  (void **)&recvbuf, &recvsize );
    if( res>=0 && recvsize>0 && recvbuf!=NULL ) {
        for( i=0; i<recvsize ; i++ ) {
            if( recvbuf[i] & 4 )
                rec |= ( 1<<i ) ;
            if( recvbuf[i] & 8 )
                rec |= ( 1<<(i+16) ) ;
        }
        freedvrdata( recvbuf ); 
        return rec ;
    }


    channel_info ci ;
    // only get the first 2 channel (for PWII)
    for( i=0; i<2 ; i++ ) {
        memset(&ci, 0, sizeof(ci));
        ci.size = sizeof(ci);
        if( getchannelinfo( i, &ci )>=0 ) {
            if( ci.status & 4 ) {
                rec |= (1<<i);
            }
        }
    }
    return rec ;
}

// get recording state, 1: recording, 0: no recording
int decoder::getsystemstate(string & st)
{
	unsigned char * recvbuf = NULL ;
    int  recvsize = 0 ;
    int res ;

    res = recvdvrdata( PROTOCOL_PW_GETSYSTEMSTATUS,  (void **)&recvbuf, &recvsize );
    if( res>=0 && recvsize>0 && recvbuf!=NULL ) {
		st = (char *)recvbuf ;
        freedvrdata( recvbuf ); 
    }
	return res ;
}

// idlist is an array of string pointer
int decoder::getofficeridlist( string * idlist[], int maxlist )
{
	int i;
    unsigned char * recvbuf = NULL ;
    int  recvsize = 0 ;
    int res ;
	int off = 0;
    res = recvdvrdata( PROTOCOL_PW_GETPOLICEIDLIST,  (void **)&recvbuf, &recvsize );
    if( res>=0 && recvsize>0 && recvbuf!=NULL ) {
		for( i=0; i<maxlist; i++ ) {
			if( off<recvsize ) {
				idlist[i] = new string((char*)recvbuf+off);
				off+=64 ;
			}
			else {
				break;
			}
		}
        return i ;
    }

	return 0;
}

// idlist is an array of string pointer
int decoder::setofficerid( char * oid )
{
    unsigned char * recvbuf = NULL ;
    int  recvsize = 0 ;
    int res ;
	int off = 0;
    res = senddvrdata(PROTOCOL_PW_SETPOLICEID, oid, strlen(oid)+1);
    if( res>=0 ) {
		return 1 ;
    }
	return 0;
}

// get vri list size in bytes
int decoder::getvrilistsize( int * rowsize )
{
	unsigned char * recvbuf = NULL ;
    int  recvsize = 0 ;
    int res ;
	*rowsize = 0 ;
    res = recvdvrdata( PROTOCOL_PW_GETVRILISTSIZE,  (void **)&recvbuf, &recvsize );
	if( recvsize > 0 ) {
		sscanf( (char *)recvbuf, "%d,%d", &recvsize, rowsize ) ;
		freedvrdata(recvbuf) ;
		return recvsize ;
	}
	return 0 ;
}

// get vri list in array
int decoder::getvrilist( char * vri, int vsize )
{
    unsigned char * recvbuf = NULL ;
    int  recvsize = 0 ;
    int res ;
    res = recvdvrdata( PROTOCOL_PW_GETVRILIST,  (void **)&recvbuf, &recvsize );
	if( recvsize > 0 ) {
		if( vsize>recvsize ) vsize = recvsize ;
		memcpy( vri, recvbuf, vsize );
		freedvrdata(recvbuf) ;
		return vsize ;
	}
	return -1 ;
}

// idlist is an array of string pointer
int decoder::setvrilist( char * vrilist, int vrisize )
{
    int res = senddvrdata(PROTOCOL_PW_SETVRILIST, vrilist, vrisize);
    if( res>=0 ) {
		return 1 ;
    }
	return 0;
}


// set PWZ6 covert mode
int decoder::covert( int onoff )
{
	char cov[4] ;
	if( onoff ) {
		cov[0] = 1 ;
	}
	else {
		cov[0] = 0 ;
	}
    int res = senddvrdata(PROTOCOL_PW_SETCOVERTMODE, cov, 4);
    if( res>=0 ) {
		return 1 ;
    }
	return 0;
}

int decoder::configurePlayer()
{
	if( m_hplayer!=NULL && library[m_library]->configurePlayer) {
		return library[m_library]->configurePlayer(m_hplayer);
	}
	else
		return DVR_ERROR ;
}

int decoder::lockdata( struct dvrtime * begintime, struct dvrtime * endtime )
{
	if( m_hplayer!=NULL && library[m_library]->lockdata) {
		return library[m_library]->lockdata(m_hplayer, begintime, endtime);
	}
	else
		return DVR_ERROR ;
}

int decoder::unlockdata( struct dvrtime * begintime, struct dvrtime * endtime )
{
	if( m_hplayer!=NULL && library[m_library]->unlockdata) {
		return library[m_library]->unlockdata(m_hplayer, begintime, endtime);
	}
	else
		return DVR_ERROR ;
}

int decoder::cleandata( int mode, struct dvrtime * begintime, struct dvrtime * endtime )
{
	if( m_hplayer!=NULL && library[m_library]->cleandata) {
		return library[m_library]->cleandata(m_hplayer, mode, begintime, endtime );
	}
	else
		return DVR_ERROR ;
}

int decoder::checkpassword()
{
	if( m_hplayer!=NULL && library[m_library]->setuserpassword &&
		m_playerinfo.videokeytype == 1 ) {
		UserPass usernamepassword;
        usernamepassword.m_servername = m_playerinfo.servername ;
		int res=-1 ;
		while( res<0 ) {
			if( usernamepassword.DoModal(IDD_DIALOG_USERPASSWORD)==IDOK ) {
                char * pass= usernamepassword.m_password ;
				res = library[m_library]->setuserpassword(m_hplayer, usernamepassword.m_username, pass, strlen(pass)+1 );
			}
			else {
				return DVR_ERROR ;
			}
		}
		return 0 ;			// password passwd
	}
	else
		return 0 ;			// consider it is ok (no password required)
}

int decoder::pwkeypad( int keycode, int press )
{
	if( m_hplayer!=NULL && library[m_library]->pwkeypad ) {
		return library[m_library]->pwkeypad(m_hplayer, keycode, press );
	}
	else
		return DVR_ERROR ; 
}

int decoder::getlocation(char * locationbuffer, int buffersize)
{
	if( m_hplayer!=NULL && library[m_library]->getlocation ) {
        return library[m_library]->getlocation(m_hplayer, locationbuffer, buffersize);
    }
	else {
		return DVR_ERROR ;
    }
}

int decoder::setblurarea( int channel, struct blur_area * ba, int banumber )
{
	if( m_hplayer!=NULL && library[m_library]->setblurarea ) {
        return library[m_library]->setblurarea(m_hplayer, channel, ba, banumber );
    }
	else {
		return DVR_ERROR ;
    }
}

int decoder::clearblurarea( int channel )
{
	if( m_hplayer!=NULL && library[m_library]->clearblurarea ) {
        return library[m_library]->clearblurarea(m_hplayer, channel);
    }
	else {
		return DVR_ERROR ;
    }
}

int decoder::showzoomin(int channel, struct zoom_area * za)
{
	if (m_hplayer != NULL && library[m_library]->showzoomin) {
		return library[m_library]->showzoomin(m_hplayer, channel, za);
	}
	else
		return DVR_ERROR;
}

int decoder::drawrectangle(int channel, float x, float y, float w, float h)
{
	if (m_hplayer != NULL && library[m_library]->drawrectangle) {
		return library[m_library]->drawrectangle(m_hplayer, channel, x, y, w, h );
	}
	else
		return DVR_ERROR;
}

int decoder::drawline(int channel, float x1, float y1, float x2, float y2)
{
	typedef int(*_f)(HPLAYER handle, int channel, float x1, float y1, float x2, float y2);
	if (m_hplayer != NULL && library[m_library]) {
		_f f = (_f)library[m_library]->getapi("drawline");
		if (f) {
			return f(m_hplayer, channel, x1, y1, x2, y2);
		}
		return 0;
	}
	return DVR_ERROR;
}

int decoder::clearlines(int channel)
{
	typedef int(*_f)(HPLAYER handle, int channel);
	if (m_hplayer != NULL && library[m_library]) {
		_f f = (_f)library[m_library]->getapi("clearlines");
		if (f) {
			return f(m_hplayer, channel);
		}
		return 0;
	}
	return DVR_ERROR;
}

int decoder::setdrawcolor(int channel, COLORREF color)
{
	typedef int(*_f)(HPLAYER handle, int channel, COLORREF color);
	if (m_hplayer != NULL && library[m_library]) {
		_f f = (_f)library[m_library]->getapi("setdrawcolor");
		if (f) {
			return f(m_hplayer, channel, color);
		}
		return 0;
	}
	return DVR_ERROR;
}

int decoder::supportzoomin()
{
	return (m_hplayer != NULL && library[m_library]->showzoomin);
}

int decoder::g_close()
{
	int i; 
	g_playstat=PLAY_STOP;
	for(i=0; i<MAXDECODER; i++ ) {
		g_decoder[i].detach();
		g_decoder[i].close();
	}
    g_minitrack_used=0;     //assume minitrack interface also closed
	return 0 ;
}

int decoder::g_play()
{
	int i; 
	g_playstat = PLAY_PLAY ;
	for(i=0; i<MAXDECODER; i++ ) {
		g_decoder[i].play();
	}
	return 0 ;
}

int decoder::g_pause()
{
	int i; 
	for(i=0; i<MAXDECODER; i++ ) {
		g_decoder[i].pause();
	}
	return 0 ;
}

int decoder::g_stop()
{
	int i; 
	for(i=0; i<MAXDECODER; i++ ) {
		g_decoder[i].stop();
	}
	return 0 ;
}

int decoder::g_fastforward(int speed )
{
	int i; 
	for(i=0; i<MAXDECODER; i++ ) {
		g_decoder[i].fastforward(speed);
	}
	return 0 ;
}

int decoder::g_slowforward(int speed )
{
	int i; 
	for(i=0; i<MAXDECODER; i++ ) {
		g_decoder[i].slowforward(speed);
	}
	return 0 ;
}

int decoder::g_oneframeforward()
{
	int i; 
	for(i=0; i<MAXDECODER; i++ ) {
		g_decoder[i].oneframeforward();
	}
	return 0 ;
}

int decoder::g_getdecframes()
{
	int frames=0;
	int i; 
	for(i=0; i<MAXDECODER; i++ ) {
		frames+=g_decoder[i].getdecframes();
	}
	return frames ;
}

int decoder::g_backward(int speed )
{
	int i; 
	for(i=0; i<MAXDECODER; i++ ) {
		g_decoder[i].backward(speed);
	}
	return 0 ;
}

int decoder::g_seek(struct dvrtime * t )
{
	int i; 
	for(i=0; i<MAXDECODER; i++ ) {
		g_decoder[i].seek(t);
	}
	return 0 ;
}

static struct dvrtime g_currenttime ;
int decoder::g_getcurrenttime(struct dvrtime * t )
{
	int i; 
	for(i=0; i<MAXDECODER; i++ ) {
		if( g_decoder[i].getcurrenttime(t)>=0 ) {
			if( t->year>1980 && t->year<2100 )  {
				g_currenttime = *t ;
				return 1 ;
            }
		}
	}
	return DVR_ERROR ; 
}

static struct dvrtime livetime ;
static DWORD  xtick = 0 ;
void getlivetimethread(void * dvrt)
{
	if( decoder::g_getcurrenttime( &livetime ) > 0 ) {
		xtick = GetTickCount() ;
	}
	else {
		livetime.year = 2000 ;
	}
}

// update dvrtime using background thread, to reduce app lag
int decoder::g_getlivetime(struct dvrtime * t )
{
	DWORD dtick = GetTickCount()-xtick ;
	g_currenttime = livetime ;
	time_add( g_currenttime, dtick/1000 );
	*t = g_currenttime ;
	if( dtick > 60000 ) {
		_beginthread( getlivetimethread,0,&livetime);
	}
	return 1 ;
}

// max 128 charater
// 1: success
// 0: failed
int decoder::g_getlibname(int index, LPTSTR libname)
{
	if( index<num_library ) {
		_tcscpy(libname, library[index]->modulename);
		return 1 ;
	}
	return 0;
}

LPCSTR decoder::g_getlibname(int index)
{
	if( index<num_library ) {
        return (LPCSTR)library[index]->modulename ;
	}
	return NULL;
}

// 4 integer version number
// 1: success
// 0: failed
int decoder::g_getlibversion(int index, int * version )
{
	struct player_info plyinfo ;
	if( index<num_library ) {
		memset(&plyinfo, 0, sizeof(plyinfo));
		plyinfo.size = sizeof( plyinfo );
		if( library[index]->getplayerinfo(NULL, &plyinfo)>=0 ) {
			version[0] = plyinfo.playerversion[0];
			version[1] = plyinfo.playerversion[1];
			version[2] = plyinfo.playerversion[2];
			version[3] = plyinfo.playerversion[3];
		}
		else {
			version[0] = 0;
			version[1] = 0;
			version[2] = 0;
			version[3] = 0;
		}
		if( version[2]==0 && version[3]==0 ) {
			version[2] = library[index]->filedate / 10000 - 2000;
			version[3] = library[index]->filedate % 10000 ;
		}
		return 1 ;
	}
	return 0;
}

int decoder::g_checkpassword()
{
	int res=0 ;
	int i; 
	for(i=0; i<MAXDECODER; i++ ) {
		if( g_decoder[i].isopen() ) {
			if( g_decoder[i].checkpassword()<0 ) {
				return -1 ;
			}
		}
	}
	return 0 ; 
}


// minitrack support

int g_minitrack_up ;							// if minitrack working?
static decoder * g_minitrack_decoder ;			// current selected decoder
static struct dvr_func g_minitrack_func ;
int g_minitrack_used ;

// DVRViewer function interface for minitrack service

//  Open dvr media base on servername ;
int minitrack_openserver_s( char * servername )
{
	int i ;
//	if( g_minitrack_up ) {
		if( decoder::finddevice(3) == DVR_ERROR_BUSY ) {	// find DVR and disks
			Sleep(200);						// wait a bit
		}
		for( i=0; i<decoder::polldevice() ; i++ ) {
        	decoder dec ;
			if( dec.open( i, PLY_PLAYBACK )>0 ) {
				if( strcmp( dec.m_playerinfo.servername, servername )==0 ) {		// fount this server
					dec.close();						// close this decoder
					g_maindlg->ClosePlayer() ;				// close player screens
					decoder::g_close();					// close all decoders
					g_decoder[0].open(i, PLY_PLAYBACK) ;	// open on g_decoder
					g_maindlg->SetPlaymode(CLIENTMODE_PLAYBACK) ;
					g_maindlg->StartPlayer();				// start player
					return 0;
				}
				dec.close();
			}
		}
		decoder::stopfinddevice();		// clean device list
//	}
	return DVR_ERROR ;
}

int minitrack_openserver( char * servername )
{
    if( strcmp( g_decoder[0].getservername(), servername )==0 ) {
        // same server, don't reopen
        decoder::g_play();
        return 0 ;
    }
    else {
        g_maindlg->OpenDvr(PLY_PLAYBACK, CLIENTMODE_PLAYBACK, TRUE, servername);
        if( g_decoder[0].isopen() ) {
            return 0;
        }
        else {
            return DVR_ERROR ;
        }
    }
}

//  Open DVR server use IP address or network name;
int minitrack_openremote( char * remoteserver )
{
//	if( g_minitrack_up ) {
		g_maindlg->ClosePlayer() ;				// close player screens
		decoder::g_close();					// to make sure all decoder closed
        string remote ;
		remote = remoteserver ;
        g_maindlg->PlayRemote( remote, PLY_PLAYBACK );
		return 0;
//	}
//	return DVR_ERROR ;
}

//  Open DVR server use IP address or network name;
int minitrack_openlive( char * remoteserver )
{
//	if( g_minitrack_up ) {
		g_maindlg->ClosePlayer() ;				// close player screens
		decoder::g_close();					// to make sure all decoder closed
        string remote ;
		remote = remoteserver ;
        g_maindlg->PlayRemote( remote, PLY_PREVIEW );
		return 0;
//	}
//	return DVR_ERROR ;
}

//  Open DVR media on local hard drive;
int minitrack_openharddrive( char * dirname )
{
//	if( g_minitrack_up ) {
		g_maindlg->ClosePlayer() ;				// close player screens
		decoder::g_close();					// to make sure all decoder closed
		g_decoder[0].openharddrive( dirname );
		g_maindlg->SetPlaymode( CLIENTMODE_PLAYBACK );
		g_maindlg->StartPlayer();				// start player
		return 0;
//	}
//	return DVR_ERROR ;
}

//  Open single .DVR or .264 file;
int minitrack_openfile( char * dvrfilename )
{
//	if( g_minitrack_up ) {
		g_maindlg->ClosePlayer() ;				// close player screens
		decoder::g_close();					// to make sure all decoder closed
        string fname(dvrfilename) ;
		g_maindlg->Playfile(fname) ;
		return 0;
//	}
//	return DVR_ERROR ;
}

//  Close current player;
int minitrack_close()
{
//	if( g_minitrack_up ) {
		g_maindlg->ClosePlayer() ;				// close player screens
		decoder::g_close();					// to make sure all decoder closed
		return 0;
//	}
//	return DVR_ERROR ;
}

//  Get current player information;
int minitrack_getplayerinfo( struct player_info * pplayerinfo )
{
//	if( g_minitrack_up ) {
		if( g_minitrack_decoder &&
			g_minitrack_decoder->isopen() ) {
			return g_minitrack_decoder->getplayerinfo( pplayerinfo );
		}
//	}
	return DVR_ERROR ;
}

// Get channel information 
int minitrack_getchannelinfo( int channel, struct channel_info * pchannelinfo )
{
//	if( g_minitrack_up ) {
		if( g_minitrack_decoder &&
			g_minitrack_decoder->isopen() ) {
			return g_minitrack_decoder->getchannelinfo( channel, pchannelinfo ) ;
		}
//	}
	return DVR_ERROR ;
}

//  Start play video
int minitrack_play()
{
//	if( g_minitrack_up ) {
	::SendDlgItemMessage( g_maindlg->getHWND(), IDC_PLAY, BM_CLICK, 0, 0 );
		return 0;
//	}
//	return DVR_ERROR ;
}
    
//  Pause;
int minitrack_pause()
{
//	if( g_minitrack_up ) {
	::SendDlgItemMessage( g_maindlg->getHWND(), IDC_PAUSE, BM_CLICK, 0, 0 );
		return 0;
//	}
//	return DVR_ERROR ;
}
    
//  Stop;
int minitrack_stop()
{
//	if( g_minitrack_up ) {
	::SendDlgItemMessage( g_maindlg->getHWND(), IDC_PAUSE, BM_CLICK, 0, 0 );
		decoder::g_stop();
		return 0;
//	}
//	return DVR_ERROR ;
}
    
//  Play fast forward;
int minitrack_fastforward( int speed )
{
//	if( g_minitrack_up ) {
	::SendDlgItemMessage( g_maindlg->getHWND(), IDC_FAST, BM_CLICK, 0, 0 );
		return 0;
//	}
//	return DVR_ERROR ;
}
    
//  Play slow forward;
int minitrack_slowforward( int speed )
{
//	if( g_minitrack_up ) {
	::SendDlgItemMessage( g_maindlg->getHWND(), IDC_SLOW, BM_CLICK, 0, 0 );
		return 0;
//	}
//	return DVR_ERROR ;
}
    
//  Play one frame forward
int minitrack_oneframeforward()
{
//	if( g_minitrack_up ) {
	::SendDlgItemMessage( g_maindlg->getHWND(), IDC_STEP, BM_CLICK, 0, 0 );
	return 0;
//	}
//	return DVR_ERROR ;
}
    
//  Play backward;
int minitrack_backward( int speed )
{
//	if( g_minitrack_up ) {
		decoder::g_backward(speed);
		return 0;
//	}
//	return DVR_ERROR ;
}
    
//  Seek to specified time;
int minitrack_seek( struct dvrtime * t )
{
//	if( g_minitrack_up ) {
	g_maindlg->SeekTime( t );
	return 0;
//	}
//	return DVR_ERROR ;
}

//  Get current playing time;
int minitrack_getcurrenttime( struct dvrtime * t )
{
    int res = DVR_ERROR ;
    //	if( g_minitrack_up ) {
    if( g_minitrack_decoder&&
        g_minitrack_decoder->isopen() ) {
            g_minitrack_used=1 ;
			*t = g_currenttime ;
			res = 1 ;
            //res = g_minitrack_decoder->getcurrenttime( t );
    }
    //	}
    return res ;
}
    
//  Get playing status;
//  ( 0: stop, 1: pause, 90-99: slow, 100 play 101-110: fast play)
int minitrack_getplayerstatus()            
{
//	if( g_minitrack_up ) {
		return g_playstat ;
//	}
//	return DVR_ERROR ;
}
    
//  Get current login user name;
int minitrack_getusername( char * buffer, int buffersize)
{
//	if( g_minitrack_up ) {
        strncpy( buffer, g_username, buffersize );
		return (int)strlen( buffer );
//	}
//	return DVR_ERROR ;
}

// receive dvr server data;
int minitrack_senddvrdata( int protocol, void * sendbuf, int sendsize)
{
//	if( g_minitrack_up ) {
		if( g_minitrack_decoder &&
			g_minitrack_decoder->isopen() ) {
				return g_minitrack_decoder->senddvrdata(protocol, sendbuf, sendsize);
		}
//	}
	return DVR_ERROR ;
}
    
// receive dvr server data;
int minitrack_recvdvrdata( int protocol, void ** precvbuf, int * precvsize)
{
//	if( g_minitrack_up ) {
		if( g_minitrack_decoder &&
			g_minitrack_decoder->isopen() ) {
				return g_minitrack_decoder->recvdvrdata( protocol, precvbuf, precvsize );
		}
//	}
	return DVR_ERROR ;
}
    
// free dvr server data	
int minitrack_freedvrdata( void * dvrbuf)
{
//	if( g_minitrack_up ) {
		if( g_minitrack_decoder &&
			g_minitrack_decoder->isopen() ) {
				return g_minitrack_decoder->freedvrdata( dvrbuf );
		}
//	}
	return DVR_ERROR ;
}

// free dvr server data	
int minitrack_getlocation( char * locationbuffer, int buffersize )
{
//	if( g_minitrack_up ) {
		if( g_minitrack_decoder &&
			g_minitrack_decoder->isopen() ) {
                g_minitrack_used=1 ; ;
				return g_minitrack_decoder->getlocation( locationbuffer, buffersize );
		}
//	}
	return DVR_ERROR ;
}

// start minitrack service
int decoder::g_minitrack_start(decoder * pdec)
{
	struct player_info plyinfo ;
	int index ;
	g_minitrack_up = 1 ;
	g_minitrack_decoder = pdec ;

	for( index=0; index<num_library; index++ ) {
		memset(&plyinfo, 0, sizeof(plyinfo));
		plyinfo.size = sizeof( plyinfo );
		if( library[index]->getplayerinfo(NULL, &plyinfo)>=0 ) {
			if( (plyinfo.features & PLYFEATURE_MINITRACK) &&
				(library[index]->minitrack_start != NULL ) ) {
					library[index]->minitrack_start( &g_minitrack_func, g_maindlg->getHWND() ) ;
			}
		}
	}
	return 0;
}

// DVRView selected a new DVR server
int decoder::g_minitrack_selectserver(decoder * pdec)
{
	int index ;
//	if( !g_minitrack_up ) {
//		return DVR_ERROR ;
//	}
	if( pdec!=g_minitrack_decoder ) {
		struct player_info plyinfo ;
		g_minitrack_decoder = pdec ;
		for( index=0; index<num_library; index++ ) {
			memset(&plyinfo, 0, sizeof(plyinfo));
			plyinfo.size = sizeof( plyinfo );
			if( library[index]->getplayerinfo(NULL, &plyinfo)>=0 ) {
				if( (plyinfo.features & PLYFEATURE_MINITRACK) &&
					(library[index]->minitrack_selectserver != NULL ) ) {
					library[index]->minitrack_selectserver() ;
				}
			}
		}
	}
	return 0;
}

// stop minitrack service
int decoder::g_minitrack_stop( )
{
	int index ;

	struct player_info plyinfo ;
	for( index=0; index<num_library; index++ ) {
		memset(&plyinfo, 0, sizeof(plyinfo));
		plyinfo.size = sizeof( plyinfo );
		if( library[index]->getplayerinfo(NULL, &plyinfo)>=0 ) {
			if( (plyinfo.features & PLYFEATURE_MINITRACK) &&
				(library[index]->minitrack_stop != NULL ) ) {
				library[index]->minitrack_stop() ;
			}
		}
	}
	g_minitrack_decoder = NULL ;
	g_minitrack_up = 0;		// minitrack service down
    g_minitrack_used = 0 ;  // assume not using minitrack
	return 0;
}

// send patrol witness user event to minitrack DLL
int decoder::g_minitrack_sendpwevent(void *buf, int bufsize)
{
	int index ;

	struct player_info plyinfo ;
	for( index=0; index<num_library; index++ ) {
		memset(&plyinfo, 0, sizeof(plyinfo));
		plyinfo.size = sizeof( plyinfo );
		if( library[index]->getplayerinfo(NULL, &plyinfo)>=0 ) {
			if( (plyinfo.features & PLYFEATURE_MINITRACK) &&
				(library[index]->senddvrdata != NULL ) ) {
				library[index]->senddvrdata(NULL, PROTOCOL_PWPLAYEREVENT, buf, bufsize) ;
			}
		}
	}
	return 0;
}

// start minitrack service
int decoder::g_minitrack_init_interface()
{
	struct player_info plyinfo ;
	int index ;
//	g_minitrack_up = 1 ;
//	g_minitrack_decoder = NULL ;
	// initialize interface function
	g_minitrack_func.openserver       = minitrack_openserver ;
	g_minitrack_func.openremote       = minitrack_openremote ;
	g_minitrack_func.openharddrive    = minitrack_openharddrive ;
	g_minitrack_func.openfile         = minitrack_openfile ;
	g_minitrack_func.close            = minitrack_close ;
	g_minitrack_func.getplayerinfo    = minitrack_getplayerinfo ;
	g_minitrack_func.getchannelinfo   = minitrack_getchannelinfo ;
	g_minitrack_func.play             = minitrack_play ;
	g_minitrack_func.pause            = minitrack_pause ;
	g_minitrack_func.stop             = minitrack_stop ;
	g_minitrack_func.fastforward      = minitrack_fastforward ;
	g_minitrack_func.slowforward      = minitrack_slowforward ;
	g_minitrack_func.oneframeforward  = minitrack_oneframeforward ;
	g_minitrack_func.backward         = minitrack_backward ;
	g_minitrack_func.seek             = minitrack_seek ;
	g_minitrack_func.getcurrenttime   = minitrack_getcurrenttime ;
	g_minitrack_func.getplayerstatus  = minitrack_getplayerstatus ;
	g_minitrack_func.getusername      = minitrack_getusername ;
	g_minitrack_func.senddvrdata      = minitrack_senddvrdata ;
	g_minitrack_func.recvdvrdata      = minitrack_recvdvrdata ;
	g_minitrack_func.freedvrdata      = minitrack_freedvrdata ;
	g_minitrack_func.getlocation      = minitrack_getlocation ;
	g_minitrack_func.openlive         = minitrack_openlive ;

	for( index=0; index<num_library; index++ ) {
		memset(&plyinfo, 0, sizeof(plyinfo));
		plyinfo.size = sizeof( plyinfo );
		if( library[index]->getplayerinfo(NULL, &plyinfo)>=0 ) {
			if( (plyinfo.features & PLYFEATURE_MINITRACK) &&
				(library[index]->minitrack_init_interface != NULL ) ) {
				library[index]->minitrack_init_interface( &g_minitrack_func ) ;
			}
		}
	}
	return 0;
}
