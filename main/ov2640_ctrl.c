#include "ov2640_ctrl.h"
#include "app_state.h"
#include "esp_camera.h"

#define REG_BANK_SELECT 0xFF

static uint8_t g_bank = 0xFF;

static inline sensor_t* cam_sensor(void) {
  return esp_camera_sensor_get();
}

bool ov2640_set_bank(ov2640_bank_t bank) {
  if (g_bank == (uint8_t)bank) return true;
  sensor_t *s = cam_sensor();
  if (!s) return false;

  xSemaphoreTake(g_app.sccb_mutex, portMAX_DELAY);
  // bank is selected by writing 0xFF (mask=0xFF)
  int r = s->set_reg(s, 0 /*bank ignored for set_reg*/, REG_BANK_SELECT, 0xFF, (uint8_t)bank);
  xSemaphoreGive(g_app.sccb_mutex);

  if (r != 0) return false;
  g_bank = (uint8_t)bank;
  return true;
}

bool ov2640_read_reg(ov2640_bank_t bank, uint8_t addr, uint8_t *val) {
  if (!val) return false;
  if (!ov2640_set_bank(bank)) return false;

  sensor_t *s = cam_sensor();
  if (!s) return false;

  xSemaphoreTake(g_app.sccb_mutex, portMAX_DELAY);
  int r = s->get_reg(s, 0, addr);
  xSemaphoreGive(g_app.sccb_mutex);

  if (r < 0) return false;
  *val = (uint8_t)r;
  return true;
}

bool ov2640_write_reg(ov2640_bank_t bank, uint8_t addr, uint8_t value) {
  if (!ov2640_set_bank(bank)) return false;

  sensor_t *s = cam_sensor();
  if (!s) return false;

  xSemaphoreTake(g_app.sccb_mutex, portMAX_DELAY);
  int r = s->set_reg(s, 0, addr, 0xFF, value);
  xSemaphoreGive(g_app.sccb_mutex);

  return r == 0;
}

bool ov2640_modify_reg(ov2640_bank_t bank, uint8_t addr, uint8_t mask, uint8_t value) {
  if (!ov2640_set_bank(bank)) return false;

  sensor_t *s = cam_sensor();
  if (!s) return false;

  xSemaphoreTake(g_app.sccb_mutex, portMAX_DELAY);
  int r = s->set_reg(s, 0, addr, mask, value);
  xSemaphoreGive(g_app.sccb_mutex);

  return r == 0;
}
