#ifndef SEEDS_H_INCLUDED
#define SEEDS_H_INCLUDED

#include "displayhack.h"	/* pixel definition */

 /* //////////////////////////////////////////////////////////////////////// */

void			SetupSeeds(char **argv, int width, int height, int pitch);
void			ResizeSeeds(int width, int height, int pitch);
void			ShutdownSeeds(void);

const char	       *SelectSeed(const char *Name);
const char	       *CycleSeedUp(void);
const char	       *CycleSeedDown(void);
void			UpdateSeed(void);
void			SeedKeypress(int Key);

extern void	      (*Seed)(pixel *target);


/* for the children.. */

struct xy_t { int x, y; };
int			DrawBall(int x, int y, int diameter, pixel *Target);
int			DrawLine(int x1, int y1, int x2, int y2, pixel *Target);
int			DrawPoint(int x, int y, pixel *Target);


 /* //////////////////////////////////////////////////////////////////////// */

#endif

