#include "web_interface.h"
#include "esp_http_server.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <sys/stat.h>

#include "ov2640_control.h"
#include "camera_control.h"
#include "sync_protocol.h"

static const char *TAG = "WEB";

static httpd_handle_t server = NULL;

// File path for web content
#define BASE_PATH "/spiffs"
#define WWW_PATH BASE_PATH "/www"

// Register API handlers
static esp_err_t api_registers_single_get_handler(httpd_req_t *req);
static esp_err_t api_registers_single_post_handler(httpd_req_t *req);
static esp_err_t api_registers_range_get_handler(httpd_req_t *req);
static esp_err_t api_registers_range_post_handler(httpd_req_t *req);
static esp_err_t api_registers_dump_handler(httpd_req_t *req);
static esp_err_t api_registers_apply_handler(httpd_req_t *req);
static esp_err_t api_capture_handler(httpd_req_t *req);
static esp_err_t api_camera_config_handler(httpd_req_t *req);
static esp_err_t api_system_status_handler(httpd_req_t *req);

// File serving handler
static esp_err_t file_get_handler(httpd_req_t *req) {
    char filepath[256];
    
    // Get requested file path
    if (req->uri[strlen(req->uri) - 1] == '/') {
        snprintf(filepath, sizeof(filepath), "%s%sindex.html", WWW_PATH, req->uri);
    } else {
        snprintf(filepath, sizeof(filepath), "%s%s", WWW_PATH, req->uri);
    }
    
    // Check if file exists
    FILE *file = fopen(filepath, "r");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open file: %s", filepath);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Set content type based on file extension
    const char *ext = strrchr(filepath, '.');
    if (ext) {
        if (strcmp(ext, ".html") == 0) {
            httpd_resp_set_type(req, "text/html");
        } else if (strcmp(ext, ".css") == 0) {
            httpd_resp_set_type(req, "text/css");
        } else if (strcmp(ext, ".js") == 0) {
            httpd_resp_set_type(req, "application/javascript");
        } else if (strcmp(ext, ".png") == 0) {
            httpd_resp_set_type(req, "image/png");
        } else if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) {
            httpd_resp_set_type(req, "image/jpeg");
        } else if (strcmp(ext, ".json") == 0) {
            httpd_resp_set_type(req, "application/json");
        }
    }
    
    // Send file in chunks
    char chunk[1024];
    size_t read_bytes;
    while ((read_bytes = fread(chunk, 1, sizeof(chunk), file)) > 0) {
        if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
            fclose(file);
            return ESP_FAIL;
        }
    }
    
    fclose(file);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

// Initialize web server
esp_err_t web_server_init(void) {
    // Mount SPIFFS for web content
    esp_vfs_spiffs_conf_t conf = {
        .base_path = BASE_PATH,
        .partition_label = NULL,
        .max_files = 20,
        .format_if_mount_failed = true
    };
    
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SPIFFS");
        return ret;
    }
    
    // Start HTTP server
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 20;
    config.stack_size = 8192;
    
    ESP_LOGI(TAG, "Starting web server on port %d", config.server_port);
    
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start web server");
        return ESP_FAIL;
    }
    
    // Register URI handlers
    
    // API endpoints
    httpd_uri_t api_registers_single_get = {
        .uri = "/api/registers/single",
        .method = HTTP_GET,
        .handler = api_registers_single_get_handler,
        .user_ctx = NULL
    };
    
    httpd_uri_t api_registers_single_post = {
        .uri = "/api/registers/single",
        .method = HTTP_POST,
        .handler = api_registers_single_post_handler,
        .user_ctx = NULL
    };
    
    httpd_uri_t api_registers_range_get = {
        .uri = "/api/registers/range",
        .method = HTTP_GET,
        .handler = api_registers_range_get_handler,
        .user_ctx = NULL
    };
    
    httpd_uri_t api_registers_range_post = {
        .uri = "/api/registers/range",
        .method = HTTP_POST,
        .handler = api_registers_range_post_handler,
        .user_ctx = NULL
    };
    
    httpd_uri_t api_registers_dump = {
        .uri = "/api/registers/dump",
        .method = HTTP_GET,
        .handler = api_registers_dump_handler,
        .user_ctx = NULL
    };
    
    httpd_uri_t api_registers_apply = {
        .uri = "/api/registers/apply",
        .method = HTTP_POST,
        .handler = api_registers_apply_handler,
        .user_ctx = NULL
    };
    
    httpd_uri_t api_capture = {
        .uri = "/api/capture",
        .method = HTTP_POST,
        .handler = api_capture_handler,
        .user_ctx = NULL
    };
    
    httpd_uri_t api_camera_config = {
        .uri = "/api/camera/config",
        .method = HTTP_POST,
        .handler = api_camera_config_handler,
        .user_ctx = NULL
    };
    
    httpd_uri_t api_system_status = {
        .uri = "/api/system/status",
        .method = HTTP_GET,
        .handler = api_system_status_handler,
        .user_ctx = NULL
    };
    
    httpd_register_uri_handler(server, &api_registers_single_get);
    httpd_register_uri_handler(server, &api_registers_single_post);
    httpd_register_uri_handler(server, &api_registers_range_get);
    httpd_register_uri_handler(server, &api_registers_range_post);
    httpd_register_uri_handler(server, &api_registers_dump);
    httpd_register_uri_handler(server, &api_registers_apply);
    httpd_register_uri_handler(server, &api_capture);
    httpd_register_uri_handler(server, &api_camera_config);
    httpd_register_uri_handler(server, &api_system_status);
    
    // Static file handler (must be last)
    httpd_uri_t file_handler = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = file_get_handler,
        .user_ctx = NULL
    };
    
    httpd_register_uri_handler(server, &file_handler);
    
    ESP_LOGI(TAG, "Web server initialized");
    return ESP_OK;
}

