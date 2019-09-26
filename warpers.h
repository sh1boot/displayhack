#ifndef WARPERS_H_INCLUDED
#define WARPERS_H_INCLUDED

 /* //////////////////////////////////////////////////////////////////////// */

void			SetupWarpers(char **argv);
void			ShutdownWarpers(void);

const char	       *SelectWarper(const char *name);
const char	       *CycleWarperUp(void);
const char	       *CycleWarperDown(void);
void			WarperKeypress(int key);

#if defined(__GNUC__)
__extension__ typedef long long	warp_real;
#else
typedef long long	warp_real;
#endif

extern void	      (*Warp)(warp_real *x, warp_real *y, unsigned int c);

 /* //////////////////////////////////////////////////////////////////////// */

#endif

