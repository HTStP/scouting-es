#ifndef PROCESSOR_H
#define PROCESSOR_H

#include "tbb/pipeline.h"
    
#include <iostream>
#include <fstream>
#include "config.h"
//reformatter

class Slice;

class StreamProcessor: public tbb::filter {
public:
  StreamProcessor(size_t, bool, std::string, config::InputType);
  void* operator()( void* item )/*override*/;
  ~StreamProcessor();

private:
  Slice* process(Slice& input, Slice& out);
  
  std::ofstream myfile;
private:
  size_t max_size;
  uint64_t nbPackets;
  bool doZS;
  std::string systemName;
  config::InputType inputType;
};

#endif
