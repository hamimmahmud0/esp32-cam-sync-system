#include "web_server.h"
#include "app_config.h"
#include "app_state.h"
#include "cam_manager.h"
#include "trigger_gpio.h"
#include "ov2640_ctrl.h"
#include "slave_client.h"
#include "reg_profiles.h"

#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static const char *TAG="WEB";
static httpd_handle_t g_http = NULL;

#if CONFIG_ROLE_SLAVE
static SemaphoreHandle_t g_arm_sem = NULL;
static char g_armed_id[64] = {0};
static char g_armed_pf[16] = {0};
static char g_armed_fs[16] = {0};
static char g_armed_ext[8] = {0};
static volatile bool g_is_armed = false;
#endif

static esp_err_t send_file(httpd_req_t *req, const char *path, const char *ctype) {
  FILE *f = fopen(path, "rb");
  if (!f) return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "file not found");
  httpd_resp_set_type(req, ctype);

  char buf[1024];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
    if (httpd_resp_send_chunk(req, buf, (ssize_t)n) != ESP_OK) break;
  }
  fclose(f);
  httpd_resp_send_chunk(req, NULL, 0);
  return ESP_OK;
}

static const char* guess_ctype(const char *uri) {
  if (strstr(uri, ".js")) return "application/javascript";
  if (strstr(uri, ".css")) return "text/css";
  if (strstr(uri, ".json")) return "application/json";
  if (strstr(uri, ".html")) return "text/html";
  return "text/plain";
}

static esp_err_t h_root(httpd_req_t *req) {
  char path[256];
  snprintf(path, sizeof(path), "%s/index.html", WWW_DIR);
  return send_file(req, path, "text/html");
}

static esp_err_t h_registers_page(httpd_req_t *req) {
  char path[256];
  snprintf(path, sizeof(path), "%s/registers.html", WWW_DIR);
  return send_file(req, path, "text/html");
}

static esp_err_t h_www_any(httpd_req_t *req) {
  // URI like /www/app.js -> SD path: <WWW_DIR>/app.js
  const char *uri = req->uri;
  if (strncmp(uri, "/www/", 5) != 0) return httpd_resp_send_err(req, 404, "bad");
  char path[256];
  snprintf(path, sizeof(path), "%s/%s", WWW_DIR, uri + 5);
  return send_file(req, path, guess_ctype(uri));
}

static esp_err_t h_stream(httpd_req_t *req) {
  static const char *boundary = "123456789000000000000987654321";
  char hdr[128];

  httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=123456789000000000000987654321");

  while (g_app.stream_enabled) {
    xSemaphoreTake(g_app.cam_mutex, portMAX_DELAY);
    camera_fb_t *fb = esp_camera_fb_get();
    xSemaphoreGive(g_app.cam_mutex);

    if (!fb) break;
    if (fb->format != PIXFORMAT_JPEG) {
      esp_camera_fb_return(fb);
      continue;
    }

    int hlen = snprintf(hdr, sizeof(hdr),
      "\r\n--%s\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
      boundary, (unsigned)fb->len);

    if (httpd_resp_send_chunk(req, hdr, hlen) != ESP_OK) { esp_camera_fb_return(fb); break; }
    if (httpd_resp_send_chunk(req, (const char*)fb->buf, (ssize_t)fb->len) != ESP_OK) { esp_camera_fb_return(fb); break; }

    esp_camera_fb_return(fb);
  }

  httpd_resp_send_chunk(req, NULL, 0);
  return ESP_OK;
}

static void make_capture_paths(const char *id, char *bin_path, int bin_max, char *json_path, int json_max, const char *ext) {
  snprintf(bin_path, bin_max, "%s/%s.%s", CAPTURES_DIR, id, ext);
  snprintf(json_path, json_max, "%s/%s.json", CAPTURES_DIR, id);
}

