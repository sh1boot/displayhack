#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "warpers.h"
#include "i_maths.h"
#include "displayhack.h" /* for ScreenWidth, ScreenHeight, and MemWidth */
#include "rmpd.h"

#include "fixwin32.h"

#define RMPD_WIDTH		256
#define RMPD_HEIGHT		RMPD_WIDTH

#if 0 /* wraparound */
#define wraparound()		(*x &= 0xFFFF, *y &= 0xFFFF)
#else
#define wraparound()
#endif

 /* //////////////////////////////////////////////////////////////////////// */

typedef void		WarperFunc(warp_real *x, warp_real *y, unsigned int c);

typedef struct
{
	char		       *Name;
	WarperFunc	       *Function;
}			WarperInfo;

static WarperFunc	warp_Null,
			warp_test2,
			warp_TwistOut,
			warp_Racer,
			warp_Spiral,
			warp_Twirl,
			warp_Overlay,
			warp_Knotted,
			warp_Radial,
			warp_Trellis,
			warp_Waffle,
			warp_Streak,
			warp_Meander,
			warp_Segmented,
			warp_Bow,
			warp_Bow2,
			warp_RMPD,
			warp_RMPD2,
			warp_Pixel,
			warp_Pixel2,
			warp_Hall,
			warp_Combo,
			warp_Mix2,
			warp_Mix3,
			warp_Sampler;


 /* //////////////////////////////////////////////////////////////////////// */

WarperFunc	       *Warp = NULL;

static WarperInfo	Warpers[] =
{
	{ "null", warp_Null },
	{ "test2", warp_test2 },
	{ "twistout", warp_TwistOut },
	{ "racer", warp_Racer },
	{ "spiral", warp_Spiral },
	{ "twirl", warp_Twirl },
	{ "overlay", warp_Overlay },
	{ "knotted", warp_Knotted },
	{ "radial", warp_Radial },
	{ "trellis", warp_Trellis },
	{ "waffle", warp_Waffle },
	{ "streak", warp_Streak },
	{ "meander", warp_Meander },
	{ "segmented", warp_Segmented },
	{ "bow", warp_Bow },
	{ "bow2", warp_Bow2 },
	{ "rmpd", warp_RMPD },
	{ "rmpd2", warp_RMPD2 },
	{ "pixel", warp_Pixel },
	{ "pixel2", warp_Pixel2 },
	{ "hall", warp_Hall },
	{ "combo", warp_Combo },
	{ "mix2", warp_Mix2 },
	{ "mix3", warp_Mix3 },
	{ "sampler", warp_Sampler }
};

static WarperInfo      *WarpPtr = Warpers;

static unsigned short	HeightMap[RMPD_HEIGHT][RMPD_WIDTH];

 /* //////////////////////////////////////////////////////////////////////// */

#if !defined(__GNUC__)
#define __inline__
#endif

#define shl(x, y) ((x) << (y))
#define shr(x, y) ((x) >> (y))

__inline__ static warp_real xsin(warp_real theta)
{
	long tmp;
	i_sincos(&tmp, NULL, (int)theta);
	return (warp_real)tmp;
}

__inline__ static warp_real xcos(warp_real theta)
{
	long tmp;
	i_sincos(NULL, &tmp, (int)theta);
	return (warp_real)tmp;
}


__inline__ static void polarise(warp_real *r, warp_real *theta, warp_real x, warp_real y)
{
	if (theta != NULL) *theta = (warp_real)i_atan2(y, x);
	if (r != NULL) *r = i_sqrt(x * x + y * y);
}


__inline__ static void depolarise(warp_real *x, warp_real *y, warp_real r, warp_real theta)
{
	long sinval, cosval;

	i_sincos(&sinval, &cosval, theta);

	if (x != NULL) *x = shr((warp_real)(cosval * r + SIN_SCALE / 2), SIN_SCALE_BITS);
	if (y != NULL) *y = shr((warp_real)(sinval * r + SIN_SCALE / 2), SIN_SCALE_BITS);
}

__inline__ static warp_real notatzero(warp_real x)
{
	if (x >= (warp_real)4096 || x <= (warp_real)-4096)
		return 0x10000;
	return shr(x * x, 8);
}

 /* //////////////////////////////////////////////////////////////////////// */

static void warp_Null(warp_real *x, warp_real *y, unsigned int c)
{
}

