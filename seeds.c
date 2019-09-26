#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "seeds.h"
#include "i_maths.h"

#if !defined(__GNUC__)
#define __inline__
#endif

 /* //////////////////////////////////////////////////////////////////////// */

typedef void		SeedFunc(pixel *Destination);

typedef struct
{
	char		       *Name;
	SeedFunc	       *Function;
} SeedInfo;

extern SeedFunc		seed_Walker,
			seed_Qix,
			seed_Balls;

#if defined(USE_TV_SEEDS)
extern void		SetupTV(void);
extern void		ShutdownTV(void);
extern void		TVKeypress(int Key);

extern SeedFunc		seed_TVOutline,
			seed_TV;
#endif

 /* //////////////////////////////////////////////////////////////////////// */

SeedFunc	       *Seed = NULL;

static SeedInfo		Seeds[] =
{
	{ "qix", seed_Qix },
	{ "walker", seed_Walker },
	{ "balls", seed_Balls },
#if defined(USE_TV_SEEDS)
	{ "tvoutline", seed_TVOutline },
	{ "tvplus", seed_TV }
#endif
},		       *SeedPtr = Seeds;

static int		bufwidth,
			bufheight,
			bufpitch,
			genscale;


  /* /////////////////////////////////////////////////////////////////////// */

void SetupSeeds(char **argv, int width, int height, int pitch)
{
#if defined(USE_TV_SEEDS)
	SetupTV();
#endif
	SeedPtr = Seeds;
	Seed = SeedPtr->Function;
	ResizeSeeds(width, height, pitch);
}

void ResizeSeeds(int width, int height, int pitch)
{
	bufwidth = width;
	bufheight = height;
	bufpitch = pitch;
	genscale = (bufwidth + bufheight) / 2; /* sqrt(bufwidth * bufheight) is preferable */
}

void ShutdownSeeds(void)
{
#if defined(USE_TV_SEEDS)
	ShutdownTV();
#endif
}

const char *SelectSeed(const char *Name)
{
	SeedInfo	       *ptr;


	if (Name == NULL)
	{
		SeedPtr = Seeds + (rand() >> 8) % (sizeof(Seeds) / sizeof(*Seeds));
		Seed = SeedPtr->Function;
		return SeedPtr->Name;
	}

	for (ptr = Seeds; ptr < Seeds + sizeof(Seeds) / sizeof(*Seeds); ptr++)
		if (strcmp(ptr->Name, Name) == 0)
		{
			SeedPtr = ptr;
			break;
		}

	Seed = SeedPtr->Function;
	return SeedPtr->Name;
}

const char *CycleSeedUp(void)
{
	SeedPtr++;

	if (SeedPtr >= Seeds + sizeof(Seeds) / sizeof(*Seeds))
		SeedPtr = Seeds;

	Seed = SeedPtr->Function;
	return SeedPtr->Name;
}

const char *CycleSeedDown(void)
{
	SeedPtr--;

	if (SeedPtr < Seeds)
		SeedPtr = Seeds + sizeof(Seeds) / sizeof(*Seeds) - 1;

	Seed = SeedPtr->Function;
	return SeedPtr->Name;
}

void UpdateSeed(void)
{
	static SeedFunc *lastseed = NULL;

	if (lastseed != Seed)
	{
		Seed(NULL);
		lastseed = Seed;
	}
}

void SeedKeypress(int Key)
{
#if defined(USE_TV_SEEDS)
	TVKeypress(Key);
#endif
}


/* utilities for the kiddies */

static __inline__ void pixfiddle(pixel *ptr)
{
	int		val;

	val = rand() & 0xFFF;
	if (val >= 0xF00)
		*ptr |= 0xFE;
	else
		*ptr = ((*ptr ^ 0xFF) * (unsigned char)val >> 8) ^ 0xFF;
}

static __inline__ void HLine(pixel *Target, int len)
{
	while (--len >= 0)
		pixfiddle(Target++);
}

