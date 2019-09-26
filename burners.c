#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "burners.h"
#include "warpers.h"

#define RATIO_POINTS			32

#define SQUARE_SIZE			32

#if 0
#define wraparound(x) ((x) -= floor((x) + 0.5))
#else
#define wraparound(x)
#endif

#define LU_Scales(x, y) RatioTable->scales[!((x) & 0x10000)][((y) & 0xFFFF) * RATIO_POINTS >> 16][((x) & 0xFFFF) * RATIO_POINTS >> 16]


 /* //////////////////////////////////////////////////////////////////////// */

typedef struct
{
	int period;
	unsigned long feedback, twiddle, bits;
} LFSR_state;

typedef struct
{
	signed int		xoffset,
				yoffset;
} WarpData;

struct fuzz_mul_t
{
	unsigned char		tl, tc, tr, tz;
	unsigned char		cl, cc, cr, cz;
	unsigned char		bl, bc, br, bz;
};

typedef struct
{
	unsigned int		offset;
	struct fuzz_mul_t      *scaleptr;
} FuzzData;

typedef struct
{
	const char	       *Name;
	BurnerFunc	       *Func;
	enum
	{
		BI_NOTHING,
		BI_SHUFFLE,
		BI_FLAMER
	}			Type;
	void		       *Data;
} BurnerInfo;

static struct fuzz_mul_t	NullScale = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static struct ratiotable_t
{
	struct fuzz_mul_t	scales[2][RATIO_POINTS][RATIO_POINTS];
	unsigned int		decay_scale;
}			ShortFlameData,
			LongFlameData,
		       *RatioTable = &ShortFlameData;

static int		randval = 0;

static int		bufwidth,
			bufheight,
			longedge,
			bufpitch;

 /* //////////////////////////////////////////////////////////////////////// */

extern void		FlameRaw(pixel *destination, pixel *source, FuzzData *map, int memwidth, int count);
extern void		LakeRaw(pixel *destination, pixel *source, int memwidth, int count);
extern void		CockupRaw(pixel *destination, pixel *source, FuzzData *map, int memwidth, int count);
extern void		TestRaw(pixel *destination, pixel *source, FuzzData *map, int memwidth, int count);

static BurnerFunc	burn_Flame,
			burn_Flat,
			burn_Squareish,
			burn_Flakeish,
			burn_Lake,
			burn_Cockup,
			burn_Test;

BurnerFunc	       *Burn = NULL;

static BurnerInfo	Burners[] =
{
	{ "short flame", burn_Flame, BI_FLAMER, &ShortFlameData },
	{ "long flame", burn_Flame, BI_FLAMER, &LongFlameData },
	{ "square", burn_Squareish, BI_SHUFFLE, NULL },
	{ "flake", burn_Flakeish, BI_SHUFFLE, NULL },
	{ "flat", burn_Flat, BI_NOTHING, NULL },
	{ "lake", burn_Lake, BI_NOTHING, NULL },
/*	{ "test", burn_Test, BI_FLAMER, &LongFlameData },
	{ "cockup", burn_Cockup, BI_FLAMER, &LongFlameData } */
};
static BurnerInfo       *BurnPtr = Burners;


static WarpData        *WarpBuffer = NULL;
static FuzzData        *FuzzBuffer = NULL;

 /* //////////////////////////////////////////////////////////////////////// */


static void burn_Flame(pixel *Destination, pixel *Source)
{
	FlameRaw(Destination, Source, FuzzBuffer, bufpitch, bufpitch * (bufheight - 1) + bufwidth);
}

static void burn_Flat(pixel *Destination, pixel *Source)
{
	int count = bufpitch * bufheight;

	while (--count >= 0)
		*Destination++ = *Source++ * 31 >> 5;
}

