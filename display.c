#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <SDL.h>
#include <stdarg.h>

#include "display.h"

#define YUV_DOUBLE

static unsigned long	flags;
static int		windowwidth,
			windowheight;
static int		ratelimit = 30;

static int		bufwidth,
			bufheight,
			bufpitch;

static SDL_TimerID	timerid;
static SDL_Surface     *Screen;
static SDL_Overlay     *Overlay;
static SDL_Color	Palette[256];
static unsigned char	Alpha[256];
static struct { unsigned char y, u, y2, v; }
			YUVPalette[256];
static int		paldirtylow,
			paldirtyhigh;

static char	 const *hardtitle = NULL;


static void resizewindow(int width, int height)
{
	windowwidth = width;
	windowheight = height;
	OpenDisplay();
}

static int resizeoverlay(int width, int height)
{
#if defined(YUV_DOUBLE)
	width *= 2;
#endif
	if (Overlay != NULL && (Overlay->w != width || Overlay->h != height))
	{
		SDL_FreeYUVOverlay(Overlay);
		Overlay = NULL;
	}

	if (Overlay == NULL)
	{
		static const struct { Uint32 val; const char * const name; } formats[] =
		{
			{ SDL_YUY2_OVERLAY, "YUY2" },
			{ SDL_UYVY_OVERLAY, "UYVY" },
			{ SDL_YVYU_OVERLAY, "YVYU" },
			{ SDL_YV12_OVERLAY, "YV12" },
			{ SDL_IYUV_OVERLAY, "IYUV" },
			{ SDL_YUY2_OVERLAY, "YUY2 (default)" }
		};
		int i;

		for (i = 0; i < sizeof(formats) / sizeof(*formats); i++)
		{
			if (Overlay != NULL)
				SDL_FreeYUVOverlay(Overlay);
			Overlay = SDL_CreateYUVOverlay(width, height, formats[i].val, Screen);

			if (Overlay == NULL)
			{
				fprintf(stderr, "SDL_CreateYUVOverlay() failed: %s\n", SDL_GetError());
				return -1;
			}

			if (Overlay->hw_overlay)
			{
				fprintf(stderr, "overlay format: %s\n", formats[i].name);
				return 0;
			}
		}
		fprintf(stderr, "using non-hardware overlay: %s\n", formats[i - 1].name);
	}

	return 0;
}


static int Pal2YUV(unsigned char *source, int width, int height, int memwidth)
{
	unsigned char	       *dest,
			       *destu,
			       *destv;
	int			uplane,
				vplane;
	int			x,
				y;

	if (SDL_LockYUVOverlay(Overlay) < 0)
	{
		fprintf(stderr, "SDL_LockOverlay failed: %s\n", SDL_GetError());
		return -1;
	}
	
	switch (Overlay->format)
	{
	case SDL_YV12_OVERLAY:
	case SDL_IYUV_OVERLAY:
		uplane = (Overlay->format == SDL_IYUV_OVERLAY) ? 1 : 2;
		vplane = (Overlay->format == SDL_YV12_OVERLAY) ? 1 : 2;
		dest = Overlay->pixels[0];
		destu = Overlay->pixels[uplane];
		destv = Overlay->pixels[vplane];

		for (y = 0; y < height; y++)
		{
			for (x = 0; x < width; x++)
			{
				*dest++ = YUVPalette[*source].y;
#if defined(YUV_DOUBLE)
				*dest++ = (YUVPalette[*source].y + YUVPalette[*(source + 1)].y) / 2;
#endif
				source++;
			}

			if ((y & 1) == 0)
			{
				source -= width;
				for (x = 0; x < width; x += 2)
				{
					*destu++ = YUVPalette[*source].u;
					*destv++ = YUVPalette[*source].v;
					source++;
#if defined(YUV_DOUBLE)
					*destu++ = YUVPalette[*source].u;
					*destv++ = YUVPalette[*source].v;
#endif
					source++;
				}
#if defined(YUV_DOUBLE)
				destu += Overlay->pitches[uplane] - width;
				destv += Overlay->pitches[vplane] - width;
#else
				destu += Overlay->pitches[uplane] - width / 2;
				destv += Overlay->pitches[vplane] - width / 2;
#endif
			}
			source += memwidth - width;
#if defined(YUV_DOUBLE)
			dest += Overlay->pitches[0] - width * 2;
#else
			dest += Overlay->pitches[0] - width;
#endif
		}
		break;

	case SDL_YUY2_OVERLAY:
	case  SDL_UYVY_OVERLAY:
	case   SDL_YVYU_OVERLAY:
		dest = Overlay->pixels[0];
		destu = dest + 1;
		destv = dest + 3;
		switch (Overlay->format)
		{
		case SDL_UYVY_OVERLAY:
			dest++;
			destu = dest - 1;
			destv = dest + 1;
			break;
		case SDL_YVYU_OVERLAY:
			destu = dest + 3;
			destv = dest + 1;
			break;
		}

		for (y = 0; y < height; y++)
		{
			for (x = 0; x < width; x += 2)
			{
				*dest = YUVPalette[*source].y;
				dest += 2;
				*destu = YUVPalette[*source].u;
				destu += 4;
#if defined(YUV_DOUBLE)
				*dest = (YUVPalette[*source].y + YUVPalette[*(source + 1)].y) / 2;
#else
				*dest = YUVPalette[*(source + 1)].y;
#endif
				dest += 2;
				*destv = YUVPalette[*source].v;
				destv += 4;
				source++;
#if defined(YUV_DOUBLE)
				*dest = YUVPalette[*source].y;
				dest += 2;
				*destu = YUVPalette[*source].u;
				destu += 4;
				*dest = (YUVPalette[*source].y + YUVPalette[*(source + 1)].y) / 2;
				dest += 2;
				*destv = YUVPalette[*source].v;
				destv += 4;
#endif
				source++;
			}
			source += memwidth - width;
#if defined(YUV_DOUBLE)
			dest += Overlay->pitches[0] - width * 4;
			destu += Overlay->pitches[0] - width * 4;
			destv += Overlay->pitches[0] - width * 4;
#else
			dest += Overlay->pitches[0] - width * 2;
			destu += Overlay->pitches[0] - width * 2;
			destv += Overlay->pitches[0] - width * 2;
#endif
		}
		break;

	default:
		fprintf(stderr, "Unsupported YUV format\n");
		SDL_UnlockYUVOverlay(Overlay);
		return -1;
	}

	SDL_UnlockYUVOverlay(Overlay);

	return 0;
}


