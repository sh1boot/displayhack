#ifndef __DISPLAY_H_INCLUDED
#define __DISPLAY_H_INCLUDED


 /* //////////////////////////////////////////////////////////////////////// */

void			SetupDisplay(char **argv, int width, int height, int pitch);
void			ResizeDisplay(int width, int height, int pitch);
void			SetDisplayName(const char *shortname, const char *longfmt, ...);
void			ShutdownDisplay(void);

void			OpenDisplay(void);
void			ToggleFullscreen(void);
int			SaveSnapshot(char *fmt);
void			SetPaletteEntry(int i, unsigned char r, unsigned char g, unsigned char b);
void			SetPaletteEntryA(int i, unsigned char r, unsigned char g, unsigned char b, unsigned char a);
int			GetKey(void);
void			Display(unsigned char *source);

 /* //////////////////////////////////////////////////////////////////////// */

#endif