static void burn_Squareish(pixel *destination, pixel *source)
{
	unsigned long xnoise_base = (rand() << 12) ^ rand();
	unsigned long ynoise = (rand() << 12) ^ rand();
	WarpData *map = WarpBuffer;
	int excess = bufpitch - bufwidth;
	int hcount = bufheight;

	while (--hcount >= 0)
	{
		int wcount = bufwidth;
		unsigned long xnoise = xnoise_base;

		ynoise = (ynoise * 16196459 + 16196129) & 0xFFFFFFFF;

		while (--wcount >= 0)
		{
			unsigned int x, y;

			xnoise = (xnoise * 16196231 + 16196119) & 0xFFFFFFFF;

			x = (map->xoffset + (xnoise >> 15) - (xnoise >> 18) - 0xE000) >> 16;
			y = (map->yoffset + (ynoise >> 15) - (ynoise >> 18) - 0xE000) >> 16;
			
			if ((unsigned)x < (unsigned)bufwidth && (unsigned)y < (unsigned)bufheight)
				*destination = source[bufpitch * y + x] * 63 >> 6;
			else
				*destination = 0;
			destination++;
			map++;
		}
		destination += excess;
	}
}


static void burn_Flakeish(pixel *destination, pixel *source)
{
	unsigned long xnoise_base = (rand() << 12) ^ rand();
	unsigned long ynoise = (rand() << 12) ^ rand();
	WarpData *map = WarpBuffer;
	int excess = bufpitch - bufwidth;
	int hcount = bufheight;

	while (--hcount >= 0)
	{
		int wcount = bufwidth;
		unsigned long xnoise = xnoise_base;

		ynoise = (ynoise * 16196459 + 16196129) & 0xFFFFFFFF;

		while (--wcount >= 0)
		{
			unsigned int x, y;

			xnoise = (xnoise * 16196231 + 16196119) & 0xFFFFFFFF;

			x = (map->xoffset + (ynoise >> 15) - (ynoise >> 18) - 0xE000) >> 16;
			y = (map->yoffset + (xnoise >> 15) - (xnoise >> 18) - 0xE000) >> 16;
			
			if ((unsigned)x < (unsigned)bufwidth && (unsigned)y < (unsigned)bufheight)
			{
				x = source[bufpitch * y + x];
				if (x > 0) x--;
				*destination = x;
			}
			else
				*destination = 0;
			destination++;
			map++;
		}
		destination += excess;
	}
}


static void burn_Lake(pixel *Destination, pixel *Source)
{
	LakeRaw(Destination, Source, bufpitch, bufpitch * (bufheight - 1) + bufwidth);
}

static void burn_Cockup(pixel *Destination, pixel *Source)
{
	CockupRaw(Destination, Source, FuzzBuffer, bufpitch, bufpitch * (bufheight - 1) + bufwidth);
}

static void burn_Test(pixel *Destination, pixel *Source)
{
	TestRaw(Destination, Source, FuzzBuffer, bufpitch, bufpitch * (bufheight - 1) + bufwidth);
}

/* ========================================================================= */

static void SyncSquare(int X, int Y, int W, int H)
{
	WarpData	       *warpptr;
	FuzzData	       *fuzzptr;
	unsigned int		xint,
				yint;
	int			x,
				y;
	unsigned int		xp_width,
				xp_height;

	W += 1 + (X & 1);
	X &= ~1;
	W &= ~1;

	xp_width = bufwidth << 16;
	xp_height = bufheight << 16;


	warpptr = WarpBuffer + Y * bufwidth + X;
	fuzzptr = FuzzBuffer + Y * bufpitch + X;

	y = H;
	while (--y >= 0)
	{
		x = W;
		while (--x >= 0)
		{
			xint = warpptr->xoffset + 0x8000 / RATIO_POINTS;
			yint = warpptr->yoffset + 0x8000 / RATIO_POINTS;
			if (xint < xp_width && yint < xp_height)
			{
				fuzzptr->scaleptr = &LU_Scales(xint, yint);
				xint >>= 16;
				yint >>= 16;
				xint--;
				yint--;
				fuzzptr->offset = (signed)yint * bufpitch + ((signed)xint & ~1);
			}
			else
			{
				fuzzptr->scaleptr = &NullScale;
				fuzzptr->offset = 0;
			}

			warpptr++;
			fuzzptr++;
		}
		warpptr += bufwidth - W;
		fuzzptr += bufpitch - W;
	}
}


