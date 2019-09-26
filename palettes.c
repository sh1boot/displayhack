#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#if !defined(NO_FILE_ACCESS)
# if defined(_WINDOWS)
#  include "dirent_win32.h"
#  define snprintf _snprintf
#  define strcasecmp _strcmpi
# else
#  include <dirent.h>
# endif
#endif

#include "palettes.h"
#include "display.h"

 /* //////////////////////////////////////////////////////////////////////// */

typedef struct
{
	int			Index;
	unsigned char		Red, Green, Blue, Alpha;
}			PaletteEntry;

typedef struct Palette_t
{
	struct Palette_t       *Next,
			       *Prev;
	char		       *Name,
			       *Description;
	int			Length;
	const PaletteEntry     *Data;
}			Palette;

 /* //////////////////////////////////////////////////////////////////////// */

#define RANDPAL_SIZE 32
static PaletteEntry	RandPalette[RANDPAL_SIZE + 1];
static Palette		DefaultPalette = { &DefaultPalette, &DefaultPalette, "randpal", "randomly generated palette", RANDPAL_SIZE, RandPalette };
static Palette	       *PalettePtr = &DefaultPalette;
static int		NumPalettes = 1;
static Palette	       *OldPalettePtr = NULL;

static struct
{
	unsigned char		r, g, b, a;
}			CurrentPaletteData[256],
			NewPaletteData[256],
			OldPaletteData[256];

 /* //////////////////////////////////////////////////////////////////////// */

static int handy_rand(int b, int r)
{
	b += rand() % (r + r + 1) - r;
	if (b < 0) b = 0;
	if (b > 255) b = 255;
	return b;
}

static void Randomise(void)
{
	int i, step;
	int vividity = rand() / (RAND_MAX / 1536) + rand() / (RAND_MAX / 512);

	RandPalette[0].Index = 0;
	RandPalette[0].Red = handy_rand(128, 128);
	RandPalette[0].Green = handy_rand(128, 128);
	RandPalette[0].Blue = handy_rand(128, 128);
	RandPalette[0].Alpha = 0xFF;
	for (i = 1; i < RANDPAL_SIZE - 1; i++)
		RandPalette[i].Index = handy_rand(i * 256 / RANDPAL_SIZE, 96 / RANDPAL_SIZE);
	RandPalette[RANDPAL_SIZE - 1].Index = 255;
	RandPalette[RANDPAL_SIZE].Index = 256;
	RandPalette[RANDPAL_SIZE].Red = handy_rand(128, 128);
	RandPalette[RANDPAL_SIZE].Green = handy_rand(128, 128);
	RandPalette[RANDPAL_SIZE].Blue = handy_rand(128, 128);
	RandPalette[RANDPAL_SIZE].Alpha = 0xFF;

	for (step = RANDPAL_SIZE / 2; step >= 1; step >>= 1)
		for (i = step; i < RANDPAL_SIZE; i += step + step)
		{
			RandPalette[i].Red = handy_rand((RandPalette[i - step].Red + RandPalette[i + step].Red) / 2,
					step * vividity / RANDPAL_SIZE);
			RandPalette[i].Green = handy_rand((RandPalette[i - step].Green + RandPalette[i + step].Green) / 2,
					step * vividity / RANDPAL_SIZE);
			RandPalette[i].Blue = handy_rand((RandPalette[i - step].Blue + RandPalette[i + step].Blue) / 2,
					step * vividity / RANDPAL_SIZE);
			RandPalette[i].Alpha = 0xFF;
		}
}

static void Graduate(PaletteEntry Start, PaletteEntry End)
{
	int		steps,
			divisor,
			RedAcc,
			GreenAcc,
			BlueAcc,
			AlphaAcc;
	int		curr;

	steps = (int)End.Index - (int)Start.Index;
	curr = Start.Index;

	RedAcc = Start.Red * steps;
	GreenAcc = Start.Green * steps;
	BlueAcc = Start.Blue * steps;
	AlphaAcc = Start.Alpha * steps;

	divisor = steps;

	while (steps--)
	{
		RedAcc = RedAcc + End.Red - Start.Red;
		GreenAcc = GreenAcc + End.Green - Start.Green;
		BlueAcc = BlueAcc + End.Blue - Start.Blue;
		AlphaAcc = AlphaAcc + End.Alpha - Start.Alpha;

		curr++;
		NewPaletteData[curr].r = RedAcc / divisor;
		NewPaletteData[curr].g = GreenAcc / divisor;
		NewPaletteData[curr].b = BlueAcc / divisor;
		NewPaletteData[curr].a = AlphaAcc / divisor;
	}
}

