#ifndef MICRONDMA_INPUT_FILER_H
#define MICRONDMA_INPUT_FILER_H

#include <memory>

#include "libmicron/micron_dma.h"
#include "InputFilter.h"
#include "config.h"

class MicronDmaInputFilter : public InputFilter {
	public:
		MicronDmaInputFilter( size_t, size_t , ctrl&, config& );
		virtual ~MicronDmaInputFilter();

	protected:
		ssize_t readInput(char **buffer, size_t bufferSize); // Override
		void print(std::ostream& out) const;  // Override

	private:
		micron_private* pico_;
		int stream1_;

		ssize_t runMicronDAQ(char **buffer, size_t bufferSize);
};
typedef std::shared_ptr<MicronDmaInputFilter> MicronDmaInputFilterPtr;

#endif // MICRONDMA_INPUT_FILER_H
