#include "app_config.h"
#include "app_state.h"
#include "sdmmc_mount.h"
#include "mdns_names.h"
#include "cam_manager.h"
#include "web_server.h"
#include "wifi_sta.h"

#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG="MAIN";

void app_main(void) {
  ESP_ERROR_CHECK(nvs_flash_init());

  g_app.cam_mutex = xSemaphoreCreateMutex();
  g_app.sccb_mutex = xSemaphoreCreateMutex();
  g_app.stream_enabled = true;

  if (!wifi_sta_start_and_wait()) {
    ESP_LOGE(TAG, "Wi-Fi not connected; mDNS and sync may not work");
  }

  if (!sdmmc_mount_and_prepare()) {
    ESP_LOGE(TAG, "SD mount failed; expected SDIO 4-bit FAT32");
  }

  mdns_start_with_http();

  if (!cam_manager_init()) {
    ESP_LOGE(TAG, "Camera init failed");
  }

  if (!web_server_start()) {
    ESP_LOGE(TAG, "Web server failed");
  }

  ESP_LOGI(TAG, "Ready: http://%s.local/  stream: /stream  registers: /registers", APP_HOSTNAME);
}
