#include "ov2640_control.h"
#include "ov2640_regs.h"
#include "sccb_driver.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "OV2640";

// Current register bank
static ov2640_bank_t current_bank = REG_BANK_DSP;

// Set register bank
bool ov2640_set_bank(ov2640_bank_t bank) {
    if (current_bank == bank) {
        return true;
    }
    
    if (!sccb_write_byte(REG_BANK_SELECT, bank)) {
        ESP_LOGE(TAG, "Failed to set bank to %d", bank);
        return false;
    }
    
    current_bank = bank;
    return true;
}

// Read register from specified bank
bool ov2640_read_reg(uint8_t reg_addr, uint8_t *value, ov2640_bank_t bank) {
    if (!ov2640_set_bank(bank)) {
        return false;
    }
    
    return sccb_read_byte(reg_addr, value);
}

// Write register to specified bank
bool ov2640_write_reg(uint8_t reg_addr, uint8_t value, ov2640_bank_t bank) {
    if (!ov2640_set_bank(bank)) {
        return false;
    }
    
    return sccb_write_byte(reg_addr, value);
}

// Modify specific bits in register
bool ov2640_modify_reg(uint8_t reg_addr, uint8_t mask, uint8_t value, ov2640_bank_t bank) {
    uint8_t current;
    
    if (!ov2640_read_reg(reg_addr, &current, bank)) {
        return false;
    }
    
    current = (current & ~mask) | (value & mask);
    
    return ov2640_write_reg(reg_addr, current, bank);
}

