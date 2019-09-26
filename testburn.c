#include "burners.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef struct
{
	signed int		xoffset,
				yoffset;
} warpdata;

#define import(x) ((x))
#define export(x) ((x))

extern void		FlameRaw(pixel *destination, pixel *source, void *map, int memwidth, int count);

static int 		tmpsize = -1;
static pixel	       *tmpbuf = NULL,
		       *tmpbufbase = NULL;

void LakeRaw(pixel *destination, pixel *source, int memwidth, int count)
{
	pixel *presource = destination;
	int hcount = count / memwidth;
	int width = (memwidth - 1) & ~7;
	static int noise = 512;

	if (tmpsize != count || tmpbuf == NULL)
	{
		free(tmpbufbase);
		tmpsize = count;
		tmpbufbase = calloc(count + memwidth + memwidth + 64, sizeof(pixel));
		tmpbuf = tmpbufbase + memwidth;
	}
	assert(tmpbuf);

	memcpy(tmpbuf, destination, count);
	presource = tmpbuf;

	while (--hcount >= 0)
	{
		int wcount = width;
		while (--wcount >= 0)
		{
			int sum = import(source[-1-memwidth]) + import(source[ 0-memwidth]) + import(source[ 1-memwidth])
				+ import(source[-1         ]) +                             + import(source[ 1         ])
				+ import(source[-1+memwidth]) + import(source[ 0+memwidth]) + import(source[ 1+memwidth]);

			sum -= ((import(*presource) * 4 +
				import(presource[-memwidth]) + import(presource[-1]) +
				import(presource[1]) + import(presource[memwidth])) >> 1)
				+ (0x80 << 2);
			sum *= 253;
			sum += noise;
			noise = sum & (1 << (8 + 2)) - 1;
			sum >>= (8 + 2);

			sum += 0x80;

			if (sum < 0x10) sum = 0x10;
			if (sum > 0xF0) sum = 0xF0;
			*destination++ = export(sum);
			presource++;
			source++;
		}
		destination += memwidth - width;
		presource += memwidth - width;
		source += memwidth - width;
	}
}

void CockupRaw(pixel *destination, pixel *source, warpdata *map, int memwidth, int count)
{
	pixel *presource = destination;
	int hcount = count / memwidth;
	int width = (memwidth - 1) & ~7;

	if (tmpsize != count || tmpbuf == NULL)
	{
		free(tmpbufbase);
		tmpsize = count;
		tmpbufbase = calloc(count + memwidth + memwidth + 64, sizeof(pixel));
		tmpbuf = tmpbufbase + memwidth;
	}
	assert(tmpbuf);

	FlameRaw(tmpbuf, presource, map, memwidth, count);
	presource = tmpbuf;

	while (--hcount >= 0)
	{
		int wcount = width;
		while (--wcount >= 0)
		{
			int sum = import(source[-1-memwidth]) + import(source[ 0-memwidth]) + import(source[ 1-memwidth])
				+ import(source[-1         ]) +                             + import(source[ 1         ])
				+ import(source[-1+memwidth]) + import(source[ 0+memwidth]) + import(source[ 1+memwidth]);
#if 0
			sum -= ((import(*presource) + 0x80) << 2);
#else
			sum += (import(*source) - import(*presource)) << 2;
#endif
			sum = (sum * 253) >> (8 + 2);

			sum += 0x80;

			if (sum < 0x10) sum = 0x10;
			if (sum > 0xF0) sum = 0xF0;
			*destination++ = export(sum);
			presource++;
			source++;
		}
		destination += memwidth - width;
		presource += memwidth - width;
		source += memwidth - width;
	}
}

void TestRaw(pixel *destination, pixel *source, warpdata *map, int memwidth, int count)
{
	pixel *presource = destination;
	int hcount = count / memwidth;
	int width = (memwidth - 1) & ~7;

	if (tmpsize != count || tmpbuf == NULL)
	{
		free(tmpbufbase);
		tmpsize = count;
		tmpbufbase = calloc(count + memwidth + memwidth + 64, sizeof(pixel));
		tmpbuf = tmpbufbase + memwidth;
		free(tmpbuf);
	}
	assert(tmpbuf);

	FlameRaw(tmpbuf, presource, map, memwidth, count);
	FlameRaw(presource, tmpbuf, map, memwidth, count);

	FlameRaw(tmpbuf, source, map, memwidth, count);
	source = tmpbuf;

	while (--hcount >= 0)
	{
		int wcount = width;
		while (--wcount >= 0)
		{
			int sum = import(source[-1-memwidth]) + import(source[ 0-memwidth]) + import(source[ 1-memwidth])
				+ import(source[-1         ]) +                             + import(source[ 1         ])
				+ import(source[-1+memwidth]) + import(source[ 0+memwidth]) + import(source[ 1+memwidth]);

			sum -= ((import(*presource) + 0x80) << 2);
			sum = (sum * 253) >> (8 + 2);

			sum += 0x80;

			if (sum < 0x10) sum = 0x10;
			if (sum > 0xF0) sum = 0xF0;
			*destination++ = export(sum);
			presource++;
			source++;
		}
		destination += memwidth - width;
		presource += memwidth - width;
		source += memwidth - width;
	}
}

