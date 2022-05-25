#ifndef OUTPUT_H
#define OUTPUT_H

#include <cstdio>
#include <stdint.h>
#include <string>
#include "tbb/pipeline.h"

#include "controls.h"

//! Filter that writes each buffer to a file.
class OutputStream: public tbb::filter {


public:
  OutputStream( const std::string output_filename_base,  const std::string output_filename_prefix, ctrl& c );
  void* operator()( void* item ) /*override*/;

private:
  void open_next_file();
  void close_and_move_current_file();

private:
  std::string my_output_filename_base;
  std::string my_output_filename_prefix;
  uint32_t totcounts;
  uint64_t current_file_size;
  int32_t file_count;
  ctrl& control;
  FILE *current_file;
  uint32_t current_run_number;
  std::string journal_name;
};

#endif
