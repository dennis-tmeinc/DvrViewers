// util.h : define About Dialog and Password Dialog
//

#pragma once

#ifndef __UTIL_H__
#define __UTIL_H__

#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
using namespace Gdiplus;

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <string.h>
#include <stdarg.h>

#include <objidl.h>

#include "decoder.h"
#include "cstr.h"

// Memory Stream Interface for Image loading
class MemoryStream :
	public IStream
{
	ULONG ref ;
	char * buffer ;
    int    allocated ;
	int    buffersize ;
	int    pointer ;
public:
    // dup, 1: allocate new memory and duplicated the contentes
	MemoryStream(void * bbuffer, int bsize, int dup=0) {
		buffersize = bsize ;
        if( bbuffer ) {
            if( dup ) {
                buffer = new char [buffersize];
                memcpy( buffer, bbuffer, buffersize );
                allocated = 1 ;
            }
            else {
                buffer = (char *)bbuffer ;
                allocated = 0 ;
            }
        }
        else {
            buffer = new char [buffersize] ;
            allocated = 1 ;
        }
		pointer = 0 ;
		ref = 1 ;           // initial ref counter to 1
	}
    	
    virtual ~MemoryStream() {
        if( allocated ) {
            delete [] buffer ;
        }
	}

    HRESULT STDMETHODCALLTYPE Seek( 
        /* [in] */ LARGE_INTEGER dlibMove,
        /* [in] */ DWORD dwOrigin,
		/* [out] */ ULARGE_INTEGER *plibNewPosition) {
			int move = (int) dlibMove.QuadPart ;
			if( dwOrigin == STREAM_SEEK_END ) {
				pointer = buffersize ;
			}
			else if( dwOrigin == STREAM_SEEK_SET ) {
				pointer = 0 ;
			}
			pointer += move ;
			if( pointer<0 ) {
				pointer=0 ;
			}
			if( pointer>buffersize ) {
				pointer=buffersize ;
			}
			if( plibNewPosition ) {
				plibNewPosition->QuadPart = (ULONGLONG) pointer ;
			}
			return S_OK ;
	}
    
	HRESULT STDMETHODCALLTYPE SetSize( ULARGE_INTEGER NewSize) {
		return E_NOTIMPL ;
	}
    
    HRESULT STDMETHODCALLTYPE CopyTo( IStream *pstm,
        /* [in] */ ULARGE_INTEGER cb,
        /* [out] */ ULARGE_INTEGER *pcbRead,
        /* [out] */ ULARGE_INTEGER *pcbWritten)
	{
		return E_NOTIMPL ;
	}
    
    virtual HRESULT STDMETHODCALLTYPE Commit( 
		/* [in] */ DWORD grfCommitFlags){
			return E_NOTIMPL ;
	}
    
    virtual HRESULT STDMETHODCALLTYPE Revert( void)
	{
		return E_NOTIMPL ;
	}
    
    virtual HRESULT STDMETHODCALLTYPE LockRegion( 
        /* [in] */ ULARGE_INTEGER libOffset,
        /* [in] */ ULARGE_INTEGER cb,
		/* [in] */ DWORD dwLockType) {
			return E_NOTIMPL ;
	}
    
    virtual HRESULT STDMETHODCALLTYPE UnlockRegion( 
        /* [in] */ ULARGE_INTEGER libOffset,
        /* [in] */ ULARGE_INTEGER cb,
		/* [in] */ DWORD dwLockType) {
			return E_NOTIMPL ;
	}
    
    virtual HRESULT STDMETHODCALLTYPE Stat( 
        /* [out] */ STATSTG *pstatstg,
		/* [in] */ DWORD grfStatFlag) {
            if( pstatstg ) {
                pstatstg->pwcsName = NULL ;
                pstatstg->type = STGTY_STREAM ;
                (pstatstg->cbSize).QuadPart = (ULONGLONG) buffersize ;
                pstatstg->ctime.dwHighDateTime = 0 ;
                pstatstg->ctime.dwLowDateTime = 0 ;
                pstatstg->mtime =pstatstg->ctime ;
                pstatstg->atime =pstatstg->ctime ;
                pstatstg->grfMode = 0 ;
                pstatstg->grfLocksSupported=LOCK_WRITE;
                pstatstg->clsid = CLSID_NULL ;
                pstatstg->grfStateBits=0;
                pstatstg->reserved=0;
                return S_OK ;
            }
            return STG_E_INVALIDPOINTER;
	}
    
    HRESULT STDMETHODCALLTYPE Clone( 
		/* [out] */ IStream **ppstm) {
			return E_NOTIMPL ;
	}
	
	// ISequentialStream

	HRESULT STDMETHODCALLTYPE Read( 
            /* [length_is][size_is][out] */ void *pv,
            /* [in] */ ULONG cb,
			/* [out] */ ULONG *pcbRead) {
		int asize = buffersize-pointer ;
		if( asize<=0 || ref<=0 ) {
			if( pcbRead ) {
				*pcbRead = 0 ;
			}
			return S_FALSE ;
		}
		if( (ULONG)asize > cb ) {
			asize = (int) cb ;
		}
		memcpy( pv, buffer+pointer, asize );
		pointer+=asize ;
		if( pcbRead ) {
			*pcbRead = asize ;
		}
		return S_OK ;
	}
        
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Write( 
            /* [size_is][in] */ const void *pv,
            /* [in] */ ULONG cb,
			/* [out] */ ULONG *pcbWritten){
				if( pcbWritten ) {
					*pcbWritten = 0 ;
				}
			return STG_E_CANTSAVE ;
	}

	// IUnknown interface

     HRESULT STDMETHODCALLTYPE QueryInterface( 
                /* [in] */ REFIID riid,
				/* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject) {
         *ppvObject = NULL;
         if (::IsEqualGUID(riid, IID_IUnknown) ||
             ::IsEqualGUID(riid, IID_ISequentialStream) ||
             ::IsEqualGUID(riid, IID_IStream))
         {
             *ppvObject = (void*)(IStream*)this;
             AddRef();
             return S_OK;
         }
         return E_NOINTERFACE;
     }
            
	 virtual ULONG STDMETHODCALLTYPE AddRef( void) {
		 return ++ref ;
	 }
            
	 virtual ULONG STDMETHODCALLTYPE Release( void) {
         if( --ref==0 ) {
             delete this ;
             return 0;
         }
         return ref ;
     }

};

class WaitCursor 
{
protected:
	HCURSOR m_savedcursor ;
public:
	WaitCursor(){
		m_savedcursor = ::SetCursor(LoadCursor(NULL,(LPCTSTR)IDC_WAIT));
		ShowCursor(TRUE);
	}
	~WaitCursor(){
		ShowCursor(FALSE);
		if( m_savedcursor ) {
			::SetCursor(m_savedcursor);
		}
	}
};

struct dvrtime * time_timedvr(SYSTEMTIME * psystime, struct dvrtime * dt);
SYSTEMTIME * time_dvrtime(struct dvrtime * dt, SYSTEMTIME * psystime);
int time_compare(struct dvrtime & t1, struct dvrtime &t2);
int operator- ( struct dvrtime &t1, struct dvrtime &t2) ;
void time_add(struct dvrtime &t, int seconds);
struct dvrtime time_now();

char * getapppath();
// char * getappname();
int getfilepath(string & appfile);

#define PORTABLE_FILE_TAG1	(0x170cb1a6)
#define PORTABLE_FILE_TAG2	(0x92489386)
#define PORTABLE_FILE_TAG3	(0x764db810)

int copyportablefiles(char * targetfile, char * password);
int isportableapp();
HRESULT CreateLink( LPCSTR PathObj, LPCSTR PathLink, LPCSTR Desc) ;

// load an image to CImage
// int	loadimg( CImage * pimg, LPCTSTR resource );
Bitmap * loadimage( LPCTSTR resource );
Bitmap * loadbitmap( LPCTSTR resource );
int savebitmap( LPCTSTR savefilename, Bitmap* pbmp  );
int openhtmldlg( char * servername, char * page );
int	openfolder(HWND hparent, string &folder);

int reg_save( char * appname, char * name, char * value );
// save integer
int reg_save( char * appname, char * name, int value );
int reg_read( char * appname, char * name, string &value );
// read integer value, for error return -1 ;
int reg_read( char * appname, char * name );

// over loaded registrey functions
int reg_save( char * name, char * value );
// save integer
int reg_save( char * name, int value );
int reg_read( char * name, string &value );
// read integer value, for error return -1 ;
int reg_read( char * name );

// options file op
void * option_open();
void option_close( void * op );
string * option_get( void * op, char * key );
string * option_getvalue( char * key );

// save image support
int GetEncoderClsid(LPCTSTR filename, CLSID* pClsid);

#ifdef _DEBUG
void dbgout( char * format, ... ) ;
#else
inline void dbgout(...){}
#endif

#endif      // __UTIL_H__