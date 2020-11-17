/* Copyright (C) Teemu Suutari */

#include "archivefs_integration.h"

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

int archivefs_integration_fileOpen(const char *filename,uint32_t *length,void **file)
{
	int fd,l;

	fd=open(filename,O_RDONLY);
	if (fd<0)
		return ARCHIVEFS_ERROR_FILE_NOT_FOUND;
	l=lseek(fd,0,SEEK_END);
	if (l<0)
		return ARCHIVEFS_ERROR_INVALID_FORMAT;
	*file=(void*)(size_t)fd;
	*length=l;
	return 0;
}

int archivefs_integration_fileClose(void *file)
{
	close((int)file);
	return 0;
}

int32_t archivefs_integration_fileRead(void *dest,uint32_t length,uint32_t offset,void *file)
{
	int fd=(int)file;
	int ret=lseek(fd,offset,SEEK_SET);
	if (ret<0)
		return ARCHIVEFS_ERROR_INVALID_FORMAT;
 	ret=read(fd,dest,length);
	if (ret<0)
		return ARCHIVEFS_ERROR_INVALID_FORMAT;
 	return ret;
}

void *archivefs_malloc(uint32_t size)
{
	return malloc(size);
}

void archivefs_free(void *ptr)
{
	free(ptr);
}