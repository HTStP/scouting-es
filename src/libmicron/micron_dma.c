/*
 * Library encapsulating Micron functions
 */

#include <picodrv.h>
#include <pico_errors.h>

#include "micron_dma.h"

int micron_run_bit_file(const char *bitFilePath, micron_private** micron)
{
	//PicoDrv **drvpp
	return RunBitFile(bitFilePath, (PicoDrv**) micron);

}

int micron_find_pico(uint32_t model, micron_private** micron)
{
	return FindPico(model, (PicoDrv**) micron);
}

int micron_create_stream(micron_private* micron, int streamNum)
{
	return ((PicoDrv*) micron)->CreateStream(streamNum);
}

void micron_close_stream(micron_private* micron, int streamHandle)
{
	((PicoDrv*) micron)->CloseStream(streamHandle);
}

int micron_read_stream(micron_private* micron, int streamHandle, void *buf, int size)
{
	return ((PicoDrv*) micron)->ReadStream(streamHandle, buf, size);

}

const char *micron_picoerrors_fullerror(int erC, char *resultP, int resultSize)
{
	return PicoErrors_FullError(erC, resultP, resultSize);
}
