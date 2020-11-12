/* Copyright (C) Teemu Suutari */

#include "container_integration.h"

#include <proto/exec.h>
#include <proto/dos.h>
#include <stdlib.h>

int container_integration_fileOpen(const char *filename,uint32_t *length,void **file)
{
	BPTR fd,lock;
	struct FileInfoBlock fib;
	BOOL success;
	
	lock=Lock(filename,ACCESS_READ);
	if (!lock)
		return CONTAINER_ERROR_FILE_NOT_FOUND;
	success=Examine(lock,&fib);
	UnLock(lock);
	if (!success)
		return CONTAINER_ERROR_FILE_NOT_FOUND;
	if (fib.fib_DirEntryType>0)
		return CONTAINER_ERROR_INVALID_FILE_TYPE;

	fd=Open(filename,MODE_OLDFILE);
	if (!fd)
		return CONTAINER_ERROR_FILE_NOT_FOUND;
	*file=(void*)fd;
	*length=fib.fib_Size;
	return 0;
}

int container_integration_fileClose(void *file)
{
	Close((BPTR)file);
	return 0;
}

int32_t container_integration_fileRead(void *dest,uint32_t length,uint32_t offset,void *file)
{
	int32_t ret;
	Seek((BPTR)file,offset,OFFSET_BEGINNING);
	ret=Read((BPTR)file,dest,length);
	if (ret<0)
		return CONTAINER_ERROR_INVALID_FORMAT;
	return ret;
}

void *container_malloc(uint32_t size)
{
	/* AllocVec is V36 */
	void *ptr;

	size+=4;
	ptr=AllocMem(size,MEMF_CLEAR);
	*(uint32_t*)ptr=size;
	return (char*)ptr+4;
}

void container_free(void *ptr)
{
	uint32_t size;

	ptr=(char*)ptr-4;
	size=*(uint32_t*)ptr;
	FreeMem(ptr,size);
}
