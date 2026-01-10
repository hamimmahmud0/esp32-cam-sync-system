idf_component_register(
  SRCS
    "app_main.c"
    "app_state.c"
    "sdmmc_mount.c"
    "mdns_names.c"
    "trigger_gpio.c"
    "cam_manager.c"
    "ov2640_ctrl.c"
    "reg_cache.c"
    "reg_profiles.c"
    "slave_client.c"
    "web_server.c"
    "wifi_sta.c"
  INCLUDE_DIRS "."
  REQUIRES esp_http_server esp_http_client mdns nvs_flash esp_timer driver fatfs sdmmc json esp_wifi esp_netif esp_event
)
