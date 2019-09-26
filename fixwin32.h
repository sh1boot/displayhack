#if defined(__WIN32__) && !defined(FIXWIN32_H_INCLUDED)
#define FIXWIN32_H_INCLUDED

#ifndef M_PI
#define M_PI				3.14159265358979
#endif

#define rint(x)				floor(x + 0.5)
#define putenv(x)			_putenv(x)

#endif /* defined(__WIN32__) && !defined(FIXWIN32_H_INCLUDED) */

