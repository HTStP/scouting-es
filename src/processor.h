#ifndef PROCESSOR_H
#define PROCESSOR_H

#include "tbb/pipeline.h"
    
#include <iostream>
#include <fstream>
#include <vector>

//reformatter

class Slice;

class StreamProcessor: public tbb::filter {
public:
  enum class ProcessorType { PASS_THROUGH, GMT };

public:
  StreamProcessor(size_t max_size_, bool doZS_, ProcessorType processorType_, uint32_t nOrbitsPerDMAPacket_);
  void* operator()( void* item )/*override*/;
  ~StreamProcessor();

private:
  Slice* process(Slice& input, Slice& out);
  bool CheckFrameMultBlock(uint32_t inputSize);  
  std::vector<unsigned int> CountBX(Slice& input, char* p);
  uint32_t FillOrbit(Slice& input, Slice& out, std::vector<unsigned int>& bx_vect);
 
  std::ofstream myfile;
private:
  size_t max_size;
  uint64_t nbPackets;
  bool doZS;
  uint32_t nOrbitsPerDMAPacket;
  ProcessorType processorType;
};

#endif
