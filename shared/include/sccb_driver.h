#ifndef SCCB_DRIVER_H
#define SCCB_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// SCCB Addresses
#define OV2640_ADDR_WRITE  0x60
#define OV2640_ADDR_READ   0x61
#define REG_BANK_SELECT    0xFF

// Function prototypes
esp_err_t sccb_init(void);
bool sccb_write_byte(uint8_t reg_addr, uint8_t value);
bool sccb_read_byte(uint8_t reg_addr, uint8_t *value);
bool sccb_write_burst(uint8_t start_reg, uint8_t *data, size_t len);
bool sccb_read_burst(uint8_t start_reg, uint8_t *buffer, size_t len);
bool sccb_test_connection(void);

#endif // SCCB_DRIVER_H