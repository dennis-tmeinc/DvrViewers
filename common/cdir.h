
#ifndef __CDIR_H__
#define __CDIR_H__

#include "../common/cstr.h"

// a dir finding class

class dirfind {
protected:
    HANDLE m_dirhandle ;
    WIN32_FIND_DATA m_finddata ;
    string m_dir;
    string m_filename ;
    string m_filepath ;

	int _findnext()
	{
		if (m_dirhandle == NULL) {
			m_filepath = m_dir;
			m_filepath += m_filename;
			// m_filepath.printf("%s%s", (char *)m_dir, (char *)m_filename);
			m_dirhandle = FindFirstFile(m_filepath, &m_finddata);
			if (m_dirhandle == INVALID_HANDLE_VALUE) {
				m_dirhandle = NULL;
				return FALSE;
			}
		}
		else {
			if (!FindNextFile(m_dirhandle, &m_finddata)) {
				return FALSE;
			}
		}
		m_filename = m_finddata.cFileName;
		m_filepath = m_dir;
		m_filepath += m_filename;
		// m_filepath.printf("%s%s", (char *)m_dir, (char *)m_filename);
		return TRUE;
	}

public:
    dirfind( const char * dirpath=NULL, const char * filenamepattern = "*"){
        m_dirhandle=NULL ;
        open( dirpath, filenamepattern) ;
    }

    ~dirfind(){
        close();
    }

    void close() {
        if( m_dirhandle!=NULL ) {
            FindClose( m_dirhandle ) ;
            m_dirhandle=NULL ;
        }
    }

	void open(const char * dirpath = NULL, const char * filenamepattern = "*") {
		int l;
		close();
		if (dirpath && (l = lstrlenA(dirpath))>0) {
			m_dir = dirpath;
			if (dirpath[l - 1] != '\\' && dirpath[l - 1] != ':') {
				m_dir += "\\";
			}
		}
		else {
			m_dir.clean();
		}
		m_filename = filenamepattern;
	}

	int isdir() {
		return (m_finddata.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) != 0;
	}

	int isfile() {
		return !isdir();
	}

	int find() {
		while (_findnext()) {
			if (isfile() || m_finddata.cFileName[0] != '.') {
				return TRUE;
			}
		}
		return FALSE;
	}

	// find files only
	int findfile() {
		while( _findnext() ) {
			if (isfile()) {
				return TRUE;
			}
		}
		return FALSE;
	}

	// find dirs only
	int finddir() {
		while (_findnext()) {
			if (isdir() && m_finddata.cFileName[0] != '.' ) {
				return TRUE;
			}
		}
		return FALSE;
	}

	int find(const char * filenamepattern) {
		if (m_dirhandle == NULL) {
			m_filename = filenamepattern;
		}
		return find();
	}

    string & filepath(){
        return m_filepath ;
    }

    string & filename(){
        return m_filename ;
    }

    WIN32_FIND_DATA * finddata() {
        return & m_finddata ;
    }

};



#endif	// __CDIR_H__
