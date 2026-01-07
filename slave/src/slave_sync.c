#include "slave_sync.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "SLAVE_SYNC";

// Master connection information
static char master_ip[16] = "192.168.4.1";
static bool master_connected = false;

// Initialize synchronization
esp_err_t sync_init(sync_mode_t mode) {
    if (mode == SLAVE_MODE) {
        // Slave mode: connect to master
        ESP_LOGI(TAG, "Initializing slave synchronization");
        
        // Attempt to connect to master
        if (connect_to_master() != ESP_OK) {
            ESP_LOGW(TAG, "Failed to connect to master, will retry");
            // Schedule retry
        }
    }
    
    return ESP_OK;
}

// Connect to master
esp_err_t connect_to_master(void) {
    ESP_LOGI(TAG, "Attempting to connect to master at %s", master_ip);
    
    // Send registration request
    char url[64];
    snprintf(url, sizeof(url), "http://%s/api/slave/register", master_ip);
    
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 5000,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    
    // Get slave IP
    char ip_str[16];
    wifi_get_ip(ip_str, sizeof(ip_str));
    
    // Create registration data
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "ip", ip_str);
    cJSON_AddStringToObject(root, "type", "slave");
    
    char *post_data = cJSON_PrintUnformatted(root);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    esp_http_client_set_header(client, "Content-Type", "application/json");
    
    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code == 200) {
            master_connected = true;
            ESP_LOGI(TAG, "Successfully registered with master");
        } else {
            ESP_LOGE(TAG, "Registration failed with status: %d", status_code);
            master_connected = false;
        }
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        master_connected = false;
    }
    
    esp_http_client_cleanup(client);
    cJSON_Delete(root);
    free(post_data);
    
    return master_connected ? ESP_OK : ESP_FAIL;
}

// Check if master is connected
bool is_master_connected(void) {
    return master_connected;
}

// Process register update from master
esp_err_t process_register_update(const char *data, size_t len) {
    cJSON *root = cJSON_Parse(data);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse register update");
        return ESP_FAIL;
    }
    
    cJSON *bank_json = cJSON_GetObjectItem(root, "bank");
    cJSON *addr_json = cJSON_GetObjectItem(root, "addr");
    cJSON *value_json = cJSON_GetObjectItem(root, "value");
    
    if (!cJSON_IsNumber(bank_json) || !cJSON_IsNumber(addr_json) || !cJSON_IsNumber(value_json)) {
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    
    uint8_t bank = bank_json->valueint;
    uint8_t addr = addr_json->valueint;
    uint8_t value = value_json->valueint;
    
    // Apply register change
    if (!ov2640_write_reg(addr, value, bank)) {
        ESP_LOGE(TAG, "Failed to apply register update 0x%02X=0x%02X", addr, value);
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Applied register update: bank=%d, 0x%02X=0x%02X", bank, addr, value);
    cJSON_Delete(root);
    return ESP_OK;
}

// Send capture complete notification to master
esp_err_t send_capture_complete(slave_state_t *state) {
    if (!master_connected) {
        return ESP_FAIL;
    }
    
    char url[64];
    snprintf(url, sizeof(url), "http://%s/api/slave/capture-complete", master_ip);
    
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 5000,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "filename", state->current_filename);
    cJSON_AddNumberToObject(root, "size", state->file_size);
    
    char *post_data = cJSON_PrintUnformatted(root);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    esp_http_client_set_header(client, "Content-Type", "application/json");
    
    esp_err_t err = esp_http_client_perform(client);
    
    esp_http_client_cleanup(client);
    cJSON_Delete(root);
    free(post_data);
    
    return err;
}

// Heartbeat task (runs periodically)
void heartbeat_task(void *arg) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000)); // Every 10 seconds
        
        if (master_connected) {
            // Send heartbeat to master
            char url[64];
            snprintf(url, sizeof(url), "http://%s/api/slave/heartbeat", master_ip);
            
            esp_http_client_config_t config = {
                .url = url,
                .method = HTTP_METHOD_POST,
                .timeout_ms = 3000,
            };
            
            esp_http_client_handle_t client = esp_http_client_init(&config);
            
            cJSON *root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "status", "alive");
            cJSON_AddNumberToObject(root, "uptime", xTaskGetTickCount() * portTICK_PERIOD_MS);
            
            char *post_data = cJSON_PrintUnformatted(root);
            esp_http_client_set_post_field(client, post_data, strlen(post_data));
            esp_http_client_set_header(client, "Content-Type", "application/json");
            
            esp_err_t err = esp_http_client_perform(client);
            
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Heartbeat failed, master may be disconnected");
                master_connected = false;
                // Schedule reconnection
            }
            
            esp_http_client_cleanup(client);
            cJSON_Delete(root);
            free(post_data);
        } else {
            // Attempt to reconnect
            connect_to_master();
        }
    }
}