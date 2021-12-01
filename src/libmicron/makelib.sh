#
# Simple workaround creating a Micron library
#
# exit when any command fails
set -e

rm -f *.o *.a

echo "Compilling..."
g++ -Wl,--copy-dt-needed-entries -std=c++11 -Wall -Wextra -g -rdynamic -Wno-multichar -DLINUX -DPOSIX \
-fmessage-length=0 -fdiagnostics-show-option -fms-extensions -Wno-write-strings -DOSNAME=“Linux” \
-I. -I${PICOBASE}/software/include/linux -I${PICOBASE}/software/include \
-c \
${PICOBASE}/software/source/pico_drv_linux.cpp \
${PICOBASE}/software/source/GString.cpp \
${PICOBASE}/software/source/pico_drv.cpp \
${PICOBASE}/software/source/pico_errors.cpp \
${PICOBASE}/software/source/linux/linux.cpp \
${PICOBASE}/software/source/pico_drv_swsim.cpp \
micron_dma.c

echo "Creating library..."
ar rcs libmicron.a GString.o linux.o pico_drv.o pico_drv_linux.o pico_drv_swsim.o pico_errors.o micron_dma.o