static void ext_from_pixformat(const char *pf, char *out, int out_max) {
  if (!pf) { snprintf(out, out_max, "jpg"); return; }
  if (!strcmp(pf, "rgb565")) snprintf(out, out_max, "rgb565");
  else if (!strcmp(pf, "yuv422")) snprintf(out, out_max, "yuv");
  else if (!strcmp(pf, "gray")) snprintf(out, out_max, "gray");
  else snprintf(out, out_max, "jpg");
}

static bool parse_hex_u8(const char *s, uint8_t *out) {
  if (!s) return false;
  long v = strtol(s, NULL, 0);
  if (v < 0 || v > 255) return false;
  *out = (uint8_t)v;
  return true;
}

// ------------------ CAPTURE APIs ------------------

static esp_err_t api_capture_local(httpd_req_t *req) {
  char body[256];
  int n = httpd_req_recv(req, body, sizeof(body)-1);
  if (n <= 0) return httpd_resp_send_err(req, 400, "no body");
  body[n]=0;

  cJSON *root = cJSON_Parse(body);
  if (!root) return httpd_resp_send_err(req, 400, "bad json");

  const char *id = cJSON_GetObjectItem(root, "id") ? cJSON_GetObjectItem(root, "id")->valuestring : "cap_local";
  const char *pf = cJSON_GetObjectItem(root, "pixformat") ? cJSON_GetObjectItem(root, "pixformat")->valuestring : "jpeg";
  const char *fs = cJSON_GetObjectItem(root, "framesize") ? cJSON_GetObjectItem(root, "framesize")->valuestring : "uxga";

  cam_profile_t cap = {
    .framesize = (!strcmp(fs,"svga") ? FRAMESIZE_SVGA : (!strcmp(fs,"cif") ? FRAMESIZE_CIF : FRAMESIZE_UXGA)),
    .pixformat = (!strcmp(pf,"rgb565") ? PIXFORMAT_RGB565 :
                 (!strcmp(pf,"yuv422") ? PIXFORMAT_YUV422 :
                 (!strcmp(pf,"gray") ? PIXFORMAT_GRAYSCALE : PIXFORMAT_JPEG))),
    .jpeg_quality = CAPTURE_DEFAULT_JPEG_QUALITY,
    .fb_count = 1
  };
  cam_manager_set_capture_profile(&cap);

  char ext[8]; ext_from_pixformat(pf, ext, sizeof(ext));

  char bin_path[256], json_path[256], meta[128];
  make_capture_paths(id, bin_path, sizeof(bin_path), json_path, sizeof(json_path), ext);

  bool ok = cam_manager_capture_to_file(bin_path, meta, sizeof(meta));
  if (ok) {
    FILE *jf = fopen(json_path, "wb");
    if (jf) { fwrite(meta, 1, strlen(meta), jf); fclose(jf); }
  }

  cJSON_Delete(root);
  if (!ok) return httpd_resp_send_err(req, 500, "capture failed");
  return httpd_resp_sendstr(req, "{\"ok\":true}");
}

#if CONFIG_ROLE_MASTER
static uint32_t next_id_counter(void) {
  static uint32_t c = 0;
  return ++c;
}

static void make_shared_id(char *out, int out_max) {
  snprintf(out, out_max, "cap_%08u", (unsigned)next_id_counter());
}

