/*
 *  aatv - a program to watch TV on a text-based console
 *  Copyright (C) 2000 Florent de Lamotte
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <sys/mman.h>

/*#include <asm/types.h>*/
#include <linux/videodev.h>

#include "grabber.h"

char		       *grab_device = "/dev/video0";
int			grab_width = 384;
int			grab_height = 288;
int			grab_input = 0;
int			grab_norm = VIDEO_MODE_PAL;

struct video_picture grab_pict;

/* grabber device variables */

static struct video_capability	grab_cap;
static struct video_mmap	grab_buf[2];
static struct video_window	grab_win;
static struct video_audio 	grab_audio;
static struct video_mbuf	grab_info;

static int		grab_fd,
			grab_size,
			have_mmap;
static int		current_buffer;
static int		buffers_nb;
static unsigned char   *grab_data[2];
static struct video_channel	grab_chan;

void grab_init (void)
{
	int			i;
	
	/* open the device */

	if ((grab_fd = open(grab_device, O_RDWR)) == -1) 
	  	fprintf(stderr, "open %s: %s\n", grab_device, strerror(errno)), exit(1);

	/* fill the grabber structures */

	if (ioctl(grab_fd,VIDIOCGCAP,&grab_cap) == -1)
		fprintf(stderr, "%s: no v4l device\n", grab_device), exit(1);

	if (ioctl(grab_fd, VIDIOCGCHAN, &grab_chan)==-1)
		perror("ioctl VIDIOCGCHAN"), exit(1);

	if (ioctl(grab_fd, VIDIOCGAUDIO, &grab_audio) == -1)
		perror("ioctl VIDIOCGAUDIO"), exit(1);
	
	if (ioctl(grab_fd, VIDIOCGMBUF, &grab_info) == -1)
		perror ("ioctl VIDIOCGMBUF"), exit(1);
	
	if (ioctl(grab_fd, VIDIOCGPICT, &grab_pict) == -1)
		perror ("ioctl VIDIOCGPICT"), exit(1);
     	
	if (ioctl(grab_fd, VIDIOCGWIN, &grab_win) == -1)
	  	perror("ioctl VIDIOCGWIN"), exit(1);


	current_buffer = 0;

	/* Find if we can capture in greylevel */

	grab_pict.palette = VIDEO_PALETTE_GREY;
	set_picture_properties();
	if (grab_pict.palette != VIDEO_PALETTE_GREY)
		fprintf (stderr, "Your device doesn't seem to be able to capture in GRAYLEVEL\n"), exit(1);
	
	/* turn audio off */

	grab_audio.flags |= VIDEO_AUDIO_MUTE;
	if (ioctl(grab_fd, VIDIOCSAUDIO, &grab_audio) == -1)
		perror("ioctl VIDIOCSAUDIO"), exit(1);

	/* sets the station */

	set_video_norm(VIDEO_MODE_PAL);
	set_video_source(grab_input);
	set_video_norm(grab_norm);
	
	/* try to setup mmap-based capture */

	buffers_nb = grab_info.frames == 1 ? 1 : 2;

	for (i = 0; i < buffers_nb; i++)
	{
		grab_buf[i].format = grab_pict.palette; 
		grab_buf[i].frame  = i;
		grab_buf[i].width  = grab_width;
		grab_buf[i].height = grab_height;
	}

	grab_data[0] = mmap(0, (size_t)grab_info.size, PROT_READ | PROT_WRITE, MAP_SHARED, grab_fd, 0);

	if ((int)grab_data[0] != -1)
	{
	  	have_mmap = 1;
		for (i = 1; i < buffers_nb; i++)
		{
			grab_data[i] = grab_data[0] + grab_info.offsets[i];
			ioctl(grab_fd, VIDIOCMCAPTURE, &grab_buf[i]);
		}
		return;
	}

   	/* fallback to read() */

   	fprintf(stderr,"no mmap support available, using read()\n");
	have_mmap = 0;
	memset(&grab_win, 0, sizeof(struct video_window));
	grab_win.width  = grab_width;
	grab_win.height = grab_height;

	if (ioctl(grab_fd, VIDIOCSWIN, &grab_win) == -1)
		perror("ioctl VIDIOCSWIN"), exit(1);

	if (ioctl(grab_fd, VIDIOCGWIN, &grab_win) == -1)
		perror("ioctl VIDIOCGWIN"), exit(1);

	grab_size = grab_win.width * grab_win.height;

	if ((grab_data[0] = malloc((size_t)grab_size)) == NULL)
		perror("malloc"), exit(EXIT_FAILURE);
}

void grab_free (void)
{
	if (have_mmap == 0)
		free(grab_data[0]);
}

unsigned char *grab_one(int *width, int *height)
{
	int			rc;

	if (have_mmap)
	{
		if (ioctl(grab_fd, VIDIOCMCAPTURE, &grab_buf[current_buffer]) == -1)
		{
    		perror("ioctl VIDIOCMCAPTURE");
    		return NULL;
    	}

		current_buffer = (current_buffer + 1) % buffers_nb;

		if (ioctl(grab_fd, VIDIOCSYNC, &grab_buf[current_buffer]) == -1)
		{
			perror("ioctl VIDIOCSYNC");
			return NULL;
		}

		*width  = grab_buf[current_buffer].width;
		*height = grab_buf[current_buffer].height;
	}
	else
	{
		rc = read(grab_fd, grab_data[0], (size_t)grab_size);
		if (grab_size != rc)
		{
			fprintf(stderr, "grabber read error (rc = %d)\n", rc);
			return NULL;
		}

		*width  = grab_win.width;
		*height = grab_win.height;
	}

	return grab_data[current_buffer];
}

unsigned long get_freq(void)
{
	unsigned long		freq = 0;

	if (ioctl(grab_fd, VIDIOCGFREQ, &freq) == -1)
		perror("ioctl VIDIOCGFREQ");

	return (freq * 125 / 2);
}

int change_freq(unsigned long freq)
{
	freq = freq * 2 / 125;
	if (ioctl(grab_fd, VIDIOCSFREQ, &freq) == -1)
		perror("ioctl VIDIOCSFREQ");

	return 0;
}

int set_video_norm(int vidnorm)
{
	grab_chan.norm = vidnorm;
	if (ioctl(grab_fd, VIDIOCSCHAN, &grab_chan))
	{
		perror ("ioctl VIDIOCSCHAN");
		return -1;	
	}

	return 0;
}

int get_video_norm(void)
{
	return grab_chan.norm;
}

int set_video_source(int source)
{
	if (source >= grab_cap.channels)
		return -1;
	
	grab_chan.channel = source;
	if (ioctl(grab_fd, VIDIOCSCHAN, &grab_chan) == -1)
	{
		perror ("ioctl VIDIOCSCHAN");
		return -1;	
	}

	return 0;
}

int get_video_source(void)
{
	return grab_chan.channel;
}

void set_picture_properties(void)
{
	if (ioctl(grab_fd, VIDIOCSPICT, &grab_pict) == -1)
		perror ("ioctl VIDIOCSPICT");

	if (ioctl(grab_fd, VIDIOCGPICT, &grab_pict) == -1)
		perror ("ioctl VIDIOCGPICT");
}

void get_picture_properties(void)
{
	if (ioctl(grab_fd, VIDIOCGPICT, &grab_pict) == -1)
		perror ("ioctl VIDIOCGPICT");
}
