#include "trigger_gpio.h"
#include "app_config.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"

#if CONFIG_ROLE_SLAVE
static SemaphoreHandle_t g_slave_sem = NULL;

static void IRAM_ATTR trig_isr(void *arg) {
  (void)arg;
  if (g_slave_sem) {
    BaseType_t hp = pdFALSE;
    xSemaphoreGiveFromISR(g_slave_sem, &hp);
    if (hp) portYIELD_FROM_ISR();
  }
}
#endif

bool trigger_gpio_init(SemaphoreHandle_t slave_sem) {
#if CONFIG_ROLE_MASTER
  gpio_config_t io = {
    .pin_bit_mask = 1ULL << TRIGGER_GPIO,
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = 0,
    .pull_down_en = 0,
    .intr_type = GPIO_INTR_DISABLE
  };
  gpio_config(&io);
  gpio_set_level(TRIGGER_GPIO, 0);
  return true;
#else
  g_slave_sem = slave_sem;
  gpio_config_t io = {
    .pin_bit_mask = 1ULL << TRIGGER_GPIO,
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = 0,
    .pull_down_en = 1,
    .intr_type = GPIO_INTR_POSEDGE
  };
  gpio_config(&io);
  gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
  gpio_isr_handler_add(TRIGGER_GPIO, trig_isr, NULL);
  return true;
#endif
}

void trigger_master_pulse_us(uint32_t us) {
#if CONFIG_ROLE_MASTER
  gpio_set_level(TRIGGER_GPIO, 1);
  esp_rom_delay_us(us);
  gpio_set_level(TRIGGER_GPIO, 0);
#else
  (void)us;
#endif
}