static void warp_test2(warp_real *x, warp_real *y, unsigned int c)
{
	static WarperFunc      *mywarp = NULL;
	static unsigned int	lastc;
	warp_real		theta,
				r;
	warp_real		base_r,
				base_theta;

	if (mywarp == NULL || lastc != c)
	{
		static WarperFunc *tab[4] = { warp_Hall, warp_Bow2, warp_Twirl, warp_Spiral };

		lastc = c;
		c /= 1013;
		mywarp = tab[c & 3];
	}
	polarise(&r, &theta, *x, *y);

	theta += DEG_180 / 2;

	theta *= 3;
	r *= 5;

	base_r = r & ~0xFFFF;
	base_theta = theta & ~0xFFFF;

	r = (r & 0xFFFF) - 0x8000;
	theta = (theta & 0xFFFF) - 0x8000;

	if ((base_r ^ base_theta) & 0x10000)
		r = -r;

	mywarp(&r, &theta, c);

	if ((base_r ^ base_theta) & 0x10000)
		r = -r;

	r = base_r + r + 0x8000;
	theta = base_theta + theta + 0x8000;

	r /= 5;
	theta /= 3;

	theta -= DEG_180 / 2;

	depolarise(x, y, r, theta);
}

static void warp_TwistOut(warp_real *x, warp_real *y, unsigned int c)
{
	warp_real		temp;
	static unsigned int	lastc = (unsigned int)-1;
	static warp_real	theta_r,
				theta_i;

	if (c != lastc)
	{
		depolarise(&theta_r, &theta_i, SIN_SCALE - shr(((c & 3) + 1) * SIN_SCALE, 8), shr((((c & 3) + 1) * DEG_180 * 85), 16));
		lastc = c;
	}

	temp = shr((*x * theta_r + *y * theta_i + SIN_SCALE / 2), SIN_SCALE_BITS);
	*y = shr((*y * theta_r - *x * theta_i + SIN_SCALE / 2), SIN_SCALE_BITS);
	*x = temp;
}

static void warp_Racer(warp_real *x, warp_real *y, unsigned int c)
{
	*x = shr(*x * 13 + 8, 4);
	*y = shr(*y * 13 + 8, 4);
}

static void warp_Spiral(warp_real *x, warp_real *y, unsigned int c)
{
	warp_real		theta,
				r,
				th_off;

	polarise(&r, &theta, *x, *y);

	r += 132;
	depolarise(&th_off, NULL, shr(DEG_180 * 288 + 32768, 16), theta + shr(r * DEG_360 * 3 + 32768, 16));
	theta += th_off;

	depolarise(x, y, r, theta);
}

static void warp_Twirl(warp_real *x, warp_real *y, unsigned int c)
{
	warp_real		theta,
				r,
				d;

	polarise(&r, &theta, *x, *y);

	depolarise(NULL, &d, 2 * notatzero(r), theta * 5 + shr(r * DEG_180 * 4 + 32768, 16));
	theta -= shr(d * DEG_180 * 104, SIN_SCALE_BITS + 16);
	r += shr(d, SIN_SCALE_BITS - 7);

	depolarise(x, y, r, theta);
}

static void warp_Overlay(warp_real *x, warp_real *y, unsigned int c)
{
	warp_real		x2,
				y2,
				x3,
				y3;

	x3 = x2 = *x;
	y3 = y2 = *y;

	x2 += 0x4000;
	y2 += -0x4000;
	x3 += -0x2B84;
	y3 += 0x0290;

	warp_TwistOut(x, y, c);
	warp_Twirl(&y2, &x2, c);
	warp_Twirl(&x3, &y3, c);

	x2 -= 0x4000;
	y2 -= -0x4000;
	x3 -= -0x2B84;
	y3 -= 0x0290;

	*x = shr((*x + x2 + x3) * 21845 + 32768, 16);
	*y = shr((*y + y2 + y3) * 21845 + 32768, 16);
}

