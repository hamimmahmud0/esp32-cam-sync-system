#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "esp_log.h"    

static const char *TAG = "SDMMC";

#define MOUNT_POINT "/sdcard"

esp_err_t init_sdcard(void)
{
    esp_err_t ret;

    /* FAT mount configuration */
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,   // good for large writes
    };

    sdmmc_card_t *card;

    ESP_LOGI(TAG, "Initializing SD card (SDMMC 4-bit)");

    /* SDMMC host configuration */
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;   // 50 MHz SDR
    host.flags &= ~SDMMC_HOST_FLAG_DDR;          // ensure SDR

    /* SDMMC slot configuration (ESP32-CAM fixed pins) */
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 4;

    /* ESP32-CAM SDMMC pin mapping */
    slot_config.clk = 14;
    slot_config.cmd = 15;
    slot_config.d0  = 2;
    slot_config.d1  = 4;
    slot_config.d2  = 12;
    slot_config.d3  = 13;

    /* Enable internal pullups (external 10k still REQUIRED) */
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    /* Mount filesystem */
    ret = esp_vfs_fat_sdmmc_mount(
        MOUNT_POINT,
        &host,
        &slot_config,
        &mount_config,
        &card
    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card (%s)", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SD card mounted successfully");
    sdmmc_card_print_info(stdout, card);
}
