#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

#include "win32ss_hook.h"

#include "displayhack.h"
#include "display.h"
#include "palettes.h"
#include "burners.h"
#include "warpers.h"
#include "seeds.h"

char const *SaverName = "ScreenHack";
int SaverTickPeriod = 33;  /* initialised in case it's read too soon */

static int mintick, maxtick, nostaticpalettes;
static char const *palette_path;

const saver_config_item_t SaverConfig[] =
{
   { "TickPeriod", &SaverTickPeriod, 33 },
   { "VaryMinTicks", &mintick, 100 },
   { "VaryMaxTicks", &maxtick, 400 },
   { "NoStaticPalettes", &nostaticpalettes },
   { "ExtraPalettePath", (int *)&palette_path, (int)"c:/Program Files/ScreenHack/palettes/", 1 },
   { NULL, NULL, 0, 0 }
};


static int refcount = 0;

static int bufpitch, bufwidth, bufheight;
static pixel *databuffer = NULL, *bufpage0 = NULL, *bufpage1 = NULL;
static saver_rgbquad palette[256];
static int paldirtylow, paldirtyhigh;
static int varytick, varyctl;


struct saver_priv_state_t
{
   int width, height, pitch;
   int tick;
};


saver_state_t *saver_init(int width, int height, int pitch)
{
   char *argv[] = { "C:/windows/whatever.scr", "-paldir", NULL, NULL, NULL };
   saver_state_t *state;

   if ((state = malloc(sizeof(*state))) == NULL)
      return state;

   state->width = width;
   state->height = height;
   state->pitch = pitch;
   state->tick = 0;

   if (refcount++ == 0)
   {
      srand((long)time(NULL));

      if (SaverTickPeriod < 10)     SaverTickPeriod = 10;
      if (SaverTickPeriod > 1000)   SaverTickPeriod = 1000;
      if (mintick < 10)             mintick = 10;
      if (maxtick < mintick)        maxtick = mintick;

      varyctl = -1;
      bufwidth = state->width;
      bufheight = state->height;
      bufpitch = state->pitch;

      if ((databuffer = calloc(2 * bufpitch * (bufheight + 4) + 32 + 4, sizeof(pixel))) == NULL)
      {
         perror("displayhack: calloc() for data buffer failed");
	 exit(EXIT_FAILURE);
      }

      {
         pixel *tmpptr = databuffer;

         /* 32-byte alignment for cache happiness */
         tmpptr = (void *)((unsigned long)(tmpptr + 31) & ~31);
         /* two line clearance for top edge of top line */
         tmpptr += bufpitch + bufpitch;
         /* two byte clearance for upper-left corner of upper-left pixel */
         tmpptr += 2;

         bufpage0 = (void *)tmpptr;
         bufpage1 = (void *)(tmpptr + bufpitch * (bufheight + 4));
      }

      paldirtylow = 0;
      paldirtyhigh = 255;

      argv[2] = (char *)palette_path;
      if (nostaticpalettes != 0)
              argv[3] = "-nostaticpal";

      SetupSeeds(argv, bufwidth, bufheight, bufpitch);
      SetupWarpers(argv);
      SetupBurners(argv, bufwidth, bufheight, bufpitch);
      SetupPalettes(argv);

      SelectSeed(NULL);
      SelectWarper(NULL);
      SelectBurner(NULL);
      SelectPalette(NULL);
   }

   return state;
}

void saver_keystroke(saver_state_t *state, int key)
{
   switch (key)
   {
   case '1':   CycleSeedUp();       break;
   case '2':   CycleWarperUp();     break;
   case '3':   CycleBurnerUp();     break;
   case '4':   CyclePaletteUp();    break;
   case '9':   varyctl = -1;        break;
   case '0':   varyctl = 0;         break;
   case '\t':  
      SelectSeed(NULL);
      SelectWarper(NULL);
      SelectBurner(NULL);
      SelectPalette(NULL);
      break;
   default:
      return;
   }
   varyctl = -varyctl;
}

int saver_advance(saver_state_t *state, void *buffer)
{
   static int toggle = 0;
   static int tick_throttle = -1;

   if ((++state->tick - tick_throttle) > 0)
   {
      tick_throttle = state->tick;

      if (varyctl < 0)
      {
         /* (maxtick - mintick) may be larger than RAND_MAX, so waste some time on a bigger rand() */
         varytick = (((rand() >> 8) ^ (rand() << 4) ^ (rand() << 16)) & 0x7FFFFFFFul) % (unsigned)(maxtick - mintick + 1) + mintick;
         varyctl = 1;
      }

      if (varyctl > 0 && --varytick <= 0)
      {
         static int lastchange = -1;
         int change;

         change = (rand() >> 8) % (4u - (lastchange >= 0));
         if (change >= lastchange)
            change++;
         lastchange = change;

         switch (change)
         {
         case 0: SelectSeed(NULL);    break;
         case 1: SelectWarper(NULL);  break;
         case 2: SelectBurner(NULL);  break;
         case 3: SelectPalette(NULL); break;
         }
         varyctl = -1;
      }

      UpdateSeed();
      UpdatePalette();
      UpdateBurner();
      toggle = !toggle;
      if (toggle)
      {
         Seed(bufpage0);
         Burn(bufpage1, bufpage0);
         memcpy(buffer, bufpage1, state->pitch * state->height);
      }
      else
      {
         Seed(bufpage1);
         Burn(bufpage0, bufpage1);
         memcpy(buffer, bufpage0, state->pitch * state->height);
      }
      return 1;
   }

   return 0;
}


int saver_checkpalette(saver_state_t *state, saver_rgbquad const **palptr, int *start, int *count)
{
   if (paldirtylow < paldirtyhigh)
   {
      *palptr = palette;
      *start = paldirtylow;
      *count = paldirtyhigh - paldirtylow + 1;
      paldirtylow = 256;
      paldirtyhigh = -1;

      return 1;
   }
   return 0;
}

void saver_shutdown(saver_state_t *state)
{
   assert(refcount > 0);
   assert(state != NULL);

   free(state);
   if (--refcount == 0)
   {
      ShutdownPalettes();
      ShutdownBurners();
      ShutdownWarpers();
      ShutdownSeeds();
      free(databuffer);
      databuffer = NULL;
   }
}


/* from display.h */
void SetPaletteEntryA(int i, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
   palette[i].r = r;
   palette[i].g = g;
   palette[i].b = b;
   palette[i].a = 0;

   if (i < paldirtylow)
      paldirtylow = i;
   if (i > paldirtyhigh)
      paldirtyhigh = i;
}