int DrawBall(int x, int y, int radius, pixel *Target)
{
	int			d, dinc;
	pixel		       *ttop, *tbot, *tup, *tdn;

	x = (x + 0x8000) * bufwidth / 0x10000;
	y = (y + 0x8000) * bufheight / 0x10000;
	radius = radius * genscale / 0x10000;
	x -= radius;
	y -= radius;

	if ((unsigned)x >= bufwidth - radius * 2u || (unsigned)y >= bufheight - radius * 2u)
		return 0;


	ttop = Target + y * bufpitch + (x + radius);
	tdn = ttop + radius * bufpitch;
	tup = tdn - bufpitch;
	tbot = tup + radius * bufpitch;

	d = 3 - (2 * radius);
	dinc = 6;
	x = 0;
	y = radius;
	while (x < y)
	{
		HLine(tup - y, y + y);
		HLine(tdn - y, y + y);
		tup -= bufpitch;
		tdn += bufpitch;

		if (d < 0)
		{
			d += dinc;
		}
		else
		{
			y--;
			d -= 4 * y;
			HLine(ttop - x, x + x);
			HLine(tbot - x, x + x);
			ttop += bufpitch;
			tbot -= bufpitch;
		}
		x++;
		d += dinc;
		dinc += 4;
	}
	return 1;
}



int DrawVertLine(int x1, int y1, int x2, int y2, pixel *Target)
{
	int			xacc,
				xinc;


	if ((unsigned)x1 >= bufwidth - 1u || (unsigned)x2 >= bufwidth - 1u)
		return 1;

	if (y1 > y2)
	{
		x1 ^= x2, x2 ^= x1, x1 ^= x2;
		y1 ^= y2, y2 ^= y1, y1 ^= y2;
	}
	if (y1 < 0 || y2 >= bufheight)
		return 1;

	xacc = (x1 << 16) + 32768;
	if (y1 != y2)
		xinc = ((x2 << 16) - xacc) / (y2 - y1);
	else
		xinc = 0;

	Target += y1 * bufpitch;
	while (y1 <= y2)
	{
		pixfiddle(Target + (xacc >> 16));
		pixfiddle(Target + (xacc >> 16) + 1);

		y1++;
		xacc += xinc;
		Target += bufpitch;
	}
	return 0;
}

int DrawHorizLine(int x1, int y1, int x2, int y2, pixel *Target)
{
	int			yacc,
				yinc;


	if ((unsigned)y1 >= bufheight - 1u || (unsigned)y2 >= bufheight - 1u)
		return 0;

	if (x1 > x2)
	{
		x1 ^= x2, x2 ^= x1, x1 ^= x2;
		y1 ^= y2, y2 ^= y1, y1 ^= y2;
	}
	if (x1 < 0 || x2 >= bufwidth)
		return 0;

	yacc = (y1 << 16) + 32768;
	if (x1 != x2)
		yinc = ((y2 << 16) - yacc) / (x2 - x1);
	else
		yinc = 0;

	Target += x1;
	while (x1 <= x2)
	{
		pixfiddle(Target + (yacc >> 16) * bufpitch);
		pixfiddle(Target + (yacc >> 16) * bufpitch + bufpitch);

		x1++;
		Target++;
		yacc += yinc;
	}
	return 1;
}

int DrawLine(int x1, int y1, int x2, int y2, pixel *Target)
{
	x1 = (x1 + 0x8000) * bufwidth / 0x10000;
	y1 = (y1 + 0x8000) * bufheight / 0x10000;
	x2 = (x2 + 0x8000) * bufwidth / 0x10000;
	y2 = (y2 + 0x8000) * bufheight / 0x10000;

	if (abs(x1 - x2) > abs(y1 - y2))
		return DrawHorizLine(x1, y1, x2, y2, Target);
	else
		return DrawVertLine(x1, y1, x2, y2, Target);
}

int DrawPoint(int x, int y, pixel *Target)
{
	x = (x + 0x8000) * bufwidth / 0x10000;
	y = (y + 0x8000) * bufheight / 0x10000;

	if ((unsigned)x >= bufwidth - 1u || (unsigned)y >= bufheight - 1u)
		return 0;

	pixfiddle(Target + bufpitch * y + x);
	return 0;
}

