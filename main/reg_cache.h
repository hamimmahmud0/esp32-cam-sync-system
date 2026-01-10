#pragma once
#include <stdint.h>
#include "ov2640_ctrl.h"

typedef struct {
  uint8_t val[2][256];
  uint8_t dirty[2][256];
} reg_cache_t;

void reg_cache_init(reg_cache_t *c);
void reg_cache_set(reg_cache_t *c, ov2640_bank_t bank, uint8_t addr, uint8_t v);
void reg_cache_mark_clean(reg_cache_t *c, ov2640_bank_t bank, uint8_t addr);
