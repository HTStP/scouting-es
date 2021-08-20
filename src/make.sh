gcc -c wz_dma.c wz_dma.h -I. -Iwzdma
g++ scdaq.cc -Wl,--copy-dt-needed-entries -lrt -ltbb -lboost_thread -lcurl -std=c++11 -Wall -Wextra -g -rdynamic -O3 -Wno-multichar -DLINUX -DPOSIX -I. -I${PICOBASE}/software/include/linux -I${PICOBASE}/software/include -Iwzdma -fmessage-length=0 -fdiagnostics-show-option -fms-extensions -Wno-write-strings -DOSNAME=“Linux” -o scdaq micronDAQ.h ${PICOBASE}/software/source/pico_drv_linux.cpp ${PICOBASE}/software/source/GString.cpp ${PICOBASE}/software/source/pico_drv.cpp ${PICOBASE}/software/source/pico_errors.cpp ${PICOBASE}/software/source/linux/linux.cpp ${PICOBASE}/software/source/pico_drv_swsim.cpp file_input.h processor.h elastico.h output.h format.h server.h controls.h config.h session.h log.h slice.h elastico.h  WZDmaInputFilter.h InputFilter.h tools.h wz_dma.o FileDmaInputFilter.h dma_input.h dma_input.h micronDAQ.cc config.cc dma_input.cc elastico.cc FileDmaInputFilter.cc file_input.cc  InputFilter.cc output.cc  processor.cc  session.cc  slice.cc WZDmaInputFilter.cc 