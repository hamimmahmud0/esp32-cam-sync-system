#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum { REG_BANK_DSP = 0x00, REG_BANK_SENSOR = 0x01 } ov2640_bank_t;

bool ov2640_set_bank(ov2640_bank_t bank);
bool ov2640_read_reg(ov2640_bank_t bank, uint8_t addr, uint8_t *val);
bool ov2640_write_reg(ov2640_bank_t bank, uint8_t addr, uint8_t val);
bool ov2640_modify_reg(ov2640_bank_t bank, uint8_t addr, uint8_t mask, uint8_t val);
