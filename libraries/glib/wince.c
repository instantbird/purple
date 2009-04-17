/***************************************************************
  wince.c (originally io_wce.c)

  author : uema2
  date   : Nov 30, 2002

  You can freely use, copy, modify, and redistribute
  the whole contents.
  
  Downloaded on https://trac.xiph.org/browser/trunk/oggdsf/src/lib/helper/wince/
  
  Modified by Quentin Raynaud for Instantbird. This is a 
  makeup of files found in the above source tree.
  
***************************************************************/

#include "wince.h"
#include "errno.h"

#define _O_RDONLY       0x0000  /* open for reading only */
#define _O_WRONLY       0x0001  /* open for writing only */
#define _O_RDWR         0x0002  /* open for reading and writing */

#define _O_CREAT        0x0100  /* create and open file */

/* char -> wchar_t */
static wchar_t*
wce_mbtowc(const char* a)
{
	int length;
	wchar_t *wbuf;

	length = MultiByteToWideChar(CP_ACP, 0,
		a, -1, NULL, 0);
	wbuf = (wchar_t*)malloc( (length+1)*sizeof(wchar_t) );
	MultiByteToWideChar(CP_ACP, 0,
		a, -1, wbuf, length);

	return wbuf;
}

int
_wcreat(const wchar_t *wfile, int mode)
{
	HANDLE h = (HANDLE)_wopen(wfile, mode);
	CloseHandle(h);
	
	return !!h;
}

int
_wunlink(const wchar_t *wfile)
{
	BOOL rc;

	rc = DeleteFileW(wfile);
	
	return rc == TRUE ? 0 : -1;
}

/* replace "open" with "CreateFile", etc. */
int
_wopen(const wchar_t *wfile, int mode, va_list arg)
{
	DWORD access = 0, share = 0, create = 0;
	HANDLE h;

	if( (mode & _O_RDWR) != 0 )
		access = GENERIC_READ|GENERIC_WRITE;
	else if( (mode & _O_RDONLY) != 0 )
		access = GENERIC_READ;
	else if( (mode & _O_WRONLY) != 0 )
		access = GENERIC_WRITE;

	if( (mode & _O_CREAT) != 0 )
		create = CREATE_ALWAYS;
	else
		create = OPEN_ALWAYS;

	h = CreateFileW(wfile, access, share, NULL,
			create, 0, NULL);

	return (int)h;
}

int _open(const char *file, int mode, va_list arg)
{
	wchar_t* wfile = wce_mbtowc(file);
	int ret = _wopen(wfile, mode, arg);
	free(wfile);
	return ret;
}

int
_read(int fd, void *buffer, int length)
{
	DWORD dw;
	ReadFile( (HANDLE)fd, buffer, length, &dw, NULL );
	return (int)dw;
}

int
_write(int fd, const void *buffer, unsigned count)
{
	DWORD dw;
	WriteFile( (HANDLE)fd, buffer, count, &dw, NULL );
	return (int)dw;
}

long
_lseek(int handle, long offset, int origin)
{
	DWORD flag, ret;

	switch(origin)
	{
	case SEEK_SET: flag = FILE_BEGIN;   break;
	case SEEK_CUR: flag = FILE_CURRENT; break;
	case SEEK_END: flag = FILE_END;     break;
	default:       flag = FILE_CURRENT; break;
	}

	ret = SetFilePointer( (HANDLE)handle, offset, NULL, flag );
	return ret==0xFFFFFFFF ? -1 : 0;
}

const __int64 _onesec_in100ns = (__int64)10000000;

/* __int64 <--> FILETIME */
static __int64
wce_FILETIME2int64(FILETIME f)
{
	__int64 t;

	t = f.dwHighDateTime;
	t <<= 32;
	t |= f.dwLowDateTime;
	return t;
}

/* FILETIME utility */
static FILETIME
wce_getFILETIMEFromYear(WORD year)
{
	SYSTEMTIME s={0};
	FILETIME f;

	s.wYear      = year;
	s.wMonth     = 1;
	s.wDayOfWeek = 1;
	s.wDay       = 1;

	SystemTimeToFileTime( &s, &f );
	return f;
}

/* FILETIME <--> time_t */
static time_t
wce_FILETIME2time_t(const FILETIME* f)
{
	FILETIME f1601, f1970;
	__int64 t, offset;

	f1601 = wce_getFILETIMEFromYear(1601);
	f1970 = wce_getFILETIMEFromYear(1970);

	offset = wce_FILETIME2int64(f1970) - wce_FILETIME2int64(f1601);

	t = wce_FILETIME2int64(*f);

	t -= offset;
	return (time_t)(t / _onesec_in100ns);
}

/* _findfirst, _findnext, _findclose. */
/* replace them with FindFirstFile, etc. */
long
_findfirst( const wchar_t *wfile, struct _wfinddata_t *fi )
{
	HANDLE h;
	WIN32_FIND_DATAW fda;

	h = FindFirstFileW( wfile, &fda );
	if( h==NULL )
	{
		errno = EINVAL; return -1;
	}

	fi->attrib = fda.dwFileAttributes;
	fi->time_create = wce_FILETIME2time_t( &fda.ftCreationTime );
	fi->time_access = wce_FILETIME2time_t( &fda.ftLastAccessTime );
	fi->time_write = wce_FILETIME2time_t( &fda.ftLastWriteTime );
	fi->size = fda.nFileSizeLow + (fda.nFileSizeHigh<<32);
	wcscpy(fi->name, fda.cFileName);

	return (long)h;
}

