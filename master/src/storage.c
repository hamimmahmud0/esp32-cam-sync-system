#include "storage.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static const char *TAG = "STORAGE";

static sdmmc_card_t *card = NULL;
static bool mounted = false;

// Initialize SD card
esp_err_t storage_init(void) {
    ESP_LOGI(TAG, "Initializing SD card");
    
    // Mount configuration
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    
    // SDMMC host configuration
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
    
    // Slot configuration
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    
    // Enable pull-ups
    slot_config.width = 4;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    
    // Mount filesystem
    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, 
                                           &mount_config, &card);
    
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s)", 
                    esp_err_to_name(ret));
        }
        return ret;
    }
    
    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);
    
    // Create directory structure
    create_directory_structure();
    
    mounted = true;
    ESP_LOGI(TAG, "SD card mounted successfully");
    return ESP_OK;
}

// Create directory structure
void create_directory_structure(void) {
    const char *directories[] = {
        "/sdcard/captures",
        "/sdcard/config",
        "/sdcard/logs",
        "/sdcard/reg_profiles",
        NULL
    };
    
    for (int i = 0; directories[i] != NULL; i++) {
        struct stat st;
        if (stat(directories[i], &st) != 0) {
            if (mkdir(directories[i], 0755) != 0) {
                ESP_LOGW(TAG, "Failed to create directory: %s", directories[i]);
            } else {
                ESP_LOGI(TAG, "Created directory: %s", directories[i]);
            }
        }
    }
}

// Get storage information
storage_info_t storage_get_info(void) {
    storage_info_t info = {0};
    
    if (!mounted || !card) {
        return info;
    }
    
    FATFS *fs;
    DWORD free_clusters;
    
    // Get volume label
    FRESULT res = f_getfree("0:", &free_clusters, &fs);
    if (res == FR_OK) {
        uint32_t total_sectors = (fs->n_fatent - 2) * fs->csize;
        uint32_t free_sectors = free_clusters * fs->csize;
        
        info.total_kb = (total_sectors * card->csd.sector_size) / 1024;
        info.free_kb = (free_sectors * card->csd.sector_size) / 1024;
        info.used_kb = info.total_kb - info.free_kb;
        info.mounted = true;
    }
    
    return info;
}

// Check if storage is mounted
bool storage_is_mounted(void) {
    return mounted;
}

// Write data to file
bool storage_write_file(const char *path, const uint8_t *data, size_t len) {
    if (!mounted) {
        return false;
    }
    
    FILE *file = fopen(path, "wb");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open file: %s", path);
        return false;
    }
    
    size_t written = fwrite(data, 1, len, file);
    fclose(file);
    
    if (written != len) {
        ESP_LOGE(TAG, "Failed to write complete file: %s", path);
        return false;
    }
    
    ESP_LOGI(TAG, "Written %d bytes to %s", len, path);
    return true;
}

// Read data from file
bool storage_read_file(const char *path, uint8_t *buffer, size_t max_len, size_t *read_len) {
    if (!mounted) {
        return false;
    }
    
    FILE *file = fopen(path, "rb");
    if (!file) {
        return false;
    }
    
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size > max_len) {
        fclose(file);
        return false;
    }
    
    size_t read = fread(buffer, 1, file_size, file);
    fclose(file);
    
    if (read_len) {
        *read_len = read;
    }
    
    return read == file_size;
}

// List files in directory
int storage_list_files(const char *path, file_info_t *files, int max_files) {
    if (!mounted) {
        return 0;
    }
    
    DIR *dir = opendir(path);
    if (!dir) {
        return 0;
    }
    
    struct dirent *entry;
    int count = 0;
    
    while ((entry = readdir(dir)) != NULL && count < max_files) {
        if (entry->d_type == DT_REG) {  // Regular file
            char full_path[256];
            snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
            
            struct stat st;
            if (stat(full_path, &st) == 0) {
                strncpy(files[count].name, entry->d_name, sizeof(files[count].name) - 1);
                files[count].size = st.st_size;
                files[count].modified = st.st_mtime;
                count++;
            }
        }
    }
    
    closedir(dir);
    return count;
}

// Unmount storage
esp_err_t storage_unmount(void) {
    if (!mounted) {
        return ESP_OK;
    }
    
    esp_err_t ret = esp_vfs_fat_sdmmc_unmount();
    if (ret == ESP_OK) {
        mounted = false;
        card = NULL;
        ESP_LOGI(TAG, "SD card unmounted");
    }
    
    return ret;
}