#ifndef MICRON_DMA_H
#define MICRON_DMA_H


typedef void micron_private;

int micron_run_bit_file(const char *bitFilePath, micron_private** micron);
int micron_find_pico(uint32_t model, micron_private** micron);
int micron_create_stream(micron_private* micron, int streamNum);
void micron_close_stream(micron_private* micron, int streamHandle);
int micron_read_stream(micron_private* micron, int streamHandle, void *buf, int size);
const char *micron_picoerrors_fullerror(int erC, char *resultP, int resultSize);

#endif  // MICRON_DMA_H