static void DoFuzzSquare(int X, int Y)
{
	WarpData	       *warpptr;
	warp_real		oldx,
				oldxsave,
				oldy,
				inc,
				newx,
				newy,
				xcen,
				ycen;
	int			x,
				y;

	inc = (0x2000000 + longedge) / (longedge + longedge);
	xcen = (warp_real)0x8000 * bufwidth / longedge;
	ycen = (warp_real)0x8000 * bufheight / longedge;

	oldx = (((warp_real)X << 24) + 0x800000) / longedge - (xcen << 8);
	oldy = (((warp_real)Y << 24) + 0x800000) / longedge - (ycen << 8);
	oldxsave = oldx;

	warpptr = WarpBuffer + Y * bufwidth + X;

	y = SQUARE_SIZE;
	while (--y >= 0)
	{
		oldx = oldxsave;
		x = SQUARE_SIZE;
		while (--x >= 0)
		{
			newx = oldx >> 8;
			newy = oldy >> 8;
			Warp(&newx, &newy, randval);
			wraparound(newx);
			wraparound(newy);
			warpptr->xoffset = (newx + xcen) * longedge;
			warpptr->yoffset = (newy + ycen) * longedge;
			warpptr++;
			oldx += inc;
		}
		warpptr += bufwidth - SQUARE_SIZE;
		oldy += inc;
	}

	if (BurnPtr->Type == BI_FLAMER)
		SyncSquare(X, Y, SQUARE_SIZE, SQUARE_SIZE);
}

static void LFSR_setup(LFSR_state *state, unsigned int period, unsigned long seed)
{
	/* data available from: http://www.ece.cmu.edu/~koopman/lfsr/index.html */
	struct
	{
		unsigned long period, feedback[8];
	} stuff[] = {
		{     0x000F, { 0x0009, 0x000C, 0x0009, 0x000C, 0x0009, 0x000C, 0x0009, 0x000C } },
		{     0x003F, { 0x0021, 0x002D, 0x0030, 0x0033, 0x0036, 0x0039, 0x002D, 0x0036 } },
		{     0x00FF, { 0x008E, 0x0096, 0x00AF, 0x00B2, 0x00C3, 0x00C6, 0x00E1, 0x00FA } },
		{     0x03FF, { 0x0204, 0x026B, 0x02A1, 0x02FB, 0x0327, 0x037E, 0x03AA, 0x03FC } },
		{     0x0FFF, { 0x0829, 0x09D4, 0x0B23, 0x0C77, 0x0CB2, 0x0CC5, 0x0D70, 0x0D8F } },
		{     0x3FFF, { 0x2015, 0x2079, 0x2130, 0x21F5, 0x22F3, 0x2362, 0x241D, 0x2496 } },
		{     0xFFFF, { 0x8016, 0x80D0, 0x80E3, 0x8248, 0x845F, 0x85A8, 0x864A, 0x8679 } },
		{   0xFFFFFF, { 0x80000D, 0x80012B, 0x800221, 0x80044E, 0x80055D, 0x80067F, 0x8007E4, 0x800916 } },
		{ 0xFFFFFFFF, { 0x80000057, 0x80000375, 0x80000417, 0x800005F1, 0x800007D4, 0x80000923, 0x80000B71, 0x80000EA6 } },
	}, *stuffptr = stuff;

	while (stuffptr->period < period)
		stuffptr++;

	state->period = period;
	state->feedback = stuffptr->feedback[(seed >> 16) & 7];
	state->twiddle = seed % period;
	state->bits = (seed >> 8) % stuffptr->period + 1;

	printf("period: %d  feedback: 0x%04lX  twiddle: %lu  bits: 0x%04lX\n", state->period, state->feedback, state->twiddle, state->bits);
}

static int LFSR_next(LFSR_state *state)
{
	do
	{
		int bit = state->bits & 1;
		state->bits >>= 1;
		if (bit)
			state->bits ^= state->feedback;
	} while (state->bits > state->period);

	return (state->bits + state->twiddle) % state->period;
}

