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

// PR1ME WITNESS Viewer on top of TVS VIEWER
#define  APP_TVS_PR1ME

// No Live View for Pr1me Viewer
#define NOPLAYLIVE

// #define  APP_TVS_NEW_ORLEANS

#define APPNAME	"Pr1meWitnessViewer"

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


// PAL screen resolution support (remember to remove it)
// #define PAL_SCREEN_SUPPORT 1