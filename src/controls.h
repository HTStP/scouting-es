#ifndef CONTROLS_H
#define CONTROLS_H
#include <stdint.h>
#include <atomic>

struct ctrl {
  uint32_t run_number;
  std::atomic<bool> running;
  /* Always write data to a file regardless of the run status */
  bool output_force_write;
  uint64_t max_file_size;
  int packets_per_report;
};
#endif 
