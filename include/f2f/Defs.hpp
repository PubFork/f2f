#ifndef _F2F_API_DEFS_H
#define _F2F_API_DEFS_H

#if (defined _WINDOWS)
#  ifdef F2F_API_EXPORTS
#    define F2F_API_DECL __declspec (dllexport)
#  else
#    define F2F_API_DECL __declspec (dllimport)
#  endif
#else
#  define F2F_API_DECL __attribute__((visibility("default")))
#endif

#endif