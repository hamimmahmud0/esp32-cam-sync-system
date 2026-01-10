#include "slave_client.h"
#include "app_config.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

#if CONFIG_ROLE_MASTER
static const char *TAG="SLV";

static void make_url(char *dst, int max, const char *path) {
  snprintf(dst, max, "http://%s.local%s", SLAVE_MDNS_HOST, path);
}

bool slave_http_post_json(const char *path, const char *json_body) {
  char url[256];
  make_url(url, sizeof(url), path);

  esp_http_client_config_t cfg = {
    .url = url,
    .timeout_ms = 4000,
  };
  esp_http_client_handle_t c = esp_http_client_init(&cfg);
  esp_http_client_set_method(c, HTTP_METHOD_POST);
  esp_http_client_set_header(c, "Content-Type", "application/json");
  esp_http_client_set_post_field(c, json_body, (int)strlen(json_body));

  esp_err_t err = esp_http_client_perform(c);
  int code = esp_http_client_get_status_code(c);
  esp_http_client_cleanup(c);

  if (err != ESP_OK || code != 200) {
    ESP_LOGE(TAG, "POST %s failed err=%s code=%d", url, esp_err_to_name(err), code);
    return false;
  }
  return true;
}

bool slave_http_get(const char *path, char *out, int out_max) {
  char url[256];
  make_url(url, sizeof(url), path);

  esp_http_client_config_t cfg = { .url=url, .timeout_ms=4000 };
  esp_http_client_handle_t c = esp_http_client_init(&cfg);
  esp_http_client_set_method(c, HTTP_METHOD_GET);

  esp_err_t err = esp_http_client_perform(c);
  int code = esp_http_client_get_status_code(c);
  if (err != ESP_OK || code != 200) {
    esp_http_client_cleanup(c);
    return false;
  }
  int n = esp_http_client_read_response(c, out, out_max-1);
  if (n < 0) n = 0;
  out[n] = 0;
  esp_http_client_cleanup(c);
  return true;
}
#else
bool slave_http_post_json(const char *path, const char *json_body){ (void)path;(void)json_body; return false; }
bool slave_http_get(const char *path, char *out, int out_max){ (void)path;(void)out;(void)out_max; return false; }
#endif
