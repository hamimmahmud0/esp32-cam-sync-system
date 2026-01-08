#include <string.h>
#include <ctype.h>
#include "esp_log.h"
#include "mdns.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"


static const char *TAG = "mDNS_Demo";
const char *MDNS_HOSTNAME_PREFIX = "dual-esp32cam-master";
const char *MDNS_INSTANCE = "Dual ESP32-CAM Master Device";



esp_err_t start_mdns(void)
{
    // Initialize mDNS
    esp_err_t err = mdns_init();
    if (err) {
        ESP_LOGE(TAG, "mDNS Init failed: %d", err);
        return err;
    }

    // Set hostname (required)
    #ifdef ESP32_CAM_MASTER
    // master device
    // hostname has dual-esp32cam-master-xxxxxx.local pattern
    // where xxxxxx is last 3 bytes of MAC in hex
    uint64_t chipid = esp_efuse_mac_get_default();
    uint32_t mac_suffix = (uint32_t)(chipid & 0xFFFFFF);
    char hostname[64];
    snprintf(hostname, sizeof(hostname), "%s-%06X", MDNS_HOSTNAME_PREFIX, mac_suffix);
    mdns_hostname_set(hostname);
    #else
    // slave device
    // hostname has dual-esp32cam-slave-xxxxxx.local pattern
    // where xxxxxx is last 3 bytes of MAC in hex
    uint64_t chipid = esp_efuse_mac_get_default();
    uint32_t mac_suffix = (uint32_t)(chipid & 0xFFFFFF);
    char hostname[64];
    snprintf(hostname, sizeof(hostname), "dual-esp32cam-slave-%06X", mac_suffix);
    mdns_hostname_set(hostname);
    #endif
    
    // Set default instance name
    mdns_instance_name_set(MDNS_INSTANCE);
    return ESP_OK;
}

void add_http_service(int port)
{
    // Add HTTP service
    mdns_service_add(NULL, "_http", "_tcp", port, NULL, 0);
    
    // Add service instance name
    mdns_service_instance_name_set("_http", "_tcp", "Dual cam HTTP Server");
    
    // Add TXT items (optional metadata)
    mdns_txt_item_t txt_records[] = {
        {"path", "/"},
        {"version", "1.0"}
    };
    mdns_service_txt_set("_http", "_tcp", txt_records, 2);
    // Logging
    ESP_LOGI(TAG, "mDNS HTTP service added on port %d", port);
}


void browse_http_services(mdns_result_t *results)
{
    ESP_LOGI(TAG, "Browsing services...");
    
    
    esp_err_t err = mdns_query("_http", "_tcp", NULL, NULL, 0, 1000, &results);
    
    if (err) {
        ESP_LOGE(TAG, "Query Failed: %d", err);
        return;
    }
    
    if (!results) {
        ESP_LOGI(TAG, "No results found!");
        return;
    }

    mdns_result_t *r = results;
    while (r) {
        ESP_LOGI(TAG, "Found service: %s", r->instance_name);
        ESP_LOGI(TAG, "  Host: %s.local", r->hostname);
        ESP_LOGI(TAG, "  Port: %d", r->port);
        
        // Print IP addresses
        mdns_ip_addr_t *addr = r->addr;
        while (addr) {
            ESP_LOGI(TAG, "  IP: " IPSTR, IP2STR(&addr->addr.u_addr.ip4));
            addr = addr->next;
        }
        
        r = r->next;
    }
    
    //mdns_query_results_free(results);
}


typedef struct {
    const char **service_name;
    const char **protocol_name;
    const char **instance_name;
    uint16_t *port;
    const char ***txt_items;
    size_t *txt_count;
} mdns_service_info_t;



void add_mdns_service(mdns_service_info_t *service_info)
{
    mdns_service_add(
        NULL,
        *(service_info->service_name),
        *(service_info->protocol_name),
        *(service_info->port),
        NULL,
        0
    );
    
    mdns_service_instance_name_set(
        *(service_info->service_name),
        *(service_info->protocol_name),
        *(service_info->instance_name)
    );
    
    mdns_service_txt_set(
        *(service_info->service_name),
        *(service_info->protocol_name),
        *(service_info->txt_items),
        *(service_info->txt_count)
    );
}

void remove_mdns_service(const char *service_name, const char *protocol_name)
{
    mdns_service_remove(service_name, protocol_name);
}



esp_err_t init_mdns(void)
{
    esp_err_t ret = start_mdns();
    if (ret != ESP_OK) {
        return ret;
    }

    #ifdef HTTP_PORT
    add_http_service(HTTP_PORT);
    #else
    add_http_service(80);
    #endif

    // logging
    ESP_LOGI(TAG, "mDNS initialized with hostname: dual-esp32cam-master.local");

    return ESP_OK;
}



#ifdef ESP32_CAM_MASTER
#define MAX_SLAVES 10
char slave_hostnames[MAX_SLAVES][64];
int slave_ports[MAX_SLAVES];
int slave_count = 0;

/* search task for available slave devices */
void discover_slave_devices(void *pvParameters)
{
    mdns_result_t *results = NULL;

    /* Browse for HTTP services */
    browse_http_services(&results);

    mdns_result_t *r = results;

    while (r && slave_count < MAX_SLAVES) {

        if (r->hostname) {
            /*
             * Expected pattern:
             * dual-esp32cam-slave-xxxxxx.local
             */
            const char *pattern = "dual-esp32cam-slave-";
            size_t pattern_len = strlen(pattern);

            if (strncmp(r->hostname, pattern, pattern_len) == 0) {

                const char *suffix = r->hostname + pattern_len;
                char mac_suffix[7] = {0};

                /* Extract last 6 hex chars */
                if (strlen(suffix) >= 6) {
                    strncpy(mac_suffix, suffix, 6);

                    /* Validate hex */
                    bool valid = true;
                    for (int i = 0; i < 6; i++) {
                        if (!isxdigit((unsigned char)mac_suffix[i])) {
                            valid = false;
                            break;
                        }
                    }

                    if (valid) {
                        ESP_LOGI(TAG,
                                 "Found slave: %s (MAC suffix: %s)",
                                 r->hostname, mac_suffix);

                        /* Store hostname (or notify main task) */
                        strncpy(slave_hostnames[slave_count],
                                r->hostname,
                                sizeof(slave_hostnames[0]) - 1);
                        slave_ports[slave_count] = r->port;
                        slave_count++;
                    }
                }
            }
        }

        r = r->next;
    }

    /* Free mDNS results */
    if (results) {
        mdns_query_results_free(results);
    }

    ESP_LOGI(TAG, "Discovery complete. Slaves found: %d", slave_count);

    vTaskDelete(NULL);
}
#endif