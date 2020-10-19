/* Copyright (C) Teemu Suutari */

#include "container_private.h"
#include "container_common.h"
#include "container_integration.h"

#define HDR_EOCD_SIZE (22)
#define HDR_EOCD_OFFSET_HDR (0)
#define HDR_EOCD_OFFSET_DISKNO (4)
#define HDR_EOCD_OFFSET_CDDISKNO (6)
#define HDR_EOCD_OFFSET_NUMENTRIES_DISK (8)
#define HDR_EOCD_OFFSET_NUMENTRIES (10)
#define HDR_EOCD_OFFSET_CDLENGTH (12)
#define HDR_EOCD_OFFSET_CDOFFSET (16)
#define HDR_EOCD_OFFSET_COMMENTLENGTH (20)

#define HDR_CFH_SIZE (46)
#define HDR_CFH_OFFSET_HDR (0)
#define HDR_CFH_OFFSET_CREATE_VERSION (4)
#define HDR_CFH_OFFSET_EXTRACT_VERSION (6)
#define HDR_CFH_OFFSET_FLAGS (8)
#define HDR_CFH_OFFSET_METHOD (10)
#define HDR_CFH_OFFSET_MTIME (12)
#define HDR_CFH_OFFSET_CRC (16)
#define HDR_CFH_OFFSET_PACKEDSIZE (20)
#define HDR_CFH_OFFSET_RAWSIZE (24)
#define HDR_CFH_OFFSET_NAMELENGTH (28)
#define HDR_CFH_OFFSET_EXTRALENGTH (30)
#define HDR_CFH_OFFSET_COMMENTLENGTH (32)
#define HDR_CFH_OFFSET_DISK_NUMBER_START (34)
#define HDR_CFH_OFFSET_INTERNAL_FILE_ATTRIBUTES (36)
#define HDR_CFH_OFFSET_EXTERNAL_FILE_ATTRIBUTES (38)
#define HDR_CFH_OFFSET_LOCAL_HEADER_OFFSET (42)

#define HDR_LFH_SIZE (30)
#define HDR_LFH_OFFSET_HDR (0)
#define HDR_LFH_OFFSET_EXTRACT_VERSION (4)
#define HDR_LFH_OFFSET_FLAGS (6)
#define HDR_LFH_OFFSET_METHOD (8)
#define HDR_LFH_OFFSET_MTIME (10)
#define HDR_LFH_OFFSET_CRC (14)
#define HDR_LFH_OFFSET_PACKEDSIZE (18)
#define HDR_LFH_OFFSET_RAWSIZE (22)
#define HDR_LFH_OFFSET_NAMELENGTH (26)
#define HDR_LFH_OFFSET_EXTRALENGTH (28)

static int container_zip_fileOpen(struct container_file_state *file_state,struct container_cached_file_entry *entry)
{
	uint8_t hdr[HDR_LFH_SIZE];
	uint32_t lfhLength;
	int ret;
	struct container_state *container=file_state->container;

	if (entry->dataType&0x100U)
	{
		/* yay! now go for the local file header */
		if ((ret=container_common_simpleRead(hdr,HDR_LFH_SIZE,entry->dataOffset,container))<0) return ret;
		/* lets keep it minimal */
		if (GET_LE32(hdr)!=0x4034b50U)
			return CONTAINER_ERROR_INVALID_FORMAT;
		lfhLength=HDR_LFH_SIZE+GET_LE16(hdr+HDR_LFH_OFFSET_NAMELENGTH)+GET_LE16(hdr+HDR_LFH_OFFSET_EXTRALENGTH);
		if (entry->dataOffset+lfhLength+entry->dataLength>=container->fileLength)
			return CONTAINER_ERROR_INVALID_FORMAT;
		entry->dataOffset+=lfhLength;
		entry->dataType&=~0x100U;
	}
	file_state->state.zip.entry=entry;
	return 0;
}

static int container_zip_fileRead(void *dest,struct container_file_state *file_state,uint32_t length,uint32_t offset)
{
	/* does not check file type */
	struct container_cached_file_entry *entry;

	entry=file_state->state.zip.entry;
	if (entry->dataType)
		return CONTAINER_ERROR_DECOMPRESSION_ERROR;
	if (length+offset>entry->length)
		return CONTAINER_ERROR_INVALID_READ;
	return container_common_simpleRead(dest,length,entry->dataOffset+offset,file_state->container);
}

