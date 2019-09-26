#if !defined(GETTSC_C_INCLUDED)
#define GETTSC_C_INCLUDED


#if defined(__GNUC__) && defined(__i386__)
__extension__ typedef long long tscsnap;

static __inline__ tscsnap GetTSC(void)
{
	unsigned		a,
				d;

	__asm__ __volatile__ (".byte 0x0f, 0x31" :"=a" (a), "=d" (d));

	return ((tscsnap)d << 32) + a;
}
#elif defined(__VIDEOCORE__)
#include <vc/hardware.h>
typedef long long tscsnap;

static tscsnap GetTSC(void)
{
   return STC;
}
#else
typedef long tscsnap;

static tscsnap GetTSC(void)
{
	return (tscsnap)clock();
}
#endif

#endif /* defined(__GCC__) && defined(__i386__) */

