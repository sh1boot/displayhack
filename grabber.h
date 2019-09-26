#if !defined(GRABBER_H_INCLUDED)

extern char	       *grab_device; 
extern int		grab_width; 
extern int		grab_height;
extern int		grab_input;
extern int		grab_norm;

extern struct video_picture	grab_pict;

void			grab_init(void);
void			grab_free(void);
unsigned char	       *grab_one(int *width, int *height);
unsigned long		get_freq(void);
int			change_freq(unsigned long freq);
void			set_picture_properties(void),
			get_picture_properties(void);
int			set_video_source(int),
			get_video_source(void);
int			set_video_norm(int),
			get_video_norm(void);

#endif /* !defined(GRABBER_H_INCLUDED) */ 
 