static void warp_Knotted(warp_real *x, warp_real *y, unsigned int c)
{
	warp_real		theta,
				r,
				d;
	warp_real		yoff,
				xoff,
				xdelta,
				ydelta;

	xoff = shr(xsin(shr(*x * DEG_180 * 144, 20)) * (0x10000 / 14) + SIN_SCALE / 2, SIN_SCALE_BITS);
	yoff = shr(xsin(shr(*x * DEG_180 * 61, 20)) * (0x10000 / 14) + SIN_SCALE / 2, SIN_SCALE_BITS);

	xdelta = shr(xcos(shr((*y + yoff) * 22 * DEG_180, 16)) * 98 + SIN_SCALE / 2, SIN_SCALE_BITS);
	ydelta = shr(xcos(shr((*x + xoff) * 22 * DEG_180, 16)) * 98 + SIN_SCALE / 2, SIN_SCALE_BITS);

	polarise(&r, &theta, *x, *y);

	d = shr(   xsin(theta * 5 + shr(r * DEG_180 * 6 + 32768, 16))
	         * xsin(theta * 2 + shr(r * DEG_180 * 6 + 32768, 16))
	         * notatzero(r)
	         + ((warp_real)SIN_SCALE * SIN_SCALE),
	       1 + SIN_SCALE_BITS + SIN_SCALE_BITS);
	theta -= shr(d * DEG_180 + (1 << 22), 23);
	r += shr(d * 455 + 32768, 16);

	depolarise(x, y, r, theta);
	*x += xdelta;
	*y += ydelta;
}

static void warp_Radial(warp_real *x, warp_real *y, unsigned int c)
{
	warp_real		theta,
				r,
				r2;


	polarise(&r, &theta, *x, *y);

	r2 = r + (((xsin(theta * 25) - 45875) * notatzero(r) * 51 + (warp_real)SIN_SCALE * 32768) >> (SIN_SCALE_BITS + 16));
	theta += (xsin(r * DEG_180 / 0x1000) * DEG_180 * 87 + (warp_real)SIN_SCALE * 32768) >> (SIN_SCALE_BITS + 16);

	depolarise(x, y, r2, theta);
}

static void warp_Trellis(warp_real *x, warp_real *y, unsigned int c)
{
	warp_real xoff, yoff;

	yoff = shr(xsin(shr(*x * DEG_360 * 2956 + (1 << 23), 24)) * 164 + SIN_SCALE / 2, SIN_SCALE_BITS);
	xoff = shr(xsin(shr(*y * DEG_360 * 2560 + (1 << 23), 24)) * 164 + SIN_SCALE / 2, SIN_SCALE_BITS);

	*x += xoff;
	*y += yoff;
}


static void warp_Waffle(warp_real *x, warp_real *y, unsigned int c)
{
	warp_real xoff, yoff;

	yoff = (xsin((*x * DEG_360 * 8868 + (1 << 23)) >> 24) * 260 + SIN_SCALE / 2) >> SIN_SCALE_BITS;
	xoff = (xsin((*y * DEG_360 * 7680 + (1 << 23)) >> 24) * 260 + SIN_SCALE / 2) >> SIN_SCALE_BITS;

	*x += xoff;
	*y += yoff;
}


static void warp_Streak(warp_real *x, warp_real *y, unsigned int c)
{
	*x += ((((int)*y + 1024) & ~2047) - (int)*y) / 4;
	*y += 32;
}

static void warp_Meander(warp_real *x, warp_real *y, unsigned int c)
{
	warp_real		tx,
				ty,
				ix,
				iy;
	warp_real		k = 132;
	const int		rep = 8;

#if 1
	tx = *x;
	*x -= *y;
	*y += tx;
#endif

	iy = ((int)*y * rep + 0x8000) & ~0xFFFF;
	*x -= iy / (5 * rep);

	ix = ((int)*x * rep + 0x8000) & ~0xFFFF;

	tx = *x * rep - ix;
	ty = *y * rep - iy;

	if ((int)ix & 0x10000)
		k = -k;

	if (llabs(tx - 0x199A) < ty || llabs(tx + 0x199A) < -ty)
	{
		ty = llabs(ty);
		if (ty >= 2 * 0x199A && ty < 4 * 0x199A)
			*x -= k;
		else if (ty >= 4 * 0x199A && 0) /* adjust transfer corners */
			*y -= k;
		else
			*x += k;
	}
	else
	{
		tx = llabs(tx);
		if (tx >= 0x199A && tx < 3 * 0x199A)
			*y -= k;
		else
			*y += k;
	}

	*x += iy / (5 * rep);
#if 1
	tx = *x;
	*x = (*x + *y) / 2;
	*y = (*y - tx) / 2;
#endif
}

