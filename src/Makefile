# example makefile
#
# Notes:
# - the indents must be tabs
# - if you need to specify a non-standard compiler, then set CXX
#   (it defaults to g++)
# - if you need to pass flags to the linker, set LDFLAGS rather
#   than alter the ${TARGET}: ${OBJECTS} rule
# - to see make's built-in rules use make -p

# target executable name
TARGET = scdaq

# source files
SOURCES = config.cc DmaInputFilter.cc elastico.cc FileDmaInputFilter.cc InputFilter.cc output.cc processor.cc scdaq.cc session.cc slice.cc WZDmaInputFilter.cc MicronDmaInputFilter.cc
C_SOURCES = wz_dma.c

# work out names of object files from sources
OBJECTS = $(SOURCES:.cc=.o)
OBJECTS += $(C_SOURCES:.c=.o)

# compiler flags (do not include -c here as it's dealt with by the
# appropriate rules; CXXFLAGS gets passed as part of command
# invocation for both compilation (where -c is needed) and linking
# (where it's not.)
CXXFLAGS = -std=c++11 -Wall -Wextra -O0 -g -rdynamic
#CXXFLAGS = -std=c++11 -Wall -Wextra -g -rdynamic

CFLAGS = $(CXXFLAGS)
LDFLAGS = -Llibmicron -ltbb -ltbbmalloc -lboost_thread -lcurl -lpthread -lcrypto

CPPFLAGS = -I. -Iwzdma

# default target (to build all)
all: ${TARGET}

# clean target
clean:
	rm ${OBJECTS} ${TARGET}

# rule to link object files to create target executable
# $@ is the target, here $(TARGET), and $^ is all the
# dependencies separated by spaces (duplicates removed),
# here ${OBJECTS}
${TARGET}: ${OBJECTS}
	${LINK.cc} -o $@ $^ -lmicron

# no rule is needed here for compilation as make already
# knows how to do it

# header dependencies (in more complicated cases, you can
# look into generating these automatically.)

#product.o : product.h test1.h test2.h

#test1.o : product.h test1.h test2.h

#test2.o : product.h test2.h

scdaq.o:	processor.h elastico.h output.h format.h server.h controls.h config.h session.h log.h
config.o:	config.h log.h
DmaInputFilter.o:	DmaInputFilter.h slice.h
elastico.o:	elastico.h format.h slice.h controls.h log.h
FileDmaInputFilter.o:	FileDmaInputFilter.h InputFilter.h log.h
InputFilter.o:	InputFilter.h log.h
output.o:	output.h slice.h log.h
processor.o:	processor.h slice.h format.h log.h
session.o:	session.h log.h
slice.o: 	slice.h
WZDmaInputFilter.o:	WZDmaInputFilter.h InputFilter.h tools.h log.h
wz_dma.o:	wz_dma.h
MicronDmaInputFilter.o: MicronDmaInputFilter.h libmicron/micron_dma.h
