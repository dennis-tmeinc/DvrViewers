// tvsid.h : Defines tvs key structure 
//

#ifndef __TVSID_H__
#define __TVSID_H__

#include "crypt.h"

struct tvskey_data {
	char checksum[128] ;        // md5 check of key data (use 16 bytes only). calculated from field "size"
    int size ;                  // total file size exclude the checksum part
    int version ;                // 100
    char tag[4] ;                // "TVS"
    char usbid[32] ;             // MFnnnn, INnnnn, PLnnnn
    char usbkeyserialid[512] ;   // Example: Ven_OCZ&Prod_RALLY2&Rev_1100\AA04012700017614
    char videokey[512] ;         // TVS video encryption key, only 256 bytes used.
    char manufacturerid[32] ;    // MFnnnn
    int counter ;                // Installer key counter
    int keyinfo_start ;          // offset of key text information
    int keyinfo_size ;           // size of key text information
    char mfpassword[512] ;       // encrypted manufacturer key password, 256 bytes used.
    char plpassword[512] ;       // encrypted police key password, 256 bytes used.
    char inpassword[512] ;       // encrypted installer key password, 256 bytes used.
    char ispassword[512] ;       // encrypted inspector key password, 256 bytes used.
    char oppassword[512] ;       // encrypted operator key password, 256 bytes used.
   // followed by key text information
} ;

extern struct tvskey_data * tvskeydata ;

struct tvskey_data * tvs_readtvskey();
int tvs_checktvskey();
void md5_checksum( char * checksum, unsigned char * data, unsigned int datalen );

struct tvs_info {
	char serverinfo[48] ;
	char productid[8] ;
	char ivcs[24] ;
	char medallion[24] ;
	char lincenseplate[24] ;
} ;

#endif      // __TVSID_H__

