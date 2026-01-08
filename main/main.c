#define ESP32_CAM_MASTER
#define HTTP_PORT 80

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "nvs_flash.h"
#include "cJSON.h"

#include "sdcard.h"
#include "wifi.h"
#include "ini.h"
#include "server.h"
#include "mdns_util.h"
#include "esp_system.h" 


// TAG for logging
static const char *TAG = "main";


// operational modes master vs slave
typedef enum {
    MODE_MASTER,
    MODE_SLAVE
} system_mode_t;

// save all init ret
typedef struct {
    esp_err_t camera_init_ret;
    esp_err_t sdcard_init_ret;
    esp_err_t wifi_connect_ret;
    esp_err_t ini_load_ret;
    esp_err_t http_server_ret;
    esp_err_t mdns_init_ret;
} init_status_t;



// Global system state
typedef struct {
    #ifdef ESP32_CAM_MASTER
    #else
    #endif
    camera_state_t cam;
    system_mode_t mode;
    bool sync_armed;
    SemaphoreHandle_t mutex;
    QueueHandle_t cmd_queue;
    TaskHandle_t capture_task;
    system_state_enum_t system_state;
    init_status_t init_status;
    app_config_t *app_config;
    uint64_t chipid;

} system_state_t;

static system_state_t g_state;

//system state
typedef enum {
    SYSTEM_STATE_INIT,
    SYSTEM_STATE_READY,
    SYSTEM_STATE_ERROR
} system_state_enum_t;


// Command types
typedef enum {
    CMD_CAPTURE_SINGLE,
    CMD_CAPTURE_BURST,
    CMD_CAPTURE_SYNCED_BURST,
    CMD_CAPTURE_VIDEO,
    CMD_REGISTER_WRITE,
    CMD_REGISTER_SYNC,
    CMD_SYSTEM_CONFIG
} command_type_t;

typedef struct {
    command_type_t type;
    void *data;
    size_t data_len;
} command_t;





esp_err_t init_system(void)
{
    g_state.chipid = esp_efuse_mac_get_default();
    memset(&g_state, 0, sizeof(g_state));
    g_state.mutex = xSemaphoreCreateMutex();
    g_state.cmd_queue = xQueueCreate(10, sizeof(command_t));
    #ifdef ESP32_CAM_MASTER
        g_state.mode = MODE_MASTER;  // default mode
    #else
        g_state.mode = MODE_SLAVE;
    #endif
    g_state.sync_armed = false;
    g_state.app_config = &g_config;

    // init SD card
    esp_err_t sdcard_ret = init_sdcard();
    if (sdcard_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SD card");
    }
    g_state.init_status.sdcard_init_ret = sdcard_ret;


    // load ini config from SD card
    esp_err_t ini_ret = load_ini_config();
    if (ini_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load ini config");
    }
    g_state.init_status.ini_load_ret = ini_ret;

    // connect to WiFi
    esp_err_t wifi_ret = wifi_connect_sta(
        g_config.wifi_ssid,
        g_config.wifi_pass
    );
    if (wifi_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect to WiFi");
    }
    g_state.init_status.wifi_connect_ret = wifi_ret;

    // init mDNS
    esp_err_t mdns_ret = init_mdns();
    if (mdns_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize mDNS");
    }
    g_state.init_status.mdns_init_ret = mdns_ret;


    // start HTTP server
    esp_err_t http_ret = ESP_OK;
    httpd_handle_t server = start_webserver();
    if (server == NULL) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        http_ret = ESP_FAIL;
    }
    g_state.init_status.http_server_ret = http_ret;

}





void app_main(void)
{
    // set system state to init
    g_state.system_state = SYSTEM_STATE_INIT;

    // init system if error than log and set system state to error
    esp_err_t ret = init_system();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "System initialization failed");
        g_state.system_state = SYSTEM_STATE_ERROR;
    } else {
        g_state.system_state = SYSTEM_STATE_READY;
        ESP_LOGI(TAG, "System initialized successfully");
    }

    
}