static void warp_Segmented(warp_real *x, warp_real *y, unsigned int c)
{
	warp_real		theta,
				r,
				d;

	polarise(&r, &theta, *x, *y);

	d = xsin(theta * 5 + ((r * DEG_180 * 8 + 32768) >> 16)) * 10;
	theta += (d * DEG_180 + SIN_SCALE * 128) >> (SIN_SCALE_BITS + 8);
	r += (d * 32 + SIN_SCALE / 2) >> SIN_SCALE_BITS;

	depolarise(x, y, r, theta);
}

static void warp_Bow(warp_real *x, warp_real *y, unsigned int c)
{
	warp_real		temp;

	temp = *x;
	*x -= *y;
	*y += temp;

	*x += (*x * llabs(*x) * 3 + (1 << 21)) >> 22;
	*y -= (*y * llabs(*y) * 3 + (1 << 21)) >> 22;

	temp = *x;
	*x = (*x + *y) >> 1;
	*y = (*y - temp) >> 1;

	temp = (*y * *y * *y + ((warp_real)1 << 34)) >> 35;
	*y  += (*x * *x * *x + ((warp_real)1 << 34)) >> 35;
	*x += temp;
}

static void warp_Bow2(warp_real *x, warp_real *y, unsigned int c)
{
	warp_real		temp;
	warp_real		xsqr, ysqr;

	temp = *x;
	*x -= *y;
	*y += temp;

	*x += (*x + 16) >> 5;
	*y -= (*y + 16) >> 5;

	temp = *x;
	*x = (*x + *y + 1) >> 1;
	*y = (*y - temp + 1) >> 1;

	xsqr = (int)(*x * *x + 32768) >> 16;
	ysqr = (int)(*y * *y + 32768) >> 16;

	temp = (ysqr * *y * (0x10000 - xsqr) + ((warp_real)1 << 32)) >> (32 + 1);
	*y  += (xsqr * *x * (0x10000 - ysqr) * 5 + ((warp_real)1 << 35)) >> (32 + 4);
	*x += temp;
}

static warp_real getRMPDpoint(unsigned int x, unsigned int y)
{
	int			xi,
				yi;

	xi = x >> 16;
	yi = y >> 16;
	x &= 0xFFFF;
	y &= 0xFFFF;

	return (  ((HeightMap[yi + 0][xi] * (0x10000 - x) + HeightMap[yi + 0][xi + 1] * x) >> 16)  * (0x10000 - y)
	 	+ ((HeightMap[yi + 1][xi] * (0x10000 - x) + HeightMap[yi + 1][xi + 1] * x) >> 16)  *            y ) >> 16;
}

static void warp_RMPD(warp_real *x, warp_real *y, unsigned int c)
{
	warp_real		xoff,
				yoff;
	warp_real		p,
				pr,
				pd;
	int      		x2 = (((int)*x + 0x8000) & 0xFFFF) * (RMPD_WIDTH - 2),
				y2 = (((int)*y + 0x8000) & 0xFFFF) * (RMPD_HEIGHT - 2);

	p = getRMPDpoint(x2, y2);
	pr = getRMPDpoint(x2 + 0x10000, y2);
	pd = getRMPDpoint(x2, y2 + 0x10000);

	xoff = p - pr;
	yoff = p - pd;

	*x += yoff >> 1;
	*y -= xoff >> 1;

	*x += xoff >> 4;
	*y += yoff >> 4;
	wraparound();
}

static void warp_RMPD2(warp_real *x, warp_real *y, unsigned int c)
{
	warp_real		xoff,
				yoff;
	warp_real		p,
				pr,
				pd;
	int      		x2 = (((int)*x + 0x8000) & 0xFFFF) * (RMPD_WIDTH - 2),
				y2 = (((int)*y + 0x8000) & 0xFFFF) * (RMPD_HEIGHT - 2);

	p = getRMPDpoint(x2, y2);
	pr = getRMPDpoint(x2 + 0x10000, y2);
	pd = getRMPDpoint(x2, y2 + 0x10000);

	xoff = p - pr;
	yoff = p - pd;

	*x += xoff >> 1;
	*y += yoff >> 1;
	wraparound();
}

static void warp_Pixel(warp_real *x, warp_real *y, unsigned int c)
{
	*x = (*x * 7 + (((int)*x + 1024) & ~2047)) >> 3;
	*y = (*y * 7 + (((int)*y + 1024) & ~2047)) >> 3;
}

static void warp_Pixel2(warp_real *x, warp_real *y, unsigned int c)
{
	*x = (*x * 9 - (((int)*x + 1024) & ~2047)) >> 3;
	*y = (*y * 9 - (((int)*y + 1024) & ~2047)) >> 3;
}

