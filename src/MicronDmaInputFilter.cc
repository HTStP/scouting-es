
#include <ctype.h>
#include <cstring>
#include <system_error>

#include "MicronDmaInputFilter.h"
#include <boost/multiprecision/cpp_int.hpp>

#include "log.h"

using uint256_t  = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<256,  256,  boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>, boost::multiprecision::et_off>;


MicronDmaInputFilter::MicronDmaInputFilter( size_t packetBufferSize, size_t nbPacketBuffers, ctrl& control, config& conf ) :
	InputFilter( packetBufferSize, nbPacketBuffers, control )
{
	std::string bitFileName = conf.getBitFileName();
	bool loadBitFile = conf.getLoadBitFile();
    int err;

	// The RunBitFile function will locate a Pico card that can run the given bit file, and is not already
	//   opened in exclusive-access mode by another program. It requests exclusive access to the Pico card
	//   so no other programs will try to reuse the card and interfere with us.

	if (loadBitFile) {
		LOG(DEBUG) << "Loading FPGA with '" << bitFileName << "' ..."; 
		err = micron_run_bit_file(bitFileName.c_str(), &pico_);
	} else {
		err = micron_find_pico(0x852, &pico_);
	}

	LOG(TRACE) << "Opening streams to and from the FPGA";
	stream1_ = micron_create_stream(pico_, 1);

	if (stream1_ < 0) {
		// All functions in the Pico API return an error code.  If that code is < 0, then you should use
		// the PicoErrors_FullError function to decode the error message.
		//fprintf(stderr, "%s: CreateStream error: %s\n", "bitfile", PicoErrors_FullError(stream1_, 0, packetBufferSize));

		throw std::runtime_error( bitFileName + ": CreateStream error: " + micron_picoerrors_fullerror(stream1_, 0, packetBufferSize) );
	}
	LOG(TRACE) << "Created Micron DMA input filter"; 
}

MicronDmaInputFilter::~MicronDmaInputFilter() {
	// streams are automatically closed when the PicoDrv object is destroyed, or on program termination, but
	//  we can also close a stream manually.
	micron_close_stream(pico_, stream1_);
	LOG(TRACE) << "Destroyed Micron DMA input filter";
}


// add pad bytes to next multiple of 16 bytes
int pad_for_16bytes(int len) {
	int pad_len = len;
	if(len%16 !=0) {
		pad_len = len + (16 - len%16); 
	}
	return pad_len;
}

// print <count> 256-bit numbers from buf
void print256(FILE *f, void *buf, int count)
{
	uint32_t	*u32p = (uint32_t*) buf;

	for (int i=0; i < count; ++i){

		fprintf(f, "0x%08x_%08x_%08x_%08x_%08x_%08x_%08x_%08x\n", u32p[8*i+7], u32p[8*i+6], u32p[8*i+5],u32p[8*i+4], u32p[8*i+3], u32p[8*i+2], u32p[8*i+1], u32p[8*i]);
	}
}


// print <count> 128-bit numbers from buf
void print128(FILE *f, void *buf, int count)
{
	uint32_t	*u32p = (uint32_t*) buf;

	for (int i=0; i < count; ++i)
		fprintf(f, "0x%08x_%08x_%08x_%08x\n", u32p[4*i+3], u32p[4*i+2], u32p[4*i+1], u32p[4*i]);
}


ssize_t MicronDmaInputFilter::runMicronDAQ(char **buffer, size_t bufferSize)
{
	// Usually Pico streams come in two flavors: 4Byte wide, 16Byte wide (equivalent to 32bit, 128bit respectively)
	// However, all calls to ReadStream and WriteStream must be 16Byte aligned (even for 4B wide streams)
	// There is also an 'undocumented' 32Byte wide (256 bit) stream 
	// We are using that stream here (and in the firmware)
	//
	// Now allocate 32B aligned space for read and write stream buffers
	//
	// Similarily, the size of the call, in bytes, must also be a multiple of 16Bytes.
	//
	// err = posix_memalign((void**)&rbuf, 32, size);
    //
    // NOTE: Buffer are already preallocated and aligned to 32 bytes boundaries

	// As with WriteStream, a ReadStream will block until all data is returned from the FPGA to the host.
	//
	// A user application will either have a deterministic amount of results, or a non-deterministic amount. 
	// - When a non-deterministic amount of results are expected, and given the blocking nature of the ReadStream,
	//   a user should use the GetBytesAvailable() call to determine the amount of data available for retrieval.
	// - When a deterministic amount of results is expected, a user can skip the performance impacting
	//   GetBytesAvailable() call and request the full amount of results. The user could also call ReadStream()
	//   iteratively, without GetBytesAvailable(), in which case getting smaller chunks of results may allow
	//   additional processing of the data on the host while the FPGA generates more results. 
	//   Either approach works, and should be kept in mind when tuning performance of your application.
	//
	//
	// By calling GetBytesAvailable, one can see how much data is ready to read // not used for speed
	//i = pico->GetBytesAvailable(stream1_, true /* reading */);
	//	if (i < 0){
	//	fprintf(stderr, "%s: GetBytesAvailable error: %s\n", "bitfile", PicoErrors_FullError(i, ibuf, sizeof(ibuf)));
	//	exit(-1);
	//	}

	// Here is where we actually call ReadStream
	// This reads "size" number of bytes of data from the output stream specified by our stream handle (e.g. 'stream') 
	// into our host buffer (rbuf)
	// This call will block until it is able to read the entire "size" Bytes of data.
	int err = micron_read_stream(pico_, stream1_, *buffer, bufferSize);
	if (err < 0) {
		//fprintf(stderr, "%s: ReadStream error: %s\n", "bitfile", PicoErrors_FullError(err, **ibuf, getPacketBufferSize()));
		throw std::runtime_error( "ReadStream finished with error" );
	}

	return bufferSize;
}


/**************************************************************************
 * Entry points are here
 * Overriding virtual functions
 */

//Print some additional info
void MicronDmaInputFilter::print(std::ostream& out) const
{
	// out
	// 	<< ", DMA errors " << stats.nbDmaErrors
	// 	<< ", oversized " << stats.nbDmaOversizedPackets
	// 	<< ", resets " << stats.nbBoardResets;
}

// Read a packet from DMA
ssize_t MicronDmaInputFilter::readInput(char **buffer, size_t bufferSize)
{
	return runMicronDAQ(buffer, bufferSize);
}
