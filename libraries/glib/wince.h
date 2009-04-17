/***************************************************************
  wince.h (originally io.h)

  You can freely use, copy, modify, and redistribute
  the whole contents.
  
  Downloaded on https://trac.xiph.org/browser/trunk/oggdsf/src/lib/helper/wince/
  
  Modified by Quentin Raynaud for Instantbird. This is a 
  makeup of files found in the above source tree.
  
***************************************************************/

#ifndef _WINCE_H_
#define _WINCE_H_

#include <windows.h>

#ifndef _TIME_T_DEFINED
typedef unsigned long time_t;
#define _TIME_T_DEFINED
#endif

#ifndef _FSIZE_T_DEFINED
typedef unsigned long _fsize_t; /* Could be 64 bits for Win32 */
#define _FSIZE_T_DEFINED
#endif

#define FILENAME_MAX 260

/* MoveFileEx definition. */
#define MOVEFILE_REPLACE_EXISTING       0x00000001

/* To help gmessages.c */
# define IsDebuggerPresent() 0
# define getpid GetCurrentProcessId

#ifndef _FINDDATA_T_DEFINED
struct _finddata_t {
        unsigned    attrib;
        time_t      time_create;    /* -1 for FAT file systems */
        time_t      time_access;    /* -1 for FAT file systems */
        time_t      time_write;
        _fsize_t    size;
        char        name[FILENAME_MAX];
};
#define _FINDDATA_T_DEFINED
#endif

#ifndef _WFINDDATA_T_DEFINED
struct _wfinddata_t {
        unsigned    attrib;
        time_t      time_create;    /* -1 for FAT file systems */
        time_t      time_access;    /* -1 for FAT file systems */
        time_t      time_write;
        _fsize_t    size;
        wchar_t     name[FILENAME_MAX];
};
#define _WFINDDATA_T_DEFINED
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _write
# undef _write
#endif

int _wcreat(const wchar_t *wfile, int mode);
int _wunlink(const wchar_t *wfile);
int _wopen(const wchar_t *wfile, int mode,...);
int _open(const char *wfile, int mode,...);
int _read(int fd, void *buffer, int length);
int _write(int fd, const void *buffer, unsigned count);
long _lseek(int handle, long offset, int origin);
long _findfirst( const wchar_t *filespec, struct _wfinddata_t *fi );
int _findnext( long handle, struct _wfinddata_t *fi );
int _findclose( long handle );
int _wchmod(const wchar_t *wpath, int mode);
int _waccess(const wchar_t *wfilename, int flags);

int _wrmdir(const wchar_t * wdir);
int _wmkdir(const wchar_t * wdir);

wchar_t * _tfullpath(wchar_t * fullPath, const wchar_t * oldPath, unsigned maxSize);

BOOL MoveFileExW(LPCWSTR oldname, LPCWSTR newname, DWORD dwFlags);

int _wstat(const wchar_t *wfilename, struct stat *st);
#define _close(IN) CloseHandle((HANDLE)(IN))
#define _commit(IN) !FlushFileBuffers((HANDLE)(IN))

#ifdef __cplusplus
extern "C" {
#endif

#define _errno errno

// Those are for glib files that need slighlty different symbol names
#define open _open
#define read _read
#define write _write
#define lseek _lseek
#define _tfindfirst _findfirst
#define _tfindnext _findnext
#define strnicmp _strnicmp

#include "wince-stat.h"

#ifndef _UNICODE
# define _UNICODE
#endif

#endif
