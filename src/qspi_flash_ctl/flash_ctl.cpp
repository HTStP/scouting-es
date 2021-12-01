/*===================================================================================
 File: flash_ctl.cpp.

 This module is a simple utility designed to illustrate the operation of the QSPI
 flash controller.  A stream connects to the QSPI flash controller and the software
 has commands for erasing, programming, verifying and reading the flash, and reading
 the ID register of the flash.

 This program interfaces with the firmware module flash_ctl.v in the firmware directory.

=====================================================================================*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <getopt.h>
#include <picodrv.h>
#include <pico_errors.h>
#include </usr/include/boost/format.hpp>
#include "i2c.h"

// Globals
PicoDrv     *pico;
int         streamhandle;
uint32_t    *readbuf, *writebuf;
int         verbose = 0;

// Routine to read the controller busy output pin until it is deasserted
void check_qspi_flash_ctl_busy() {
    int err;
    char        ibuf[1024];

    writebuf[0] = 0x8;          // Data length in bytes of stream command
    writebuf[1] = 0x1000;       // Status register Address
    writebuf[2] = 0x1;          // Read cmd, bit[64]=1
    writebuf[3] = 0x0;          // unused
    if (verbose) {
        printf("Checking busy");
    } 

    do {
        err = pico->WriteStream(streamhandle, &writebuf[0], 16);
        if (err < 0) {
            fprintf(stderr, "flash_ctl_wr: WriteStream error: %s\n", PicoErrors_FullError(err, ibuf, sizeof(ibuf)));
            exit(-1);
        }
        err = pico->ReadStream(streamhandle, readbuf, 16);
        if (err < 0) {
            fprintf(stderr, "flash_ctl_wr: ReadStream error: %s\n", PicoErrors_FullError(err, ibuf, sizeof(ibuf)));
            exit(-1);
        }
        if (verbose) {
            printf("%s", (readbuf[0] & 0x01) ? "." : "\n");
        }
    } while ((readbuf[0] & 0x01) == 0x01); // loop until qspi_flash_ctl busy bit is low
}

// Routine to write 64-bits of data to a PicoBus register
void picobus_wr(uint32_t addr, uint64_t data) {
    int err;
    char        ibuf[1024];

    // Send qspi flash controller write command to stream interface
    writebuf[0] = 0x8;                                // Data length in bytes of PicoBus cycle 
    writebuf[1] = addr;                               // 32-bit Picobus Address
    writebuf[2] = 0x0;                                // Write cmd, bit[64] = 0
    writebuf[3] = 0x0;                                // unused upper portion of stream command word
    writebuf[4] = (uint32_t)  data & 0xFFFFFFFF;      // low 32-bits of 64-bit data 
    writebuf[5] = (uint32_t) (data>>32) & 0xFFFFFFFF; // high 32-bits of 64-bit data
    writebuf[6] = 0x0;                                // unused upper portion of stream data word
    writebuf[7] = 0x0;                                // unused upper portion of stream data word
    if (verbose) printf("Sending %08X%08X to   %03X \n", (uint32_t)(data>>32) & 0xFFFFFFFF, (uint32_t) data & 0xFFFFFFFF, addr);
    err = pico->WriteStream(streamhandle, &writebuf[0], 32);  // Stream write of 32 bytes, two 128-bit words
    if (err < 0) {
        fprintf(stderr, "picobus_wr: WriteStream error: %s\n", PicoErrors_FullError(err, ibuf, sizeof(ibuf)));
        exit(-1);
    }
}

// Routine to read 64-bits of data from a PicoBus register 
uint64_t picobus_rd(uint32_t addr) {
    int err;
    char        ibuf[1024];
    uint64_t    readdata;

    // clear our read buffer to prepare for the read.
    memset(readbuf, 0, sizeof(readbuf));

    // Send qspi flash controller read command to stream interface
    writebuf[0] = 0x8;                                // Data length in bytes of stream command
    writebuf[1] = addr;                               // 32-bit Picobus Address
    writebuf[2] = 0x1;                                // Read cmd, bit[64] = 1
    writebuf[3] = 0x0;                                // unused upper portion of stream command word
    err = pico->WriteStream(streamhandle, &writebuf[0], 16);
    if (err < 0) {
        fprintf(stderr, "picobus_rd: WriteStream error: %s\n", PicoErrors_FullError(err, ibuf, sizeof(ibuf)));
        exit(-1);
    }

    // Read the response data from the stream interface.
    err = pico->ReadStream(streamhandle, readbuf, 16);
    if (err < 0) {
        fprintf(stderr, "picobus_rd: ReadStream error: %s\n", PicoErrors_FullError(err, ibuf, sizeof(ibuf)));
        exit(-1);
    }

    readdata = ( (uint64_t) readbuf[1]<<32) | readbuf[0];
    return(readdata);
}

// Routine to write 64-bits of data to a qspi flash controller register
void flash_ctl_wr(uint32_t addr, uint64_t data) {
    int err;
    char        ibuf[1024];

    check_qspi_flash_ctl_busy();

    // Send qspi flash controller write command to stream interface
    writebuf[0] = 0x8;                                // Data length in bytes of PicoBus cycle 
    writebuf[1] = addr & 0xFF8;                       // 12-bit Picobus Address
    writebuf[2] = 0x0;                                // Write cmd, bit[64] = 0
    writebuf[3] = 0x0;                                // unused upper portion of stream command word
    writebuf[4] = (uint32_t)  data & 0xFFFFFFFF;      // low 32-bits of 64-bit data 
    writebuf[5] = (uint32_t) (data>>32) & 0xFFFFFFFF; // high 32-bits of 64-bit data
    writebuf[6] = 0x0;                                // unused upper portion of stream data word
    writebuf[7] = 0x0;                                // unused upper portion of stream data word
    if (verbose) printf("Sending %08X%08X to   %03X \n", (uint32_t)(data>>32) & 0xFFFFFFFF, (uint32_t) data & 0xFFFFFFFF, addr & 0xFF8);
    err = pico->WriteStream(streamhandle, &writebuf[0], 32);  // Stream write of 32 bytes, two 128-bit words
    if (err < 0) {
        fprintf(stderr, "flash_ctl_wr: WriteStream error: %s\n", PicoErrors_FullError(err, ibuf, sizeof(ibuf)));
        exit(-1);
    }
}

// Routine to read 64-bits of data from a qspi flash controller register and check
// if the received data equals the expected data
void flash_ctl_rd(uint32_t addr, uint64_t exp_data, int check_data) {
    int err;
    char        ibuf[1024];

    check_qspi_flash_ctl_busy();

    // clear our read buffer to prepare for the read.
    memset(readbuf, 0, sizeof(readbuf));

    // Send qspi flash controller read command to stream interface
    writebuf[0] = 0x8;                                // Data length in bytes of stream command
    writebuf[1] = addr & 0xFF8;                       // 12-bit Address
    writebuf[2] = 0x1;                                // Read cmd, bit[64] = 1
    writebuf[3] = 0x0;                                // unused upper portion of stream command word
    err = pico->WriteStream(streamhandle, &writebuf[0], 16);
    if (err < 0) {
        fprintf(stderr, "flash_ctl_rd: WriteStream error: %s\n", PicoErrors_FullError(err, ibuf, sizeof(ibuf)));
        exit(-1);
    }

    // Read the response data from the stream interface.
    err = pico->ReadStream(streamhandle, readbuf, 16);
    if (err < 0) {
        fprintf(stderr, "flash_ctl_rd: ReadStream error: %s\n", PicoErrors_FullError(err, ibuf, sizeof(ibuf)));
        exit(-1);
    }

    if (check_data) {
        if (((uint32_t) (exp_data & 0xFFFFFFFF) == readbuf[0]) && ((uint32_t) ((exp_data>>32) & 0xFFFFFFFF) == readbuf[1])) {
            if (verbose) {
                printf("Reading %08X%08X from %03X ", readbuf[1], readbuf[0], addr & 0xFF8);
                printf(" MATCH\n");
            } 
        } else { 
            printf("Reading %08X%08X from %03X ", readbuf[1], readbuf[0], addr & 0xFF8);
            printf(" MISMATCH, EXPECTED %08X%08X\n", (uint32_t) ((exp_data>>32) & 0xFFFFFFFF), (uint32_t) (exp_data & 0xFFFFFFFF));
        }
    }
}

void check_read_flag_status_reg_ready () {
    int i;

    if (verbose) printf("checking flash flag status register ready bit\n");

    i=0;
    do { flash_ctl_wr(0x000, (uint64_t) 0x0000000100000070);      // READ_FLAG_STATUS_CMD 
         flash_ctl_rd(0x018, (uint64_t) 0x0000000000000081, 0);   // expected data 81h
         if ((i & 0x00003FFF) == 0x00003FFF) printf(".");         // User output for each 16K times through the loop
         fflush(stdout);                                          // Cause the buffered output to print each period
         i=i+1;
    } while ((readbuf[0] & 0x80) != 0x80);                        // wait for bit[7] to go high, ready

    if (verbose) printf("Flash flag status register = %02X ready bit is high\n", readbuf[0]);
}

void soft_reset() {
    int err;
    char        ibuf[1024];

    // Assert soft reset to the qspi flash controller
    writebuf[0] = 0x0; 
    writebuf[1] = 0x0; 
    writebuf[2] = 0x03;   // assert soft reset 
    writebuf[3] = 0x0; 

    if (verbose) printf("Soft Reset = 1 Writing Stream %d with %d Bytes\n", streamhandle, 16);
    err = pico->WriteStream(streamhandle, writebuf, 16);
    if (err < 0) {
        fprintf(stderr, "soft_reset: WriteStream error: %s\n", PicoErrors_FullError(err, ibuf, sizeof(ibuf)));
        exit(-1);
    }

    usleep(10000); // 10 ms

    writebuf[0] = 0x0; 
    writebuf[1] = 0x0; 
    writebuf[2] = 0x02;   // deassert soft reset 
    writebuf[3] = 0x0; 

    if (verbose) printf("Soft Reset = 0 Writing Stream %d with %d Bytes\n", streamhandle, 16);
    err = pico->WriteStream(streamhandle, writebuf, 16);
    if (err < 0) {
        fprintf(stderr, "soft_reset: WriteStream error: %s\n", PicoErrors_FullError(err, ibuf, sizeof(ibuf)));
        exit(-1);
    }

    // Let the fifos come out of reset before sending the next command
    usleep(10000); // 10 ms
}

// Program the flash volatile configuration register, enhanced volatile configuration register
// and set the controller into quad spi mode
void init_flash() {

    flash_ctl_wr(0x000, (uint64_t) 0x0000000000000006); // WEL
    flash_ctl_wr(0x020, (uint64_t) 0x00000000000000AB); // wr_data, 10 dummy clock cycles, disable XIP, continous addresses no wrap
    flash_ctl_wr(0x000, (uint64_t) 0x0000000100000081); // wr_cmd, write volatile config reg
    flash_ctl_wr(0x000, (uint64_t) 0x0000000000000006); // WEL
    flash_ctl_wr(0x020, (uint64_t) 0x000000000000006F); // wr_data, Quad I/O protocol enabled, DTR disabled, disable hold on DQ3, 30 ohm drive DAA - was 7F
    flash_ctl_wr(0x000, (uint64_t) 0x0000000100000061); // wr cmd, write enhanced volatile config reg
    flash_ctl_wr(0x010, (uint64_t) 0x0000000000000100); // set qspi_mode=1, puts the controller in qspi mode
    flash_ctl_wr(0x000, (uint64_t) 0x0000000000000006); // WEL
    flash_ctl_wr(0x000, (uint64_t) 0x00000000000000B7); // ENTER 4BYTE Address Mode
}

// Reset the flash volatile configuration register, enhanced volatile configuration register
// and extended address register to the power-on reset default condition.
// This is necessary for Xilinx FPGA's to be able to successfully configure from flash
void reset_flash() {

    flash_ctl_wr(0x000, (uint64_t) 0x0000000000000066); // Reset Enable
    flash_ctl_wr(0x000, (uint64_t) 0x0000000000000099); // Reset Memory
    flash_ctl_wr(0x000, (uint64_t) 0x0000000000000006); // WEL
    flash_ctl_wr(0x000, (uint64_t) 0x00000000000000E9); // Exit 4BYTE Address Mode
    flash_ctl_wr(0x010, (uint64_t) 0x0000000000000000); // set qspi_mode=0, takes the controller out of qspi mode
}

void read_id_reg() {

    flash_ctl_wr(0x000, (uint64_t) 0x00000003000000AF); // issue rd_id cmd - 3 bytes
    flash_ctl_rd(0x018, (uint64_t) 0x000000000020BA20, 0); // rd_data out - MFG ID - 20h micron, MEM_TYPE - BAh(3V),BBh (1.8V), MEM_CAP - 21h(1Gb),20h(512Mb)

    printf("\nQSPI Flash READID Register = %08X \n", readbuf[0]);
    printf("Key: MFG ID - 20h micron, MEM_TYPE - BAh(3V), BBh(1.8V), MEM_CAP - 22h(2Gb), 21h(1Gb), 20h(512Mb), 19h(256Mb) \n");
}

// Reverse the bytes of an 8-byte word swapping bytes 7,6,5,4,3,2,1,0 to 0,1,2,3,4,5,6,7
// This is for the mgmt_qspi_flash_ctl.v module that sends out byte 7 first and byte 0 last
// but the flash expects byte 0 first and byte 7 last.
void byte_swap(unsigned char* ptr) {       
    unsigned char  tmp;

    for (int i=0; i<4; i++) {
        tmp        = *(ptr+i);
        *(ptr+i)   = *(ptr+7-i);
        *(ptr+7-i) = tmp;
    }
}

// Read and print 256-bytes of flash data
void read_flash(uint32_t size, uint32_t address) {

    printf("\nReading Flash Contents: %xh Bytes from %08Xh\n", size, address);
    printf("Output is compatible with:  od -t x4 -A x -v bitstream.bin \n");

    for (int j=0; j<size; j=j+8) {
        flash_ctl_wr(0x008, (uint64_t) address + j);           // spi_addr for 8 bytes of data
        flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec);    // issue the quad fast read cmd - 0xec, with 10 dummy clocks
        flash_ctl_rd(0x018, (uint64_t) 0x0000000000000000, 0); // rd the data_out register to get the 8 bytes of data
        byte_swap((unsigned char*) &readbuf[0]);               // Swap order of 8-bytes for the mgmt_qspi_flash_ctl.v 
        if ((j & 0x008) == 0) {
            printf("\n%06x ", j);
            printf("%08x %08x ", readbuf[0], readbuf[1]);
        } else {
            printf("%08x %08x", readbuf[0], readbuf[1]);
        }
    }
    printf("\n");
}

// Erase entire flash contents 
// Issue die erase command C4 for the 1Gbit flash on the SB-852, each die is 512Mbit or 64MB
// Issue die erase command C4 for the 2Gbit flash on the SB-851, each die is 512Mbit or 64MB
// Issue bulk erase command C7 for the 256Mbit flash on the AC-510
// Issue bulk erase command C7 for the 512Mbit flash on the AC-511
void erase_flash() {
    int flash_size = 0;

    read_id_reg();                     // get the flash device size
    flash_size = readbuf[0] & 0xFF;  // 22h(2Gb), 21h(1Gb), 20h(512Mb), 19h(256Mb) 

    switch (flash_size) {
        case 0x19:
                   printf("\nErasing Flash contents - Flash Size is 256 Mbit\n");
                   flash_ctl_wr(0x000, (uint64_t) 0x0000000000000006); // WEL
                   flash_ctl_wr(0x008, (uint64_t) 0x0000000000000000); // Fill out the SPI_ADDR - 0x00000000
                   flash_ctl_wr(0x000, (uint64_t) 0x00000000000000C7); // Bulk Erase, NQ25_256 requires C7 
                   check_read_flag_status_reg_ready ();  
                   break;
        case 0x20:
                   printf("\nErasing Flash contents - Flash Size is 512 Mbit\n");
                   flash_ctl_wr(0x000, (uint64_t) 0x0000000000000006); // WEL
                   flash_ctl_wr(0x008, (uint64_t) 0x0000000000000000); // Fill out the SPI_ADDR - 0x00000000
                   flash_ctl_wr(0x000, (uint64_t) 0x00000000000000C7); // Bulk Erase, NQ25_512 requires C7 
                   check_read_flag_status_reg_ready ();  
                   break;
        case 0x21:
                   printf("\nErasing Flash contents - Flash Size is 1 Gbit\n");
                   printf("Erasing Die 1 ");
                   flash_ctl_wr(0x000, (uint64_t) 0x0000000000000006); // WEL
                   flash_ctl_wr(0x008, (uint64_t) 0x0000000000000000); // Fill out the SPI_ADDR - 0x00000000 for die 1, 64MB
                   flash_ctl_wr(0x000, (uint64_t) 0x00000000000000C4); // Die Erase, MT25_1G requires C4 
                   check_read_flag_status_reg_ready ();
                   flash_ctl_wr(0x000, (uint64_t) 0x0000000000000006); // WEL
                   flash_ctl_wr(0x008, (uint64_t) 0x0000000004000000); // Fill out the SPI_ADDR - 0x04000000 for die 2, 64MB
                   flash_ctl_wr(0x000, (uint64_t) 0x00000000000000C4); // Die Erase, MT25_1G requires C4 
                   printf("\nErasing Die 2 ");
                   check_read_flag_status_reg_ready ();  
                   break;
        case 0x22:
                   printf("\nErasing Flash contents - Flash Size is 2 Gbit\n");
                   flash_ctl_wr(0x000, (uint64_t) 0x0000000000000006); // WEL
                   flash_ctl_wr(0x008, (uint64_t) 0x0000000000000000); // Fill out the SPI_ADDR - 0x00000000 for die 1, 64MB
                   flash_ctl_wr(0x000, (uint64_t) 0x00000000000000C4); // Die Erase, MT25_1G requires C4 
                   printf("Erasing Die 1 ");
                   check_read_flag_status_reg_ready ();
                   flash_ctl_wr(0x000, (uint64_t) 0x0000000000000006); // WEL
                   flash_ctl_wr(0x008, (uint64_t) 0x0000000004000000); // Fill out the SPI_ADDR - 0x04000000 for die 2, 64MB
                   flash_ctl_wr(0x000, (uint64_t) 0x00000000000000C4); // Die Erase, MT25_1G requires C4 
                   printf("\nErasing Die 2 ");
                   check_read_flag_status_reg_ready ();
                   flash_ctl_wr(0x000, (uint64_t) 0x0000000000000006); // WEL
                   flash_ctl_wr(0x008, (uint64_t) 0x0000000008000000); // Fill out the SPI_ADDR - 0x08000000 for die 3, 64MB
                   flash_ctl_wr(0x000, (uint64_t) 0x00000000000000C4); // Die Erase, MT25_1G requires C4 
                   printf("\nErasing Die 3 ");
                   check_read_flag_status_reg_ready ();
                   flash_ctl_wr(0x000, (uint64_t) 0x0000000000000006); // WEL
                   flash_ctl_wr(0x008, (uint64_t) 0x000000000C000000); // Fill out the SPI_ADDR - 0x0C000000 for die 4, 64MB
                   flash_ctl_wr(0x000, (uint64_t) 0x00000000000000C4); // Die Erase, MT25_2G requires C4 
                   printf("\nErasing Die 4 ");
                   check_read_flag_status_reg_ready ();  
                   break;
        default:
                   printf("Flash Size is unknown, exiting");
                   break;
    }
    printf("\n");
}

void erase_half_flash(int half) {
    int      flash_size = 0;
    uint32_t addr; 

    read_id_reg();                   // get the flash device size
    flash_size = readbuf[0] & 0xFF;  // 22h(2Gb), 21h(1Gb), 20h(512Mb), 19h(256Mb) 

    switch (flash_size) {
        case 0x19:
                   printf("\nErasing %s half of Flash contents - Flash Size is 256 Mbit\n", half ? "High" : "Low");
                   if (half == 1) {
                       addr = 0x01000000;             // 16MB is half way through 256 Mbit or 32 MB flash
                   } else {
                       addr = 0x0;                    // 16MB is half way through 256 Mbit or 32 MB flash
                   }
                   printf("\nErasing Sectors ");
                   for (int i=addr; i < (addr+0x01000000); i+= 0x10000) {  // erase 64KB per iteration
                       flash_ctl_wr(0x000, (uint64_t) 0x0000000000000006); // WEL
                       flash_ctl_wr(0x008, (uint64_t) i);                  // Fill out the SPI_ADDR of 64KB sector 
                       flash_ctl_wr(0x000, (uint64_t) 0x00000000000000D8); // Sector Erase  
                       printf(".");
                       check_read_flag_status_reg_ready ();
                   }
                   break;
        case 0x20:
                   printf("\nErasing %s half of Flash contents - Flash Size is 512 Mbit\n", half ? "High" : "Low");
                   if (half == 1) {
                       addr = 0x02000000;             // 32MB is half way through 512 Mbit or 64 MB flash
                   } else {
                       addr = 0x0;                    // 32MB is half way through 512 Mbit or 64 MB flash
                   }
                   printf("\nErasing Sectors ");
                   for (int i=addr; i < (addr+0x02000000); i+= 0x10000) {  // erase 64KB per iteration
                       flash_ctl_wr(0x000, (uint64_t) 0x0000000000000006); // WEL
                       flash_ctl_wr(0x008, (uint64_t) i);                  // Fill out the SPI_ADDR of 64KB sector 
                       flash_ctl_wr(0x000, (uint64_t) 0x00000000000000D8); // Sector Erase  
                       printf(".");
                       check_read_flag_status_reg_ready ();
                   }
                   break;
        case 0x21:
                   printf("\nErasing %s half of Flash contents - Flash Size is 1 Gbit\n", half ? "High" : "Low");
                   flash_ctl_wr(0x000, (uint64_t) 0x0000000000000006);     // WEL
                   if (half == 1) {
                       flash_ctl_wr(0x008, (uint64_t) 0x0000000004000000); // Fill out the SPI_ADDR - 0x04000000 for die 2, 64MB
                       printf("Erasing Die 2 ");
                   } else {
                       flash_ctl_wr(0x008, (uint64_t) 0x0000000000000000); // Fill out the SPI_ADDR - 0x00000000 for die 1, 64MB
                       printf("Erasing Die 1 ");
                   }
                   flash_ctl_wr(0x000, (uint64_t) 0x00000000000000C4);     // Die Erase, MT25_1G requires C4 
                   check_read_flag_status_reg_ready ();
                   break;
        case 0x22:
                   printf("\nErasing %s half of Flash contents - Flash Size is 2 Gbit\n", half ? "High" : "Low");
                   flash_ctl_wr(0x000, (uint64_t) 0x0000000000000006);     // WEL
                   if (half == 1) {
                       flash_ctl_wr(0x008, (uint64_t) 0x0000000008000000); // Fill out the SPI_ADDR - 0x04000000 for die 3, 64MB
                       printf("Erasing Die 3 ");
                   } else {
                       flash_ctl_wr(0x008, (uint64_t) 0x0000000000000000); // Fill out the SPI_ADDR - 0x00000000 for die 1, 64MB
                       printf("Erasing Die 1 ");
                   }
                   flash_ctl_wr(0x000, (uint64_t) 0x00000000000000C4);     // Die Erase, MT25_2G requires C4 
                   check_read_flag_status_reg_ready ();
                   flash_ctl_wr(0x000, (uint64_t) 0x0000000000000006);     // WEL
                   if (half == 1) {
                       flash_ctl_wr(0x008, (uint64_t) 0x000000000C000000); // Fill out the SPI_ADDR - 0x04000000 for die 4, 64MB
                       printf("\nErasing Die 4 ");
                   } else {
                       flash_ctl_wr(0x008, (uint64_t) 0x0000000004000000); // Fill out the SPI_ADDR - 0x00000000 for die 2, 64MB
                       printf("\nErasing Die 2 ");
                   }
                   flash_ctl_wr(0x000, (uint64_t) 0x00000000000000C4);     // Die Erase, MT25_2G requires C4 
                   check_read_flag_status_reg_ready ();
                   break;
        default:
                   printf("Flash Size is unknown, exiting");
                   break;
    }
    printf("\n");
}

// Program flash device with contents of .bin file using 256-byte writes 
// The flash must be previously erased for the program to work correctly
void program_flash(char* progbitfile, uint32_t address, uint32_t model) {

    FILE           *file;
    int            size=0;
    int            bitstream_pad;
    unsigned char* data = NULL;
    uint64_t*      data_ptr;
    int            err;
    uint64_t       program_data;
 
    file = fopen(progbitfile, "rb");
    if (file == 0) {
        printf("Couldn't open file %s \n", progbitfile);
        exit(-1);
    }

    printf("\nProgramming Flash device with '%s' \n", progbitfile);

    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);

    bitstream_pad = 256 - (size % 256);

    printf("Bitstream file size = %d, padding %d bytes \n", size, bitstream_pad);

    data = (unsigned char *) malloc(size + bitstream_pad);  // allocate memory for the bitstream file, rounded up to the next 256-byte boundary
    memset(data+size, 0xff, bitstream_pad);                 // Set the pad bytes to all 1s

    // Read the bitstream file
    if ((err = fread(data, 1, size, file)) != size) {
        free(data);
        printf("fread error\n");
        exit(-1);
    }
    fclose(file);

    data_ptr = (uint64_t*) data;

    printf("Programming %08Xh bytes at address %08Xh\n", size+bitstream_pad, address);

    for (int i=0; i<(size + bitstream_pad); i+=256) {       // program 256 bytes per loop
        if ((i & 0x3FF00) == 0x3FF00) printf(".");          // User output for each 256K bytes programmed
        fflush(stdout);                                     // Cause the buffered output to print each period

        flash_ctl_wr(0x008, (uint64_t) (address + i));      // Set the ADDR to program 
        for (int j=0; j<32; j++) {
            program_data = *(data_ptr+i/8+j);               // 64-bit Data to program to the flash
            byte_swap((unsigned char*) &program_data);      // Swap order of the 8-bytes to program for the mgmt_qspi_flash_ctl.v 
            flash_ctl_wr(0x020, (uint64_t) program_data);   // Fill the WR_DATA, 8 bytes per write, 256 bytes per flash program command
        }
        check_read_flag_status_reg_ready ();                // Wait for flash busy to deassert from last program command before issuing the next one
        flash_ctl_wr(0x000, (uint64_t) 0x0000000000000006); // WEL
        flash_ctl_wr(0x000, (uint64_t) 0x0000010000000032); // issue QUAD_FAST_PGM, specify 256 bytes of WR_DATA   
    }
    check_read_flag_status_reg_ready ();                    // Wait for flash busy to deassert after the last program command
    printf("\nFlash programming done \n");
}

// Verify flash device contents begninning at address against verifybitfile
void verify_flash(char* verifybitfile, uint32_t address) {

    FILE           *file;
    int            size=0;
    int            bitstream_pad;
    unsigned char* data = NULL;
    uint64_t*      data_ptr;
    int            err;
 
    file = fopen(verifybitfile, "rb");
    if (file == 0) {
        printf("Couldn't open file %s \n", verifybitfile);
        exit(-1);
    }

    printf("\nVerify Flash contents beginning at %08Xh against '%s' \n", address, verifybitfile);

    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);

    bitstream_pad = 256 - (size % 256);

    printf("Bitstream file size = %d, padding %d bytes \n", size, bitstream_pad);

    data = (unsigned char *) malloc(size + bitstream_pad);  // allocate memory for the bitstream file, rounded up to the next 256-byte boundary
    memset(data+size, 0xff, bitstream_pad);                 // Set the pad bytes to all 1s

    // Read the bitstream file
    if ((err = fread(data, 1, size, file)) != size) {
        free(data);
        printf("fread error\n");
        exit(-1);
    }
    fclose(file);

    data_ptr = (uint64_t*) data;

    for (int i=0; i<(size + bitstream_pad); i+=8) {            // Read and verify 8-bytes per loop
        if ((i & 0x1FFF8) == 0x1FFF8) printf(".");             // User output for each 128K iterations
        fflush(stdout);                                        // Cause the buffered output to print each period

        flash_ctl_wr(0x008, (uint64_t) (address + i));         // spi_addr to read for 8 bytes of data
        flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec);    // issue the quad fast read cmd - 0xec, with 10 dummy clocks
        flash_ctl_rd(0x018, (uint64_t) 0x0000000000000000, 0); // rd the data_out register to get the 8 bytes of data
        byte_swap((unsigned char*) &readbuf[0]);               // Swap order of 8-bytes for the mgmt_qspi_flash_ctl.v 
 
        if (*data_ptr++ != *((uint64_t *) readbuf)) {
            printf("\nVerify FAILURE at flash address %08X, Expected %016lX Found %016lX \n", address+i, *--data_ptr, *((uint64_t *) readbuf));
            exit(-1);
        }

    }
    printf("\nFlash Verify done, flash contents match %s \n", verifybitfile);
}

// 64K Sector erase, 256-byte write at 0000h, 256-byte page write at 8000h, read back and verify
// Note this test does not do a byte swap on the data so the mgmt_qspi_flash_ctl.v actually 
// reverses the byte order of each 8-byte word but it is just data and if what is written is what
// is read then it passes.
void quad_256byte_write_read_test () {
    printf("\nRunning 4BYTE_QUAD_256BYTE_WRBUFFER_WR_RD_multiple Test \n");

    flash_ctl_wr(0x000, (uint64_t) 0x0000000000000006); // WEL
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000000000); // Fill out the SPI_ADDR - sector 0- 0x00000000
    flash_ctl_wr(0x000, (uint64_t) 0x00000000000000D8); // Sector Erase  DAA - was DCh, not recognized on NQ25_256
    check_read_flag_status_reg_ready ();

    flash_ctl_wr(0x000, (uint64_t) 0x0000000000000006); // WEL
    flash_ctl_wr(0x020, (uint64_t) 0x123456789ABCDEF6); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x2222222222222222); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x3333333333333333); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x4444444444444444); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x5555555555555555); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x6666666666666666); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x7777777777777777); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x8888888888888888); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x9999999999999999); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0xAAAAAAAAAAAAAAAA); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0xBBBBBBBBBBBBBBBB); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0xCCCCCCCCCCCCCCCC); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0xDDDDDDDDDDDDDDDD); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0xEEEEEEEEEEEEEEEE); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0xFFFFFFFFFFFFFFFF); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x0000000000000000); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x1234111111111111); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x1234222222222222); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x1234333333333333); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x1234444444444444); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x1234555555555555); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x1234666666666666); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x1234777777777777); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x1234888888888888); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x1234999999999999); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x1234AAAAAAAAAAAA); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x1234BBBBBBBBBBBB); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x1234CCCCCCCCCCCC); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x1234DDDDDDDDDDDD); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x1234EEEEEEEEEEEE); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x1234FFFFFFFFFFFF); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0xAAAAAAAAAFFFFFFF); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000000000); // Start at ADDR 0x00000000 
    flash_ctl_wr(0x000, (uint64_t) 0x0000010000000032); // issue QUAD_FAST_PGM, & specify 256 bytes of WR_DATA   DAA 3Eh does not work on N25Q256

    check_read_flag_status_reg_ready ();

    flash_ctl_wr(0x000, (uint64_t) 0x0000000000000006); // WEL
    flash_ctl_wr(0x020, (uint64_t) 0x1111111112341111); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x2222222212342222); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x3333333312343333); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x4444444412344444); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x5555555512345555); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x6666666612346666); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x7777777712347777); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x8888888812348888); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x9999999912349999); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0xAAAAAAAA1234AAAA); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0xBBBBBBBB1234BBBB); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0xCCCCCCCC1234CCCC); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0xDDDDDDDD1234DDDD); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0xEEEEEEEE1234EEEE); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0xFFFFFFFF1234FFFF); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0xAAAAAAAAAFFFFFFF); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x223456789ABCDEF6); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x5555555511111111); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0xaaaa111155559999); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x8888bbbb22227777); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x999999993333dddd); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x6666666666666666); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x5555555522222222); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x8888888888888888); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x2222221111111199); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0xAA444444443333AA); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0xBBBB444444444BBB); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0xCC666666666666CC); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0xDDDDD33333D3333D); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0xEE333333333222EE); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0xFFFFFFFFFFFFFFFF); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x020, (uint64_t) 0x0555502222220400); // Fill the WR_DATA 0-7bytes 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000008000); // Start at ADDR 0x00008000 
    flash_ctl_wr(0x000, (uint64_t) 0x0000010000000032); // issue QUAD_FAST_PGM, & specify 256 bytes of WR_DATA   

    check_read_flag_status_reg_ready ();

    flash_ctl_wr(0x008, (uint64_t) 0x0000000000000000); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec, 10 dummy clocks
    flash_ctl_rd(0x018, (uint64_t) 0x123456789abcdef6, 1); // rd the data_out register to get first 8 bytes
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000000008); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x2222222222222222, 1);
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000000010); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x3333333333333333, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000000018); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x4444444444444444, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000000020); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x5555555555555555, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000000028); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x6666666666666666, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000000030); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x7777777777777777, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000000038); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x8888888888888888, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000000040); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x9999999999999999, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000000048); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0xaaaaaaaaaaaaaaaa, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000000050); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0xbbbbbbbbbbbbbbbb, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000000058); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0xcccccccccccccccc, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000000060); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0xdddddddddddddddd, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000000068); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0xeeeeeeeeeeeeeeee, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000000070); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0xffffffffffffffff, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000000078); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x0000000000000000, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000000080); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x1234111111111111, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000000088); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x1234222222222222, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000000090); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x1234333333333333, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000000098); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x1234444444444444, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x00000000000000a0); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x1234555555555555, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x00000000000000a8); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x1234666666666666, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x00000000000000b0); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x1234777777777777, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x00000000000000b8); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x1234888888888888, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x00000000000000c0); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x1234999999999999, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x00000000000000c8); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x1234aaaaaaaaaaaa, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x00000000000000d0); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x1234bbbbbbbbbbbb, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x00000000000000d8); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x1234cccccccccccc, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x00000000000000e0); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x1234dddddddddddd, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x00000000000000e8); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x1234eeeeeeeeeeee, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x00000000000000f0); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x1234ffffffffffff, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x00000000000000f8); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0xaaaaaaaaafffffff, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000008000); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x1111111112341111, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000008008); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x2222222212342222, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000008010); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x3333333312343333, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000008018); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x4444444412344444, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000008020); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x5555555512345555, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000008028); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x6666666612346666, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000008030); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x7777777712347777, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000008038); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x8888888812348888, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000008040); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x9999999912349999, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000008048); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0xAAAAAAAA1234AAAA, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000008050); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0xBBBBBBBB1234BBBB, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000008058); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0xCCCCCCCC1234CCCC, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000008060); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0xDDDDDDDD1234DDDD, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000008068); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0xEEEEEEEE1234EEEE, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000008070); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0xFFFFFFFF1234FFFF, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000008078); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0xAAAAAAAAAFFFFFFF, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000008080); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x223456789ABCDEF6, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000008088); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x5555555511111111, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000008090); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0xaaaa111155559999, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x0000000000008098); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x8888bbbb22227777, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x00000000000080a0); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x999999993333dddd, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x00000000000080a8); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x6666666666666666, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x00000000000080b0); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x5555555522222222, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x00000000000080b8); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x8888888888888888, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x00000000000080c0); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x2222221111111199, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x00000000000080c8); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0xAA444444443333AA, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x00000000000080d0); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0xBBBB444444444BBB, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x00000000000080d8); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0xCC666666666666CC, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x00000000000080e0); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0xDDDDD33333D3333D, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x00000000000080e8); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0xEE333333333222EE, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x00000000000080f0); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0xFFFFFFFFFFFFFFFF, 1); 
    flash_ctl_wr(0x008, (uint64_t) 0x00000000000080f8); // fill out the spi_addr for first 8 bytes
    flash_ctl_wr(0x000, (uint64_t) 0x000000080000a0ec); // issue the quad fast read cmd - 0xec
    flash_ctl_rd(0x018, (uint64_t) 0x0555502222220400, 1); 
}

/*
 * prints out the correct usage for this application
 */
