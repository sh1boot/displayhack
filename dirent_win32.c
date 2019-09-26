#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>

#include "dirent_win32.h"

struct dir_t
{
	long handle;
	int first;
	struct _finddata_t work;
	struct dirent result;
};

DIR *opendir(const char *path)
{
	char temppath[1024];
	DIR *result;

	if ((result = malloc(sizeof(*result))) == NULL)
		return NULL;

	_snprintf(temppath, sizeof(temppath) - 1, "%s/*", path);

	result->handle = _findfirst(temppath, &result->work);
	result->first = 1;

	return result;
}

struct dirent *readdir(DIR *directory)
{
	if (directory->handle == -1)
		return NULL;

	if (directory->first == 0)
		if (_findnext(directory->handle, &directory->work) == -1)
			return NULL;


	directory->result.d_name = directory->work.name;
	directory->result.d_namlen = strlen(directory->work.name);
	directory->first = 0;

	return &directory->result;
}

void closedir(DIR *directory)
{
	_findclose(directory->handle);
	free(directory);
}
