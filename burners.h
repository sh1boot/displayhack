#ifndef BURNERS_H_INCLUDED
#define BURNERS_H_INCLUDED

#include "displayhack.h"

 /* //////////////////////////////////////////////////////////////////////// */

void			SetupBurners(char **argv, int width, int height, int pitch);
void			ResizeBurners(int width, int height, int pitch);
void			ShutdownBurners(void);

const char	       *SelectBurner(const char *name);
const char	       *CycleBurnerUp(void);
const char	       *CycleBurnerDown(void);
void			UpdateBurner(void);
void			BurnerKeypress(int key);

typedef void		BurnerFunc(pixel *destination, pixel *source);
extern BurnerFunc      *Burn;

 /* //////////////////////////////////////////////////////////////////////// */

#endif