static void warp_Hall(warp_real *x, warp_real *y, unsigned int c)
{
	warp_real abs_x = llabs(*x), abs_y = llabs(*y);
	if ((!(abs_x < abs_y) && (abs_x + 2 * abs_y / 5 < 0x4ccd))
	  || ((abs_x < abs_y) && (abs_y + 2 * abs_x / 5 < 0x4ccd)))
			*x += *x >> 1, *y += *y >> 1;
}

static void warp_Combo(warp_real *x, warp_real *y, unsigned int c)
{
	long xoff[4], yoff[4];
	static unsigned int lastc = (unsigned int)-1;

	if (lastc != c)
	{
		int i;
		for (i = 0; i < 4; i++)
		{
			c /= 67;
			xoff[i] = ((c & 0x07) << 12) - (0x7000 / 2);
			yoff[i] = ((c & 0x38) <<  9) - (0x7000 / 2);
		}
		lastc = c;
	}

	*x += xoff[0], *y += yoff[0];
	warp_Overlay(x, y, c);
	warp_Overlay(y, x, c);
	*x += xoff[1] - xoff[0], *y += yoff[1] - yoff[0];
	warp_TwistOut(x, y, 0);
	*x += xoff[2] - xoff[1], *y += yoff[2] - yoff[1];
	warp_Radial(x, y, c);
	*x += xoff[3] - xoff[2], *y += yoff[3] - yoff[2];
	*x = -*x, *y = -*y;
	warp_Knotted(x, y, c);
	*x = -*x, *y = -*y;
	*x +=         - xoff[3], *y +=         - yoff[3];
	warp_Trellis(x, y, c);
	wraparound();
}

static void warp_Mix2(warp_real *x, warp_real *y, unsigned int c)
{
	static unsigned int lastc;
	static WarperFunc *mywarp[4] = { NULL, NULL, NULL, NULL };
	unsigned long p, r;
	warp_real x2 = *x, y2 = *y;

	if (mywarp[0] == NULL || lastc != c)
	{
		static WarperFunc *tab[9] = { warp_Bow, warp_Trellis, warp_RMPD2, warp_Bow2, warp_Streak, warp_Racer, warp_Pixel2, warp_Overlay, warp_Twirl };
		lastc = c;
		c /= 239;
		mywarp[0] = tab[c % 9]; c /= 9;
		mywarp[1] = tab[c % 9]; c /= 9;
		mywarp[2] = tab[c % 9]; c /= 9;
		mywarp[3] = tab[c % 9];
	}
	p = (unsigned long)(*x * 2 + *y);

	r = p & 0xFFFF;
	p >>= 16;

	mywarp[(p + 0) & 3](x, y, c);
	mywarp[(p + 1) & 3](&x2, &y2, c);

	*x += ((x2 - *x) * r) >> 16;
	*y += ((y2 - *y) * r) >> 16;
}

static void warp_Mix3(warp_real *x, warp_real *y, unsigned int c)
{
	static unsigned int lastc;
	static WarperFunc *mywarp[4] = { NULL, NULL, NULL, NULL };
	unsigned long p, r;
	warp_real x2 = *x, y2 = *y;

	if (mywarp[0] == NULL || lastc != c)
	{
		static WarperFunc *tab[10] = { warp_Bow, warp_Trellis, warp_RMPD2, warp_Bow2, warp_Streak, warp_Racer, warp_Pixel2, warp_Overlay, warp_Twirl, warp_Mix2 };
		lastc = c;
		c /= 239;
		mywarp[0] = tab[c % 10]; c /= 10;
		mywarp[1] = tab[c % 10]; c /= 10;
		mywarp[2] = tab[c % 10]; c /= 10;
		mywarp[3] = tab[c % 10];
	}
        {
	    int      		x2 = (((int)*x + 0x8000) & 0xFFFF) * (RMPD_WIDTH - 2),
				y2 = (((int)*y + 0x8000) & 0xFFFF) * (RMPD_HEIGHT - 2);

	    p = getRMPDpoint(x2, y2) * 5 + 0x8000;
        }

	r = p & 0xFFFF;
	p >>= 16;

	mywarp[(p + 0) & 3](x, y, c);
	mywarp[(p + 1) & 3](&x2, &y2, c);

	*x += ((x2 - *x) * r) >> 16;
	*y += ((y2 - *y) * r) >> 16;
}