int
_findnext( long handle, struct _wfinddata_t *fi )
{
	WIN32_FIND_DATAW fda;
	BOOL b;

	b = FindNextFileW( (HANDLE)handle, &fda );

	if( b==FALSE )
	{
		errno = ENOENT; return -1;
	}

	fi->attrib = fda.dwFileAttributes;
	fi->time_create = wce_FILETIME2time_t( &fda.ftCreationTime );
	fi->time_access = wce_FILETIME2time_t( &fda.ftLastAccessTime );
	fi->time_write = wce_FILETIME2time_t( &fda.ftLastWriteTime );
	fi->size = fda.nFileSizeLow + (fda.nFileSizeHigh<<32);
	wcscpy(fi->name, fda.cFileName);

	return 0;
}

int
_findclose( long handle )
{
	BOOL b;
	b = FindClose( (HANDLE)handle );
	return b==FALSE ? -1 : 0;
}

int
_wchmod(const wchar_t *path, int mode)
{
	return 0;
}

int
_waccess(const wchar_t *filename, int flags)
{
	return 0777;
}

int
_wrmdir(const wchar_t * wdir)
{
	BOOL rc;

	/* replace with RemoveDirectory. */
	rc = RemoveDirectoryW(wdir);

	return rc==TRUE ? 0 : -1;
}

int
_wmkdir(const wchar_t * wdir)
{
	BOOL rc;

	/* replace with CreateDirectory. */
	rc = CreateDirectoryW(wdir, NULL);

	return rc==TRUE ? 0 : -1;
}

wchar_t *
_tfullpath(wchar_t * fullPath, const wchar_t * oldPath, unsigned maxSize)
{
	return wcsncpy(fullPath, oldPath, maxSize);
}

BOOL
MoveFileExW(LPCWSTR woldname, LPCWSTR wnewname, DWORD dwFlags)
{
	BOOL b;

	if( (dwFlags&MOVEFILE_REPLACE_EXISTING)!=0 )
		DeleteFileW( wnewname );

	b = MoveFileW( woldname, wnewname );

	return b;
}

int _wstat(const wchar_t *wfilename, struct stat *st)
{
	DWORD dwAttribute;
	HANDLE h;
	DWORD dwSizeLow=0, dwSizeHigh=0, dwError=0;
	WIN32_FIND_DATAW fd;

	dwAttribute = GetFileAttributesW(wfilename);
	if(dwAttribute==0xFFFFFFFF)
		return -1;

	st->st_mode = 0;
	if((dwAttribute & FILE_ATTRIBUTE_DIRECTORY) != 0)
		st->st_mode += S_IFDIR;
	else
		st->st_mode += S_IFREG;

	/* initialize */
	st->st_atime = 0;
	st->st_mtime = 0;
	st->st_ctime = 0;
	st->st_size  = 0;
	st->st_dev   = 0;

	h = FindFirstFileW(wfilename, &fd);
	if(h == INVALID_HANDLE_VALUE)
	{
		if(wfilename[wcslen(wfilename)-1] == L'\\')
		{
			wchar_t *wfile = NULL;
			wcscpy(wfile, wfilename);
			wfile[wcslen(wfilename)-1] = L'\0';
			h = FindFirstFileW(wfilename, &fd);
			free(wfile);
			if(h == INVALID_HANDLE_VALUE)
				return 0;
		}
		else
			return 0;
	}

	/* FILETIME -> time_t */
	st->st_atime = wce_FILETIME2time_t(&fd.ftLastAccessTime);
	st->st_mtime = wce_FILETIME2time_t(&fd.ftLastWriteTime);
	st->st_ctime = wce_FILETIME2time_t(&fd.ftCreationTime);
	st->st_size  = fd.nFileSizeLow;

	FindClose( h );
	return 0;
}

void *bsearch(const void *key, const void *base,
	      size_t nmemb, size_t size,
	      int (*compar)(const void *, const void *))
{
	size_t odd_mask, bytes;
	const char *center, *high, *low;
	int comp;

	odd_mask = ((size ^ (size - 1)) >> 1) + 1;
	low = base;
	bytes = nmemb == 0 ? size : size + 1;
	center = low + nmemb * size;
	comp = 0;
	while (bytes != size) {
		if (comp > 0) {
			low = center;
		} else {
			high = center;
		}
		bytes = high - low;
		center = low + ((bytes & odd_mask ? bytes - size : bytes) >> 1);
		comp = compar(key, center);
		if (comp == 0) {
			return (void *)center;
		}
	}
	return NULL;
}

char* ctime( const time_t *t )
{
	// Wed Jan 02 02:03:55 1980\n\0
	static char buf[30]={0};
	char week[] = "Sun Mon Tue Wed Thr Fri Sat ";
	char month[]= "Jan Feb Mar Apl May Jun Jul Aug Sep Oct Nov Dec ";
	struct tm tms;

	tms = *localtime(t);

	strncpy( buf,    week+tms.tm_wday*4, 4 );
	strncpy( buf+4,  month+tms.tm_mon*4, 4 );
	sprintf( buf+8,  "%02d ", tms.tm_mday );
	sprintf( buf+11, "%02d:%02d:%02d %d\n", 
		tms.tm_hour, tms.tm_min, tms.tm_sec, tms.tm_year+1900 );
	return buf;
}
