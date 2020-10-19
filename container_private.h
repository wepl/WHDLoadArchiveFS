/* Copyright (C) Teemu Suutari */

#ifndef CONTAINER_PRIVATE_H
#define CONTAINER_PRIVATE_H

#include "container_api.h"
#include "container_lha.h"
#include "container_zip.h"

#define CONTAINER_TYPE_FILE (-3)
#define CONTAINER_TYPE_DIR (2)

/* radically smaller than fib (depending on the length of the filename / filenote though) */
struct container_cached_file_entry
{
	uint32_t				dataOffset;
	uint32_t				dataLength;
	/* 0 unpacked, 1+ packed */
	uint32_t				dataType;
	uint32_t				length;

	int32_t					fileType;
	uint32_t				protection;
	uint32_t				mtimeDays;
	uint32_t				mtimeMinutes;
	uint32_t				mtimeTicks;

	char					*path;
	char					*pathAndName;
	char					*filename;
	char					*filenote;

	struct container_cached_file_entry	*prev;
	struct container_cached_file_entry	*next;
};


struct container_state;

struct container_file_state
{
	struct container_state			*container;

	union
	{
		struct container_lha_file_state	lha;
		struct container_zip_file_state	zip;
	} state;
};

struct container_state
{
	/* reading the file */
	void					*file;
	uint32_t				fileLength;

	/* contents of the container */
	struct container_cached_file_entry	*firstEntry;
	struct container_cached_file_entry	*lastEntry;
	
	/* pointers to the implementation */
	int (*fileOpen)(struct container_file_state *file_state,struct container_cached_file_entry *entry);
	int (*fileRead)(void *dest,struct container_file_state *file_state,uint32_t length,uint32_t offset);

	union
	{
		struct container_lha_state	lha;
		struct container_zip_state	zip;
	} state;
};

struct container_combined_state
{
	struct container_state			container;
	struct container_file_state		fileState;

	struct container_cached_file_entry	*currentFile;
};

struct FIB
{
	/* unknown */
	uint32_t				diskKey;
	/* >0 dir, <0 file
	   -3 file
	   -4 hard link to file
	   -5 pipe
	   1 root
	   2 dir
	   3 softlink
	   4 hard link to dir
	*/
	uint32_t				dirEntryType;
	uint8_t					filename[108];
	uint32_t				protection;
	/* obsolete?!? */
	uint32_t				entryType;
	uint32_t				size;
	uint32_t				blocks;
	/* mtime */
	uint32_t				mtimeDays;
	uint32_t				mtimeMinutes;
	uint32_t				mtimeTicks;
	uint8_t					comment[80];
	/* not really used */
	uint16_t				uid;
	uint16_t				gid;
	uint8_t					reserved[32];
};

#endif
