#pragma once

#include "../common/cwin.h"
#include "../common/cstr.h"

// CallTmeDlg dialog

class CallTmeDlg : public Dialog
{
public:
	string m_code;

protected:
    virtual int OnInitDialog()
	{
        SetDlgItemText(m_hWnd, IDC_CODE, m_code );
		return TRUE;
	}
};
