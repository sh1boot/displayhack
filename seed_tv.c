#include <stdlib.h>
#include "grabber.h"
#include "displayhack.h"

extern void OutlineRaw(pixel *Destination, unsigned char *Source, int MemWidth, int Count);
extern void AddSaturatedRaw(pixel *Destination, unsigned char *Source, int Count);

void seed_TVOutline(pixel *Target, int MemWidth, int Count)
{
	unsigned char	       *image;
	int			width,
				height;


	image = grab_one(&width, &height);

	if (image == NULL)
		return;

	height -= 2;
	while (Count > 0)
	{
		OutlineRaw(Target, image, width, width);

		image += width;
		Target += MemWidth;
		Count -= MemWidth;
	}
}

void seed_TV(pixel *Target, int MemWidth, int Count)
{
	unsigned char	       *image;
	int			width,
				height;


	image = grab_one(&width, &height);

	if (image == NULL)
		return;

	height -= 2;
	while (Count > 0)
	{
		AddSaturatedRaw(Target, image, width);

		image += width;
		Target += MemWidth;
		Count -= MemWidth;
	}
}

void TVKeypress(int Key);

void SetupTV(void)
{
	grab_width = ScreenWidth;
	grab_height = ScreenHeight;
	grab_init();
	get_picture_properties();
	set_video_source(1);
	(void)TVKeypress('0');
}

void ShutdownTV(void)
{
	grab_free();
}

void TVKeypress(int Key)
{
	switch (Key)
	{
	case '`':
		set_video_source(1);
		break;
	case '~':
		set_video_source(2);
		break;
	case '1':
		set_video_source(0);
		change_freq(62250);
		break;
	case '2':
		set_video_source(0);
		change_freq(203250);
		break;
	case '3':
		set_video_source(0);
		change_freq(189250);
		break;
	case '4':
		set_video_source(0);
		change_freq(224250);
		break;
	case '0':
		set_video_source(0);
		change_freq(55250);
		break;
	
	}
}

