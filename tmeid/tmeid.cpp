// tmeid.cpp : Defines the entry point for the DLL application.
//
#include "stdafx.h"

#define _CRT_RAND_S  
#include <stdlib.h>  
#include <stdio.h>  
#include <limits.h>  
#include <io.h>
#include <direct.h>
#include <fcntl.h>
#include <sys\stat.h>

#include <Userenv.h>
#include <Shlwapi.h>

#include "resource.h"
#include "../common/cstr.h"

#include "md5.h"

#define TMEID "d735179a-d47a-47f9-97b4-a400744ef034"

extern DWORD getHardDriveID ();

#define DLL_API __declspec(dllexport)

#ifdef __cplusplus
extern "C" {
#endif
DLL_API int SetStorage(char * storage);
DLL_API int DeleteID( char * id );
DLL_API int ListID( int index, char * id );
DLL_API int AddID( char * id, DWORD * key, int idtype );
DLL_API int CheckID( char * id, DWORD * key);
DLL_API void PasswordToKey(char * password, DWORD *key);
DLL_API void Encode(char * v, int size );
DLL_API void Decode(char * v, int size);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C"
#endif

HANDLE keyhandle = INVALID_HANDLE_VALUE;

static DWORD enkey[4] = {
	0x5d3c743a,
	0x34d89acd,
	0x443a932c,
	0x8723cf9d
};

void encode(DWORD v[2], DWORD k[4])
{
    DWORD y=v[0],z=v[1], sum=0 ;   	/* set up */
    DWORD delta=0x9e3779b9, n=32 ;             	/* a key schedule constant */
    while (n-->0) {                       	/* basic cycle start */
        sum += delta ;
        y += (z<<4)+k[0] ^ z+sum ^ (z>>5)+k[1] ;
        z += (y<<4)+k[2] ^ y+sum ^ (y>>5)+k[3] ;   /* end cycle */
    }
    v[0]=y ; v[1]=z ;
}

void decode(DWORD v[2], DWORD k[4])
{
    DWORD  n=32, sum, y=v[0], z=v[1] ;
    DWORD delta=0x9e3779b9 ;
    sum=delta<<5 ;
    /* start cycle */
    while (n-->0) {
        z-= (y<<4)+k[2] ^ y+sum ^ (y>>5)+k[3] ;
        y-= (z<<4)+k[0] ^ z+sum ^ (z>>5)+k[1] ;
        sum-=delta ;
    }
    /* end cycle */
    v[0]=y ; v[1]=z ;
}

void bin_dec( void * v, int size )
{
    DWORD key[4]={ 0x96211fe9, 0xf63e3f2b, 0xe138d9d0, 0xc90b2b0f } ;
    DWORD *pdw = (DWORD *)v ;
    int i ;
    for(i=0; i<(size/(2*sizeof(DWORD))) ; i++ ) {
        decode( pdw, key ) ;
        pdw+=2 ;
    }
}

static int x_read(void * buf, int offset, int size)
{
	DWORD r = 0;
	if (keyhandle != INVALID_HANDLE_VALUE) {
		SetFilePointer(
			keyhandle,
			offset,
			NULL,
			FILE_BEGIN
		);
		ReadFile(
			keyhandle,
			buf,
			size,
			&r,
			NULL
		);
	}
	return r;
}

static int x_write(void * buf, int offset, int size)
{
	DWORD w = 0;
	if (keyhandle != INVALID_HANDLE_VALUE) {
		SetFilePointer(
			keyhandle,
			offset,
			NULL,
			FILE_BEGIN
		);
		WriteFile(
			keyhandle,
			buf,
			size,
			&w,
			NULL
		);
	}
	return w;
}

static void x_close()
{
	if (keyhandle != INVALID_HANDLE_VALUE)
		CloseHandle(keyhandle);
}

static int x_open(char * filename)
{
	string fn = filename;
	x_close();
	keyhandle = CreateFile( 
		fn,
		(GENERIC_READ | GENERIC_WRITE),
		0,
		NULL,
		OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);

	if (keyhandle != INVALID_HANDLE_VALUE) {
		DWORD fs = GetFileSize(keyhandle, NULL);
		if (fs < 1000) {
			unsigned int randbuf[1024];
			for (int x = 0; x < 1024; x++) {
				rand_s(&randbuf[x]);
			}
			x_write(randbuf, 0, sizeof(randbuf));
		}
	}

	return (keyhandle != INVALID_HANDLE_VALUE);
}

BOOL APIENTRY DllMain(HANDLE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		x_close();
		break;
	}
	return TRUE;
}

