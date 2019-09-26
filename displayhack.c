#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#if defined(__APPLE__) || defined(__WIN32__)
#include <SDL.h>        /* linking problem workaround re: main() */
#endif

#include "displayhack.h"
#include "display.h"
#include "palettes.h"
#include "burners.h"
#include "warpers.h"
#include "seeds.h"

#include "gettsc.c"

#if !defined(__WIN32__)
static void sigchange(int signum);
static void sigdontchange(int signum);
#endif

static int		bufwidth, 
			bufheight,
			bufpitch;

static pixel	       *databuffer = NULL;

static pixel	       *bufpage0 = NULL,
		       *bufpage1 = NULL;


static int		allowvary = 1;
static const char       *changeto = NULL;
static int		varytick = 100;

static const char      *seedname = NULL,
		       *warpname = NULL,
		       *burnname = NULL,
		       *palname = NULL;

#if !defined(__WIN32__)
static void sigchange(int signum)
{
	allowvary = 1;
	varytick = 0;
}

static void sigdontchange(int signum)
{
	allowvary = 0;
}
#endif

static void resizethis(int width, int height)
{
	int			pitch = (width + 8 + 3) & ~3;
	pixel		       *newbuffer;
	pixel		       *tmpptr;

	if ((newbuffer = calloc(2 * pitch * (height + 4) + 32 + 4, sizeof(pixel))) == NULL)
	{
		perror("displayhack: calloc() for data buffer failed");
		exit(EXIT_FAILURE);
	}

	tmpptr = newbuffer;

	/* 32-byte alignment for cache happiness */
	tmpptr = (void *)((unsigned long)(tmpptr + 31) & ~31);
	/* two line clearance for top edge of top line */
	tmpptr += pitch + pitch;
	/* two byte clearance for upper-left corner of upper-left pixel */
	tmpptr += 2;

	if (databuffer != NULL)
	{
		pixel		       *buf0 = (void *)tmpptr,
				       *buf1 = (void *)(tmpptr + pitch * (height + 4));
		int			x,
					y,
					xacc,
					xstep,
					yacc,
					ystep;

		ystep = (bufheight << 16) / height;
		yacc = ystep / 2;

		y = height;
		while (--y >= 0)
		{
			if (yacc >= 0x10000)
			{
				bufpage0 += bufpitch * (yacc >> 16);
				bufpage1 += bufpitch * (yacc >> 16);
				yacc &= 0xFFFF;
			}
			xstep = (bufwidth << 16) / width;
			xacc = xstep / 2;
			x = width;
			while (--x >= 0)
			{
				*buf0++ = bufpage0[xacc >> 16];
				*buf1++ = bufpage1[xacc >> 16];
				xacc += xstep;
			}
			yacc += ystep;
			buf0 += pitch - width;
			buf1 += pitch - width;
		}
		free(databuffer);
	}
	databuffer = newbuffer;

	bufpage0 = (void *)tmpptr;
	bufpage1 = (void *)(tmpptr + pitch * (height + 4));

	bufwidth = width;
	bufheight = height;
	bufpitch = pitch;
}

void setupthis(char **argv)
{
	int			width = 800,
				height = 600;


	if (argv != NULL)
	{
		char **wb_argv = argv;
		while (*argv != NULL)
		{
			if (strcmp(*argv, "-width") == 0)
				width = atoi(*++argv);
			else if (strcmp(*argv, "-height") == 0)
				height = atoi(*++argv);
			else if (strcmp(*argv, "-seed") == 0)
				seedname = *++argv;
			else if (strcmp(*argv, "-warp") == 0)
				warpname = *++argv;
			else if (strcmp(*argv, "-burn") == 0)
				burnname = *++argv;
			else if (strcmp(*argv, "-palette") == 0)
				palname = *++argv;
			else if (strcmp(*argv, "-static") == 0)
				allowvary = 0;
			else
				*wb_argv++ = *argv;
			argv++;
		}
		*wb_argv = NULL;
	}

	if (width < 80 || width > 2048 || height < 60 || height > 1536)
	{
		fprintf(stderr, "Bad buffer dimensions: %dx%d\n", width, height);
		exit(EXIT_FAILURE);
	}
	bufpitch = width + 8;

	resizethis(width, height);
	srand((unsigned int)time(NULL));
#if !defined(__WIN32__)
	signal(SIGUSR1, sigchange);
	signal(SIGUSR2, sigdontchange);
#endif
}