void PrintUsage(){
    printf("Syntax: ./flash_ctl [--b <bitfile> | --m <module>]  [optional args]\n");
    printf("    --b               <bitfile>     AC-510, AC-511 only, Loads the FPGA with the bitfile containing the flash_ctl firmware\n");
    printf("    --device_id       <device id>   /dev/pico*, program specific pico device \n");
    printf("    --m               <module>      Micron module to use for this run (exclusive with --b option)\n");
    printf("                                    Supported modules: 0x510, 0x511, 0x851, 0x852, 0xEA5\n");
    printf("    --readid                        Reads the Flash ID register \n");
    printf("    --write_read_test               64K Sector Erase, program, read back and verify two 256-byte pages\n");
    printf("    --read            <address>     Read and print --size bytes from flash beginning at address, rounded to 256-byte boundary\n");
    printf("    --size            <bytes>       Number of bytes used for the --read command, default 256-bytes\n");
    printf("    --erase                         Erase the entire flash device\n");
    printf("    --erase_half        0 | 1       Erase the low half(0) or high half(1) of the flash device\n");
    printf("    --program         <binfile>     Program .bin file into the flash device starting at --address. Flash must be previously erased\n");
    printf("    --verify          <binfile>     Verify the flash contents starting at --address against .bin file \n");
    printf("    --address         <address>     Starting Address in flash used for --program, --verify and --config, default 0\n");
    printf("    --select_flash      0 | 1       SB-852 only, Set the SB-852 to use flash device 0 or 1\n");
    printf("    --config                        Issue IPROG command to cause the FPGA to reconfigure from flash starting at --address.  \n");
    printf("    --cpldregs                      SB-852 only, Read and print all of the SB-852 CPLD registers\n");
    printf("\n");
}