// API: Read single register
static esp_err_t api_registers_single_get_handler(httpd_req_t *req) {
    char query[100];
    char bank_str[10], addr_str[10];
    uint8_t bank, addr, value;
    
    // Parse query parameters
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing query parameters");
        return ESP_FAIL;
    }
    
    if (httpd_query_key_value(query, "bank", bank_str, sizeof(bank_str)) != ESP_OK ||
        httpd_query_key_value(query, "addr", addr_str, sizeof(addr_str)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid query parameters");
        return ESP_FAIL;
    }
    
    // Convert parameters
    bank = strtol(bank_str, NULL, 0);
    addr = strtol(addr_str, NULL, 0);
    
    // Read register
    if (!ov2640_read_reg(addr, &value, bank)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read register");
        return ESP_FAIL;
    }
    
    // Create JSON response
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "bank", bank);
    cJSON_AddNumberToObject(root, "addr", addr);
    cJSON_AddNumberToObject(root, "value", value);
    
    char *response = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    
    cJSON_free(response);
    cJSON_Delete(root);
    return ESP_OK;
}

// API: Write single register
static esp_err_t api_registers_single_post_handler(httpd_req_t *req) {
    char buf[100];
    int ret, remaining = req->content_len;
    cJSON *root = NULL;
    
    // Read request body
    if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    
    buf[ret] = '\0';
    
    // Parse JSON
    root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    // Extract parameters
    cJSON *bank_json = cJSON_GetObjectItem(root, "bank");
    cJSON *addr_json = cJSON_GetObjectItem(root, "addr");
    cJSON *value_json = cJSON_GetObjectItem(root, "value");
    cJSON *mask_json = cJSON_GetObjectItem(root, "mask");
    
    if (!cJSON_IsNumber(bank_json) || !cJSON_IsNumber(addr_json) || !cJSON_IsNumber(value_json)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing required fields");
        return ESP_FAIL;
    }
    
    uint8_t bank = bank_json->valueint;
    uint8_t addr = addr_json->valueint;
    uint8_t value = value_json->valueint;
    uint8_t mask = mask_json ? mask_json->valueint : 0xFF;
    
    // Write register
    bool success;
    if (mask == 0xFF) {
        success = ov2640_write_reg(addr, value, bank);
    } else {
        success = ov2640_modify_reg(addr, mask, value, bank);
    }
    
    // Create response
    cJSON *response = cJSON_CreateObject();
    if (success) {
        cJSON_AddStringToObject(response, "status", "ok");
    } else {
        cJSON_AddStringToObject(response, "status", "error");
        cJSON_AddStringToObject(response, "message", "Failed to write register");
    }
    
    char *response_str = cJSON_PrintUnformatted(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response_str, strlen(response_str));
    
    cJSON_free(response_str);
    cJSON_Delete(response);
    cJSON_Delete(root);
    
    return ESP_OK;
}

