#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <assert.h>
#include <stdarg.h>

#include <vc/vcore.h>
#include <vc/hardware.h>
#include "univ/univ.h"
#include "vclib.h"
#include "vc_image.h"

#include "game.h"

#include "display.h"

static int		windowwidth,
			windowheight;
static int		ratelimit = 30;

static DISP_MODES_T     DisplayMode;
static VC_IMAGE_T      *Screen;
static int		bufwidth,
			bufheight,
			bufpitch;
static short	        Palette[256];
static int		paldirtylow,
			paldirtyhigh;

static char	 const *hardtitle = NULL;


static void resizewindow(int width, int height)
{
	windowwidth = width;
	windowheight = height;
	OpenDisplay();
}


void OpenDisplay(void)
{
   if (Screen != NULL)
      vc_image_free(Screen);

   Screen = vc_image_malloc(VC_IMAGE_8BPP, windowwidth, windowheight, 0);

	paldirtylow = 0;
	paldirtyhigh = 255;
}


void SetDisplayName(const char *shortname, const char *longfmt, ...)
{
}

void ToggleFullscreen(void)
{
}

int SaveSnapshot(char *fmt)
{
	return 0;
}

void SetPaletteEntry(int i, unsigned char r, unsigned char g, unsigned char b)
{
	SetPaletteEntryA(i, r, g, b, 0xFF);
}

void SetPaletteEntryA(int i, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
   Palette[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);

	if (i < paldirtylow)
		paldirtylow = i;
	if (i > paldirtyhigh)
		paldirtyhigh = i;
}

void Display(unsigned char *source)
{
   int hcount = bufheight;
   unsigned char *dest = Screen->image_data;

   if (paldirtylow < 256 || paldirtyhigh >= 0)
   {
      vc_image_set_palette(Palette);
      paldirtylow = 256;
      paldirtyhigh = -1;
   }

   while (--hcount >= 0)
   {
      vclib_memcpy(dest, source, Screen->width);
      source += bufpitch;
      dest += Screen->pitch;
   }
   vc_image_present(Screen, DisplayMode);
}

int GetKey(void)
{
   static GAME_ACTIONS_T    game_action = {0, 0, 0, 0, 0};

   game_poll_host(&game_action);
   if (game_button_pressed(UNIV_A))
      return 's';
   if (game_button_pressed(UNIV_B))
      return 'w';
   if (game_button_pressed(UNIV_C))
      return 'c';
   if (game_button_pressed(UNIV_D))
      return '\t';
   if (game_button_pressed(UNIV_ACTION))
      return 'b';
   if (game_button_pressed(UNIV_UP))
      return ' ';
   if (game_button_pressed(UNIV_DOWN))
      return ' ';
   if (game_button_pressed(UNIV_LEFT))
      return ' ';
   if (game_button_pressed(UNIV_RIGHT))
      return ' ';

   return 0;
}

void SetupDisplay(char **argv, int width, int height, int pitch)
{
   univ_init();
   univ_clearleds(-1);
   univ_setleds(1);
   (void)game_initialise(0, argv);
   DisplayMode = univ_getsafedisplaymode();

	windowwidth = bufwidth = width;
	windowheight = bufheight = height;
	bufpitch = pitch;

        assert(width == univ_dispwidth(DisplayMode));
        assert(height == univ_dispheight(DisplayMode));


}


void ResizeDisplay(int width, int height, int pitch)
{
	bufwidth = width;
	bufheight = height;
	bufpitch = pitch;

	resizewindow(width, height);
}

void ShutdownDisplay(void)
{
	vc_image_free(Screen);
        Screen = NULL;
}

