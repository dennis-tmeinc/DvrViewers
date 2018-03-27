// SelectDvr.cpp : implementation file
//
#include "stdafx.h"

#include <Shlobj.h>

#include "dvrclient.h"
#include "SelectCamera.h"
#include "RemoteIpDlg.h"
#include "decoder.h"
#include "../common/util.h"

SelectCamera::SelectCamera() 
{
    int i=0; 
    for( i=0; i<MAXSCREEN; i++ ) {
        m_decoder[i] = -1 ;
        m_channel[i] = -1 ;
        m_used[i] = 0;
        m_selected[i] = 0 ;
    }
    // select first channel by default
    m_selected[0] = 1 ;
}

int SelectCamera::OnInitDialog()
{
    string camname ;
    int i ;
    int idx ;
    for( i=0; i<MAXSCREEN; i++ ) {
        if( m_used[i] ) {
            camname = m_cameraname[i] ;
        	idx = SendMessage( GetDlgItem(m_hWnd, IDC_LIST_CAMERALIST),  LB_ADDSTRING, 0, (LPARAM)(LPCTSTR)camname );
        	SendMessage( GetDlgItem(m_hWnd, IDC_LIST_CAMERALIST),  LB_SETITEMDATA, idx, (LPARAM)i );
           	SendMessage( GetDlgItem(m_hWnd, IDC_LIST_CAMERALIST),  LB_SETSEL, m_selected[i], (LPARAM)i );
            m_selected[i] = 0 ;
        }
    }

    SetTimer( m_hWnd, 101, 30000, NULL );

    return TRUE;
}

int SelectCamera::OnTimer()
{
    OnOK();
    return TRUE ;
}

void SelectCamera::PreCommand()
{
    SetTimer( m_hWnd, 101, 30000, NULL );
}

int SelectCamera::OnOK()
{
    int i ;
    int idx ;
    for( i=0; i<MAXSCREEN; i++ ) {
        if( SendMessage( GetDlgItem(m_hWnd, IDC_LIST_CAMERALIST),  LB_GETSEL, i, 0 )>0 ) {
            idx = SendMessage( GetDlgItem(m_hWnd, IDC_LIST_CAMERALIST),  LB_GETITEMDATA, i, 0 );
            if( idx>=0 && idx<MAXSCREEN ) {
                m_selected[idx] = 1 ;
            }
            else {
                break ;
            }
        }
    }
    return EndDialog( IDOK );
}

