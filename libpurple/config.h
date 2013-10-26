#define HAVE_TIMEZONE 1

#ifdef XP_UNIX
# define HAVE_TM_GMTOFF 1
# define HAVE_ARPA_NAMESER_COMPAT_H 1
# define HAVE_INET_NTOP 1
#endif

#ifdef XP_WIN
# define SIZEOF_TIME_T 8
#else
# ifdef HAVE_64BIT_OS
#  define SIZEOF_TIME_T 8
# else
#  define SIZEOF_TIME_T 4
# endif
#endif

#undef HAVE_CONFIG_H
