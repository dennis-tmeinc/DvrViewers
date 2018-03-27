#pragma once

#include "PWManager.h"

#include "../common/cstr.h"
#include "../common/cwin.h"

// RemoteIpDlg dialog

class DlgSrcId : public Dialog
{
public:
    string m_curId ;
	string m_newId;
protected:
    virtual int OnInitDialog()
	{
        SetDlgItemText( m_hWnd, IDC_TEXT_OLDID, m_curId );
        SetDlgItemText( m_hWnd, IDC_EDIT_NEWID, m_newId );
        return TRUE;
	}

    virtual int OnOK()
    {
        GetDlgItemText(m_hWnd, IDC_EDIT_NEWID, m_newId.tcssize(512), 512 );
		return EndDialog(IDOK);
    }
};
