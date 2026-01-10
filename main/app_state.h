#pragma once
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef enum {
  CAM_MODE_NONE = 0,
  CAM_MODE_STREAM,
  CAM_MODE_CAPTURE
} cam_mode_t;

typedef struct {
  cam_mode_t mode;
  SemaphoreHandle_t cam_mutex;     // guards camera init/deinit + fb_get
  SemaphoreHandle_t sccb_mutex;    // guards SCCB access
  volatile bool stream_enabled;
} app_state_t;

extern app_state_t g_app;
