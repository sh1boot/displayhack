#include <stdio.h>

#include "grabber.h"


char		       *grab_device = "/dev/video0";
int			grab_width = 384;
int			grab_height = 288;
int			grab_input = 0;
int			grab_norm = 0;


void grab_init (void)
{
}

void grab_free (void)
{
}

unsigned char *grab_one(int *width, int *height)
{
	return NULL;
}

unsigned long get_freq(void)
{
	return 0;
}

int change_freq(unsigned long freq)
{
	return 0;
}

int set_video_norm(int vidnorm)
{
	return 0;
}

int get_video_norm(void)
{
	return 0;
}

int set_video_source(int source)
{
	return 0;
}

int get_video_source(void)
{
	return 0;
}

void set_picture_properties(void)
{
}

void get_picture_properties(void)
{
}
