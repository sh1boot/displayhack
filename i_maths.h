#ifndef I_MATHS_H_INCLUDED
#define I_MATHS_H_INCLUDED

#if !defined(M_PI)
#define M_PI 3.14159265358979
#endif
#if !defined(__GNUC__)
#define __extension__
#endif

#define DEG_180_BITS		16
#define SIN_SCALE_BITS		16
#define DEG_180			(long)(1 << DEG_180_BITS)
#define DEG_360			(DEG_180 * 2)
#define SIN_SCALE		(long)(1 << SIN_SCALE_BITS)

__extension__ int	i_sqrt(long long x);
int			i_atan2(long y, long x);
void			i_sincos(long *sin, long *cos, int theta);

#endif

