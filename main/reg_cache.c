#include "reg_cache.h"
#include <string.h>

void reg_cache_init(reg_cache_t *c) { memset(c, 0, sizeof(*c)); }
void reg_cache_set(reg_cache_t *c, ov2640_bank_t bank, uint8_t addr, uint8_t v) {
  c->val[bank][addr] = v;
  c->dirty[bank][addr] = 1;
}
void reg_cache_mark_clean(reg_cache_t *c, ov2640_bank_t bank, uint8_t addr) { c->dirty[bank][addr] = 0; }
