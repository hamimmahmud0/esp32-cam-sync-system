#include <string.h>
#include <sys/stat.h> // Required for checking file existence
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_server.h"

static const char *TAG = "WEB_SERVER";

/* ---------- HTTP HANDLER ---------- */
static esp_err_t root_get_handler(httpd_req_t *req)
{
    FILE *f = fopen("/sdcard/www/index.html", "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open index.html");
        httpd_resp_send_404(req, "File not found");
        return ESP_FAIL;
    }

    char buf[1024];
    size_t read_bytes;
    while ((read_bytes = fread(buf, 1, sizeof(buf), f)) > 0) {
        httpd_resp_send_chunk(req, buf, read_bytes);
    }
    fclose(f); // Crucial: Always close your file!
    httpd_resp_send_chunk(req, NULL, 0); 
    return ESP_OK;
}

/* ---------- API HANDLERS ---------- */

// GET Handler: Used for fetching data (e.g., /api/sensor/read)
static esp_err_t api_get_handler(httpd_req_t *req)
{
    char category[32];
    char action[32];

    // Parse the URI: skipping "/api/" (5 chars)
    // Format: /api/%[^/]/%s -> read until next slash, then read the rest
    if (sscanf(req->uri, "/api/%31[^/]/%31s", category, action) != 2) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid API format");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "GET API - Category: %s, Action: %s", category, action);

    if (strcmp(category, "sensor") == 0) {
        if (strcmp(action, "read") == 0) {
            const char* json_response = "{\"temperature\": 25.5, \"humidity\": 60}";
            httpd_resp_set_type(req, "application/json");
            return httpd_resp_send(req, json_response, HTTPD_RESP_USE_STRLEN);
        }
    }

    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "API Endpoint not found");
    return ESP_FAIL;
}

// POST Handler: Used for actions/settings (e.g., /api/led/update)
static esp_err_t api_post_handler(httpd_req_t *req)
{
    char category[32];
    char action[32];
    char content[128];

    if (sscanf(req->uri, "/api/%31[^/]/%31s", category, action) != 2) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid API format");
        return ESP_FAIL;
    }

    // Read the incoming POST data (JSON body)
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) { 
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) httpd_resp_send_408(req);
        return ESP_FAIL;
    }
    content[ret] = '\0'; // Null-terminate the buffer

    ESP_LOGI(TAG, "POST API - Category: %s, Action: %s, Data: %s", category, action, content);

    // Logic routing
    if (strcmp(category, "wifi") == 0 && strcmp(action, "save") == 0) {
        // Handle saving WiFi credentials here
        return httpd_resp_send(req, "Settings Saved", HTTPD_RESP_USE_STRLEN);
    }

    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "API Endpoint not found");
    return ESP_FAIL;
}

static esp_err_t file_get_handler(httpd_req_t *req)
{
    char filepath[128];
    struct stat st;

    // Construct the full path
    snprintf(filepath, sizeof(filepath), "/sdcard/www%s", req->uri);

    // Check if file exists and get info
    if (stat(filepath, &st) == -1) {
        ESP_LOGE(TAG, "File does not exist: %s", filepath);
        httpd_resp_send_404(req, "File not found");
        return ESP_FAIL;
    }

    FILE *f = fopen(filepath, "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s", filepath);
        httpd_resp_send_500(req, "Internal Server Error");
        return ESP_FAIL;
    }

    // Optional: Set Content-Type based on extension (simple version)
    if (strstr(filepath, ".css")) httpd_resp_set_type(req, "text/css");
    else if (strstr(filepath, ".js")) httpd_resp_set_type(req, "application/javascript");
    else if (strstr(filepath, ".png")) httpd_resp_set_type(req, "image/png");

    char buf[1024];
    size_t read_bytes;
    while ((read_bytes = fread(buf, 1, sizeof(buf), f)) > 0) {
        httpd_resp_send_chunk(req, buf, read_bytes);
    }
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* ---------- URI REGISTRATIONS ---------- */
static httpd_uri_t root = {
    .uri      = "/",
    .method   = HTTP_GET,
    .handler  = root_get_handler,
    .user_ctx = NULL
};

// Catch-all handler for assets
static httpd_uri_t file_serve = {
    .uri       = "/*", 
    .method    = HTTP_GET,
    .handler   = file_get_handler,
    .user_ctx  = NULL
};

// catch-all handler for apis
static httpd_uri_t api_get_serve = {
    .uri       = "/api/*", 
    .method    = HTTP_GET,
    .handler   = api_get_handler,
    .user_ctx  = NULL
};

static httpd_uri_t api_post_serve = {
    .uri       = "/api/*", 
    .method    = HTTP_POST,
    .handler   = api_post_handler,
    .user_ctx  = NULL
};


/* ---------- START SERVER ---------- */
static httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard; // Required for /*
    
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        // Register static files first or last depending on priority
        httpd_register_uri_handler(server, &root);
        
        // Register API Handlers
        httpd_register_uri_handler(server, &api_get_serve);
        httpd_register_uri_handler(server, &api_post_serve);
        
        // General file server (catch-all)
        httpd_register_uri_handler(server, &file_serve);
        
        ESP_LOGI(TAG, "Server listening on port 80");
    }
    return server;
}