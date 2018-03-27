// dvrclient.h : main header file for the PROJECT_NAME application
//

#pragma once

#include "resource.h"		// main symbols

// Customized components
#include "../common/cstr.h"
#include "../common/cwin.h"

#define SPARTAN_APP 1
#define APPNAME "SpartanViewer"

// No login screen
// #define NOLOGIN

// Snap Shot button capture all image silently
// #define CAPTUREALL 

// No "play archive" menu item
// #define NOPLAYARCHIVE
#define NOPLAYSMARTSERVER

// Snap shot save as 704x480 resolutions
#define BIGSNAPSHOT

// CdvrclientApp:
// See dvrclient.cpp for the implementation of this class
//

// use GDI+ for painting
#define NEWPAINTING