static void warp_Sampler(warp_real *x, warp_real *y, unsigned int c)
{
	static int		reclimit = 4;
	warp_real		x2,
				y2;
	int			ix,
				iy,
				choice;

	if (reclimit <= 0)
		return;

	reclimit--;

	x2 = *x * 5;
	y2 = *y * 5;
	ix = (int)(x2 + 0x8000) & ~0xFFFF;
	iy = (int)(y2 * 4 / 3 + 0x8000) & ~0xFFFF;
	x2 -= ix;
	y2 -= iy * 3 / 4;

	choice = (unsigned int)((iy >> 16) * 5 + (ix >> 16) + 100) % (sizeof(Warpers) / sizeof(*Warpers));

	Warpers[choice].Function(&x2, &y2, c);

	x2 += ix;
	y2 += iy * 3 / 4;

	*x = x2 / 5;
	*y = y2 / 5;

	reclimit++;
}


 /* //////////////////////////////////////////////////////////////////////// */

void SetupWarpers(char **argv)
{
	RMPD(&HeightMap[0][0], RMPD_WIDTH, RMPD_HEIGHT, RMPD_WIDTH);
	/* TODO: fix problem with horizontal seam */
	{
		unsigned short TempMap[RMPD_HEIGHT][RMPD_WIDTH];
		int x, y, i;

		for (i = 0; i < 2; i++)
		{
			for (y = 1; y < RMPD_WIDTH - 1; y++) for (x = 1; x < RMPD_WIDTH - 1; x++)
			{
				TempMap[y][x] =
				( HeightMap[y][x] * 12
				  + (HeightMap[y][x - 1] + HeightMap[y - 1][x] + HeightMap[y][x + 1] + HeightMap[y + 1][x]) * 9
				  + (HeightMap[y - 1][x - 1] + HeightMap[y - 1][x + 1]+ HeightMap[y + 1][x - 1] + HeightMap[y + 1][x + 1]) * 4
				  + ((rand() >> 8) & 63) 
				) >> 6;
			}

			for (x = 0; x < RMPD_WIDTH; x++)
			{
				TempMap[0][x] = TempMap[1][x];
				TempMap[RMPD_HEIGHT - 1][x] = TempMap[RMPD_HEIGHT - 2][x];
			}

			for (y = 0; y < RMPD_HEIGHT; y++)
			{
				TempMap[y][0] = TempMap[y][1];
				TempMap[y][RMPD_WIDTH - 1] = TempMap[y][RMPD_WIDTH - 2];
			}

			for (y = 1; y < RMPD_WIDTH - 1; y++) for (x = 1; x < RMPD_WIDTH - 1; x++)
			{
				HeightMap[y][x] =
				( TempMap[y][x] * 12
				  + (TempMap[y][x - 1] + TempMap[y - 1][x] + TempMap[y][x + 1] + TempMap[y + 1][x]) * 9
				  + (TempMap[y - 1][x - 1] + TempMap[y - 1][x + 1]+ TempMap[y + 1][x - 1] + TempMap[y + 1][x + 1]) * 4
				  + ((rand() >> 8) & 63) 
				) >> 6;
			}
		}
	}

	WarpPtr = Warpers;
	Warp = WarpPtr->Function;
}

const char *SelectWarper(const char *name)
{
	WarperInfo	       *ptr;


	if (name == NULL)
		WarpPtr = Warpers + (rand() >> 8) % (sizeof(Warpers) / sizeof(*Warpers));
	else
		for (ptr = Warpers; ptr < Warpers + sizeof(Warpers) / sizeof(*Warpers); ptr++)
			if (strcmp(ptr->Name, name) == 0)
			{
				WarpPtr = ptr;
				break;
			}

	Warp = WarpPtr->Function;
	return WarpPtr->Name;
}

const char *CycleWarperUp(void)
{
	WarpPtr++;

	if (WarpPtr >= Warpers + sizeof(Warpers) / sizeof(*Warpers))
		WarpPtr = Warpers;

	Warp = WarpPtr->Function;
	return WarpPtr->Name;
}

const char *CycleWarperDown(void)
{
	WarpPtr--;

	if (WarpPtr < Warpers)
		WarpPtr = Warpers + sizeof(Warpers) / sizeof(*Warpers) - 1;

	Warp = WarpPtr->Function;
	return WarpPtr->Name;
}

void ShutdownWarpers(void)
{
}

void WarperKeypress(int key)
{
}