static void StretchLine(unsigned char *dest, unsigned char *source, int dlen, int slen)
{
	unsigned int		acc,
				step;

	step = (slen << 16) / dlen;
	acc = step / 2;

	while (dlen-- > 0)
	{
		*dest++ = source[0] + (((source[1] - source[0]) * acc) >> 16);
		acc += step;
		source += acc >> 16;
		acc &= 0xFFFFu;
	}
}

static void MixLine(unsigned char *dest, unsigned char *source1, unsigned char *source2, int dlen, unsigned int mix)
{
	while (dlen-- > 0)
	{
		*dest++ = *source1 + (((*source2 - *source1) * mix) >> 16);
		source1++;
		source2++;
	}
}


static void StretchRect(unsigned char *dest, int dwid, int dhi, int dpit, unsigned char *source, int swid, int shi, int spit)
{
	unsigned char	       *holder1,
			       *holder2;
	unsigned int		acc,
				step;


	if ((holder1 = malloc(dwid * 2)) == NULL)
		return;

	holder2 = holder1 + dwid;

	step = (shi << 16) / dhi;
	acc = 0x10000 + step / 2;
	source -= spit;

	while (dhi-- > 0)
	{
		if (acc > 0xFFFFu)
		{
			source += (acc >> 16) * spit;
			acc &= 0xFFFFu;

			StretchLine(holder1, source, dwid, swid);
			StretchLine(holder2, source + spit, dwid, swid);
		}
		MixLine(dest, holder1, holder2, dwid, acc);
		dest += dpit;
		acc += step;
	}

	free(holder1);
}

static Uint32 mytimer(Uint32 interval, void *param)
{
	SDL_Event ev = { SDL_USEREVENT };
	SDL_PushEvent(&ev);
	return interval;
}


static int SaveTGA(SDL_Surface *surface, const char *file)
{
	FILE		       *fptr;
	int			value = 0;
	unsigned char		tgapreamble[12] = { 0, 0, 2, 0, 0, 0, 0, 0,  0, 0, 0, 0 },
				tgapostamble[2] = { 32, 8 };

	if ((fptr = fopen(file, "wb")) == NULL)
		return -1;

	fwrite(tgapreamble, 1, sizeof(tgapreamble), fptr);
	fputc((int)(unsigned char)(surface->w), fptr);
	fputc((int)(unsigned char)(surface->w >> 8), fptr);
	fputc((int)(unsigned char)(surface->h), fptr);
	fputc((int)(unsigned char)(surface->h >> 8), fptr);
	fwrite(tgapostamble, 1, sizeof(tgapostamble), fptr);

	if (SDL_LockSurface(surface) >= 0)
	{
		unsigned char	       *src = surface->pixels;
		int			linec = bufheight,
					colc;

		while (--linec >= 0)
		{
			colc = surface->w;
			while (--colc >= 0)
			{
				fputc((int)Palette[*src].b, fptr);
				fputc((int)Palette[*src].g, fptr);
				fputc((int)Palette[*src].r, fptr);
				fputc((int)Alpha[*src] ^ 0xFF, fptr);
				src++;
			}
			src += surface->pitch - surface->w;
		}

		SDL_UnlockSurface(surface);
	}
	else
		value = -1;

	fclose(fptr);

	return value;
}