int container_zip_initialize(struct container_state *container)
{
	struct container_cached_file_entry *entry;

	uint8_t hdr[HDR_CFH_SIZE];
	uint32_t cdOffset,cdLength,packedLength,rawLength,mtime,numEntries,nameLength,extraLength,commentLength,dataType,i,j,k;
	uint32_t offset,attributes,cfhLength;
	int ret,isAmiga,isDir;
	uint8_t *stringSpace;

	/* First thing first: Find end of central directory */
	if (container->fileLength<HDR_EOCD_SIZE)
		return CONTAINER_ERROR_INVALID_FORMAT;
	if ((ret=container_common_simpleRead(hdr,HDR_EOCD_SIZE,container->fileLength-HDR_EOCD_SIZE,container))<0) return ret;
	for (i=0;i<65536U;i++)
	{
		if (GET_LE32(hdr)==0x6054b50U && GET_LE16(hdr+HDR_EOCD_OFFSET_COMMENTLENGTH)==i)
			break;
		/* Read new content byte by byte, this is the slow bit (but practically really rare) */
		if (i+1>container->fileLength-HDR_EOCD_SIZE)
			return CONTAINER_ERROR_INVALID_FORMAT;
		for (j=HDR_EOCD_SIZE-1;j;j--)
			hdr[j]=hdr[j-1];
		if ((ret=container_common_simpleRead(hdr,1,container->fileLength-HDR_EOCD_SIZE-i-1,container))<0) return ret;
	}
	if (i==65536U)
		return CONTAINER_ERROR_INVALID_FORMAT;

	if (GET_LE16(hdr+HDR_EOCD_OFFSET_DISKNO))
		return CONTAINER_ERROR_UNSUPPORTED_FORMAT;
	if (GET_LE16(hdr+HDR_EOCD_OFFSET_DISKNO))
		return CONTAINER_ERROR_UNSUPPORTED_FORMAT;
	numEntries=GET_LE16(hdr+HDR_EOCD_OFFSET_NUMENTRIES);
	if (numEntries!=GET_LE16(hdr+HDR_EOCD_OFFSET_NUMENTRIES_DISK))
		return CONTAINER_ERROR_UNSUPPORTED_FORMAT;
	cdLength=GET_LE32(hdr+HDR_EOCD_OFFSET_CDLENGTH);
	cdOffset=GET_LE32(hdr+HDR_EOCD_OFFSET_CDOFFSET);
	if (cdOffset+cdLength>container->fileLength)
		 return CONTAINER_ERROR_INVALID_FORMAT;

	for (i=0;i<numEntries;i++)
	{
		if ((ret=container_common_simpleRead(hdr,HDR_CFH_SIZE,cdOffset,container))<0) return ret;
		if (GET_LE32(hdr)!=0x2014b50U)
			return CONTAINER_ERROR_INVALID_FORMAT;
		/* patching, encryption */
		if (hdr[HDR_CFH_OFFSET_FLAGS]&0x61U)
			return CONTAINER_ERROR_UNSUPPORTED_FORMAT;
		/* not a bug: OS is defined in the high (LSB) byte of create version */
		isAmiga=hdr[HDR_CFH_OFFSET_CREATE_VERSION+1]==1;
#ifndef CONTAINER_ALLOW_NON_AMIGA_ARCHIVES
		if (!isAmiga)
			return CONTAINER_ERROR_NON_AMIGA_ARCHIVE;
#endif
		dataType=GET_LE16(hdr+HDR_CFH_OFFSET_METHOD);
		if (dataType)
		{
			if (dataType!=8)
				return CONTAINER_ERROR_UNSUPPORTED_FORMAT;
			dataType=1;
		}
/* until we support compression... */
#if 1
		if (dataType)
			return CONTAINER_ERROR_UNSUPPORTED_FORMAT;
#endif
		if (GET_LE16(hdr+HDR_CFH_OFFSET_DISK_NUMBER_START))
			 return CONTAINER_ERROR_INVALID_FORMAT;

		mtime=GET_LE32(hdr+HDR_CFH_OFFSET_MTIME);
		packedLength=GET_LE32(hdr+HDR_CFH_OFFSET_PACKEDSIZE);
		rawLength=GET_LE32(hdr+HDR_CFH_OFFSET_RAWSIZE);
		nameLength=GET_LE16(hdr+HDR_CFH_OFFSET_NAMELENGTH);
		extraLength=GET_LE16(hdr+HDR_CFH_OFFSET_EXTRALENGTH);
		commentLength=GET_LE16(hdr+HDR_CFH_OFFSET_COMMENTLENGTH);
		/* Dos file attributes... */
		isDir=!!(hdr[HDR_CFH_OFFSET_EXTERNAL_FILE_ATTRIBUTES]&0x10U);
		if (isDir)
		{
			packedLength=0;
			rawLength=0;
		}
		attributes=hdr[HDR_CFH_OFFSET_EXTERNAL_FILE_ATTRIBUTES+2]^0xfU;
		offset=GET_LE32(hdr+HDR_CFH_OFFSET_LOCAL_HEADER_OFFSET);
		cfhLength=HDR_CFH_SIZE+nameLength+extraLength+commentLength;
		if (cfhLength>cdLength)
			return CONTAINER_ERROR_INVALID_FORMAT;

		/* Mark that we need to parse the LFH when opening the file
		   for the first time
		*/
		if (packedLength)
			dataType|=0x100U;

		if (!isAmiga)
			commentLength=0;

		entry=container_malloc(sizeof(struct container_cached_file_entry)+nameLength*2+commentLength+3);
		if (!entry)
			return CONTAINER_ERROR_MEMORY_ALLOCATION_FAILED;
		entry->prev=0;
		entry->next=0;
		stringSpace=(void*)(entry+1);

		entry->filenote=(char*)stringSpace+nameLength*2+2;
		*entry->filenote=0;
		if (commentLength)
		{
			if ((ret=container_common_simpleRead(entry->filenote,commentLength,cdOffset+HDR_CFH_SIZE+nameLength+extraLength,container))<0)
			{
				container_free(entry);
				return ret;
			}
			entry->filenote[commentLength]=0;
		}
		entry->pathAndName=(char*)stringSpace;
		*stringSpace=0;
		if (nameLength)
		{
			if ((ret=container_common_simpleRead(stringSpace,nameLength,cdOffset+HDR_CFH_SIZE,container))<0)
			{
				container_free(entry);
				return ret;
			}
			if (stringSpace[nameLength-1]=='/')
				nameLength--;
			stringSpace[nameLength]=0;
		}
		entry->filename=(char*)stringSpace+nameLength+1;
		entry->path=entry->filename;
		*entry->filename=0;
		if (nameLength)
		{
			k=0;
			for (j=0;j<nameLength;j++)
			{
				if (stringSpace[j]=='/') k=j+1;
				entry->path[j]=stringSpace[j];
			}
			entry->path[nameLength]=0;
			entry->filename=entry->path+k;
			if (k) entry->path[k-1]=0;
				else entry->path+=nameLength;
		}

		/* Finally fill entry */

		entry->dataOffset=isDir?0:offset;
		entry->dataLength=packedLength;
		entry->dataType=dataType;
		entry->length=rawLength;

		entry->fileType=isDir?CONTAINER_TYPE_DIR:CONTAINER_TYPE_FILE;
		entry->protection=attributes;
		entry->mtimeDays=container_common_dosTimeToAmiga(mtime,&entry->mtimeMinutes,&entry->mtimeTicks);
#if 0
		printf("dataOffset 0x%x\n",entry->dataOffset);
		printf("dataLength 0x%x\n",entry->dataLength);
		printf("dataType 0x%x\n",entry->dataType);
		printf("length 0x%x\n",entry->length);
		printf("fileType %d\n",entry->fileType);
		printf("protection 0x%x\n",entry->protection);
		printf("mtime_days 0x%x\n",entry->mtimeDays);
		printf("mtime_minutes 0x%x\n",entry->mtimeMinutes);
		printf("mtime_ticks 0x%x\n",entry->mtimeTicks);
		printf("path '%s'\n",entry->path);
		printf("pathAndName '%s'\n",entry->pathAndName);
		printf("filename '%s'\n",entry->filename);
		printf("filenote '%s'\n\n",entry->filenote);
#endif
		container_common_insertFileEntry(container,entry);

		cdOffset+=cfhLength;
		cdLength-=cfhLength;
	}

	container->fileOpen=container_zip_fileOpen;
	container->fileRead=container_zip_fileRead;
	return 0;
}
