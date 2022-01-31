#ifndef DMA_INPUT_H
#define DMA_INPUT_H

#include <memory>
#include <string>

#include "tbb/pipeline.h"
#include "tbb/tick_count.h"

#include "InputFilter.h"

class DmaInputFilter: public InputFilter {
 public:
  DmaInputFilter( const std::string& deviceFileName, size_t packetBufferSize, size_t nbPacketBuffers, ctrl& control );
  ~DmaInputFilter();

protected:
  ssize_t readInput(char **buffer, size_t bufferSize); // Override
  void print(std::ostream& out) const;  // Override

private:
  int dma_fd;

  ssize_t readPacketFromDMA(char **buffer, size_t bufferSize);

  struct Statistics {
    uint64_t nbDmaErrors = 0;
    uint64_t nbDmaOversizedPackets = 0;
  } stats;
};


typedef std::shared_ptr<DmaInputFilter> DmaInputFilterPtr;

#endif