// Initialize OV2640 with default settings
bool ov2640_init_default(void) {
    ESP_LOGI(TAG, "Initializing OV2640 with default settings");
    
    // Reset all registers
    if (!ov2640_write_reg(0x12, 0x80, REG_BANK_SENSOR)) {
        ESP_LOGE(TAG, "Failed to reset sensor");
        return false;
    }
    
    vTaskDelay(100 / portTICK_PERIOD_MS);
    
    // Load default register settings
    const ov2640_reg_t default_settings[] = {
        // Sensor Bank
        {REG_BANK_SENSOR, 0xFF, 0x01},
        {REG_BANK_SENSOR, 0x12, 0x80}, // Reset
        {REG_BANK_SENSOR, 0x2C, 0xFF},
        {REG_BANK_SENSOR, 0x2E, 0xDF},
        {REG_BANK_SENSOR, 0xFF, 0x00},
        {REG_BANK_SENSOR, 0x2C, 0xFF},
        {REG_BANK_SENSOR, 0x2E, 0xDF},
        
        // DSP Bank - UXGA settings
        {REG_BANK_DSP, 0xFF, 0x00},
        {REG_BANK_DSP, 0x2C, 0xFF},
        {REG_BANK_DSP, 0x2E, 0xDF},
        {REG_BANK_DSP, 0xFF, 0x01},
        {REG_BANK_DSP, 0x3C, 0x32},
        {REG_BANK_DSP, 0x11, 0x00},
        {REG_BANK_DSP, 0x09, 0x02},
        {REG_BANK_DSP, 0x04, 0x28},
        {REG_BANK_DSP, 0x13, 0xE5},
        {REG_BANK_DSP, 0x14, 0x48},
        {REG_BANK_DSP, 0x2C, 0x0C},
        {REG_BANK_DSP, 0x33, 0x78},
        {REG_BANK_DSP, 0x3A, 0x33},
        {REG_BANK_DSP, 0x3B, 0xFB},
        {REG_BANK_DSP, 0x3E, 0x00},
        {REG_BANK_DSP, 0x43, 0x11},
        {REG_BANK_DSP, 0x16, 0x10},
        {REG_BANK_DSP, 0x39, 0x02},
        {REG_BANK_DSP, 0x35, 0x88},
        {REG_BANK_DSP, 0x22, 0x0A},
        {REG_BANK_DSP, 0x37, 0x40},
        {REG_BANK_DSP, 0x23, 0x00},
        {REG_BANK_DSP, 0x34, 0xA0},
        {REG_BANK_DSP, 0x06, 0x02},
        {REG_BANK_DSP, 0x07, 0xC0},
        {REG_BANK_DSP, 0x0D, 0xB7},
        {REG_BANK_DSP, 0x0E, 0x01},
        {REG_BANK_DSP, 0x4C, 0x00},
        {REG_BANK_DSP, 0x4A, 0x81},
        {REG_BANK_DSP, 0x21, 0x99},
        {REG_BANK_DSP, 0x24, 0x40},
        {REG_BANK_DSP, 0x25, 0x38},
        {REG_BANK_DSP, 0x26, 0x82},
        {REG_BANK_DSP, 0x5C, 0x00},
        {REG_BANK_DSP, 0x63, 0x00},
        {REG_BANK_DSP, 0x46, 0x00},
        {REG_BANK_DSP, 0x0C, 0x3C},
        {REG_BANK_DSP, 0x61, 0x70},
        {REG_BANK_DSP, 0x62, 0x80},
        {REG_BANK_DSP, 0x7C, 0x05},
        {REG_BANK_DSP, 0x20, 0x80},
        {REG_BANK_DSP, 0x28, 0x30},
        {REG_BANK_DSP, 0x6C, 0x00},
        {REG_BANK_DSP, 0x6D, 0x80},
        {REG_BANK_DSP, 0x6E, 0x00},
        {REG_BANK_DSP, 0x70, 0x02},
        {REG_BANK_DSP, 0x71, 0x94},
        {REG_BANK_DSP, 0x73, 0xC1},
        {REG_BANK_DSP, 0x3D, 0x34},
        {REG_BANK_DSP, 0x5A, 0x57},
        {REG_BANK_DSP, 0x12, 0x00},
        {REG_BANK_DSP, 0x17, 0x11},
        {REG_BANK_DSP, 0x18, 0x75},
        {REG_BANK_DSP, 0x19, 0x01},
        {REG_BANK_DSP, 0x1A, 0x97},
        {REG_BANK_DSP, 0x32, 0x36},
        {REG_BANK_DSP, 0x03, 0x0F},
        {REG_BANK_DSP, 0x37, 0x40},
        {REG_BANK_DSP, 0x4F, 0xBB},
        {REG_BANK_DSP, 0x50, 0x9C},
        {REG_BANK_DSP, 0x5A, 0x57},
        {REG_BANK_DSP, 0x6D, 0x80},
        {REG_BANK_DSP, 0x6D, 0x38},
        {REG_BANK_DSP, 0x39, 0x02},
        {REG_BANK_DSP, 0x35, 0x88},
        {REG_BANK_DSP, 0x22, 0x0A},
        {REG_BANK_DSP, 0x37, 0x40},
        {REG_BANK_DSP, 0x34, 0xA0},
        {REG_BANK_DSP, 0x06, 0x02},
        {REG_BANK_DSP, 0x0D, 0xB7},
        {REG_BANK_DSP, 0x0E, 0x01},
        {REG_BANK_DSP, 0xFF, 0x00},
        {REG_BANK_DSP, 0xE0, 0x04},
        {REG_BANK_DSP, 0xC0, 0xC8},
        {REG_BANK_DSP, 0xC1, 0x96},
        {REG_BANK_DSP, 0x86, 0x3D},
        {REG_BANK_DSP, 0x50, 0x00},
        {REG_BANK_DSP, 0x51, 0x90},
        {REG_BANK_DSP, 0x52, 0x2C},
        {REG_BANK_DSP, 0x53, 0x00},
        {REG_BANK_DSP, 0x54, 0x00},
        {REG_BANK_DSP, 0x55, 0x88},
        {REG_BANK_DSP, 0x57, 0x00},
        {REG_BANK_DSP, 0x5A, 0x90},
        {REG_BANK_DSP, 0x5B, 0x2C},
        {REG_BANK_DSP, 0x5C, 0x05},
        {REG_BANK_DSP, 0xD3, 0x02},
        {REG_BANK_DSP, 0xE0, 0x00},
    };
    
    // Apply all settings
    for (size_t i = 0; i < sizeof(default_settings)/sizeof(default_settings[0]); i++) {
        if (!ov2640_write_reg(default_settings[i].addr, 
                              default_settings[i].value,
                              default_settings[i].bank)) {
            ESP_LOGE(TAG, "Failed to write default register 0x%02X", 
                    default_settings[i].addr);
            return false;
        }
    }
    
    // Set to RAW output format
    if (!ov2640_set_output_format(OUTPUT_FORMAT_RAW)) {
        ESP_LOGE(TAG, "Failed to set RAW output format");
        return false;
    }
    
    ESP_LOGI(TAG, "OV2640 initialized successfully");
    return true;
}