static int AdvanceFuzz(int reset)
{
	static LFSR_state	state;
	static int		numsquares = 0,
				count = 0,
				hcount = 0,
				vcount = 0;
	int			i,
				j,
				x,
				y;

	if (BurnPtr->Type == BI_NOTHING)
		return 0;

	if (reset || numsquares == 0)
	{
		hcount = (bufwidth + SQUARE_SIZE - 1) / SQUARE_SIZE;
		vcount = (bufheight + SQUARE_SIZE - 1) / SQUARE_SIZE;
		numsquares = hcount * vcount;

		LFSR_setup(&state, numsquares, (rand() >> 4) ^ (rand() << 8) ^ (rand() << 20));
		count = 0;
	}


	for (j = numsquares / 64u; j >= 0; j--)
	{
		if (count >= numsquares || Warp == NULL)
			return 1;

		i = LFSR_next(&state);

		x = (int)(i % hcount) * (bufwidth - SQUARE_SIZE) / (hcount - 1);
		y = (int)(i / hcount) * (bufheight - SQUARE_SIZE) / (vcount - 1);

		DoFuzzSquare(x, y);

		count++;
	}

	return 0;
}

static void MakeRatioTable(struct ratiotable_t *ptr, float focus, float decay)
{
	int			i, j;
	unsigned int		ratios_left[RATIO_POINTS],
				ratios_right[RATIO_POINTS];

	ptr->decay_scale = decay * 65536;
	decay = ptr->decay_scale;

	for (i = 0; i < RATIO_POINTS / 2; i++)
	{
		float n, w;
		n = (float)i / RATIO_POINTS - 0.5;
		w = 0.5 * i / RATIO_POINTS;
		ratios_left[i] = decay * ((1.0 - focus) * (0.5 - w) + focus * -n);
		ratios_right[i] = decay * (1.0 - focus) * w;
	}
	for (i = RATIO_POINTS / 2; i < RATIO_POINTS; i++)
	{
		float n, w;
		n = (float)i / RATIO_POINTS - 0.5;
		w = 0.5 * i / RATIO_POINTS;
		ratios_left[i] = decay * (1.0 - focus) * (0.5 - w);
		ratios_right[i] = decay * ((1.0 - focus) * w + focus * n);
	}

	for (i = 0; i < RATIO_POINTS; i++)
	{
		unsigned long		line1, line2, line3;

		line1 = ratios_left[i];
		line3 = ratios_right[i];
		line2 = ptr->decay_scale - (line1 + line3);

		for (j = 0; j < RATIO_POINTS; j++)
		{
			unsigned long		col1, col2, col3;

			col1 = ratios_left[j];
			col3 = ratios_right[j];
			col2 = ptr->decay_scale - (col1 + col3);

			ptr->scales[0][i][j].tl = (col1 * line1 + 0x800000) >> 24;
			ptr->scales[0][i][j].tc = (col2 * line1 + 0x800000) >> 24;
			ptr->scales[0][i][j].tr = (col3 * line1 + 0x800000) >> 24;
			ptr->scales[0][i][j].tz = 0;
			ptr->scales[0][i][j].cl = (col1 * line2 + 0x800000) >> 24;
			ptr->scales[0][i][j].cr = (col3 * line2 + 0x800000) >> 24;
			ptr->scales[0][i][j].cz = 0;
			ptr->scales[0][i][j].bl = (col1 * line3 + 0x800000) >> 24;
			ptr->scales[0][i][j].bc = (col2 * line3 + 0x800000) >> 24;
			ptr->scales[0][i][j].br = (col3 * line3 + 0x800000) >> 24;
			ptr->scales[0][i][j].bz = 0;

			ptr->scales[0][i][j].cc = ((ptr->decay_scale + 128) >> 8) -
					(ptr->scales[0][i][j].tl + ptr->scales[0][i][j].tc + ptr->scales[0][i][j].tr
					+ ptr->scales[0][i][j].cl + ptr->scales[0][i][j].cr
					+ ptr->scales[0][i][j].bl + ptr->scales[0][i][j].bc + ptr->scales[0][i][j].br);

			memcpy((char *)&ptr->scales[1][i][j] + 1, &ptr->scales[0][i][j], sizeof(ptr->scales[0][i][j]) - 1);
			ptr->scales[1][i][j].tl = 0;
		}
	}
}

 /* //////////////////////////////////////////////////////////////////////// */

