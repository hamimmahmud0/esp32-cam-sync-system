#include <stdio.h>
#include <string.h>
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
#include "driver/gpio.h"

#include "sccb_driver.h"
#include "ov2640_regs.h"
#include "ov2640_control.h"
#include "slave_sync.h"
#include "storage.h"

static const char *TAG = "SLAVE";

// Global state
typedef struct {
    camera_state_t camera;
    slave_state_t sync_state;
    bool armed;
    char current_filename[64];
    FILE *current_file;
    SemaphoreHandle_t mutex;
    TaskHandle_t capture_task;
} slave_state_t;

static slave_state_t g_state;

// Initialize system
static esp_err_t system_init(void) {
    esp_err_t ret = ESP_OK;
    
    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Create mutex
    g_state.mutex = xSemaphoreCreateMutex();
    if (!g_state.mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_FAIL;
    }
    
    // Initialize SCCB
    ESP_ERROR_CHECK(sccb_init());
    
    // Initialize camera with default settings
    ESP_ERROR_CHECK(camera_init(&g_state.camera, CAMERA_SLAVE));
    
    // Initialize storage
    ESP_ERROR_CHECK(storage_init());
    
    // Initialize Wi-Fi station mode
    ESP_ERROR_CHECK(wifi_init_sta());
    
    // Initialize synchronization
    ESP_ERROR_CHECK(sync_init(SLAVE_MODE));
    
    // Initialize HTTP server for receiving commands
    ESP_ERROR_CHECK(http_server_init());
    
    return ESP_OK;
}

// Capture task
static void capture_task_handler(void *arg) {
    ESP_LOGI(TAG, "Capture task started");
    
    while (1) {
        // Wait for trigger
        if (xTaskNotifyWait(0, 0, NULL, portMAX_DELAY) == pdTRUE) {
            xSemaphoreTake(g_state.mutex, portMAX_DELAY);
            
            if (g_state.armed && g_state.current_file) {
                ESP_LOGI(TAG, "Starting capture: %s", g_state.current_filename);
                
                // Capture frame
                capture_frame_to_file(&g_state.camera, g_state.current_file);
                
                // If this was a single capture, close file
                if (g_state.sync_state.capture_type == CAPTURE_SINGLE) {
                    fclose(g_state.current_file);
                    g_state.current_file = NULL;
                    g_state.armed = false;
                    
                    // Send completion notification to master
                    send_capture_complete(&g_state.sync_state);
                }
            }
            
            xSemaphoreGive(g_state.mutex);
        }
    }
}

// HTTP server handlers
static esp_err_t api_arm_handler(httpd_req_t *req) {
    // Parse JSON request
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request");
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    // Parse parameters (simplified)
    char filename[64];
    char mode[16];
    // Extract from JSON (in practice use cJSON)
    
    xSemaphoreTake(g_state.mutex, portMAX_DELAY);
    
    // Open file for writing
    char full_path[128];
    snprintf(full_path, sizeof(full_path), "/sdcard/captures/%s", filename);
    
    g_state.current_file = fopen(full_path, "wb");
    if (!g_state.current_file) {
        xSemaphoreGive(g_state.mutex);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open file");
        return ESP_FAIL;
    }
    
    strncpy(g_state.current_filename, filename, sizeof(g_state.current_filename));
    g_state.armed = true;
    
    xSemaphoreGive(g_state.mutex);
    
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

// Main application
void app_main(void) {
    ESP_LOGI(TAG, "Starting Slave ESP32-CAM System");
    
    // Initialize system
    if (system_init() != ESP_OK) {
        ESP_LOGE(TAG, "System initialization failed");
        return;
    }
    
    // Create capture task
    xTaskCreate(capture_task_handler, "capture_task", 8192, NULL, 5, &g_state.capture_task);
    
    // Configure trigger GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_TRIGGER_INPUT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_POSEDGE
    };
    gpio_config(&io_conf);
    
    // Install GPIO ISR service
    gpio_install_isr_service(0);
    gpio_isr_handler_add(GPIO_TRIGGER_INPUT, trigger_isr_handler, NULL);
    
    ESP_LOGI(TAG, "Slave system initialized and waiting for master");
    
    // Main loop
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // Check connection to master
        if (!is_master_connected()) {
            ESP_LOGW(TAG, "Master disconnected, attempting to reconnect...");
            // Reconnection logic here
        }
    }
}

// GPIO ISR handler for trigger
static void IRAM_ATTR trigger_isr_handler(void *arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(g_state.capture_task, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}