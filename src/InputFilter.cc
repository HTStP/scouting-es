#include <cassert>
#include <iomanip>
#include <system_error>

#include "InputFilter.h"
#include "slice.h"
#include "controls.h"
#include "log.h"

InputFilter::InputFilter(size_t packetBufferSize, size_t nbPacketBuffers, ctrl& control) : 
    filter(serial_in_order),
    control_(control),
    nextSlice_(Slice::preAllocate( packetBufferSize, nbPacketBuffers )),
    nbReads_(0),
    nbBytesRead_(0),
    previousNbBytesRead_(0),
    previousStartTime_( tbb::tick_count::now() )
{ 
    minBytesRead_ = SSIZE_MAX;
    maxBytesRead_ = 0;
    previousNbReads_ = 0;

    LOG(TRACE) << "Configuration translated into:";
    LOG(TRACE) << "  MAX_BYTES_PER_INPUT_SLICE: " << packetBufferSize;
    LOG(TRACE) << "  TOTAL_SLICES: " << nbPacketBuffers;
    LOG(TRACE) << "Created input filter and allocated at " << static_cast<void*>(nextSlice_);
}

InputFilter::~InputFilter() {
  LOG(TRACE) << "Destroy input filter and delete at " << static_cast<void*>(nextSlice_);

  Slice::giveAllocated(nextSlice_);
  LOG(DEBUG) << "Input operator performed " << nbReads_ << " read";
}


void InputFilter::printStats(std::ostream& out, ssize_t lastBytesRead)
{
     // Calculate DMA bandwidth
    tbb::tick_count now = tbb::tick_count::now();
    double time_diff = (double)((now - previousStartTime_).seconds());
    previousStartTime_ = now;

    uint64_t nbReadsDiff = nbReads_ - previousNbReads_;
    previousNbReads_ = nbReads_;
    assert( nbReadsDiff > 0 );

    uint64_t nbBytesReadDiff = nbBytesRead_ - previousNbBytesRead_;
    previousNbBytesRead_ = nbBytesRead_;

    // Calculate bandwidth
    double bwd = nbBytesReadDiff / ( time_diff * 1024.0 * 1024.0 );

    // Calculate avg packet size
    uint64_t avgBytesRead = nbBytesReadDiff / nbReadsDiff;

	  // Save formatting
    std::ios state(nullptr);
	  state.copyfmt(out);

    out 
      << "#" << nbReads_ << ": Reading " << std::fixed << std::setprecision(1) << bwd << " MB/sec, " 
      << nbReadsDiff << " packet(s) min/avg/max " << minBytesRead_ <<  '/' << avgBytesRead << '/' << maxBytesRead_
      << " last " << lastBytesRead;
      
	  // Restore formatting
	  out.copyfmt(state);

    // Print additional info
    print( out );

    // Clear statistics
    minBytesRead_ = SSIZE_MAX;
    maxBytesRead_ = 0;     
}


/*
 * This function is a HACK. It should not be here!
 * It should be a part of processor. Puting it here
 * is a quick and dirty solution for debugging.
 * TODO: Fixme!
 */
void dumpPacketTrailer(char *buffer, size_t size, std::ostream& out)
{
  // If a packet doesn't have a proper trailer, we cannot check
  if (size < 32) {
    return;
  }

  // Get to the trailer
  buffer += size - 32;

  // Save formatting
  std::ios state(nullptr);
  state.copyfmt(out);

  uint64_t deadbeef = *reinterpret_cast<uint64_t *>( buffer ); 
  uint64_t autorealignCounter = *reinterpret_cast<uint64_t *>( buffer + 8 );
  uint64_t droppedOrbitCounter = *reinterpret_cast<uint64_t *>( buffer + 2*8 );
  uint64_t orbitCounter = *reinterpret_cast<uint64_t *>( buffer + 3*8 );

  if (deadbeef == 0xdeadbeefdeadbeef) {
    out 
      << ", HW: autorealign " << autorealignCounter
      << " orbits " << orbitCounter
      << " dropped " <<  droppedOrbitCounter;

  } else {
    out << ", HW trailer: " << std::hex;
    for (int i = 0; i < 32; i++) {
      unsigned int val = (uint8_t)(buffer[i]);
      out << std::setw(2) << std::setfill('0') << val;
      i++;
      val = (uint8_t)(buffer[i]);
      out << std::setw(2) << std::setfill('0') << val << ' ';
    }
  }

  // Restore formatting
  out.copyfmt(state);
}


void* InputFilter::operator()(void*) {
  // Prepare destination buffer
  char *buffer = nextSlice_->begin();
  // Available buffer size
  size_t bufferSize = nextSlice_->avail();
  ssize_t bytesRead = 0;

  nbReads_++;

  // It is optional to use the provided buffer
  bytesRead = readInput( &buffer, bufferSize );

  // This should really not happen
  assert( bytesRead != 0);

  if (buffer != nextSlice_->begin()) {
    // If read returned a different buffer, then it didn't use our buffer and we have to copy data
    // FIXME: It is a bit stupid to copy buffer, better would be to use zero copy approach 
    memcpy( nextSlice_->begin(), buffer, bytesRead );
  }

  // Notify that we processed the given buffer
  readComplete(buffer);

  // Update some stats
  nbBytesRead_ += bytesRead;

  // Update min/max
  minBytesRead_ = bytesRead < minBytesRead_ ? bytesRead : minBytesRead_;
  maxBytesRead_ = bytesRead > maxBytesRead_ ? bytesRead : maxBytesRead_;

  // Print some statistics
  if (control_.packets_per_report && (nbReads_ % control_.packets_per_report == 0)) {
    std::ostringstream log;
    printStats( log, bytesRead );
    // HACK: This function is not supposed to be called from here
    dumpPacketTrailer( nextSlice_->begin(), bytesRead, log );
    LOG(INFO) << log.str();
  }

  // Have more data to process.
  Slice* thisSlice = nextSlice_;
  nextSlice_ = Slice::getAllocated();
  
  // Adjust the end of this buffer
  thisSlice->set_end( thisSlice->end() + bytesRead );
  
  return thisSlice;

}