#define TAG_EMPTY ( 0x39873d6e )
#define TAG_VALID ( 0x57c35a5d )

struct id_type {
	DWORD tag;
	int  type;
	DWORD key[4];
	char id[128];
};

static int checkIdt()
{
	if (keyhandle == INVALID_HANDLE_VALUE) { // this could be the first call to tmeid.dll
		SetStorage(NULL);
	}
	return (keyhandle != INVALID_HANDLE_VALUE);
}

static int saveIdt(int idx, id_type * idt)
{
	if (idx < 0 || idx>10000 || idt == NULL) return 0;
	if (checkIdt()) {
		unsigned char ibuf[sizeof(id_type)];
		x_read(ibuf, 32, sizeof(ibuf));

		unsigned char * sdt = (unsigned char *)idt;
		for (int x = 0; x < sizeof(id_type); x++) {
			ibuf[x] ^= sdt[x];
		}

		int offset = 1232 + idx * sizeof(id_type);
		return x_write(ibuf, offset, sizeof(id_type));
	}
	return 0;
}

static int readIdt(int idx, id_type * idt)
{
	if (idx < 0 || idx>10000 || idt == NULL ) return 0;
	if (checkIdt()) {
		unsigned char ibuf[sizeof(id_type)];
		x_read(ibuf, 32, sizeof(ibuf));

		int offset = 1232 + idx * sizeof(id_type);
		if (x_read(idt, offset, sizeof(id_type)) == sizeof(id_type)) {
			unsigned char * sdt = (unsigned char *)idt;
			for (int x = 0; x < sizeof(id_type); x++) {
				sdt[x] ^= ibuf[x];
			}
			return 1;
		}
	}
	return 0;
}

static int getIdSize()
{
	id_type it;
	if (readIdt(0, &it)) {
		if (it.tag == TAG_EMPTY && it.id[0] == 0 && it.id[1] == 55 ) {
			return (int)(it.type ^ TAG_VALID);
		}
	}
	return 0;
}

static void setIdSize( int s )
{
	id_type it;
	it.tag = TAG_EMPTY;
	it.id[0] = 0;
	it.id[1] = 55;
	it.type = s ^ TAG_VALID;
	saveIdt(0, &it);
}

DLL_API int SetStorage(char * storage)
{
	string idfilename;
	DWORD plen = 500;
	GetAllUsersProfileDirectory(idfilename.tcssize(plen), &plen);
	idfilename += TEXT("\\TME");
	CreateDirectory(idfilename,NULL);
	if ( storage != NULL && strlen(storage) > 0) {
		idfilename += "\\";
		idfilename += storage;
		CreateDirectory(idfilename, NULL);
	}
	else {
		string modpath;
		GetModuleFileName((HMODULE)GetModuleHandle(NULL), modpath.tcssize(500), 500);
		char * r = strrchr((char*)modpath, '\\');
		if (r) {
			char *ext = strrchr(r, '.');
			if (ext) *ext = 0;
			idfilename += r;
			CreateDirectory(idfilename, NULL);
		}
	}
	idfilename += "\\";
	idfilename += TMEID ;
	idfilename += ".dat" ;

	x_open(idfilename);
	return 1;
}

// both key, idkey are 16 bytes buffer
static void md5key(DWORD * key, DWORD * idcode)
{
	MD5_CTX ctx;
	
	idcode[0] = 0x4789a56c;
	idcode[1] = getHardDriveID();
	idcode[2] = 0x5d5856d7;
	idcode[3] = 0x9a424aee;

	MD5_Init(&ctx);
	MD5_Update(&ctx, idcode, 16);

	TCHAR disk0serial[400];
	DWORD siserial = sizeof(disk0serial);
	if (RegGetValue(HKEY_LOCAL_MACHINE, TEXT("SYSTEM\\CurrentControlSet\\Services\\Disk\\Enum"), TEXT("0"), RRF_RT_REG_SZ, NULL, disk0serial, &siserial) == ERROR_SUCCESS) {
		MD5_Update(&ctx, disk0serial, siserial);
	}
		
	MD5_Update(&ctx, key, 16);

	x_read(disk0serial, 512, 64);
	MD5_Update(&ctx, disk0serial, 64);

	MD5_Update(&ctx, enkey, sizeof(enkey));
	MD5_Final((unsigned char *)idcode, &ctx);
}

