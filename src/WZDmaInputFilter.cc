#include <cassert>
#include <cerrno>
#include <system_error>
#include <string>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "tools.h"
#include "WZDmaInputFilter.h"
#include "log.h"


WZDmaInputFilter::WZDmaInputFilter( size_t packetBufferSize, size_t nbPacketBuffers, ctrl& control ) : 
  InputFilter( packetBufferSize, nbPacketBuffers, control )
{ 
  // Initialize the DMA subsystem 
	if ( wz_init( &dma_ ) < 0 ) {
    std::string msg = "Cannot initialize WZ DMA device";
    if (errno == ENOENT) {
      msg += ". Check if xdma kernel module is loaded ('lsmod | grep xdma') and the board is visible on the PCIe bus ('lspci | grep Xilinx'). Error";
    }
    throw std::system_error(errno, std::system_category(), msg);
  }

	// Start the DMA
	if ( wz_start_dma( &dma_ ) < 0) {
    throw std::system_error(errno, std::system_category(), "Cannot start WZ DMA");
	}

  LOG(TRACE) << "Created WZ DMA input filter"; 
}

WZDmaInputFilter::~WZDmaInputFilter() {
  wz_stop_dma( &dma_ );
	wz_close( &dma_ );
  LOG(TRACE) << "Destroyed WZ DMA input filter";
}


inline ssize_t WZDmaInputFilter::read_packet_from_dma(char **buffer)
{
  int tries = 1;
  ssize_t bytes_read;

  while (1) {
    bytes_read = wz_read_start( &dma_, buffer );

    if (bytes_read < 0) {
      stats.nbDmaErrors++;
      LOG(ERROR) << tools::strerror("#" + std::to_string( nbReads() ) + ": Read failed, returned: " + std::to_string(bytes_read));

      if (errno == EIO) {
        LOG(ERROR) << "#" << nbReads() << ": Trying to restart DMA (attempt #" << tries << "):";
        wz_stop_dma( &dma_ );
        wz_close( &dma_ );

        // Initialize the DMA subsystem 
        if ( wz_init( &dma_ ) < 0 ) {
          throw std::system_error(errno, std::system_category(), "Cannot initialize WZ DMA device");
        }

        if (wz_start_dma( &dma_ ) < 0) {
          throw std::system_error(errno, std::system_category(), "Cannot start WZ DMA device");
        }

        LOG(ERROR) << "Success.";
        tries++;

        if (tries == 10) {
          throw std::runtime_error("FATAL: Did not manage to restart DMA.");
        }
        continue;
      }
      throw std::system_error(errno, std::system_category(), "FATAL: Unrecoverable DMA error detected.");
    }
    break;
  }

  // Should not happen
  assert(bytes_read > 0);  

  return bytes_read;
}


inline ssize_t WZDmaInputFilter::read_packet( char **buffer, size_t bufferSize )
{
  ssize_t bytesRead;

  // Read from DMA
  int skip = 0;
  int reset = 0;
  bytesRead = read_packet_from_dma( buffer );

  // If large packet returned, skip and read again
  while ( bytesRead > (ssize_t)bufferSize ) {
    stats.nbDmaOversizedPackets++;
    skip++;
    LOG(ERROR)  
      << "#" << nbReads() << ": DMA read returned " << bytesRead << " > buffer size " << bufferSize
      << ". Skipping packet #" << skip << '.';
    if (skip >= 100) {
      reset++;
      stats.nbBoardResets++;

      if (reset > 10) {
        LOG(ERROR) << "Resets didn't help!";
        throw std::runtime_error("FATAL: DMA is still returning large packets.");
      }

      // Oversized packet is usually sign of link problem
      // Let's try to reset the board
      LOG(ERROR) << "Going to reset the board:";
      if (wz_reset_board() < 0) {
        LOG(ERROR) << "Reset finished";
      } else {
        LOG(ERROR) << "Reset failed";
      }

      LOG(ERROR) << "Waiting for 30 seconds to clear any collected crap:";
      // Sleep for 30 seconds (TCDS may be paused)
      for (int i=30; i>0; i--) {
        if (i % 5 == 0) {
          LOG(ERROR) << i << " seconds...";
        }
        usleep(1000000);
      }
      LOG(ERROR) << " OK";

      // Reset skipped packets counter
      skip = 0;
    }
    bytesRead = read_packet_from_dma( buffer );
  }

  return bytesRead;
}


/**************************************************************************
 * Entry points are here
 * Overriding virtual functions
 */


// Print some additional info
void WZDmaInputFilter::print(std::ostream& out) const
{
    out 
      << ", DMA errors " << stats.nbDmaErrors
      << ", oversized " << stats.nbDmaOversizedPackets
      << ", resets " << stats.nbBoardResets;
}


// Read a packet from DMA
ssize_t WZDmaInputFilter::readInput(char **buffer, size_t bufferSize)
{
  return read_packet( buffer, bufferSize );
}


// Notify the DMA that packet was processed
void WZDmaInputFilter::readComplete(char *buffer) {
  (void)(buffer);

  // Free the DMA buffer
  if ( wz_read_complete( &dma_ ) < 0 ) {
      throw std::system_error(errno, std::system_category(), "Cannot complete WZ DMA read");
	}  
}