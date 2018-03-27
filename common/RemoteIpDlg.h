#pragma once

#include "../common/cstr.h"
#include "../common/cwin.h"

// RemoteIpDlg dialog

class RemoteIpDlg : public Dialog
{
public:
	string m_ipaddr;
protected:
    virtual int OnInitDialog()
    {
        SetDlgItemText(m_hWnd, IDC_EDIT_IPADDR, m_ipaddr);
        return TRUE;
    }
    virtual int OnOK()
    {
        TCHAR ip[512] ;
        GetDlgItemText(m_hWnd, IDC_EDIT_IPADDR, ip, 512 );
        m_ipaddr = ip ;
		return EndDialog(IDOK);
    }
};