// API: Capture image
static esp_err_t api_capture_handler(httpd_req_t *req) {
    char buf[100];
    int ret, remaining = req->content_len;
    cJSON *root = NULL;
    
    // Read request body
    if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    
    buf[ret] = '\0';
    
    // Parse JSON
    root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    // Extract parameters
    cJSON *type_json = cJSON_GetObjectItem(root, "type");
    cJSON *count_json = cJSON_GetObjectItem(root, "count");
    cJSON *duration_json = cJSON_GetObjectItem(root, "duration");
    
    if (!cJSON_IsString(type_json)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing capture type");
        return ESP_FAIL;
    }
    
    const char *type = type_json->valuestring;
    cJSON *response = cJSON_CreateObject();
    
    // Queue capture command
    command_t cmd = {0};
    
    if (strcmp(type, "single") == 0) {
        cmd.type = CMD_CAPTURE_SINGLE;
        cJSON_AddStringToObject(response, "message", "Single capture queued");
    } else if (strcmp(type, "burst") == 0) {
        cmd.type = CMD_CAPTURE_BURST;
        int count = count_json ? count_json->valueint : 10;
        
        burst_cmd_t *data = malloc(sizeof(burst_cmd_t));
        data->count = count;
        cmd.data = data;
        cmd.data_len = sizeof(burst_cmd_t);
        
        cJSON_AddStringToObject(response, "message", "Burst capture queued");
    } else if (strcmp(type, "video") == 0) {
        cmd.type = CMD_CAPTURE_VIDEO;
        int duration = duration_json ? duration_json->valueint : 10000;
        
        video_cmd_t *data = malloc(sizeof(video_cmd_t));
        data->duration_ms = duration;
        cmd.data = data;
        cmd.data_len = sizeof(video_cmd_t);
        
        cJSON_AddStringToObject(response, "message", "Video capture queued");
    } else {
        cJSON_Delete(root);
        cJSON_Delete(response);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid capture type");
        return ESP_FAIL;
    }
    
    // Queue command
    if (xQueueSend(g_state.cmd_queue, &cmd, pdMS_TO_TICKS(1000)) != pdTRUE) {
        cJSON_AddStringToObject(response, "status", "error");
        cJSON_AddStringToObject(response, "message", "Command queue full");
    } else {
        cJSON_AddStringToObject(response, "status", "ok");
    }
    
    char *response_str = cJSON_PrintUnformatted(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response_str, strlen(response_str));
    
    cJSON_free(response_str);
    cJSON_Delete(response);
    cJSON_Delete(root);
    
    return ESP_OK;
}

// API: System status
static esp_err_t api_system_status_handler(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    
    // System information
    cJSON_AddStringToObject(root, "device", "Master");
    cJSON_AddStringToObject(root, "version", "1.0.0");
    
    // Camera status
    cJSON *camera = cJSON_CreateObject();
    cJSON_AddBoolToObject(camera, "initialized", g_state.master_cam.initialized);
    cJSON_AddNumberToObject(camera, "resolution", g_state.master_cam.resolution);
    cJSON_AddNumberToObject(camera, "format", g_state.master_cam.format);
    cJSON_AddItemToObject(root, "camera", camera);
    
    // Storage status
    cJSON *storage = cJSON_CreateObject();
    cJSON_AddBoolToObject(storage, "mounted", storage_is_mounted());
    cJSON_AddNumberToObject(storage, "free_kb", storage_get_free_kb());
    cJSON_AddNumberToObject(storage, "total_kb", storage_get_total_kb());
    cJSON_AddItemToObject(root, "storage", storage);
    
    // Slave status
    cJSON *slave = cJSON_CreateObject();
    cJSON_AddBoolToObject(slave, "connected", g_state.slave_cam.connected);
    cJSON_AddStringToObject(slave, "ip", g_state.slave_cam.ip_address);
    cJSON_AddItemToObject(root, "slave", slave);
    
    char *response = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    
    cJSON_free(response);
    cJSON_Delete(root);
    
    return ESP_OK;
}