static void ApplyPalette(void)
{
	const PaletteEntry     *Ptr;
	PaletteEntry		Last = { 0, 0, 0, 0, 0xFF },
				This;
	int			count;

	if (PalettePtr->Data == RandPalette)
		Randomise();

	count = PalettePtr->Length;
	Ptr = PalettePtr->Data;

	if (Ptr->Index == 0)
	{
		Last = *Ptr++;
		count--;
	}

	NewPaletteData[0].r = Last.Red;
	NewPaletteData[0].g = Last.Green;
	NewPaletteData[0].b = Last.Blue;
	NewPaletteData[0].a = Last.Alpha;

	while (count--)
	{
		This = *Ptr++;

		if ((int)This.Index - (int)Last.Index > 0)
			Graduate(Last, This);

		Last = This;
	}

	if (Last.Index < 0xff)
	{
		This.Index = 0xff;
		This.Red = 0xff;
		This.Green = 0xff;
		This.Blue = 0xff;
		This.Alpha = 0xff;
		Graduate(Last, This);
	}
}

#if !defined(NO_FILE_ACCESS)
static void LoadPalettes(const char *dirname)
{
	char			linebuf[1024];
	Palette		       *palptr;
	PaletteEntry	       *entptr;
	DIR		       *dirhandle;
	struct dirent	       *dirhandleEntry;
	FILE		       *fptr;
	int			entrycount,
				lastindex = -1;


	dirhandle = opendir(dirname);
	if (dirhandle == NULL)
	{
		fprintf(stderr, "Failed to open %s: %s\n", dirname, strerror(errno));
		return;
	}

	while ((dirhandleEntry = readdir(dirhandle)) != NULL)
	{
		if (strcasecmp(dirhandleEntry->d_name + strlen(dirhandleEntry->d_name) - 4, ".pal") != 0)
			continue;

		snprintf(linebuf, sizeof(linebuf), "%s/%s", dirname, dirhandleEntry->d_name);
		if ((fptr = fopen(linebuf, "rt")) == NULL)
		{
			fprintf(stderr, "\nFailed to open %s: %s\n", linebuf, strerror(errno));
			continue;
		}

		if ((palptr = calloc(1, sizeof(Palette))) == NULL
			|| (palptr->Name = strdup(dirhandleEntry->d_name)) == NULL
			|| (entptr = calloc(256, sizeof(PaletteEntry))) == NULL)
		{
			perror("palette: alloc() failed");
			exit(EXIT_FAILURE);
		}

		entrycount = 0;

		while (fgets(linebuf, sizeof(linebuf), fptr) != NULL && entrycount < 256)
		{
			int i = -1, r = -1, g = -1, b = -1, a = 0xFF;

			switch (linebuf[0])
			{
			case '\0': case '\n': case '\r': case '#':
				/* null lines */
				break;

			case '!':
				if (linebuf[0] == '!' && palptr->Description == NULL)
				{
					char *sptr;

					sptr = strchr(linebuf, '\0');
					while (isspace(*--sptr))
						*sptr = '\0';

					sptr = linebuf + 1;
					while (isspace(*sptr))
						sptr++;

					/* doing this here may lead to memory fragmentation */
					palptr->Description = strdup(sptr);
				}
				break;

			default:
				sscanf(linebuf, "%i%*[ \t:;=,]%i%*[ \t,]%i%*[ \t,]%i%*[ \t,]%i%*[ \t,]",
						 &i,          &r,       &g,       &b,       &a);

				if (r == 256) r--;
				if (g == 256) g--;
				if (b == 256) b--;
				if (a == 256) a--;
				if (((i | r | g | b | a) & ~255) == 0)
				{
					lastindex = entptr->Index = i;
					entptr->Red = r;
					entptr->Green = g;
					entptr->Blue = b;
					entptr->Alpha = a;
					entptr++;
					entrycount++;
				}
				else
					fprintf(stderr, "palette: %s: garbage: %s\n", palptr->Name, linebuf);
			}
		}
		fclose(fptr);

		if (entrycount == 0)
		{
			fprintf(stderr, "palette: %s is empty\n", palptr->Name);
			free(palptr->Name);
			free(palptr->Description);
			free(entptr);
			free(palptr);
			continue;
		}
		if (palptr->Description == NULL)
			palptr->Description = palptr->Name;
		palptr->Length = entrycount;
		palptr->Data = realloc(entptr - entrycount, sizeof(PaletteEntry) * entrycount);
		if (palptr->Data == NULL)
		{
			perror("palette: realloc() failed");
			exit(EXIT_FAILURE);
		}
		palptr->Next = &DefaultPalette;
		palptr->Prev = DefaultPalette.Prev;

		DefaultPalette.Prev->Next = palptr;
		DefaultPalette.Prev = palptr;
		NumPalettes++;
	}
	closedir(dirhandle);
}
#endif

