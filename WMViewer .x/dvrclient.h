// dvrclient.h : main header file for the PROJECT_NAME application
//

#pragma once

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")


#include "resource.h"		// main symbols

#define DVRVIEWER_APP

// WMVIEWER, start from DVRViewer
#define WMVIEWER_APP

// No login screen
// #define NOLOGIN

// Snap Shot button capture all image silently
// #define CAPTUREALL 

// No "play archive" menu item
// #define NOPLAYARCHIVE
#define NOPLAYSMARTSERVER

// Snap shot save as 704x480 resolutions
#define BIGSNAPSHOT

#define APPNAME "WMVIEWER"

#define NEWPAINTING

// for WMViwer, request of about 3 seconds jumps
#define FORWARD_JUMP   (5)
#define BACKWARD_JUMP  (5)