static esp_err_t api_capture_sync(httpd_req_t *req) {
  char body[256];
  int n = httpd_req_recv(req, body, sizeof(body)-1);
  if (n <= 0) return httpd_resp_send_err(req, 400, "no body");
  body[n]=0;

  cJSON *root = cJSON_Parse(body);
  if (!root) return httpd_resp_send_err(req, 400, "bad json");

  const char *pf = cJSON_GetObjectItem(root, "pixformat") ? cJSON_GetObjectItem(root, "pixformat")->valuestring : "jpeg";
  const char *fs = cJSON_GetObjectItem(root, "framesize") ? cJSON_GetObjectItem(root, "framesize")->valuestring : "uxga";

  char id[64];
  make_shared_id(id, sizeof(id));

  char arm_json[256];
  snprintf(arm_json, sizeof(arm_json),
    "{\"id\":\"%s\",\"pixformat\":\"%s\",\"framesize\":\"%s\"}", id, pf, fs);

  if (!slave_http_post_json("/api/arm", arm_json)) {
    cJSON_Delete(root);
    return httpd_resp_send_err(req, 500, "slave arm failed");
  }

  cam_profile_t cap = {
    .framesize = (!strcmp(fs,"svga") ? FRAMESIZE_SVGA : (!strcmp(fs,"cif") ? FRAMESIZE_CIF : FRAMESIZE_UXGA)),
    .pixformat = (!strcmp(pf,"rgb565") ? PIXFORMAT_RGB565 :
                 (!strcmp(pf,"yuv422") ? PIXFORMAT_YUV422 :
                 (!strcmp(pf,"gray") ? PIXFORMAT_GRAYSCALE : PIXFORMAT_JPEG))),
    .jpeg_quality = CAPTURE_DEFAULT_JPEG_QUALITY,
    .fb_count = 1
  };
  cam_manager_set_capture_profile(&cap);

  // Trigger pulse while both are armed (slave waits on GPIO)
  trigger_master_pulse_us(30);

  char ext[8]; ext_from_pixformat(pf, ext, sizeof(ext));
  char bin_path[256], json_path[256], meta[128];
  make_capture_paths(id, bin_path, sizeof(bin_path), json_path, sizeof(json_path), ext);

  bool ok = cam_manager_capture_to_file(bin_path, meta, sizeof(meta));
  if (ok) {
    FILE *jf = fopen(json_path, "wb");
    if (jf) { fwrite(meta, 1, strlen(meta), jf); fclose(jf); }
  }

  cJSON_Delete(root);
  if (!ok) return httpd_resp_send_err(req, 500, "master capture failed");

  char resp[128];
  snprintf(resp, sizeof(resp), "{\"ok\":true,\"id\":\"%s\"}", id);
  return httpd_resp_sendstr(req, resp);
}
#endif