/*
 * Parse the command-line arguments
 */
int ParseArgs(int argc, char* argv[], char** bitfile, uint32_t *device_id, uint32_t* model, uint32_t* readid, uint32_t* write_read_test, 
              uint32_t* read, uint32_t* address, uint32_t* size, uint32_t* erase, uint32_t* erase_half, char** progbitfile, char** verifybitfile, uint32_t* verifyaddress, 
              uint32_t* select_flash, uint32_t* config, uint32_t* cpldregs) {

    // for parsing the command-line options
    int         c;
    int         option_index;

    // flags for the command-line options
    int         readid_flag          = 0;
    int         write_read_test_flag = 0;
    int         erase_flag           = 0;
    int         config_flag          = 0;
    int         cpldregs_flag        = 0;

    // these are the command-line options
    static struct option long_options[] = {
        // these options set a flag
        {"readid",          no_argument,       &readid_flag,            1},
        {"write_read_test", no_argument,       &write_read_test_flag,   1},
        {"erase",           no_argument,       &erase_flag,             1},
        {"config",          no_argument,       &config_flag,            1},
        {"cpldregs",        no_argument,       &cpldregs_flag,          1},

        // these options are distinguished by their indices
        {"b",               required_argument,  0,         'b'},
        {"m",               required_argument,  0,         'm'},
        {"read",            required_argument,  0,         'r'},
        {"size",            required_argument,  0,         's'},
        {"program",         required_argument,  0,         'p'},
        {"verify",          required_argument,  0,         'v'},
        {"address",         required_argument,  0,         'a'},
        {"select_flash",    required_argument,  0,         'f'},
        {"erase_half",      required_argument,  0,         'e'},
        {"device_id",       required_argument,  0,         'd'},
        {0,                 0,                  0,          0}
    };

    // parse the command-line arguments
    while ((c = getopt_long(argc, argv, "", long_options, &option_index)) != -1) {
        switch (c) {
           // these options set a flag - no further processing required
            case 0:
                break;
            // these options have arguments
            case 'b':
                *bitfile    = optarg;
                if (*model != 0){
                    fprintf(stderr, "Cannot specify both a bitfile and a model; i.e. --b argument is exclusive with --m argument.\n");
                    return -1;
                }
                break;
            case 'm':
                *model        = strtoul(optarg, NULL, 0);
                if (*bitfile != NULL){
                    fprintf(stderr, "Cannot specify both a bitfile and a model; i.e. --b argument is exclusive with --m argument.\n");
                    return -1;
                }
                break;
            case 'd':
                *device_id      = strtoul(optarg, NULL, 0);
                break;
            case 'r':
                *read         = 1;
                *address      = strtoul(optarg, NULL, 0);
                if (*address != (*address & 0xFFFFFF00)) { 
                    *address  = (*address & 0xFFFFFF00);    // round to 256-byte address
                    printf("Using address = %08xh\n", *address);
                }
                break;
            case 's':
                *size      = strtoul(optarg, NULL, 0);
                if (*size != (*size & 0xFFFFFF00)) { 
                    *size  = (*size & 0xFFFFFF00);    // round to multiple of 256-bytes 
                    if (*size != 0) {
                        printf("Using size    = %xh\n", *size);
                    }
                }
                if (*size == 0) { 
                    *size  = 0x100;    // use 256-bytes min
                    printf("Using size = %8xh\n", *size);
                }
                break;
            case 'p':
                *progbitfile  = optarg;
                break;
            case 'v':
                *verifybitfile  = optarg;
                break;
            case 'a':
                *verifyaddress  = strtoul(optarg, NULL, 0);
                break;
            case 'f':
                *select_flash = strtoul(optarg, NULL, 0);
                if ((*select_flash != 0) && (*select_flash != 1)) {
                    printf("Invalid value for --select_flash, must be 0 or 1 \n");
                }
                break;
            case 'e':
                *erase_half = strtoul(optarg, NULL, 0);
                if ((*erase_half != 0) && (*erase_half != 1)) {
                    printf("Invalid value for --erase_half, must be 0 or 1 \n");
                }
                break;
        }
    }

    // check command-line arguments

    // set the options for the flags that are set 
    if (readid_flag)          *readid          = 1;
    if (write_read_test_flag) *write_read_test = 1;
    if (erase_flag)           *erase           = 1;
    if (config_flag)          *config          = 1;
    if (cpldregs_flag)        *cpldregs        = 1;

    return 0;
}

