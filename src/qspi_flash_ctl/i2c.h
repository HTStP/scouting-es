/*
 * File Name    : i2c.h
 *
 * Description  : i2c task headers
 *
 * Copyright    : 2018, Micron, Inc.
 */


// PicoBus Addresses of I2C controller registers
#define I2C_BASE         0x2000

#define I2C_CONFIG_REG   (I2C_BASE + 0x00)
#define I2C_ADDR_REG     (I2C_BASE + 0x08)
#define I2C_WRDATA_REG   (I2C_BASE + 0x10)
#define I2C_RDDATA_REG   (I2C_BASE + 0x18)

// SB-852 CPLD I2C device address
#define SB852_DEVICE_ADDRESS 0x5F

// SB-852 I2C register addresses inside the CPLD
#define SB852_CPLD_FPGA_CONFIG_REG  0x12          // Bits: 7-NO_CFG_TIMEOUT, 6-FORCE_PCIE_RESET, 5-RESET_FPGA_CFG_FLASH, 4-FPGA_IMG_LOADED, 3-FPGA_IMG_SEL, 2:0-FPGA_CFG_MODE
#define SB852_CPLD_TRIG_RESET_FPGA_PROG_REG 0xB0  // Bits: 7:4-reserved, 3:1-TRIG_RESET_FPGA_PROG_SEC, 0-TRIG_RESET_FPGA_PROG


#define SI5345_REG 0x5D
// I2C software functions
void check_i2c_busy();

void i2c_write(uint32_t dev_addr, uint32_t reg_addr, uint32_t addr_bytes, uint32_t wrdata, uint32_t data_bytes);

uint32_t i2c_read(uint32_t dev_addr, uint32_t reg_addr, uint32_t addr_bytes, uint32_t data_bytes);

