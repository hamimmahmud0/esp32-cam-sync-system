#include "sdmmc_mount.h"
#include "app_config.h"
#include "esp_log.h"

#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"

#include <sys/stat.h>


static const char *TAG = "SDMMC";

static void mkdir_if_missing(const char *p) {
  struct stat st;
  if (stat(p, &st) == 0 && S_ISDIR(st.st_mode)) return;
  mkdir(p, 0775);
}

bool sdmmc_mount_and_prepare(void) {
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.max_freq_khz = 20000; // start safe; raise later if stable

  sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
  slot.width = 4;

  esp_vfs_fat_sdmmc_mount_config_t mcfg = {
    .format_if_mount_failed = false,
    .max_files = 10,
    .allocation_unit_size = 16 * 1024
  };

  sdmmc_card_t *card = NULL;
  esp_err_t err = esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &host, &slot, &mcfg, &card);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Mount failed: %s", esp_err_to_name(err));
    return false;
  }

  mkdir_if_missing(WWW_DIR);
  mkdir_if_missing(CAPTURES_DIR);
  mkdir_if_missing(REGPROFILES_DIR);

  ESP_LOGI(TAG, "Mounted SD at %s", SD_MOUNT_POINT);
  return true;
}
