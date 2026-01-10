#pragma once
#include <stdbool.h>
#include "esp_camera.h"

typedef enum {
  CAP_FMT_JPEG,
  CAP_FMT_RGB565,
  CAP_FMT_YUV422,
  CAP_FMT_GRAY,
  CAP_FMT_BAYER8   // experimental
} cap_fmt_t;



typedef struct {
  framesize_t framesize;
  pixformat_t pixformat;
  int jpeg_quality;
  int fb_count;
} cam_profile_t;

bool cam_manager_init(void);
bool cam_manager_set_stream_profile(const cam_profile_t *p);
bool cam_manager_set_capture_profile(const cam_profile_t *p);

bool cam_manager_start_stream(void);
bool cam_manager_stop_stream(void);

bool cam_manager_capture_to_file(const char *filepath, char *meta_json_out, int meta_max);
bool ov2640_enable_bayer_raw8(bool enable, int pattern /*0=RGGB,1=BGGR,2=GRBG,3=GBRG*/);
