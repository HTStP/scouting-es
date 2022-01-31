#ifndef INPUT_FILTER_H
#define INPUT_FILTER_H

#include <cstddef>
#include <iostream>

#include "tbb/pipeline.h"
#include "tbb/tick_count.h"

#include "controls.h"

class Slice;

/*
 * This is an abstract class.
 * A derived class has to implement methods readInput and readComplete
 */ 
class InputFilter: public tbb::filter {
public:
  InputFilter(size_t packet_buffer_size, size_t number_of_packet_buffers, ctrl& control);
  virtual ~InputFilter();

  // Return the number of read calls
  uint64_t nbReads() { return nbReads_; }

protected:
  // Read input to a provided buffer or return a different buffer
  virtual ssize_t readInput(char **buffer, size_t bufferSize) = 0;

  // Notify the read that the buffer returned by readInput is processed (can be freed or reused)
  // The default implementation does nothing, since this function is required only in special cases like e.g. zero copy read
  virtual void readComplete(char *buffer) { (void)(buffer); }

  // Allow overridden function to print some additional info 
  virtual void print(std::ostream& out) const = 0;

private:
  void* operator()(void* item); 

  // NOTE: This can be moved out of this class into a separate one
  //       and run in a single thread in order to do reporting...
  void printStats(std::ostream& out, ssize_t lastBytesRead);

private:
  ctrl& control_;
  //FILE* input_file;
  Slice* nextSlice_;
  //uint32_t counts;

  // Number of successful reads
  uint64_t nbReads_;

  // Number of bytes read
  uint64_t nbBytesRead_;

  // For Performance monitoring

  // Snapshot of nbBytesRead_
  uint64_t previousNbBytesRead_;

  ssize_t minBytesRead_;
  ssize_t maxBytesRead_;

  uint64_t previousNbReads_;

  // Remember timestamp for performance monitoring 
  tbb::tick_count previousStartTime_;
};

#endif // INPUT_FILTER_H 
