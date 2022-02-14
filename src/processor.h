#ifndef PROCESSOR_H
#define PROCESSOR_H

#include "tbb/pipeline.h"
    
#include <iostream>
#include <fstream>
//reformatter

class Slice;

class StreamProcessor: public tbb::filter {
public:
  enum class ProcessorType { PASS_THROUGH, GMT };

public:
  StreamProcessor(size_t max_size_, bool doZS_, ProcessorType processorType_);
  void* operator()( void* item )/*override*/;
  ~StreamProcessor();

private:
  Slice* process(Slice& input, Slice& out);
  
  std::ofstream myfile;
private:
  size_t max_size;
  uint64_t nbPackets;
  bool doZS;
  ProcessorType processorType;
};

#endif
