/*===================================================================================
 File: i2c.cpp.

 This module contains utilities designed to communicate with the I2C port on pico
 boards such as teh SB-852.
 A stream connects to a 64-bit PicoBus that is used to read and write registers in 
 the I2C master which can then read and write registers in a slave I2C device such
 as the CPLD on the SB-852. 

 The software sequence for an I2C access is:
 Write:
 Poll  I2C_CONFIG_REG, bit[31] will be 1 while the I2C cycle is in progress, 0 when done
 Write I2C_WRDATA_REG with WriteData[31:0]  // Dont care for reads
 Write I2C_ADDR_REG   with the I2C Register Address[31:0] being accessed 
 Write I2C_CONFIG_REG[23:0] with {data_byte_count[7:0], adr_byte_count[7:0], I2C_Device_Addr[6:0], R/Wn}

 Read:
 Poll  I2C_CONFIG_REG, bit[31] will be 1 while the I2C cycle is in progress, 0 when done
 Write I2C_ADDR_REG   with the I2C Register Address[31:0] being accessed 
 Write I2C_CONFIG_REG[23:0] with {data_byte_count[7:0], adr_byte_count[7:0], I2C_Device_Addr[6:0], R/Wn}
 Poll  I2C_CONFIG_REG, bit[31] will be 1 while the I2C cycle is in progress, 0 when done
 Read  I2C_RDDATA_REG for the read data, on read cycles only
=====================================================================================*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <getopt.h>
#include <picodrv.h>
#include <pico_errors.h>

#include "i2c.h"

// Globals
extern PicoDrv     *pico;
extern int         streamhandle;
extern uint32_t    *readbuf, *writebuf;
extern int         verbose;


// Routine to poll the I2C controller cycle in progress bit until it is deasserted
void check_i2c_busy() {
    int     err;
    char    ibuf[1024];

    writebuf[0] = 0x8;            // Data length in bytes of stream command
    writebuf[1] = I2C_CONFIG_REG; // Register Address
    writebuf[2] = 0x1;            // Read cmd, bit[64]=1
    writebuf[3] = 0x0;            // unused
    if (verbose) {
        printf("Checking I2C busy");
    } 

    do {
        err = pico->WriteStream(streamhandle, &writebuf[0], 16);
        if (err < 0) {
            fprintf(stderr, "check_i2c_busy: WriteStream error: %s\n", PicoErrors_FullError(err, ibuf, sizeof(ibuf)));
            exit(-1);
        }
        err = pico->ReadStream(streamhandle, readbuf, 16);
        if (err < 0) {
            fprintf(stderr, "check_i2c_busy: ReadStream error: %s\n", PicoErrors_FullError(err, ibuf, sizeof(ibuf)));
            exit(-1);
        }
        if (verbose) {
            printf("Register %04X  =  %08X\n", I2C_CONFIG_REG, readbuf[0]);
        }
    } while ((readbuf[0] & 0x80000000) == 0x80000000); // loop until I2C cycle in progress bit is low
}

// I2C write cycle.  Device Address is 7-bits.  Register Address is 0 to 4 bytes.  Write data is 1 to 4 bytes. 
void i2c_write(uint32_t dev_addr, uint32_t reg_addr, uint32_t addr_bytes, uint32_t wrdata, uint32_t data_bytes) {
    int err;
    char        ibuf[1024];

    check_i2c_busy();

    // Write the I2C wrdata to the I2C_WRDATA_REG of the I2C controller through the stream interface
    writebuf[0] = 0x8;                                // Data length in bytes of PicoBus cycle 
    writebuf[1] = I2C_WRDATA_REG;                     // Picobus Address
    writebuf[2] = 0x0;                                // Write cmd, bit[64] = 0
    writebuf[3] = 0x0;                                // unused upper portion of stream command word
    writebuf[4] = wrdata;                             // lower 32-bits of the PicoBus 64-bit data contains the I2C write data
    writebuf[5] = 0;                                  // high 32-bits of the PicoBus 64-bit data, unused
    writebuf[6] = 0x0;                                // unused upper portion of stream data word
    writebuf[7] = 0x0;                                // unused upper portion of stream data word
    if (verbose) printf("Writing %08X to %08X \n", writebuf[1], writebuf[4]);
    err = pico->WriteStream(streamhandle, &writebuf[0], 32);  // Stream write of 32 bytes, two 128-bit words
    if (err < 0) {
        fprintf(stderr, "i2c_write: WriteStream error: %s\n", PicoErrors_FullError(err, ibuf, sizeof(ibuf)));
        exit(-1);
    }

    // Write the I2C register address to the I2C_ADDR_REG of the I2C controller through the stream interface
    writebuf[0] = 0x8;                                // Data length in bytes of PicoBus cycle 
    writebuf[1] = I2C_ADDR_REG;                       // Picobus Address
    writebuf[2] = 0x0;                                // Write cmd, bit[64] = 0
    writebuf[3] = 0x0;                                // unused upper portion of stream command word
    writebuf[4] = reg_addr;                           // lower 32-bits of the PicoBus 64-bit data contains the I2C register address
    writebuf[5] = 0;                                  // high 32-bits of the PicoBus 64-bit data, unused
    writebuf[6] = 0x0;                                // unused upper portion of stream data word
    writebuf[7] = 0x0;                                // unused upper portion of stream data word
    if (verbose) printf("Writing %08X to %08X \n", writebuf[1], writebuf[4]);
    err = pico->WriteStream(streamhandle, &writebuf[0], 32);  // Stream write of 32 bytes, two 128-bit words
    if (err < 0) {
        fprintf(stderr, "i2c_write: WriteStream error: %s\n", PicoErrors_FullError(err, ibuf, sizeof(ibuf)));
        exit(-1);
    }

    // Write the I2C device address, num address bytes and num data bytes to the I2C_CONFIG_REG of the I2C controller through the stream interface
    writebuf[0] = 0x8;                                // Data length in bytes of PicoBus cycle 
    writebuf[1] = I2C_CONFIG_REG;                     // Picobus Address
    writebuf[2] = 0x0;                                // Write cmd, bit[64] = 0
    writebuf[3] = 0x0;                                // unused upper portion of stream command word
    // lower 32-bits of the PicoBus 64-bit data contains {data_byte_count[7:0], adr_byte_count[7:0], I2C_Device_Addr[6:0], R/Wn}
    writebuf[4] = ((data_bytes & 0xFF)<<16) | ((addr_bytes & 0xFF)<<8) | ((dev_addr & 0xFF)<<1) | 0x00;    // R/Wn=0
    writebuf[5] = 0;                                  // high 32-bits of the PicoBus 64-bit data, unused
    writebuf[6] = 0x0;                                // unused upper portion of stream data word
    writebuf[7] = 0x0;                                // unused upper portion of stream data word
    if (verbose) printf("Writing %08X to %08X \n", writebuf[1], writebuf[4]);
    err = pico->WriteStream(streamhandle, &writebuf[0], 32);  // Stream write of 32 bytes, two 128-bit words
    if (err < 0) {
        fprintf(stderr, "i2c_write: WriteStream error: %s\n", PicoErrors_FullError(err, ibuf, sizeof(ibuf)));
        exit(-1);
    }
}

// I2C read cycle.  Device Address is 7-bits.  Register Address is 0 to 4 bytes.  Write data is 1 to 4 bytes. 
uint32_t i2c_read(uint32_t dev_addr, uint32_t reg_addr, uint32_t addr_bytes, uint32_t data_bytes) {
    int err;
    char        ibuf[1024];

    check_i2c_busy();

    // Write the I2C register address to the I2C_ADDR_REG of the I2C controller through the stream interface
    writebuf[0] = 0x8;                                // Data length in bytes of PicoBus cycle 
    writebuf[1] = I2C_ADDR_REG;                       // Picobus Address
    writebuf[2] = 0x0;                                // Write cmd, bit[64] = 0
    writebuf[3] = 0x0;                                // unused upper portion of stream command word
    writebuf[4] = reg_addr;                           // lower 32-bits of the PicoBus 64-bit data contains the I2C register address
    writebuf[5] = 0;                                  // high 32-bits of the PicoBus 64-bit data, unused
    writebuf[6] = 0x0;                                // unused upper portion of stream data word
    writebuf[7] = 0x0;                                // unused upper portion of stream data word
    if (verbose) printf("Writing %08X to %08X \n", writebuf[1], writebuf[4]);
    err = pico->WriteStream(streamhandle, &writebuf[0], 32);  // Stream write of 32 bytes, two 128-bit words
    if (err < 0) {
        fprintf(stderr, "i2c_read: WriteStream error: %s\n", PicoErrors_FullError(err, ibuf, sizeof(ibuf)));
        exit(-1);
    }

    // Write the I2C device address, num address bytes and num data bytes to the I2C_CONFIG_REG of the I2C controller through the stream interface
    writebuf[0] = 0x8;                                // Data length in bytes of PicoBus cycle 
    writebuf[1] = I2C_CONFIG_REG;                     // Picobus Address
    writebuf[2] = 0x0;                                // Write cmd, bit[64] = 0
    writebuf[3] = 0x0;                                // unused upper portion of stream command word
    // lower 32-bits of the PicoBus 64-bit data contains {data_byte_count[7:0], adr_byte_count[7:0], I2C_Device_Addr[6:0], R/Wn}
    writebuf[4] = ((data_bytes & 0xFF)<<16) | ((addr_bytes & 0xFF)<<8) | ((dev_addr & 0xFF)<<1) | 0x01;    // R/Wn=1
    writebuf[5] = 0;                                  // high 32-bits of the PicoBus 64-bit data, unused
    writebuf[6] = 0x0;                                // unused upper portion of stream data word
    writebuf[7] = 0x0;                                // unused upper portion of stream data word
    if (verbose) printf("Writing %08X to %08X \n", writebuf[1], writebuf[4]);
    err = pico->WriteStream(streamhandle, &writebuf[0], 32);  // Stream write of 32 bytes, two 128-bit words
    if (err < 0) {
        fprintf(stderr, "i2c_read: WriteStream error: %s\n", PicoErrors_FullError(err, ibuf, sizeof(ibuf)));
        exit(-1);
    }

    check_i2c_busy();

    // Send I2C controller read data command to the stream interface
    writebuf[0] = 0x8;                                // Data length in bytes of stream command
    writebuf[1] = I2C_RDDATA_REG;                     // Picobus Address
    writebuf[2] = 0x1;                                // Read cmd, bit[64] = 1
    writebuf[3] = 0x0;                                // unused upper portion of stream command word
    err = pico->WriteStream(streamhandle, &writebuf[0], 16);
    if (err < 0) {
        fprintf(stderr, "i2c_read: WriteStream error: %s\n", PicoErrors_FullError(err, ibuf, sizeof(ibuf)));
        exit(-1);
    }

    // clear the read buffer to prepare for the read.
    memset(readbuf, 0, sizeof(readbuf));

    // Read the response data from the stream interface.
    err = pico->ReadStream(streamhandle, readbuf, 16);
    if (err < 0) {
        fprintf(stderr, "flash_ctl_rd: ReadStream error: %s\n", PicoErrors_FullError(err, ibuf, sizeof(ibuf)));
        exit(-1);
    }

    return (readbuf[0]);
}

