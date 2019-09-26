#if !defined(DIRENT_H_INCLUDED)
#define DIRENT_H_INCLUDED

struct dirent
{
	char *d_name;
	int d_namlen;
	/* some other shit I can't be bothered implementing */
};

typedef struct dir_t DIR;

DIR *opendir(const char *path);
struct dirent *readdir(DIR *directory);
void closedir(DIR *directory);

#endif /* !defined(DIRENT_H_INCLUDED) */
