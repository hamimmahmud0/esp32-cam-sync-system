#include "mdns_names.h"
#include "app_config.h"
#include "esp_log.h"
#include "mdns.h"

static const char *TAG = "mDNS";

bool mdns_start_with_http(void) {
  ESP_ERROR_CHECK(mdns_init());
  ESP_ERROR_CHECK(mdns_hostname_set(APP_HOSTNAME));
  ESP_ERROR_CHECK(mdns_instance_name_set(APP_HOSTNAME));
  ESP_ERROR_CHECK(mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0));
  ESP_LOGI(TAG, "mDNS hostname: %s.local", APP_HOSTNAME);
  return true;
}
