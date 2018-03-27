// EnterDVRPassword.cpp : implementation file
//

#include "stdafx.h"
#include "ply614.h"
#include "EnterDVRPassword.h"


// CEnterDVRPassword dialog

IMPLEMENT_DYNAMIC(CEnterDVRPassword, CDialog)

CEnterDVRPassword::CEnterDVRPassword(CWnd* pParent /*=NULL*/)
	: CDialog(CEnterDVRPassword::IDD, pParent)
	, m_password1(_T(""))
	, m_password2(_T(""))
{

}

CEnterDVRPassword::~CEnterDVRPassword()
{
}

void CEnterDVRPassword::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_PASSWORD1, m_password1);
	DDX_Text(pDX, IDC_EDIT_PASSWORD2, m_password2);
}


BEGIN_MESSAGE_MAP(CEnterDVRPassword, CDialog)
END_MESSAGE_MAP()


// CEnterDVRPassword message handlers
