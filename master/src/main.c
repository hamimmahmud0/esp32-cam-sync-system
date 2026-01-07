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

#include "sccb_driver.h"
#include "ov2640_regs.h"
#include "camera_control.h"
#include "sync_protocol.h"
#include "web_interface.h"
#include "storage.h"

// TAG for logging
static const char *TAG = "MASTER";

// Global system state
typedef struct {
    camera_state_t master_cam;
    camera_state_t slave_cam;
    system_mode_t mode;
    bool sync_armed;
    SemaphoreHandle_t mutex;
    QueueHandle_t cmd_queue;
    TaskHandle_t capture_task;
} system_state_t;

static system_state_t g_state;

// Command types
typedef enum {
    CMD_CAPTURE_SINGLE,
    CMD_CAPTURE_BURST,
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

// System initialization
static esp_err_t system_init(void) {
    esp_err_t ret = ESP_OK;
    
    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Create mutex and queue
    g_state.mutex = xSemaphoreCreateMutex();
    g_state.cmd_queue = xQueueCreate(10, sizeof(command_t));
    
    if (!g_state.mutex || !g_state.cmd_queue) {
        ESP_LOGE(TAG, "Failed to create system resources");
        return ESP_FAIL;
    }
    
    // Initialize SCCB driver
    ESP_ERROR_CHECK(sccb_init());
    
    // Initialize camera
    ESP_ERROR_CHECK(camera_init(&g_state.master_cam, CAMERA_MASTER));
    
    // Initialize storage
    ESP_ERROR_CHECK(storage_init());
    
    // Initialize Wi-Fi
    ESP_ERROR_CHECK(wifi_init_softap());
    
    // Initialize web server
    ESP_ERROR_CHECK(web_server_init());
    
    // Initialize synchronization
    ESP_ERROR_CHECK(sync_init(MASTER_MODE));
    
    // Load configuration
    ESP_ERROR_CHECK(load_configuration());
    
    return ESP_OK;
}

// Capture task
static void capture_task_handler(void *arg) {
    ESP_LOGI(TAG, "Capture task started");
    
    while (1) {
        command_t cmd;
        if (xQueueReceive(g_state.cmd_queue, &cmd, portMAX_DELAY)) {
            xSemaphoreTake(g_state.mutex, portMAX_DELAY);
            
            switch (cmd.type) {
                case CMD_CAPTURE_SINGLE:
                    capture_single_image(&g_state.master_cam, &g_state.slave_cam);
                    break;
                    
                case CMD_CAPTURE_BURST:
                    capture_burst(&g_state.master_cam, &g_state.slave_cam, 
                                  ((burst_cmd_t*)cmd.data)->count);
                    break;
                    
                case CMD_CAPTURE_VIDEO:
                    capture_video(&g_state.master_cam, &g_state.slave_cam,
                                 ((video_cmd_t*)cmd.data)->duration_ms);
                    break;
                    
                case CMD_REGISTER_WRITE:
                    process_register_write((register_cmd_t*)cmd.data);
                    break;
                    
                case CMD_REGISTER_SYNC:
                    sync_registers_to_slave(&g_state.master_cam, &g_state.slave_cam);
                    break;
                    
                case CMD_SYSTEM_CONFIG:
                    apply_system_config((config_cmd_t*)cmd.data);
                    break;
            }
            
            if (cmd.data) {
                free(cmd.data);
            }
            
            xSemaphoreGive(g_state.mutex);
        }
    }
}

// Main application
void app_main(void) {
    ESP_LOGI(TAG, "Starting Master ESP32-CAM System");
    
    // Initialize system
    if (system_init() != ESP_OK) {
        ESP_LOGE(TAG, "System initialization failed");
        return;
    }
    
    // Create capture task
    xTaskCreate(capture_task_handler, "capture_task", 8192, NULL, 5, &g_state.capture_task);
    
    // Report system status
    ESP_LOGI(TAG, "System initialized successfully");
    ESP_LOGI(TAG, "IP Address: %s", wifi_get_ip());
    ESP_LOGI(TAG, "SD Card: %s", storage_is_mounted() ? "Mounted" : "Not mounted");
    ESP_LOGI(TAG, "Camera: %s", g_state.master_cam.initialized ? "Ready" : "Not ready");
    
    // Keep main task alive
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        // Monitor system health
        monitor_system_health();
    }
}