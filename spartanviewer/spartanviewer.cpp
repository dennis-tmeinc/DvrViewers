// spartanviewer.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "spartanviewer.h"
#include "dvrclient.h"
#include "dvrclientDlg.h"

struct btbar_ctl btbar_ctls[] = {
    {IDC_MIXAUDIO, 1, _T("AUDIO_MIXALL") },
    {IDC_PIC_SPEAKER_VOLUME, 3, _T("VOLUME_0")},
    {IDC_SLIDER_VOLUME, 1, NULL },
    {IDC_SLOW, 1, _T("PLAY_SLOW") },
    {IDC_BACKWARD, 1, _T("PLAY_BACKWARD") },
    {IDC_PAUSE, 1, _T("PLAY_PAUSE") },
    {IDC_PLAY, -1000, _T("PLAY_PLAY") },
    {IDC_FORWARD, 1, _T("PLAY_FORWARD") },
    {IDC_FAST, 1, _T("PLAY_FAST") },
    {IDC_STEP, 1, _T("PLAY_STEP") },
    {IDC_MAP,      2, _T("MAP_ENABLE") },
    {IDC_SPARTAN_1, 3, _T("PLAY_SPARTAN_1") },
    {IDC_SPARTAN_2, 1, _T("PLAY_SPARTAN_2") },
    {IDC_SPARTAN_4, 1, _T("PLAY_SPARTAN_4") },
    {0, 0, NULL }
} ;

const int control_align = 1 ;       // control layout, 0: left, 1: right ;
const int btbar_refid = IDC_MIXAUDIO;	// Volue Slider as ref
const int btbar_align = 4;		// 0: center >0: left align, <0: right align 

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

   	DvrclientDlg dlg ;
	dlg.DoModal();

	return 0 ;
}
