#include "tbb/pipeline.h"
#include "tbb/tick_count.h"
#include "tbb/task_scheduler_init.h"
#include "tbb/tbb_allocator.h"
#include "tbb/concurrent_queue.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <string>
#include <iostream>

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>


#include "InputFilter.h"
#include "FileDmaInputFilter.h"
#include "WZDmaInputFilter.h"
#include "dma_input.h"
#include "file_input.h"
#include "processor.h"
#include "elastico.h"
#include "output.h"
#include "format.h"
#include "server.h"
#include "controls.h"
#include "config.h"
#include "log.h"

using namespace std;







bool silent = false;

int run_pipeline( int nbThreads, ctrl& control, config& conf )
{
  config::InputType input = conf.getInput();

  size_t MAX_BYTES_PER_INPUT_SLICE = conf.getDmaPacketBufferSize();
  size_t TOTAL_SLICES = conf.getNumberOfDmaPacketBuffers();

  // Create empty input reader, will assing later when we know what is the data source 
  std::shared_ptr<InputFilter> input_filter;

  // Create the pipeline
  tbb::pipeline pipeline;

  if (input == config::InputType::FILE) {
      // Create file-reading writing stage and add it to the pipeline
      MAX_BYTES_PER_INPUT_SLICE = 192*conf.getBlocksPerInputBuffer();
      TOTAL_SLICES = conf.getNumInputBuffers();
      
      //input_filter = std::make_shared<FileInputFilter>( conf.getInputFile(), MAX_BYTES_PER_INPUT_SLICE, TOTAL_SLICES );
      throw std::runtime_error("input type FILE is temporarily not supported");

  } else if (input == config::InputType::DMA) {
      // Create DMA reader
      //input_filter = std::make_shared<DmaInputFilter>( conf.getDmaDevice(), MAX_BYTES_PER_INPUT_SLICE, TOTAL_SLICES );
      throw std::runtime_error("input type DMA is temporarily not supported");

  } else if (input == config::InputType::FILEDMA) {
      // Create FILE DMA reader
      input_filter = std::make_shared<FileDmaInputFilter>( conf.getInputFile(), MAX_BYTES_PER_INPUT_SLICE, TOTAL_SLICES, control );

  } else if (input == config::InputType::WZDMA ) {
      // Create WZ DMA reader
      input_filter = std::make_shared<WZDmaInputFilter>( MAX_BYTES_PER_INPUT_SLICE, TOTAL_SLICES, control );

  } else {
    throw std::invalid_argument("Configuration error: Unknown input type was specified");
  }

  // Add input reader to a pipeline
  pipeline.add_filter( *input_filter );

  LOG(INFO) << "Configuration translated into:";
  LOG(INFO) << "  MAX_BYTES_PER_INPUT_SLICE: " << MAX_BYTES_PER_INPUT_SLICE;
  LOG(INFO) << "  TOTAL_SLICES: " << TOTAL_SLICES;

  // Create reformatter and add it to the pipeline
  // TODO: Created here so we are not subject of scoping, fix later...
  StreamProcessor stream_processor(MAX_BYTES_PER_INPUT_SLICE, conf.getDoZS()); 
  if ( conf.getEnableStreamProcessor() ) {
    pipeline.add_filter( stream_processor );
  }

  // Create elastic populator (if requested)
  std::string url = conf.getElasticUrl();
  // TODO: Created here so we are not subject of scoping, fix later...
  ElasticProcessor elastic_processor(MAX_BYTES_PER_INPUT_SLICE,
              &control,
              url,
              conf.getPtCut(),
              conf.getQualCut());
  if ( conf.getEnableElasticProcessor() ) {
    pipeline.add_filter(elastic_processor);
  }

  std::string output_file_base = conf.getOutputFilenameBase();

  // Create file-writing stage and add it to the pipeline
  OutputStream output_stream( output_file_base.c_str(), control);
  pipeline.add_filter( output_stream );

  // Run the pipeline
  tbb::tick_count t0 = tbb::tick_count::now();
  // Need more than one token in flight per thread to keep all threads 
  // busy; 2-4 works
  pipeline.run( nbThreads * 4 );
  tbb::tick_count t1 = tbb::tick_count::now();

  if ( !silent ) {
    LOG(INFO) << "time = " << (t1-t0).seconds();
  }

  return 1;
}



int main( int argc, char* argv[] ) {
  (void)(argc);
  (void)(argv);
  LOG(DEBUG) << "here 0";

  try {
    config conf("scdaq.conf");
    conf.print();
    LOG(DEBUG) << "here 1";
    ctrl control;
    //    tbb::tick_count mainStartTime = tbb::tick_count::now();


    control.running = false;
    control.run_number = 0;
    control.max_file_size = conf.getOutputMaxFileSize();//in Bytes
    control.packets_per_report = conf.getPacketsPerReport();
    control.output_force_write = conf.getOutputForceWrite();

    boost::asio::io_service io_service;
    server s(io_service, conf.getPortNumber(), control);
    boost::thread t(boost::bind(&boost::asio::io_service::run, &io_service));

    int nbThreads = conf.getNumThreads();

    tbb::task_scheduler_init init( nbThreads );
    if (!run_pipeline (nbThreads, control, conf))
      return 1;

    //    utility::report_elapsed_time((tbb::tick_count::now() - mainStartTime).seconds());

    return 0;
  } catch(std::exception& e) {
    LOG(ERROR) << "error occurred. error text is :\"" << e.what() << "\"";
    return 1;
  }
}
