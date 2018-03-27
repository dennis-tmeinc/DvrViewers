
#include "cwin.h"

#include "dvrclient.h"
#include "dvrclientDlg.h"

#ifndef __TAGEVENT_H__
#define __TAGEVENT_H__

static char * trimtail( char * str )
{
	int l = strlen( str );
	while( l>0 ) {
		if( str[l-1] > ' ' ) break ;
		str[l-1] = 0 ;
		l-- ;
	}
	return str ;
}

// officer id dialog
class TagEventDialog : public Dialog {
public:
	string oid ;
	string vrilist ;
	int		vrisize ;
	int		recsize ;
	int		exsel ;

protected:

	virtual int OnInitDialog()
    {
		string s ;

		vrisize = g_decoder[0].getvrilistsize( & recsize ) ;
		vrisize *= recsize ;
		vrisize = g_decoder[0].getvrilist( vrilist.strsize(vrisize+600), vrisize+600 ) ;
		if( vrisize <= 0 ) {
			EndDialog( IDCANCEL );
			return TRUE ;
		}

		int o ;
		int i ;

		// init vri list
		for( o=0; o<vrisize; o+=recsize ) {
			strncpy( s.strsize(65), ((char *)vrilist)+o, 64 );
			((char *)s)[64]=0 ;
			i = SendDlgItemMessage( m_hWnd, IDC_LIST_TAGVRI, LB_ADDSTRING, (WPARAM)0, (LPARAM)(TCHAR *)s );
			SendDlgItemMessage( m_hWnd, IDC_LIST_TAGVRI, LB_SETITEMDATA, (WPARAM)i, (LPARAM)(o/recsize) );
		}

		// init tag list
		char * classifications [] = {
			"",
			"DUI",
			"Motor assist",
			"Drub Seizure",
			"Suspicious Vehicle",
			"Domestic call",
			"Assault",
			"Evading",
			"Traffic Warning",
			"Traffic Citation",
			"Traffic Accident",
			"Felony",
			"Misdemeanor",
			NULL
		} ;

		for( i=0; i<50; i++ ) {
			if( classifications[i]==NULL ) break ;
			s = classifications[i] ;
			SendDlgItemMessage( m_hWnd, IDC_COMBO_TAGEVENT, CB_ADDSTRING, (WPARAM)0, (LPARAM)(TCHAR *)s );
		}

		exsel = -1 ;

		return TRUE ;
	}	

	// save single vri to vrilist
	int SaveVriItem()
	{
		char * item ;
		int l;
		string s ;
		if( exsel>=0 ) {
			int o =	(int)SendDlgItemMessage( m_hWnd, IDC_LIST_TAGVRI, LB_GETITEMDATA, (WPARAM)exsel, (LPARAM)0 );
			// save all other fields
			item=((char *)vrilist)+o*recsize ;

			// tag event
			SendDlgItemMessage( m_hWnd, IDC_COMBO_TAGEVENT, WM_GETTEXT, (WPARAM)32, (LPARAM)(TCHAR *)s.tcssize(80) );
			l=strlen((char *)s);
			if( l>32 ) l = 32 ;
			memset( item+64, ' ', 32 );
			strncpy( item+64, (char *)s, l );

			// case number
			SendDlgItemMessage( m_hWnd, IDC_EDIT_TAGCASE, WM_GETTEXT, (WPARAM)64, (LPARAM)(TCHAR *)s.tcssize(80) );
			l=strlen((char *)s);
			if( l>64 ) l = 64 ;
			memset( item+64+32, ' ', 64 );
			strncpy( item+64+32, (char *)s, l );

			// notes
			SendDlgItemMessage( m_hWnd, IDC_EDIT_TAGNOTE, WM_GETTEXT, (WPARAM)255, (LPARAM)(TCHAR *)s.tcssize(300) );
			l=strlen((char *)s);
			if( l>255 ) l = 255 ;
			memset( item+64+32+64, ' ', 255 );
			strncpy( item+64+32+64, (char *)s, l );


		}
		return exsel ;
	}

	int OnSaveVri()
	{
		SaveVriItem() ;
		int res = g_decoder[0].setvrilist( vrilist, vrisize ) ;
		if( res>=0 ) {
			MessageBox(m_hWnd, _T("VRI Information Saved!"), _T(""), MB_OK );
		}
		return TRUE ;
	}

	void selectRec( int r )
	{
		string s ;
		char * item = ((char *)vrilist) + (r*recsize) ;

		// vri: 64 bytes

		// tag event, 32 bytes
		strncpy( s.strsize(40), item+64, 32 ) ;
		((char *)s)[32] = 0 ;
		trimtail( (char *)s ) ;
		//ComboBox_SetText( GetDlgItem(m_hWnd, IDC_COMBO_TAGEVENT), (TCHAR *)s );
		SendDlgItemMessage( m_hWnd, IDC_COMBO_TAGEVENT, WM_SETTEXT, (WPARAM)0, (LPARAM)(TCHAR *)s );

		// case number, 64 bytes
		strncpy( s.strsize(68), item+96, 64 ) ;
		((char *)s)[64] = 0 ;
		trimtail( (char *)s ) ;
		SendDlgItemMessage( m_hWnd, IDC_EDIT_TAGCASE, WM_SETTEXT, (WPARAM)0, (LPARAM)(TCHAR *)s );

		// Priority, 20 bytes,

		// Officer ID: 32 bytes,

		// notes, 255 bytes + <return>
		strncpy( s.strsize(300), item+64+32+64+20+32 , 255 ) ;
		((char *)s)[255] = 0 ;
		trimtail( (char *)s ) ;
		SendDlgItemMessage( m_hWnd, IDC_EDIT_TAGNOTE, WM_SETTEXT, (WPARAM)0, (LPARAM)(TCHAR *)s );

	}

	int OnListVriEvent( int Event )
	{
		int i, o ;
		char * item ;
		string s ;
		switch( Event ) {
		case LBN_SELCHANGE:
			SaveVriItem() ;
			i = SendDlgItemMessage( m_hWnd, IDC_LIST_TAGVRI, LB_GETCURSEL, 0, 0 );
			exsel = i ;
			i =	(int)SendDlgItemMessage( m_hWnd, IDC_LIST_TAGVRI, LB_GETITEMDATA, (WPARAM)i, (LPARAM)0 );
			// set all other fields
			selectRec( i ) ;

			break;

		default :
			return FALSE ;
		}
		return TRUE ;
	}

	virtual int OnCommand( int Id, int Event ) 
	{
        switch (Id)
        {
        case IDOK:
			return OnOK();

        case IDCANCEL:
            return OnCancel();

        case IDC_BUTTON_SAVEVRI:
            return OnSaveVri();

		case IDC_LIST_TAGVRI:
			return OnListVriEvent( Event );

		default:
            return FALSE ;

        }
        return FALSE;
    }

};


#endif