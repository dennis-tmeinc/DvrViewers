// dvrclient.h : main header file for the PROJECT_NAME application
//

#pragma once

#include "resource.h"		// main symbols

// Customized components
#include "cwin.h"
#include "cstr.h"

// No login screen
// #define NOLOGIN

// TVS project
#define  APP_TVS

#define  APP_PWVIEWER

// check disk volume for NON SECURE PWViewer
#define  APP_PWVIEWER_VOLUMECHECK

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