int main(int argc, char* argv[])
{
    int         err;
    char        ibuf[1024];

    char*       bitFileName     = NULL;
    uint32_t    device_id         = 0xFF ;
    uint32_t    model           = 0;
    uint32_t    readid          = 0;
    uint32_t    write_read_test = 0;
    uint32_t    read            = 0;
    uint32_t    address         = 0;
    uint32_t    size            = 0x100;  // size in bytes for the read command
    uint32_t    erase           = 0;
    uint32_t    erase_half      = 0xFF;  // 0-erase low half of flash device, 1-erase high half
    char*       progbitfile     = NULL;
    char*       verifybitfile   = NULL;
    uint32_t    verifyaddress   = 0;
    uint32_t    select_flash    = 0xFF;  // 0-select flash device 0, 1-select flash device 1
    uint32_t    config          = 0;  
    uint32_t    cpldregs        = 0;  
    uint32_t    cpld_reg_addr[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x10, 0x12, 0x13, 0x14, 0x15,
                                   0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1e, 0x20, 0x21, 0x22, 0x30, 0x31, 0x32, 0x33, 0x34,
                                   0x35, 0x36, 0x37, 0x38, 0x39, 0x3C, 0x3D, 0x3E, 0x3F, 0x3A, 0x3B, 0x3C, 0x40, 0x41, 0x41, 0x48, 0x49,
                                   0x4A, 0x4B, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x70, 0x71, 0x72, 0x73, 0x75, 0x76, 0xA0,
                                   0xA1, 0xA8, 0xA9, 0xB0 };
    uint32_t    tmp             = 0;
    uint64_t    data64;

    size_t	    bufsize = 8 * sizeof(uint32_t);  // 32-byte read and write buffers for stream commands to flash controller

    ////////////////////////////////
    // PARSE COMMAND-LINE OPTIONS //
    ////////////////////////////////
    if ((err = ParseArgs(argc, argv, &bitFileName, &device_id, &model, &readid, &write_read_test, &read, &address, &size, &erase, &erase_half, &progbitfile, 
                         &verifybitfile, &verifyaddress, &select_flash, &config, &cpldregs)) < 0){
        PrintUsage();
        return EXIT_FAILURE;
    }
    
    // The RunBitFile function will locate a Pico card that can run the given bit file, and is not already
    // opened in exclusive-access mode by another program. It requests exclusive access to the Pico card
    // so no other programs will try to reuse the card and interfere with us.
    if (bitFileName != NULL) {


        if (device_id != 0xFF) {
    
            int cardNum = device_id ;
            printf("Loading FPGA %i with '%s' ...\n", cardNum, bitFileName);
            pico = new PICODRV(cardNum);
            err = pico->LoadFPGA(bitFileName);
            if (err < 0) {
                fprintf(stderr, "LoadFPGA error: %s\n", PicoErrors_FullError(err, ibuf, sizeof(ibuf)));
                exit(1);
            }
        }
        else {

            printf("Loading FPGA with '%s' ...\n", bitFileName);
            err = RunBitFile(bitFileName, &pico);
            if (err < 0) {
                // We use the PicoErrors_FullError function to decipher error codes from RunBitFile.
                // This is more informative than just printing the numeric code, since it can report the name of a
                //   file that wasn't found, for example.
                fprintf(stderr, "%s: RunBitFile error: %s\n", argv[0], PicoErrors_FullError(err, ibuf, sizeof(ibuf)));
                exit(-1);
            }
        }
        //model = 0x851;  // temporary, uncomment for software sim only since software sim requires --b 
    } else if (model != 0) { 
        printf("Finding an FPGA matching model = 0x%0X\n", model);
        if ((err = FindPico(model, &pico)) < 0) {
            printf("Could not find an FPGA matching 0x%0X \n", model);
            exit(-1);
        }
    } else if (device_id != 0xFF) {
        int cardNum = device_id ;
        pico = new PICODRV(cardNum);

    } else {
        PrintUsage();  
        exit(-1);
    }

    // Data goes out to the Flash Controller on WriteStream ID 115 and comes back to the host on ReadStream ID 115
    // Function call to CreateStream opens stream ID 115
    printf("Opening streams to and from the Flash Controller\n");
    streamhandle = pico->CreateStream(115);   // FLASH_CTL_STREAM_ID = 115
    if (streamhandle < 0) {
        // All functions in the Pico API return an error code.  If that code is < 0, then you should use
        // the PicoErrors_FullError function to decode the error message.
        fprintf(stderr, "%s: CreateStream error: %s\n", argv[0], PicoErrors_FullError(streamhandle, ibuf, sizeof(ibuf)));
        exit(-1);
    }

    // Pico streams come in two flavors: 4Byte wide, 16Byte wide (equivalent to 32bit, 128bit respectively)
    // However, all calls to ReadStream and WriteStream must be 16Byte aligned (even for 4B wide streams)
    // Allocate 16B aligned space for read and write stream buffers
    // Similarily, the size of the call, in bytes, must be a multiple of 16Bytes. We will always use 16 or 32 bytes

    if (malloc) {
       err = posix_memalign((void**)&writebuf, 16, bufsize);
       if (writebuf == NULL || err) {
          fprintf(stderr, "%s: posix_memalign could not allocate array of %d bytes for write buffer\n", argv[0], (int) bufsize);
          exit(-1);
       }
       err = posix_memalign((void**)&readbuf, 16, bufsize);
       if (readbuf == NULL || err) {
          fprintf(stderr, "%s: posix_memalign could not allocate array of %d bytes for read buffer\n", argv[0], (int) bufsize);
          exit(-1);
       }
    } else {
       printf("%s: malloc failed\n", argv[0]);
       exit(-1);
    }

    // Assert PicoRst which goes to the mgmt_qspi_flash_ctl module
    // We don't really need this.  s_rst also resets the mgmt_qspi_flash_ctl module
    // and the flash chip itself does not have a reset pin on the AC-510 and the 
    // reset pin on the SB-852 comes from the CPLD.
    // PicoRst stops the clock to the flash
    soft_reset();

    printf("\n Beginning Flash Operations\n");

    init_flash();

    if (readid) {
        read_id_reg(); 
    }

    if (write_read_test) { 
        quad_256byte_write_read_test();                
    } 

    if (erase) {
        erase_flash();
    } 

    if ((erase_half == 0) || (erase_half == 1)) {     
        erase_half_flash(erase_half);
    } 

    if (progbitfile != NULL) {
        program_flash(progbitfile, verifyaddress, model);
    }

    if (verifybitfile != NULL) {
        verify_flash(verifybitfile, verifyaddress);
    }

    if (read) {
        read_flash(size, address);
    }
  
    if ((select_flash == 0) || (select_flash == 1)) {     // Do a read-modify-write and don't change any other bits besides bit 3
        tmp = i2c_read(SB852_DEVICE_ADDRESS, SB852_CPLD_FPGA_CONFIG_REG, 1, 1);
        printf("SB852_CPLD_FPGA_CONFIG_REG = %02X \n", tmp);
        tmp = tmp & 0xF7;                // clear bit 3, the FPGA_IMG_SEL bit
        tmp = tmp | (select_flash<<3);   // set bit 3, the FPGA_IMG_SEL bit to the value entered
        printf("Setting the SB-852 CPLD bit FPGA_IMG_SEL to %d to select flash device %d \n", select_flash, select_flash);
        i2c_write(SB852_DEVICE_ADDRESS, SB852_CPLD_FPGA_CONFIG_REG, 1, tmp, 1);
        tmp = i2c_read(SB852_DEVICE_ADDRESS, SB852_CPLD_FPGA_CONFIG_REG, 1, 1);
        printf("SB852_CPLD_FPGA_CONFIG_REG = %02X \n", tmp);
    }

    if (config) {
        // Reset the flash volatile configuration register, enhanced volatile configuration register
        // and extended address register to the power-on reset default condition so that FPGA
        // configuration from flash works right.
        reset_flash();

        // Assert IPROG or internal PROGRAM_B command through the ICAP3E module
        printf("Issuing IPROG command to reload the FPGA from flash address %08X\n", verifyaddress);

        // ICAP module interface is at 4000h. Writing 4000h will cause the 
        // firmware to issue this sequence:
        // 0xFFFFFFFF   Dummy word
        // 0xAA995566)  Sync word
        // 0x20000000)  Type 1 NOOP
        // 0x30020001)  Type 1 Write 1 word to WBSTAR
        //   address )  Warm boot start address in the flash
        // 0x30008001)  Type 1 Write 1 words to CMD
        // 0x0000000F)  IPROG command 
        // 0x20000000)  Type 1 NOOP

        picobus_wr(0x4000, verifyaddress);  // Send IPROG to trigger reconfiguration from flash at verifyaddress

        //data64 = picobus_rd(0x3000);      // temporary.  Uncomment for software simulation so the sim does not exit before the write is done.

        // exit here because FPGA is going to go dead
        exit(0);


/*      // Optional config from flash methods for SB-851 and SB-852
        // The iPROG command works for all Xilinx FPGAs so that is the primary method and this 
        // code is just left for reference.
        if (model == 0x851) {   // Assert PROGRAM_B through the program_b_register at 3000h in the StreamToFlashCtl module
            printf("Reading SB851 PROGRAM_B register \n");
            data64 = picobus_rd(0x3000); // program_b register is at 3000h
            printf("SB851 PROGRAM_B register = %X \n", data64);

            printf("Setting the SB-851 PROGRAM_B register to 0 to reload the FPGA from flash \n");
            picobus_wr(0x3000, 0x0); // program_b register is at 3000h

            printf("Reading SB851 PROGRAM_B register, it automatically returns high \n");
            data64 = picobus_rd(0x3000); // program_b register is at 3000h
            printf("SB851 PROGRAM_B register = %X \n", data64);
        }

        if (model == 0x852) {  // Assert PROGRAM_B through the CPLD 
            tmp = i2c_read(SB852_DEVICE_ADDRESS, SB852_CPLD_TRIG_RESET_FPGA_PROG_REG, 1, 1);
            printf("SB852_CPLD_TRIG_RESET_FPGA_PROG_REG = %02X \n", tmp);
            tmp = tmp | 0x01;                // set bit 0, the TRIG_RESET_FPGA_PROG bit
            printf("Setting the SB-852 CPLD TRIG_RESET_FPGA_PROG bit to 1 to reload the FPGA from flash \n");
            i2c_write(SB852_DEVICE_ADDRESS, SB852_CPLD_TRIG_RESET_FPGA_PROG_REG, 1, tmp, 1);

            // May need to just exit here since the fpga will go dead, however there is a default
            // 2 second delay between triggering the reconfiguration and it starting so the 
            // the software has time to remove the FPGA from the PCIe enumeration.
            tmp = i2c_read(SB852_DEVICE_ADDRESS, SB852_CPLD_TRIG_RESET_FPGA_PROG_REG, 1, 1);
            printf("SB852_CPLD_TRIG_RESET_FPGA_PROG_REG = %02X \n", tmp);
        }
*/
    }
//void i2c_write(uint32_t dev_addr, uint32_t reg_addr, uint32_t addr_bytes, uint32_t wrdata, uint32_t data_bytes);

         //   printf("SB852 CPLD Register %02X = %02X \n", cpld_reg_addr[j], tmp);
    if (cpldregs) {
            i2c_write(SB852_DEVICE_ADDRESS, cpld_reg_addr[58], 1, 129, 1);
            i2c_write(SB852_DEVICE_ADDRESS, cpld_reg_addr[60], 1, 5, 1);
            i2c_write(0x74, 0x1, 1, 1, 1);//write to switch to page 1
            i2c_write(0x74, 0x11F, 1, 2, 1);//write for si5341 output 4 to use N0 mux
            i2c_write(0x74, 0x124, 1, 2, 1);//write for si5341 output 5 to use N0 mux
            //i2c_write(0x74, 0x11F, 1, 1, 1);//write for si5341 output 4 to use N0 mux
            //i2c_write(0x74, 0x124, 1, 1, 1);//write for si5341 output 5 to use N0 mux
	    printf("written to cpld reg \n");
        for (int j=0; j<2048; j++) {
           // tmp = i2c_read(SB852_DEVICE_ADDRESS, cpld_reg_addr[j], 1, 1);
           // printf("SB852 CPLD Register %02X = %02X \n", cpld_reg_addr[j], tmp);
            tmp = i2c_read(0x74, j,1, 1);

            printf("SI5345 Register %02X = %02X \n", j, tmp);	
        }
    }

    // Reset the flash volatile configuration register, enhanced volatile configuration register
    // and extended address register to the power-on reset default condition.
    reset_flash();

    // streams are automatically closed when the PicoDrv object is destroyed, or on program termination, but
    //  we can also close a stream manually.
    pico->CloseStream(streamhandle);

    printf("All tests successful!\n");

    exit(0);
}