const char *SelectPalette(const char *Name)
{
	Palette		       *ptr;


	if (Name == NULL)
	{
		int i = (rand() >> 8) % NumPalettes;
		OldPalettePtr = NULL;
		PalettePtr = &DefaultPalette;
		while (--i >= 0)
			CyclePaletteUp();
		return PalettePtr->Name;
	}

	ptr = &DefaultPalette;
	do
	{
		if (strcmp(Name, ptr->Name) == 0)
		{
			PalettePtr = ptr;
			break;
		}
		ptr = ptr->Next;
	} while (ptr != &DefaultPalette);

	OldPalettePtr = NULL;
	return PalettePtr->Name;
}

const char *CyclePaletteUp(void)
{
	PalettePtr = PalettePtr->Next;
	OldPalettePtr = NULL;
	return PalettePtr->Name;
}

const char *CyclePaletteDown(void)
{
	PalettePtr = PalettePtr->Prev;
	OldPalettePtr = NULL;
	return PalettePtr->Name;
}

void UpdatePalette(void)
{
	static int		i = 1024;


	if (PalettePtr != OldPalettePtr)
	{
		OldPalettePtr = PalettePtr;
		memcpy(OldPaletteData, CurrentPaletteData, sizeof(OldPaletteData));
		ApplyPalette();
		i = 0;
	}

	if (i < 512)
	{
		int j, scale;

		i += 16;
		for (j = 0; j < 256; j++)
		{
			scale = i - j;
			if (scale > 256) scale = 256;
			if (scale < 0) scale = 0;

			CurrentPaletteData[j].r = ((int)NewPaletteData[j].r - (int)OldPaletteData[j].r) * scale / 256 + (int)OldPaletteData[j].r;
			CurrentPaletteData[j].g = ((int)NewPaletteData[j].g - (int)OldPaletteData[j].g) * scale / 256 + (int)OldPaletteData[j].g;
			CurrentPaletteData[j].b = ((int)NewPaletteData[j].b - (int)OldPaletteData[j].b) * scale / 256 + (int)OldPaletteData[j].b;
			CurrentPaletteData[j].a = ((int)NewPaletteData[j].a - (int)OldPaletteData[j].a) * scale / 256 + (int)OldPaletteData[j].a;

			SetPaletteEntryA(j, CurrentPaletteData[j].r, CurrentPaletteData[j].g, CurrentPaletteData[j].b, CurrentPaletteData[j].a);
		}
	}
}

void SetupPalettes(char **argv)
{
	char *paldir = NULL;
	int nostaticpalettes = 0;

	if (*argv != NULL)
	{
		char **argv2 = argv + 1;
		char **wb_argv = argv2;

		while (*argv2 != NULL)
		{
			     if (strcmp(*argv2, "-paldir") == 0)
				paldir = *++argv2;
			else if (strcmp(*argv2, "-nostaticpal") == 0)
				nostaticpalettes = 1;
			else
				*wb_argv++ = *argv2;
			argv2++;
		}
		*wb_argv = NULL;
	}

#if defined(STATIC_PALETTES)
	if (nostaticpalettes == 0)
	{
		int i;
#include "staticpalettes.c"

		for (i = 0; i < sizeof(paltab) / sizeof(*paltab); i++)
		{
			Palette *palptr = paltab + i;

			palptr->Next = &DefaultPalette;
			palptr->Prev = DefaultPalette.Prev;

			DefaultPalette.Prev->Next = palptr;
			DefaultPalette.Prev = palptr;
			NumPalettes++;
		}
	}
#endif

#if !defined(NO_FILE_ACCESS)
	{
		char mypaldir[1024];

		if (paldir == NULL)
		{
			/* derive palette directory from location of binary */
			char *tmpptr;

			strncpy(mypaldir, *argv, sizeof(mypaldir) - 20);
			mypaldir[sizeof(mypaldir) - 20] = '\0';

#if defined(__WIN32__)
			tmpptr = mypaldir;
			while ((tmpptr = strchr(tmpptr, '\\')) != NULL)
				*tmpptr++ = '/';
#endif

			tmpptr = strrchr(mypaldir, '/');
			if (tmpptr == NULL)
			{
				tmpptr = mypaldir;
				*tmpptr++ = '.';
			}

			*tmpptr = '\0';

#if defined(__APPLE__)	/* if we're a .app then go look in the Resources folder */
			if (tmpptr - mypaldir > 19 && strcasecmp(tmpptr - 19, ".app/Contents/MacOS") == 0)
			{
				strcpy(tmpptr - 5, "Resources");
				tmpptr += 9 - 5;
			}
#endif
			strcpy(tmpptr, "/pal");
			paldir = mypaldir;
		}

		LoadPalettes(paldir);
	}
#endif
	
	PalettePtr = &DefaultPalette;
}

void ShutdownPalettes(void)
{
}

void PaletteKeypress(int Key)
{
}

