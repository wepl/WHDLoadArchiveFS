/* Copyright (C) Teemu Suutari */

/* Skip and DecodePartial are almost the same, still different enough */

	uint32_t pos=state->rawPos,blockLength=state->blockRemaining;
	uint16_t symbol=state->remainingRepeat,distance=state->distance;
#ifndef ARCHIVEFS_LHA_DECOMPRESS_SKIP
	uint32_t bufferPos=0;
#endif
	uint16_t historyPos=state->historyPos;
	uint8_t *history=state->history;
	int32_t ret;

	if (symbol)
	{
		if (pos<distance)
		{
			uint32_t spaceLength;

			spaceLength=distance-pos;
			if (spaceLength>symbol) spaceLength=symbol;
			while (pos<targetLength && spaceLength--)
			{
				history[historyPos++]=0x20U;
				historyPos&=8191U;
#ifndef ARCHIVEFS_LHA_DECOMPRESS_SKIP
				dest[bufferPos++]=0x20U;
#endif
				pos++;
				symbol--;
			}
		}
		while (pos<targetLength && symbol--)
		{
			uint8_t ch;

			ch=history[(historyPos-distance)&8191U];
			history[historyPos++]=ch;
			historyPos&=8191U;
#ifndef ARCHIVEFS_LHA_DECOMPRESS_SKIP
			dest[bufferPos++]=ch;
#endif
			pos++;
		}
	}

	for (;pos<targetLength;)
	{
		if (!blockLength)
		{
			blockLength=ret=archivefs_lhaInitializeBlock(state);
			if (ret<0) return ret;
		}
		blockLength--;

		archivefs_lhaHuffmanDecode(symbol,state,state->symbolTree);
		if (symbol<256U)
		{
			history[historyPos++]=symbol;
			historyPos&=8191U;
#ifndef ARCHIVEFS_LHA_DECOMPRESS_SKIP
			dest[bufferPos++]=symbol;
#endif
			pos++;
		} else {
			archivefs_lhaDecodeDistance(distance,state);

			symbol-=253U;
			if (pos<distance)
			{
				uint32_t spaceLength;

				spaceLength=distance-pos;
				if (spaceLength>symbol) spaceLength=symbol;
				while (pos<targetLength && spaceLength--)
				{
					history[historyPos++]=0x20U;
					historyPos&=8191U;
#ifndef ARCHIVEFS_LHA_DECOMPRESS_SKIP
					dest[bufferPos++]=0x20U;
#endif
					pos++;
					symbol--;
				}
			}
			while (pos<targetLength && symbol--)
			{
				uint8_t ch;

				ch=history[(historyPos-distance)&8191U];
				history[historyPos++]=ch;
				historyPos&=8191U;
#ifndef ARCHIVEFS_LHA_DECOMPRESS_SKIP
				dest[bufferPos++]=ch;
#endif
				pos++;
			}
		}
	}
	state->filePos=(state->archive->blockIndex<<state->archive->blockShift)+state->archive->blockPos-state->fileOffset;
	state->rawPos=pos;
	state->blockRemaining=blockLength;
	state->remainingRepeat=symbol;
	state->distance=distance;
	state->historyPos=historyPos;
	return 0;
