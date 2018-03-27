// dvrclient.h : main header file for the PROJECT_NAME application
//

#pragma once

#include "resource.h"		// main symbols

// Customized components
#include "../common/cwin.h"
#include "../common/cstr.h"

// No login screen
// #define NOLOGIN

// TVS project
#define  APP_TVS

#define APPNAME	"PWViewer"

#include "../common/tvsid.h"

// print 2 screen in one snapshot
#define TVS_SNAPSHOT2

// Snap Shot button capture all image silently
// #define CAPTUREALL 

// No "play archive" menu item
// #define NOPLAYARCHIVE
// #define NOPLAYLIVE
#define NOPLAYSMARTSERVER

// CdvrclientApp:
// See dvrclient.cpp for the implementation of this class
//

#define NEWPAINTING

#define	SCREENBACKGROUNDCOLOR	( RGB(180, 180, 180) )

// PWII project
#define APP_PWII

#define APP_PWII_FRONTEND

#define APP_PW_TABLET
