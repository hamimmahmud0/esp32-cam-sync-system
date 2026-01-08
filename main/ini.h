#include <stdio.h>
#include <string.h>
#include "esp_log.h"

#define TAG "INI"
#define INI_LOCATION "/sdcard/config.ini"


typedef struct {
    char wifi_ssid[32];
    char wifi_pass[64];
    int  server_port;
} app_config_t;

app_config_t g_config;



static void trim(char *str)
{
    char *end;
    while (*str == ' ' || *str == '\t' || *str == '\n') str++;
    end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n')) {
        *end-- = 0;
    }
}

esp_err_t load_ini_config(void)
{
    FILE *f = fopen(INI_LOCATION, "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s", INI_LOCATION);
        return ESP_FAIL;
    }

    char line[128];
    char key[64], value[64];

    while (fgets(line, sizeof(line), f)) {

        /* Ignore comments and empty lines */
        if (line[0] == '#' || line[0] == ';' || strlen(line) < 3)
            continue;

        if (sscanf(line, "%63[^=]=%63[^\n]", key, value) == 2) {

            trim(key);
            trim(value);

            /* ---- Parse known keys ---- */
            if (strcmp(key, "wifi_ssid") == 0) {
                strncpy(g_config.wifi_ssid, value, sizeof(g_config.wifi_ssid));
            }
            else if (strcmp(key, "wifi_pass") == 0) {
                strncpy(g_config.wifi_pass, value, sizeof(g_config.wifi_pass));
            }
            else if (strcmp(key, "server_port") == 0) {
                g_config.server_port = atoi(value);
            }
            else {
                ESP_LOGW(TAG, "Unknown key: %s", key);
            }
        }
    }

    fclose(f);

    ESP_LOGI(TAG, "INI loaded:");
    ESP_LOGI(TAG, "SSID: %s", g_config.wifi_ssid);
    ESP_LOGI(TAG, "PORT: %d", g_config.server_port);

    return ESP_OK;
}
