#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <stdint.h>
#include "boost/lexical_cast.hpp"
#include <map>
#include <stdexcept>
#include "processor.h"

class config{
public:
  
  enum class InputType { WZDMA, DMA, FILEDMA, FILE };

  config(std::string filename);

  void print() const;

  InputType getInput() const {
    const std::string& input = vmap.at("input");
    if (input == "wzdma") {
      return InputType::WZDMA;
    }
    if (input == "dma") {
      return InputType::DMA;
    }
    if (input == "filedma") {
      return InputType::FILEDMA;
    }    
    if (input == "file") {
      return InputType::FILE;
    }
    throw std::invalid_argument("Configuration error: Wrong input type '" + input + "'");
  }
  const std::string& getDmaDevice() const 
  {
    return vmap.at("dma_dev");
  }
  uint64_t getDmaPacketBufferSize() const {
    std::string v = vmap.at("dma_packet_buffer_size");
    return boost::lexical_cast<uint64_t>(v.c_str()); 
  }
  uint32_t getNumberOfDmaPacketBuffers() const {
    std::string v = vmap.at("dma_number_of_packet_buffers");
    return boost::lexical_cast<uint32_t>(v.c_str()); 
  }  
  uint32_t getPacketsPerReport() const {
    std::string v = vmap.at("packets_per_report");
    return boost::lexical_cast<uint32_t>(v.c_str()); 
  } 
  const std::string& getInputFile() const {
    return vmap.at("input_file");
  }
  const std::string& getElasticUrl() const
  {
    return vmap.at("elastic_url");
  }
  uint32_t getQualCut() const {
    std::string v = vmap.at("quality_cut");
    return boost::lexical_cast<uint32_t>(v.c_str());
  }
  uint32_t getPtCut() const {
    std::string v = vmap.at("pt_cut");
    return boost::lexical_cast<uint32_t>(v.c_str());
  }

  StreamProcessor::ProcessorType getProcessorType() const {
    const std::string& input = vmap.at("processor_type");
    if (input == "PASS_THROUGH") {
      return StreamProcessor::ProcessorType::PASS_THROUGH;
    }
    if (input == "GMT") {
      return StreamProcessor::ProcessorType::GMT;
    }
    throw std::invalid_argument("Configuration error: Wrong processor type '" + input + "'");
  }

  const std::string& getOutputFilenamePrefix() const 
  {
    return vmap.at("output_filename_prefix");
  }
  const std::string& getOutputFilenameBase() const 
  {
    return vmap.at("output_filename_base");
  }
  uint64_t getOutputMaxFileSize() const {
    std::string v = vmap.at("max_file_size");
    return boost::lexical_cast<uint64_t>(v.c_str());
  }
  bool getOutputForceWrite() const {
    return (true ? vmap.at("output_force_write") == "yes" : false);
  }

  uint32_t getNumThreads() const {
    std::string v = vmap.at("threads");
    return boost::lexical_cast<uint32_t>(v.c_str());
  }
  short getPortNumber() const {
    std::string v = vmap.at("port");
    return boost::lexical_cast<short>(v.c_str());
  }
  bool getEnableStreamProcessor() const {
    return (true ? vmap.at("enable_stream_processor") == "yes" : false);
  }
  bool getEnableElasticProcessor() const {
    return (true ? vmap.at("enable_elastic_processor") == "yes" : false);
  }
  bool getDoZS() const {
    return (true ? vmap.at("doZS") == "yes" : false);
  }

private:
  
  std::map<std::string,std::string> vmap;
  

};
#endif
