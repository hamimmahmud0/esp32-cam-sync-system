#include "cam_manager.h"
#include "app_state.h"
#include "app_config.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "CAM";

static cam_profile_t g_stream = {
  .framesize = STREAM_DEFAULT_FRAMESIZE,
  .pixformat = PIXFORMAT_JPEG,
  .jpeg_quality = STREAM_DEFAULT_JPEG_QUALITY,
  .fb_count = STREAM_DEFAULT_FB_COUNT
};

static cam_profile_t g_capture = {
  .framesize = CAPTURE_DEFAULT_FRAMESIZE,
  .pixformat = CAPTURE_DEFAULT_PIXFORMAT,
  .jpeg_quality = CAPTURE_DEFAULT_JPEG_QUALITY,
  .fb_count = 1
};

static camera_config_t make_ai_thinker_cfg(const cam_profile_t *p) {
  camera_config_t c = {
    .pin_pwdn  = 32,
    .pin_reset = -1,
    .pin_xclk = 0,
    .pin_sscb_sda = 26,
    .pin_sscb_scl = 27,

    .pin_d7 = 35,
    .pin_d6 = 34,
    .pin_d5 = 39,
    .pin_d4 = 36,
    .pin_d3 = 21,
    .pin_d2 = 19,
    .pin_d1 = 18,
    .pin_d0 = 5,
    .pin_vsync = 25,
    .pin_href = 23,
    .pin_pclk = 22,

    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = p->pixformat,
    .frame_size = p->framesize,
    .jpeg_quality = p->jpeg_quality,
    .fb_count = p->fb_count,
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_LATEST
  };

  if (p->pixformat != PIXFORMAT_JPEG)
  {
    c.fb_count = 1;
    c.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    c.jpeg_quality = 63; // unused, but keep valid
  }
  else
  {
    c.fb_count = p->fb_count;
    c.grab_mode = CAMERA_GRAB_LATEST;
  }

  return c;
}

static bool cam_deinit_locked(void) {
  esp_camera_deinit();
  g_app.mode = CAM_MODE_NONE;
  return true;
}

static bool cam_init_locked(const cam_profile_t *p, cam_mode_t mode) {
  camera_config_t cfg = make_ai_thinker_cfg(p);
  esp_err_t err = esp_camera_init(&cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_camera_init failed: %s", esp_err_to_name(err));
    return false;
  }
  g_app.mode = mode;
  return true;
}

bool cam_manager_init(void) {
  xSemaphoreTake(g_app.cam_mutex, portMAX_DELAY);
  bool ok = cam_init_locked(&g_stream, CAM_MODE_STREAM);
  xSemaphoreGive(g_app.cam_mutex);
  g_app.stream_enabled = ok;
  return ok;
}

bool cam_manager_set_stream_profile(const cam_profile_t *p) { g_stream = *p; return true; }
bool cam_manager_set_capture_profile(const cam_profile_t *p) { g_capture = *p; return true; }

bool cam_manager_start_stream(void) {
  xSemaphoreTake(g_app.cam_mutex, portMAX_DELAY);
  if (g_app.mode != CAM_MODE_STREAM) {
    cam_deinit_locked();
    if (!cam_init_locked(&g_stream, CAM_MODE_STREAM)) {
      xSemaphoreGive(g_app.cam_mutex);
      return false;
    }
  }
  g_app.stream_enabled = true;
  xSemaphoreGive(g_app.cam_mutex);
  return true;
}

bool cam_manager_stop_stream(void) {
  g_app.stream_enabled = false;
  return true;
}

bool cam_manager_capture_to_file(const char *filepath, char *meta_json_out, int meta_max) {
  xSemaphoreTake(g_app.cam_mutex, portMAX_DELAY);

  g_app.stream_enabled = false;

  cam_deinit_locked();
  if (!cam_init_locked(&g_capture, CAM_MODE_CAPTURE)) {
    xSemaphoreGive(g_app.cam_mutex);
    return false;
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    ESP_LOGE(TAG, "fb_get failed");
    cam_deinit_locked();
    cam_init_locked(&g_stream, CAM_MODE_STREAM);
    g_app.stream_enabled = true;
    xSemaphoreGive(g_app.cam_mutex);
    return false;
  }

  FILE *f = fopen(filepath, "wb");
  if (!f) {
    ESP_LOGE(TAG, "open file failed: %s", filepath);
    esp_camera_fb_return(fb);
    cam_deinit_locked();
    cam_init_locked(&g_stream, CAM_MODE_STREAM);
    g_app.stream_enabled = true;
    xSemaphoreGive(g_app.cam_mutex);
    return false;
  }

  fwrite(fb->buf, 1, fb->len, f);
  fclose(f);

  if (meta_json_out && meta_max > 0) {
    snprintf(meta_json_out, meta_max,
      "{\"len\":%u,\"w\":%u,\"h\":%u,\"format\":%d}",
      (unsigned)fb->len, fb->width, fb->height, fb->format
    );
  }

  esp_camera_fb_return(fb);

  cam_deinit_locked();
  bool ok = cam_init_locked(&g_stream, CAM_MODE_STREAM);
  g_app.stream_enabled = ok;

  xSemaphoreGive(g_app.cam_mutex);
  return ok;
}
