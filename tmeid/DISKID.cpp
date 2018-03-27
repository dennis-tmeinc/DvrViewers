// DISKID.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <Windows.h>
#include <Userenv.h>
#include <Shlwapi.h>

#include <stdio.h>

#include "../common/cstr.h"

DWORD getHardDriveID ()
{
   static DWORD serialno=0;

//   serialno = ReadDiskId();
//   if( serialno==0 ) {		// if can't get harddrive serial no, get volume serial number.
//   }

   if (serialno == 0) {
	   serialno = 0x7788;
	   string Alluserprofile;
	   DWORD plen = 500;
	   GetAllUsersProfileDirectory(Alluserprofile.tcssize(plen), &plen);
	   TCHAR * tdisk = (TCHAR *)Alluserprofile;
	   tdisk[3] = 0;
	   GetVolumeInformation(tdisk, NULL, 0, &serialno, NULL, NULL, NULL, 0);
   }
   return serialno;
}
