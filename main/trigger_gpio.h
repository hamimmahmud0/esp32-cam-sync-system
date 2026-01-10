#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

bool trigger_gpio_init(SemaphoreHandle_t slave_sem);
void trigger_master_pulse_us(uint32_t us);