void SetupBurners(char **argv, int width, int height, int pitch)
{
	MakeRatioTable(&ShortFlameData, 0.05,  0.93);
	MakeRatioTable(&LongFlameData, 0.05,  0.995);

	RatioTable = &ShortFlameData;

	Burn = burn_Flame;
	ResizeBurners(width, height, pitch);
}

void ResizeBurners(int width, int height, int pitch)
{
	int			i;

	free(FuzzBuffer);
	free(WarpBuffer);

	WarpBuffer = calloc(width * height, sizeof(*WarpBuffer));
	if (WarpBuffer == NULL)
	{
		fprintf(stderr, "burner: Failed to allocate warp buffer.\n");
		exit(EXIT_FAILURE);
	}
	FuzzBuffer = calloc(pitch * height, sizeof(*FuzzBuffer));
	if (FuzzBuffer == NULL)
	{
		fprintf(stderr, "burner: Failed to allocate fuzz buffer.\n");
		exit(EXIT_FAILURE);
	}

	bufwidth = width;
	bufheight = height;
	bufpitch = pitch;
	longedge = (bufwidth > bufheight) ? bufwidth : bufheight;

	i = height * pitch; 
	while (--i >= 0)
		FuzzBuffer[i].scaleptr = &NullScale;

	AdvanceFuzz(1);
	while (AdvanceFuzz(0) == 0)
		continue;
}

void ShutdownBurners(void)
{
	free(FuzzBuffer);
	free(WarpBuffer);
	FuzzBuffer = NULL;
	WarpBuffer = NULL;
}

const char *SelectBurner(const char *name)
{
	BurnerInfo	       *ptr;


	if (name == NULL)
		BurnPtr = Burners + (rand() >> 8) % (sizeof(Burners) / sizeof(*Burners));
	else
		for (ptr = Burners; ptr < Burners + sizeof(Burners) / sizeof(*Burners); ptr++)
			if (strcmp(ptr->Name, name) == 0)
			{
				BurnPtr = ptr;
				break;
			}

	Burn = BurnPtr->Func;

	switch (BurnPtr->Type)
	{
	case BI_FLAMER:
		RatioTable = BurnPtr->Data;
		SyncSquare(0, 0, bufwidth, bufheight);
		break;
	default:
		break;
	}
	return BurnPtr->Name;
}

const char *CycleBurnerUp(void)
{
	BurnPtr++;

	if (BurnPtr >= Burners + sizeof(Burners) / sizeof(*Burners))
		BurnPtr = Burners;

	Burn = BurnPtr->Func;
	
	switch (BurnPtr->Type)
	{
	case BI_FLAMER:
		RatioTable = BurnPtr->Data;
		SyncSquare(0, 0, bufwidth, bufheight);
		break;
	default:
		break;
	}
	return BurnPtr->Name;
}

const char *CycleBurnerDown(void)
{
	BurnPtr--;

	if (BurnPtr < Burners)
		BurnPtr = Burners + sizeof(Burners) / sizeof(*Burners) - 1;

	Burn = BurnPtr->Func;

	switch (BurnPtr->Type)
	{
	case BI_FLAMER:
		RatioTable = BurnPtr->Data;
		SyncSquare(0, 0, bufwidth, bufheight);
		break;
	default:
		break;
	}
	return BurnPtr->Name;
}

void UpdateBurner(void)
{
	static void	      (*OldWarp)(warp_real *x, warp_real *y, unsigned int c) = NULL;

	if (Warp != OldWarp)
	{
		randval = ((rand() << 24) ^ (rand() << 12) ^ rand() ^ (rand() >> 12)) & 0x7FFFFFFF;
		OldWarp = Warp;
		AdvanceFuzz(1);
	}
	else
		AdvanceFuzz(0);
}

void BurnerKeypress(int key)
{
}