#if CONFIG_ROLE_SLAVE
static esp_err_t api_arm(httpd_req_t *req) {
  char body[256];
  int n = httpd_req_recv(req, body, sizeof(body)-1);
  if (n <= 0) return httpd_resp_send_err(req, 400, "no body");
  body[n]=0;

  cJSON *root = cJSON_Parse(body);
  if (!root) return httpd_resp_send_err(req, 400, "bad json");

  const char *id = cJSON_GetObjectItem(root, "id")->valuestring;
  const char *pf = cJSON_GetObjectItem(root, "pixformat")->valuestring;
  const char *fs = cJSON_GetObjectItem(root, "framesize")->valuestring;

  strncpy(g_armed_id, id, sizeof(g_armed_id)-1);
  strncpy(g_armed_pf, pf, sizeof(g_armed_pf)-1);
  strncpy(g_armed_fs, fs, sizeof(g_armed_fs)-1);
  ext_from_pixformat(g_armed_pf, g_armed_ext, sizeof(g_armed_ext));

  g_armed_id[sizeof(g_armed_id)-1]=0;
  g_armed_pf[sizeof(g_armed_pf)-1]=0;
  g_armed_fs[sizeof(g_armed_fs)-1]=0;
  g_armed_ext[sizeof(g_armed_ext)-1]=0;

  cam_profile_t cap = {
    .framesize = (!strcmp(fs,"svga") ? FRAMESIZE_SVGA : (!strcmp(fs,"cif") ? FRAMESIZE_CIF : FRAMESIZE_UXGA)),
    .pixformat = (!strcmp(pf,"rgb565") ? PIXFORMAT_RGB565 :
                 (!strcmp(pf,"yuv422") ? PIXFORMAT_YUV422 :
                 (!strcmp(pf,"gray") ? PIXFORMAT_GRAYSCALE : PIXFORMAT_JPEG))),
    .jpeg_quality = CAPTURE_DEFAULT_JPEG_QUALITY,
    .fb_count = 1
  };
  cam_manager_set_capture_profile(&cap);

  g_is_armed = true;
  cJSON_Delete(root);

  return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static void slave_capture_task(void *arg) {
  (void)arg;
  while (1) {
    xSemaphoreTake(g_arm_sem, portMAX_DELAY);
    if (!g_is_armed) continue;

    char bin_path[256], json_path[256], meta[128];
    make_capture_paths(g_armed_id, bin_path, sizeof(bin_path), json_path, sizeof(json_path), g_armed_ext);

    bool ok = cam_manager_capture_to_file(bin_path, meta, sizeof(meta));
    if (ok) {
      FILE *jf = fopen(json_path, "wb");
      if (jf) { fwrite(meta, 1, strlen(meta), jf); fclose(jf); }
    }
    g_is_armed = false;
  }
}
#endif

// ------------------ REGISTER APIs ------------------

static esp_err_t api_reg_single_get(httpd_req_t *req) {
  char q[128], bank_s[8], addr_s[16];
  if (httpd_req_get_url_query_str(req, q, sizeof(q)) != ESP_OK) return httpd_resp_send_err(req, 400, "no query");
  if (httpd_query_key_value(q, "bank", bank_s, sizeof(bank_s)) != ESP_OK) return httpd_resp_send_err(req, 400, "bank missing");
  if (httpd_query_key_value(q, "addr", addr_s, sizeof(addr_s)) != ESP_OK) return httpd_resp_send_err(req, 400, "addr missing");

  int bank = atoi(bank_s);
  int addr = (int)strtol(addr_s, NULL, 0);
  uint8_t v=0;

  if (!ov2640_read_reg((ov2640_bank_t)bank, (uint8_t)addr, &v))
    return httpd_resp_send_err(req, 500, "read failed");

  char out[96];
  snprintf(out, sizeof(out), "{\"bank\":%d,\"addr\":\"%s\",\"value\":%u}", bank, addr_s, (unsigned)v);
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_sendstr(req, out);
}

static esp_err_t api_reg_single_post(httpd_req_t *req) {
  char body[256];
  int n = httpd_req_recv(req, body, sizeof(body)-1);
  if (n<=0) return httpd_resp_send_err(req, 400, "no body");
  body[n]=0;

  cJSON *root = cJSON_Parse(body);
  if (!root) return httpd_resp_send_err(req, 400, "bad json");

  int bank = cJSON_GetObjectItem(root, "bank")->valueint;
  int addr = (int)strtol(cJSON_GetObjectItem(root, "addr")->valuestring, NULL, 0);
  int value = (int)strtol(cJSON_GetObjectItem(root, "value")->valuestring, NULL, 0);

  cJSON *maskI = cJSON_GetObjectItem(root, "mask");
  bool ok;
  if (maskI) {
    int mask = (int)strtol(maskI->valuestring, NULL, 0);
    ok = ov2640_modify_reg((ov2640_bank_t)bank, (uint8_t)addr, (uint8_t)mask, (uint8_t)value);
  } else {
    ok = ov2640_write_reg((ov2640_bank_t)bank, (uint8_t)addr, (uint8_t)value);
  }

  cJSON_Delete(root);
  if (!ok) return httpd_resp_send_err(req, 500, "write failed");
  return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static esp_err_t api_reg_range_get(httpd_req_t *req) {
  char q[128], bank_s[8], start_s[16], end_s[16];
  if (httpd_req_get_url_query_str(req, q, sizeof(q)) != ESP_OK) return httpd_resp_send_err(req, 400, "no query");
  if (httpd_query_key_value(q, "bank", bank_s, sizeof(bank_s)) != ESP_OK) return httpd_resp_send_err(req, 400, "bank missing");
  if (httpd_query_key_value(q, "start", start_s, sizeof(start_s)) != ESP_OK) return httpd_resp_send_err(req, 400, "start missing");
  if (httpd_query_key_value(q, "end", end_s, sizeof(end_s)) != ESP_OK) return httpd_resp_send_err(req, 400, "end missing");

  int bank = atoi(bank_s);
  uint8_t start, end;
  if (!parse_hex_u8(start_s, &start) || !parse_hex_u8(end_s, &end) || end < start)
    return httpd_resp_send_err(req, 400, "bad range");

  cJSON *root = cJSON_CreateObject();
  cJSON_AddNumberToObject(root, "bank", bank);
  cJSON_AddStringToObject(root, "start", start_s);
  cJSON_AddStringToObject(root, "end", end_s);
  cJSON *vals = cJSON_AddArrayToObject(root, "values");

  for (int a = start; a <= end; a++) {
    uint8_t v=0;
    if (!ov2640_read_reg((ov2640_bank_t)bank, (uint8_t)a, &v)) {
      cJSON_Delete(root);
      return httpd_resp_send_err(req, 500, "read failed");
    }
    cJSON_AddItemToArray(vals, cJSON_CreateNumber(v));
  }

  char *out = cJSON_PrintUnformatted(root);
  httpd_resp_set_type(req, "application/json");
  esp_err_t r = httpd_resp_sendstr(req, out);
  free(out);
  cJSON_Delete(root);
  return r;
}

static esp_err_t api_reg_range_post(httpd_req_t *req) {
  char body[1024];
  int n = httpd_req_recv(req, body, sizeof(body)-1);
  if (n<=0) return httpd_resp_send_err(req, 400, "no body");
  body[n]=0;

  cJSON *root = cJSON_Parse(body);
  if (!root) return httpd_resp_send_err(req, 400, "bad json");

  int bank = cJSON_GetObjectItem(root, "bank")->valueint;
  const char *start_s = cJSON_GetObjectItem(root, "start")->valuestring;
  cJSON *values = cJSON_GetObjectItem(root, "values");
  if (!cJSON_IsArray(values)) { cJSON_Delete(root); return httpd_resp_send_err(req, 400, "values must be array"); }

  uint8_t start;
  if (!parse_hex_u8(start_s, &start)) { cJSON_Delete(root); return httpd_resp_send_err(req, 400, "bad start"); }

  int count = cJSON_GetArraySize(values);
  if (count < 1 || count > 256) { cJSON_Delete(root); return httpd_resp_send_err(req, 400, "bad count"); }
  if ((int)start + count > 256) { cJSON_Delete(root); return httpd_resp_send_err(req, 400, "range overflow"); }

  for (int i=0;i<count;i++) {
    cJSON *it = cJSON_GetArrayItem(values, i);
    if (!cJSON_IsNumber(it)) { cJSON_Delete(root); return httpd_resp_send_err(req, 400, "values must be numbers"); }
    if (!ov2640_write_reg((ov2640_bank_t)bank, (uint8_t)(start+i), (uint8_t)it->valueint)) {
      cJSON_Delete(root);
      return httpd_resp_send_err(req, 500, "write failed");
    }
  }

  cJSON_Delete(root);
  return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static esp_err_t api_reg_dump_get(httpd_req_t *req) {
  cJSON *root = cJSON_CreateObject();
  cJSON *dsp = cJSON_AddArrayToObject(root, "dsp");
  cJSON *sen = cJSON_AddArrayToObject(root, "sensor");

  for (int a=0;a<256;a++) {
    uint8_t v=0;
    if (!ov2640_read_reg(REG_BANK_DSP, (uint8_t)a, &v)) { cJSON_Delete(root); return httpd_resp_send_err(req, 500, "dsp read fail"); }
    cJSON_AddItemToArray(dsp, cJSON_CreateNumber(v));
  }
  for (int a=0;a<256;a++) {
    uint8_t v=0;
    if (!ov2640_read_reg(REG_BANK_SENSOR, (uint8_t)a, &v)) { cJSON_Delete(root); return httpd_resp_send_err(req, 500, "sensor read fail"); }
    cJSON_AddItemToArray(sen, cJSON_CreateNumber(v));
  }

  char *out = cJSON_PrintUnformatted(root);
  httpd_resp_set_type(req, "application/json");
  esp_err_t r = httpd_resp_sendstr(req, out);
  free(out);
  cJSON_Delete(root);
  return r;
}

// Preset endpoints
static esp_err_t api_preset_list(httpd_req_t *req) {
  char out[1024];
  if (!presets_list_json(out, sizeof(out))) return httpd_resp_send_err(req, 500, "list failed");
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_sendstr(req, out);
}

static esp_err_t api_preset_save(httpd_req_t *req) {
  char body[256];
  int n = httpd_req_recv(req, body, sizeof(body)-1);
  if (n<=0) return httpd_resp_send_err(req, 400, "no body");
  body[n]=0;

  cJSON *root = cJSON_Parse(body);
  if (!root) return httpd_resp_send_err(req, 400, "bad json");
  const char *name = cJSON_GetObjectItem(root, "name")->valuestring;
  bool ok = presets_save_current(name);
  cJSON_Delete(root);

  if (!ok) return httpd_resp_send_err(req, 500, "save failed");
  return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static esp_err_t api_preset_load(httpd_req_t *req) {
  char body[256];
  int n = httpd_req_recv(req, body, sizeof(body)-1);
  if (n<=0) return httpd_resp_send_err(req, 400, "no body");
  body[n]=0;

  cJSON *root = cJSON_Parse(body);
  if (!root) return httpd_resp_send_err(req, 400, "bad json");
  const char *name = cJSON_GetObjectItem(root, "name")->valuestring;
  bool ok = presets_load_and_apply(name);
  cJSON_Delete(root);

  if (!ok) return httpd_resp_send_err(req, 500, "load failed");
  return httpd_resp_sendstr(req, "{\"ok\":true}");
}

#if CONFIG_ROLE_MASTER
static esp_err_t api_apply_range(httpd_req_t *req) {
  char body[1024];
  int n = httpd_req_recv(req, body, sizeof(body)-1);
  if (n<=0) return httpd_resp_send_err(req, 400, "no body");
  body[n]=0;

  if (!slave_http_post_json("/api/registers/range", body)) {
    return httpd_resp_send_err(req, 500, "slave range failed");
  }

  // Apply local too
  httpd_req_t *fake = req; (void)fake;
  // Re-parse and write locally
  cJSON *root = cJSON_Parse(body);
  if (!root) return httpd_resp_send_err(req, 400, "bad json");
  int bank = cJSON_GetObjectItem(root, "bank")->valueint;
  const char *start_s = cJSON_GetObjectItem(root, "start")->valuestring;
  cJSON *values = cJSON_GetObjectItem(root, "values");

  uint8_t start;
  if (!parse_hex_u8(start_s, &start) || !cJSON_IsArray(values)) { cJSON_Delete(root); return httpd_resp_send_err(req, 400, "bad"); }
  int count = cJSON_GetArraySize(values);

  for (int i=0;i<count;i++) {
    cJSON *it = cJSON_GetArrayItem(values, i);
    if (!cJSON_IsNumber(it)) { cJSON_Delete(root); return httpd_resp_send_err(req, 400, "values bad"); }
    if (!ov2640_write_reg((ov2640_bank_t)bank, (uint8_t)(start+i), (uint8_t)it->valueint)) {
      cJSON_Delete(root);
      return httpd_resp_send_err(req, 500, "local write failed");
    }
  }
  cJSON_Delete(root);
  return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static esp_err_t api_apply_preset(httpd_req_t *req) {
  char body[256];
  int n = httpd_req_recv(req, body, sizeof(body)-1);
  if (n<=0) return httpd_resp_send_err(req, 400, "no body");
  body[n]=0;

  cJSON *root = cJSON_Parse(body);
  if (!root) return httpd_resp_send_err(req, 400, "bad json");
  const char *name = cJSON_GetObjectItem(root, "name")->valuestring;
  cJSON_Delete(root);

  if (!slave_http_post_json("/api/registers/preset/load", body)) {
    return httpd_resp_send_err(req, 500, "slave preset load failed");
  }
  if (!presets_load_and_apply(name)) {
    return httpd_resp_send_err(req, 500, "local preset load failed");
  }
  return httpd_resp_sendstr(req, "{\"ok\":true}");
}
#endif

bool web_server_start(void) {
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  cfg.stack_size = 8192;
  cfg.max_uri_handlers = 48;
  cfg.uri_match_fn = httpd_uri_match_wildcard;

  if (httpd_start(&g_http, &cfg) != ESP_OK) return false;

  httpd_register_uri_handler(g_http, &(httpd_uri_t){ .uri="/", .method=HTTP_GET, .handler=h_root });
  httpd_register_uri_handler(g_http, &(httpd_uri_t){ .uri="/registers", .method=HTTP_GET, .handler=h_registers_page });
  httpd_register_uri_handler(g_http, &(httpd_uri_t){ .uri="/www/*", .method=HTTP_GET, .handler=h_www_any });
  httpd_register_uri_handler(g_http, &(httpd_uri_t){ .uri="/stream", .method=HTTP_GET, .handler=h_stream });

  httpd_register_uri_handler(g_http, &(httpd_uri_t){ .uri="/api/capture_local", .method=HTTP_POST, .handler=api_capture_local });
#if CONFIG_ROLE_MASTER
  httpd_register_uri_handler(g_http, &(httpd_uri_t){ .uri="/api/capture_sync", .method=HTTP_POST, .handler=api_capture_sync });
#endif
#if CONFIG_ROLE_SLAVE
  httpd_register_uri_handler(g_http, &(httpd_uri_t){ .uri="/api/arm", .method=HTTP_POST, .handler=api_arm });
#endif

  httpd_register_uri_handler(g_http, &(httpd_uri_t){ .uri="/api/registers/single", .method=HTTP_GET, .handler=api_reg_single_get });
  httpd_register_uri_handler(g_http, &(httpd_uri_t){ .uri="/api/registers/single", .method=HTTP_POST, .handler=api_reg_single_post });
  httpd_register_uri_handler(g_http, &(httpd_uri_t){ .uri="/api/registers/range", .method=HTTP_GET, .handler=api_reg_range_get });
  httpd_register_uri_handler(g_http, &(httpd_uri_t){ .uri="/api/registers/range", .method=HTTP_POST, .handler=api_reg_range_post });
  httpd_register_uri_handler(g_http, &(httpd_uri_t){ .uri="/api/registers/dump", .method=HTTP_GET, .handler=api_reg_dump_get });

  httpd_register_uri_handler(g_http, &(httpd_uri_t){ .uri="/api/registers/preset", .method=HTTP_GET, .handler=api_preset_list });
  httpd_register_uri_handler(g_http, &(httpd_uri_t){ .uri="/api/registers/preset/save", .method=HTTP_POST, .handler=api_preset_save });
  httpd_register_uri_handler(g_http, &(httpd_uri_t){ .uri="/api/registers/preset/load", .method=HTTP_POST, .handler=api_preset_load });

#if CONFIG_ROLE_MASTER
  httpd_register_uri_handler(g_http, &(httpd_uri_t){ .uri="/api/registers/apply_range", .method=HTTP_POST, .handler=api_apply_range });
  httpd_register_uri_handler(g_http, &(httpd_uri_t){ .uri="/api/registers/apply_preset", .method=HTTP_POST, .handler=api_apply_preset });
#endif

#if CONFIG_ROLE_SLAVE
  g_arm_sem = xSemaphoreCreateBinary();
  trigger_gpio_init(g_arm_sem);
  xTaskCreatePinnedToCore(slave_capture_task, "slave_capture", 8192, NULL, 10, NULL, 1);
#else
  trigger_gpio_init(NULL);
#endif

  ESP_LOGI(TAG, "HTTP server started");
  return true;
}
