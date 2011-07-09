/* Local libgadu configuration. */

/*#include "config.h" */

#ifndef __GG_LIBGADU_CONFIG_H
#define __GG_LIBGADU_CONFIG_H

/* Defined if libgadu was compiled for bigendian machine. */
#undef __GG_LIBGADU_BIGENDIAN
#ifdef WORDS_BIGENDIAN
#define __GG_LIBGADU_BIGENDIAN
#endif /* WORDS_BIGENDIAN */

#ifndef _WIN32
/* Defined if this machine has va_copy(). */
#define __GG_LIBGADU_HAVE_VA_COPY

/* Defined if this machine has __va_copy(). */
#define __GG_LIBGADU_HAVE___VA_COPY
#endif

/* Defined if this machine supports long long. */
#undef __GG_LIBGADU_HAVE_LONG_LONG
#ifdef HAVE_LONG_LONG
#define __GG_LIBGADU_HAVE_LONG_LONG
#endif /* HAVE_LONG_LONG */

/* Defined if libgadu was compiled and linked with pthread support. */
/* We don't like pthreads. */
#undef __GG_LIBGADU_HAVE_PTHREAD

/* Defined if libgadu was compiled and linked with GnuTLS encryption support. */
#ifdef HAVE_GNUTLS
#  define GG_CONFIG_HAVE_GNUTLS
#else
#  undef GG_CONFIG_HAVE_GNUTLS
#endif

/* Defined if libgadu was compiled and linked with TLS support. */
/* Always undefined in Purple. */
#undef __GG_LIBGADU_HAVE_OPENSSL

/* Include file containing uintXX_t declarations. */
#if HAVE_STDINT_H
#include <stdint.h>
#endif

/* Defined if this machine has C99-compiliant vsnprintf(). */
#ifndef _WIN32
#define __GG_LIBGADU_HAVE_C99_VSNPRINTF
#else
#undef __GG_LIBGADU_HAVE_C99_VSNPRINTF
#endif

#define vnsprintf g_vnsprintf

#ifdef _WIN32
#define random (long) rand
#endif

#ifdef _MSC_VER
typedef unsigned __int32 uint32_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int8  uint8_t;
#define snprintf _snprintf
#include <win32dep.h>
#define EINPROGRESS WSAEINPROGRESS
#define ECONNRESET WSAECONNRESET
#define ENOTCONN WSAENOTCONN
#define strncasecmp strnicmp
#define strcasecmp stricmp
#define ssize_t SSIZE_T
#endif

#endif /* __GG_LIBGADU_CONFIG_H */
