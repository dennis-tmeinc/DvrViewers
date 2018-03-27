// dvrclient.h : main header file for the PROJECT_NAME application
//

#pragma once

#include "resource.h"		// main symbols

// Customized components
#include "../common/cstr.h"
#include "../common/cwin.h"

// No login screen
// #define NOLOGIN

// TVS project
#define APP_TVS
#include "../common/tvsid.h"
extern struct tvskey_data * tvskeydata ;

// PWII project
#define APP_PWII

#define APPNAME "PWViewer"

// print 2 screen in one snapshot
#define TVS_SNAPSHOT2

// Snap Shot button capture all image silently
// #define CAPTUREALL 

// No "play archive" menu item
// #define NOPLAYARCHIVE
// #define NOPLAYLIVE
#define NOPLAYSMARTSERVER

#define NEWPAINTING