// Set resolution
bool ov2640_set_resolution(resolution_t res) {
    uint8_t com7_value;
    
    switch (res) {
        case RES_UXGA:  // 1600x1200
            com7_value = 0x00;  // UXGA mode
            break;
        case RES_SVGA:  // 800x600
            com7_value = 0x40;  // SVGA mode
            break;
        case RES_CIF:   // 400x296
            com7_value = 0x20;  // CIF mode
            break;
        default:
            ESP_LOGE(TAG, "Invalid resolution");
            return false;
    }
    
    // Set COM7 register in DSP bank
    if (!ov2640_write_reg(0x12, com7_value, REG_BANK_DSP)) {
        return false;
    }
    
    // Update window settings based on resolution
    return ov2640_set_window(0, 0, 
                            res == RES_UXGA ? 1600 : (res == RES_SVGA ? 800 : 400),
                            res == RES_UXGA ? 1200 : (res == RES_SVGA ? 600 : 296));
}

// Set output format
bool ov2640_set_output_format(output_format_t fmt) {
    uint8_t image_mode;
    
    switch (fmt) {
        case OUTPUT_FORMAT_RAW:
            image_mode = 0x01;  // RAW format
            break;
        case OUTPUT_FORMAT_YUV:
            image_mode = 0x02;  // YUV format
            break;
        case OUTPUT_FORMAT_RGB:
            image_mode = 0x04;  // RGB format
            break;
        default:
            return false;
    }
    
    // Set IMAGE_MODE register
    return ov2640_write_reg(0xDA, image_mode, REG_BANK_DSP);
}

// Set exposure value
bool ov2640_set_exposure(uint16_t exposure_lines) {
    if (exposure_lines > 0xFFF) {
        exposure_lines = 0xFFF;
    }
    
    // Write exposure LSB and MSB
    if (!ov2640_write_reg(0x10, exposure_lines & 0xFF, REG_BANK_SENSOR)) {
        return false;
    }
    
    return ov2640_write_reg(0x11, (exposure_lines >> 8) & 0x0F, REG_BANK_SENSOR);
}

// Set gain value (1x to 128x)
bool ov2640_set_gain(uint8_t gain_value) {
    if (gain_value > 0x7F) {
        gain_value = 0x7F;  // Maximum 128x gain
    }
    
    // Set AGC gain
    return ov2640_write_reg(0x00, gain_value, REG_BANK_SENSOR);
}

// Set white balance mode
bool ov2640_set_white_balance(bool auto_wb) {
    uint8_t com8;
    
    if (!ov2640_read_reg(0x13, &com8, REG_BANK_SENSOR)) {
        return false;
    }
    
    if (auto_wb) {
        com8 |= 0x02;  // Enable AWB
    } else {
        com8 &= ~0x02; // Disable AWB
    }
    
    return ov2640_write_reg(0x13, com8, REG_BANK_SENSOR);
}

// Set window (region of interest)
bool ov2640_set_window(uint16_t x_start, uint16_t y_start, 
                      uint16_t width, uint16_t height) {
    // Set window start position
    if (!ov2640_write_reg(0x17, x_start >> 8, REG_BANK_DSP) ||
        !ov2640_write_reg(0x18, x_start & 0xFF, REG_BANK_DSP) ||
        !ov2640_write_reg(0x19, y_start >> 8, REG_BANK_DSP) ||
        !ov2640_write_reg(0x1A, y_start & 0xFF, REG_BANK_DSP)) {
        return false;
    }
    
    // Set window size
    if (!ov2640_write_reg(0x1B, width >> 8, REG_BANK_DSP) ||
        !ov2640_write_reg(0x1C, width & 0xFF, REG_BANK_DSP) ||
        !ov2640_write_reg(0x1D, height >> 8, REG_BANK_DSP) ||
        !ov2640_write_reg(0x1E, height & 0xFF, REG_BANK_DSP)) {
        return false;
    }
    
    return true;
}

// Dump all registers for debugging
void ov2640_dump_registers(void) {
    uint8_t value;
    
    ESP_LOGI(TAG, "Dumping OV2640 Registers:");
    ESP_LOGI(TAG, "=========================");
    
    // Dump DSP bank
    ov2640_set_bank(REG_BANK_DSP);
    ESP_LOGI(TAG, "DSP Bank Registers:");
    for (int i = 0; i < 256; i++) {
        if (sccb_read_byte(i, &value)) {
            ESP_LOGI(TAG, "  0x%02X: 0x%02X", i, value);
        }
        vTaskDelay(1);
    }
    
    // Dump Sensor bank
    ov2640_set_bank(REG_BANK_SENSOR);
    ESP_LOGI(TAG, "Sensor Bank Registers:");
    for (int i = 0; i < 256; i++) {
        if (sccb_read_byte(i, &value)) {
            ESP_LOGI(TAG, "  0x%02X: 0x%02X", i, value);
        }
        vTaskDelay(1);
    }
}