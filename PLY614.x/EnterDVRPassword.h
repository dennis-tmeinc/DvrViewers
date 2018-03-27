#pragma once


// CEnterDVRPassword dialog

class CEnterDVRPassword : public CDialog
{
	DECLARE_DYNAMIC(CEnterDVRPassword)

public:
	CEnterDVRPassword(CWnd* pParent = NULL);   // standard constructor
	virtual ~CEnterDVRPassword();

// Dialog Data
	enum { IDD = IDD_DIALOG_FILEPASSWORD };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	CString m_password1;
	CString m_password2;
};