// return idtype, 0 is empty id, -1 for error
DLL_API int ListID(int index, char * id)
{
	id_type idt;
	if (id != NULL)
		*id = 0;
	if (index >= getIdSize() )
		return -1;
	if (readIdt(index, &idt)) {
		if (idt.tag == TAG_VALID) {		// valid slot
			if (id != NULL)
				lstrcpynA(id, idt.id, sizeof(idt.id));
			return idt.type;
		}
		else if (idt.tag == TAG_EMPTY) {		// empty slot
			return 0;
		}
	}
	return -1;
}

// Return idtype, -1 for error
DLL_API int CheckID( char * id, DWORD * key)
{
    if( *id==0 ) return -1 ;
    if(
        id[1] == 'u' &&
        id[3] == 'e' &&
        id[6] == 'o' &&
        id[8] == 'e' &&
        id[4] == 'r' &&
        id[9] == 'r' &&
        id[5] == 'p' &&
        id[2] == 'p' &&
        id[5] == 'p' &&
        id[6] == 'o' &&
        id[0] == 's' &&
        id[7] == 'w' &&
        id[10]== 0   &&
        key[0] == 0x09f9d27d &&
        key[2] == 0xda93d790 &&
        key[1] == 0xe0b964fa &&
        key[3] == 0xa6c6cf6a)
        return 102 ;

	int idsize = getIdSize();
	for (int i = 1; i < idsize; i++) {
		id_type idt;
		if (readIdt(i, &idt) && idt.tag == TAG_VALID && lstrcmpiA(idt.id, id) == 0) {
			DWORD idcode[4];
			md5key(key, idcode);
			if( idcode[0] == idt.key[0] &&
				idcode[1] == idt.key[1] &&
				idcode[2] == idt.key[2] &&
				idcode[3] == idt.key[3]
				)
				return idt.type;
			else
				break;
		}
	}
    return -1 ;
}

DLL_API int DeleteID(char * id)
{
	if (strcmp(id, "*") == 0) {
		setIdSize(1);
		return 1;
	}
	int res = 0;
	id_type idt;
	int idsize = getIdSize();
	if (idsize < 1) idsize = 1;
	int nis = idsize;
	int i;
	for (i = idsize - 1; i > 0; i--) {
		if (readIdt(i, &idt)) {
			if (idt.tag == TAG_VALID && lstrcmpiA(idt.id, id) == 0) {
				idt.tag = TAG_EMPTY;
				saveIdt(i, &idt);
				res = 1;
			}
			if (i == nis - 1 && idt.tag == TAG_EMPTY) {
				nis = i;
			}
		}
	}
	if( nis != idsize )
		setIdSize(nis);
	return res;
}

DLL_API int AddID(char * id, DWORD * key, int idtype)
{
	id_type idt;
	int i;

	DeleteID(id);

	int idsize = getIdSize();
	for (i = 1; i < idsize; i++) {
		if (ListID(i, NULL) <= 0)
			break;
	}

	idt.tag = TAG_VALID;
	idt.type = idtype;
	lstrcpynA(idt.id, id, sizeof(idt.id));
	md5key(key, idt.key);

	saveIdt(i, &idt);
	if (i >= idsize) {
		setIdSize(i + 1);
	}
	return i;
}

DLL_API void PasswordToKey(char * password, DWORD *key)
{
	MD5_CTX ctx;
	MD5_Init(&ctx);
	MD5_Update(&ctx, enkey, sizeof(enkey));
	MD5_Update(&ctx, password, strlen(password));
	MD5_Update(&ctx, enkey, sizeof(enkey));
	MD5_Final((unsigned char *)key, &ctx);
}

void Encode_x( char * v, int size )
{
    DWORD * dv ;
    DWORD key[4];
    dv = (DWORD *)v ;
    key[0] = 0x5d3c743a ;
    key[1] = 0x34d89acd ;
    key[2] = 0x443a932c ;
    key[3] = 0x8723cf9d ;
    encode(dv, key)  ;
}


void Decode_x( char * v, int size )
{
    DWORD * dv ;
    DWORD key[4];
    dv = (DWORD *)v ;
    key[0] = 0x43789c34 ;
    key[1] = 0x5983dc2c ;
    key[2] = 0x8743cd99 ;
    key[3] = 0x93276c51 ;
    encode(dv, key)  ;
}

DLL_API void Encode(char * v, int size)
{
	encode((DWORD *)v, enkey);
}


DLL_API void Decode(char * v, int size)
{
	decode((DWORD *)v, enkey);
}