static const char *cycleres(int dir)
{
	static char		tempname[16];
	static const int	screenmodes[3][2] =
	{
		{  80,  60 },
		{ 100,  75 },
		{ 128,  96 }
	};
	static int		mode = -1;
	int			width,
				height;

	if (mode == -1)
	{
		width = bufwidth;
		mode = 0;
		while (width > screenmodes[2][0])
		{
			width >>= 1;
			mode += 3;
		}
		if (width > screenmodes[0][0])
			mode++;
		if (width > screenmodes[1][0])
			mode++;
	}

	mode = (mode + dir + 15) % 15;
	width = screenmodes[mode % 3][0] << (mode / 3);
	height = screenmodes[mode % 3][1] << (mode / 3);

	resizethis(width, height);
	ResizeDisplay(bufwidth, bufheight, bufpitch);
	ResizeSeeds(bufwidth, bufheight, bufpitch);
	ResizeBurners(bufwidth, bufheight, bufpitch);

	sprintf(tempname, "%dx%d", bufwidth, bufheight);

	return tempname;
}

static const char *cycleresup(void) { return cycleres(1); }
static const char *cycleresdown(void) { return cycleres(-1); }

static int keypress(int key)
{
	static int keydest = 0;

	switch (key)
	{
	case 'a':
		printf("-palette \"%s\" -burn \"%s\" -warp \"%s\" -seed \"%s\"\n", palname, burnname, warpname, seedname);
		break;
	case 'f':	ToggleFullscreen();				break;
	case 'c':	changeto = palname = CyclePaletteUp();		break;
	case 'C':	changeto = palname = CyclePaletteDown();	break;
	case 'w':	changeto = warpname = CycleWarperUp();		break;
	case 'W':	changeto = warpname = CycleWarperDown();	break;
	case 'b':	changeto = burnname = CycleBurnerUp();		break;
	case 'B':	changeto = burnname = CycleBurnerDown();	break;
	case 's':	changeto = seedname = CycleSeedUp();		break;
	case 'S':	changeto = seedname = CycleSeedDown();		break;
	case 'r':	changeto = cycleresup();			break;
	case 'R':	changeto = cycleresdown();			break;
	case 'p':
		if (SaveSnapshot("snap%04d.bmp") < 0)
			changeto = "*write error*";
		else
			changeto = "*snap*";
		break;
	case ' ':
	case '\t':
		seedname = SelectSeed(NULL);
		warpname = SelectWarper(NULL);
		burnname = SelectBurner(NULL);
		palname = SelectPalette(NULL);
		changeto = "all-randomised";
		break;
	case '\r':
		allowvary = !allowvary;
		changeto = allowvary ? "automatic" : "manual";
		break;
	case 0:	break;
	default:
		if (key >= ' ')
		{
			switch (keydest)
			{
			case 'c':	PaletteKeypress(key);	break;
			case 'w':	WarperKeypress(key);	break;
			case 'b':	BurnerKeypress(key);	break;
			case 's':	SeedKeypress(key);	break;
			default: return -1;
			}
			key = keydest;
		}
		else return -1;
	}
	if (key > 0)
		keydest = key | 0x60;
	return 0;
}

static void profstage(tscsnap *endof)
{
	static tscsnap		prev = 0;
	tscsnap			this;

	this = GetTSC();
	if (endof != NULL)
		*endof += this - prev;
	prev = this;
}


