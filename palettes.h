#ifndef PALETTES_H_INCLUDED
#define PALETTES_H_INCLUDED

#include "displayhack.h"

 /* //////////////////////////////////////////////////////////////////////// */

void			SetupPalettes(char **argv);
void			ShutdownPalettes(void);

const char	       *SelectPalette(const char *Name);
const char	       *CyclePaletteUp(void);
const char	       *CyclePaletteDown(void);
void			UpdatePalette(void);
void			PaletteKeypress(int Key);

 /* //////////////////////////////////////////////////////////////////////// */

#endif