void OpenDisplay(void)
{
	if (Overlay != NULL)
	{
		SDL_FreeYUVOverlay(Overlay);
		Overlay = NULL;
	}

	if (timerid != NULL)
	{
		SDL_RemoveTimer(timerid);
		timerid = NULL;
	}

	if ((Screen = SDL_SetVideoMode(windowwidth, windowheight, (flags & SDL_ANYFORMAT) ? 0 : 8, flags)) == NULL)
	{
		fprintf(stderr, "Couldn't initialise video mode: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	if (ratelimit > 5 && (timerid = SDL_AddTimer(ratelimit, mytimer, NULL)) == NULL)
	{
		fprintf(stderr, "Couldn't add timer: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	if (flags & SDL_ANYFORMAT)
		resizeoverlay(bufwidth, bufheight);

	paldirtylow = 0;
	paldirtyhigh = 255;
}


void SetDisplayName(const char *shortname, const char *longfmt, ...)
{
	char longname[128];
	va_list argptr;

	if (hardtitle != NULL)
	{
		SDL_WM_SetCaption(hardtitle, hardtitle);
		return;
	}

	va_start(argptr, longfmt);
	vsnprintf(longname, sizeof(longname), longfmt, argptr);
	va_end(argptr);

	SDL_WM_SetCaption(longname, shortname);
}

void ToggleFullscreen(void)
{
	flags ^= SDL_FULLSCREEN;
	OpenDisplay();
}

int SaveSnapshot(char *fmt)
{
	static int sequence = 0;
	char tmpname[256];
	struct stat nothing;

	do
		snprintf(tmpname, sizeof(tmpname), fmt, ++sequence);
	while (stat(tmpname, &nothing) == 0 && sequence > 0);

	if (strrchr(tmpname, '.') && strcasecmp(strrchr(tmpname, '.') + 1, "bmp") == 0)
	{
		if (SDL_SaveBMP(Screen, tmpname) < 0)
			return -1;
	}
	else
	{
		if (SaveTGA(Screen, tmpname) < 0)
			return -1;
	}
	return sequence;
}

void SetPaletteEntry(int i, unsigned char r, unsigned char g, unsigned char b)
{
	SetPaletteEntryA(i, r, g, b, 0xFF);
}

void SetPaletteEntryA(int i, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	int y, u, v;

#if 0
	y =  (0.257 * r) + (0.504 * g) + (0.098 * b) + 16;
	u = -(0.148 * r) - (0.291 * g) + (0.439 * b) + 128;
	v =  (0.439 * r) - (0.368 * g) - (0.071 * b) + 128;
#else
	y =  (0.257 * r) + (0.504 * g) + (0.098 * b) + 16;
	u = -(0.148 * r) - (0.291 * g) + (0.439 * b) + 128;
	v =  (0.439 * r) - (0.368 * g) - (0.071 * b) + 128;
#endif

	Palette[i].r = r;
	Palette[i].g = g;
	Palette[i].b = b;
	Alpha[i] = a;

	YUVPalette[i].y = y;
	YUVPalette[i].u = u;
	YUVPalette[i].y2 = y;
	YUVPalette[i].v = v;

	if (i < paldirtylow)
		paldirtylow = i;
	if (i > paldirtyhigh)
		paldirtyhigh = i;
}

void Display(unsigned char *source)
{
	if (Overlay != NULL)
	{
		SDL_Rect rect = { 0, 0, 0, 0 };

		if (Pal2YUV(source, bufwidth, bufheight, bufpitch) >= 0)
		{
			rect.w = windowwidth;
			rect.h = windowheight;
			if (SDL_DisplayYUVOverlay(Overlay, &rect) < 0)
				fprintf(stderr, "SDL_DisplayYUVOverlay failed: %s\n", SDL_GetError());
		}
	}
	else
	{
		if (SDL_LockSurface(Screen) < 0)
		{
			fprintf(stderr, "SDL_LockSurface failed: %s\n", SDL_GetError());
			return;
		}

		if (flags & SDL_RESIZABLE)
			StretchRect(Screen->pixels, Screen->w, Screen->h, Screen->pitch,
					source, bufwidth, bufheight, bufpitch);
		else
		{
			unsigned char	       *dest = Screen->pixels;
			int count = bufheight;

			while (--count >= 0)
			{
				memcpy(dest, source, Screen->w);
				dest += Screen->pitch;
				source += bufpitch;
			}
		}

		SDL_UnlockSurface(Screen);
		if (paldirtylow < paldirtyhigh)
		{
			SDL_SetColors(Screen, Palette, paldirtylow, paldirtyhigh - paldirtylow + 1);
			paldirtylow = 256;
			paldirtyhigh = -1;
		}
		SDL_Flip(Screen);
	}
}

int GetKey(void)	/* very bad implementation... but it works and I'm lazy */
{
	SDL_Event		nextevent;
	int		      (*pollmode)(SDL_Event *event) = ratelimit > 5 ? SDL_WaitEvent : SDL_PollEvent;

	while (pollmode(&nextevent))
		switch (nextevent.type)
		{
		case SDL_KEYDOWN:
			if (nextevent.key.keysym.sym == SDLK_ESCAPE)
				return 'q';
			return nextevent.key.keysym.unicode;

		case SDL_KEYUP:
			break;

		case SDL_MOUSEMOTION:
			{
				static int glitch = 10;
				if (--glitch > 0)
					break;
			}
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
		case SDL_QUIT:
			return 'q';

		case SDL_VIDEORESIZE:
			if (flags & SDL_RESIZABLE)
				resizewindow(nextevent.resize.w, nextevent.resize.h);
			break;
		case SDL_USEREVENT:
			pollmode = SDL_PollEvent;
			break;
		}

	return 0;
}

void SetupDisplay(char **argv, int width, int height, int pitch)
{
	int touchymouse = 0;
	flags = SDL_HWPALETTE | SDL_HWSURFACE | SDL_DOUBLEBUF;
	windowwidth = bufwidth = width;
	windowheight = bufheight = height;
	bufpitch = pitch;

	if (argv != NULL)
	{
		char **wb_argv = argv;

		while (*argv != NULL)
		{
			     if (strcmp(*argv, "-fullscreen") == 0)
				flags |= SDL_FULLSCREEN;
			else if (strcmp(*argv, "-ratelimit") == 0)
				ratelimit = atoi(*++argv);
			else if (strcmp(*argv, "-scale") == 0)
				flags |= SDL_RESIZABLE;
			else if (strcmp(*argv, "-yuvscale") == 0)
				flags |= SDL_RESIZABLE | SDL_ANYFORMAT;
			else if (strcmp(*argv, "-geometry") == 0)
			{
				if (sscanf(*++argv, "%dx%d", &windowwidth, &windowheight) != 2
					|| windowwidth < 80 || windowheight < 60
					|| windowwidth > 2048 || windowheight > 2048)
				{
					fprintf(stderr, "Bad geometry: %s\n", *argv);
					exit(EXIT_FAILURE);
				}

				flags |= SDL_RESIZABLE;
			}
			else if (strcmp(*argv, "-windowid") == 0)
			{
				static char tmp[64];
				sprintf(tmp, "SDL_WINDOWID=%.50s", *++argv);
				putenv(tmp);
			}
			else if (strcmp(*argv, "-singlebuf") == 0)
				flags &= ~SDL_DOUBLEBUF;
			else if (strcmp(*argv, "-swpalette") == 0)
				flags &= ~SDL_HWPALETTE;
			else if (strcmp(*argv, "-swsurface") == 0)
				flags = (flags & ~SDL_HWSURFACE) | SDL_SWSURFACE;
			else if (strcmp(*argv, "-touchymouse") == 0)
				touchymouse = 1;
			else if (strcmp(*argv, "-hardtitle") == 0)
				hardtitle = *++argv;
			else
				*wb_argv++ = *argv;
			argv++;
		}
		*wb_argv = NULL;
	}

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
	{
		fprintf(stderr, "Couldn't initialise SDL: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	SDL_EnableUNICODE(1);
	SDL_EventState(SDL_ACTIVEEVENT, SDL_IGNORE);
	SDL_EventState(SDL_MOUSEMOTION, touchymouse ? SDL_ENABLE : SDL_IGNORE);
	SDL_EventState(SDL_USEREVENT, SDL_ENABLE);
	SDL_ShowCursor(SDL_DISABLE);
}


void ResizeDisplay(int width, int height, int pitch)
{
	bufwidth = width;
	bufheight = height;
	bufpitch = pitch;

	if ((flags & SDL_RESIZABLE) == 0)
		resizewindow(width, height);
	else if (flags & SDL_ANYFORMAT)
		resizeoverlay(width, height);
}

void ShutdownDisplay(void)
{
	if (Overlay != NULL)
	{
		SDL_FreeYUVOverlay(Overlay);
		Overlay = NULL;
	}
	SDL_RemoveTimer(timerid);
	timerid = NULL;
	SDL_Quit();
	Screen = NULL;
}