int main(int argc, char *argv[])
{
	struct
	{
		tscsnap		wait,
					update,
					seed,
					burn,
					display,
					total;
	}			clocks = { 0 };
	time_t			runtime;
	long			frames = 0;
	int			key;
	int			titletick = 1;
	int			touchykeys = 0;


	if (argc == 0) /* there is no such thing as a command line, so make one up */
	{
		static char *new_argv[] = { __FILE__, "-doublebuf", "-width", "240", "-height", "320", NULL };
		argv = new_argv;
	}

#if defined(__WIN32__)

	if (argv[1] != NULL && argv[1][0] == '/')
	{
#if 0 /* can't do this because it requires windows.h, which conflicts with palette.h */
		if (FindWindow("WindowScreenSaver", NULL) != NULL)
			exit(1);
#endif
		putenv("SDL_WINDOWCLASS=WindowsScreenSaver");

		switch (argv[1][1])
		{
		case 'S': case 's':
			{ static char *replacement[] = { NULL,
						"-fullscreen",
						"-touchymouse",
						"-width", "800",
						"-height", "600", NULL };
			replacement[0] = argv[0];
			argv = replacement;
                        touchykeys = 1;
			} break;
#if 0 /* causes too much trouble */
		case 'P': case 'p':
			{ static char *replacement[] = { NULL,
						"-geometry", "160x120",
						"-hardtitle", "Preview", NULL };
			char tmpenv[80];
			replacement[0] = argv[0];
			sprintf(tmpenv, "SDL_WINDOWID=%.64s", argv[2]);
			putenv(tmpenv);
			putenv("SDL_VIDEODRIVER=windib");
			argv = replacement;
			} break;
#endif
		default:
			fprintf(stderr, "unknown argument: %s\n", argv[1]);
			exit(0);
		}
	}
#endif

	setupthis(argv);

	printf("display setup...\n");
	SetupDisplay(argv, bufwidth, bufheight, bufpitch);
	printf("seed setup...\n");
	SetupSeeds(argv, bufwidth, bufheight, bufpitch);
	printf("warper setup...\n");
	SetupWarpers(argv);
	printf("burner setup...\n");
	SetupBurners(argv, bufwidth, bufheight, bufpitch);
	printf("palette setup...\n");
	SetupPalettes(argv);

	if (argv[1] != NULL)
	{
		printf("unknown arguments:");
		while (*argv != NULL)
			printf(" %s", *argv++);
		printf("\n");
		exit(EXIT_FAILURE);
	}

	printf("initial seed: %s\n", seedname = SelectSeed(seedname));
	printf("initial warper: %s\n", warpname = SelectWarper(warpname));
	printf("initial burner: %s\n", burnname = SelectBurner(burnname));
	printf("initial palette: %s\n", palname = SelectPalette(palname));

	printf("opening display...\n");
	OpenDisplay();
	ResizeDisplay(bufwidth, bufheight, bufpitch);

	runtime = time(NULL);

	for (;;)
	{
		profstage(NULL);
		UpdateSeed();
		UpdatePalette();
		UpdateBurner();
		profstage(&clocks.update);

		Seed(bufpage0);
		profstage(&clocks.seed);

		Burn(bufpage1, bufpage0);
		profstage(&clocks.burn);

		Display(bufpage1);
		profstage(&clocks.display);

		frames++;
		if ((key = GetKey()) == 'q')
			break;
		if (keypress(key) < 0 && touchykeys != 0)
			break;
		profstage(&clocks.wait);

		profstage(NULL);
		UpdateSeed();
		UpdatePalette();
		UpdateBurner();
		profstage(&clocks.update);

		Seed(bufpage1);
		profstage(&clocks.seed);

		Burn(bufpage0, bufpage1);
		profstage(&clocks.burn);

		Display(bufpage0);
		profstage(&clocks.display);

		frames++;
		if ((key = GetKey()) == 'q')
			break;
		if (keypress(key) < 0 && touchykeys != 0)
			break;
		profstage(&clocks.wait);

		if (allowvary)
			varytick--;
		if (varytick <= 0)
		{
			switch ((rand() >> 8) % 4)
			{
			case 0:
				changeto = seedname = SelectSeed(NULL);
				break;
			case 1:
				changeto = warpname = SelectWarper(NULL);
				break;
			case 2:
				changeto = burnname = SelectBurner(NULL);
				break;
			case 3:
				changeto = palname = SelectPalette(NULL);
				break;
			}
			varytick = rand() % 300 + 100;
		}
		if (changeto != NULL)
		{
			SetDisplayName("displayhack", "displayhack: %s", changeto);
			changeto = NULL;
			titletick = 33;
		}
		if (titletick > 0)
			if (--titletick <= 0)
				SetDisplayName("displayhack", "displayhack");
	}

	runtime = time(NULL) - runtime;
	printf("closing display...\n");
	ShutdownDisplay();

	ShutdownPalettes();
	ShutdownBurners();
	ShutdownWarpers();
	ShutdownSeeds();

	clocks.total = clocks.update + clocks.seed
		+ clocks.burn + clocks.display + clocks.wait;

	if (frames > 0 && clocks.total > 0)
	{
		printf(	"update phase : %9d clocks per frame (%5.2f%%)\n"
			"seed phase   : %9d clocks per frame (%5.2f%%)\n"
			"burn phase   : %9d clocks per frame (%5.2f%%) (%d clocks per pixel)\n"
			"display phase: %9d clocks per frame (%5.2f%%)\n"
			"waiting time : %9d clocks per frame (%5.2f%%)\n\n",
			(int)(clocks.update / frames),	100.0 * clocks.update / clocks.total,
			(int)(clocks.seed / frames),	100.0 * clocks.seed / clocks.total,
			(int)(clocks.burn / frames),	100.0 * clocks.burn / clocks.total,
			(int)(clocks.burn / ((tscsnap)frames * bufpitch * bufheight)),
			(int)(clocks.display / frames),	100.0 * clocks.display / clocks.total,
			(int)(clocks.wait / frames),	100.0 * clocks.wait / clocks.total);
	}
	if (runtime > 0)
		printf( "FPS: %d  (%d seconds run time)\n", (int)(frames / runtime), (int)runtime);

	return EXIT_SUCCESS;
}

