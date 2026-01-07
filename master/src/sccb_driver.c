#include "sccb_driver.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/task.h"

static const char *TAG = "SCCB";

// SCCB Configuration
#define I2C_MASTER_SCL_IO          21      // GPIO number for I2C master clock
#define I2C_MASTER_SDA_IO          22      // GPIO number for I2C master data
#define I2C_MASTER_NUM             I2C_NUM_0  // I2C port number for master dev
#define I2C_MASTER_FREQ_HZ         400000  // I2C master clock frequency (max 400kHz)
#define I2C_MASTER_TX_BUF_DISABLE  0       // I2C master doesn't need buffer
#define I2C_MASTER_RX_BUF_DISABLE  0       // I2C master doesn't need buffer

#define WRITE_BIT                  I2C_MASTER_WRITE
#define READ_BIT                   I2C_MASTER_READ
#define ACK_CHECK_EN               0x1
#define ACK_CHECK_DIS              0x0
#define ACK_VAL                    0x0
#define NACK_VAL                   0x1

// Initialize SCCB interface
esp_err_t sccb_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    
    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_param_config failed: %s", esp_err_to_name(err));
        return err;
    }
    
    err = i2c_driver_install(I2C_MASTER_NUM, conf.mode,
                            I2C_MASTER_RX_BUF_DISABLE, 
                            I2C_MASTER_TX_BUF_DISABLE, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_driver_install failed: %s", esp_err_to_name(err));
    }
    
    ESP_LOGI(TAG, "SCCB initialized at %d Hz", I2C_MASTER_FREQ_HZ);
    return err;
}

// Write single byte to SCCB
bool sccb_write_byte(uint8_t reg_addr, uint8_t value) {
    esp_err_t ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (OV2640_ADDR_WRITE << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg_addr, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, value, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "sccb_write_byte failed at 0x%02X: %s", 
                reg_addr, esp_err_to_name(ret));
        return false;
    }
    
    return true;
}

// Read single byte from SCCB
bool sccb_read_byte(uint8_t reg_addr, uint8_t *value) {
    esp_err_t ret;
    i2c_cmd_handle_t cmd;
    
    // Write register address
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (OV2640_ADDR_WRITE << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg_addr, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "sccb_read_byte (write phase) failed at 0x%02X: %s",
                reg_addr, esp_err_to_name(ret));
        return false;
    }
    
    // Read value
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (OV2640_ADDR_READ << 1) | READ_BIT, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, value, NACK_VAL);
    i2c_master_stop(cmd);
    
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "sccb_read_byte (read phase) failed at 0x%02X: %s",
                reg_addr, esp_err_to_name(ret));
        return false;
    }
    
    return true;
}

// Write multiple bytes in burst mode
bool sccb_write_burst(uint8_t start_reg, uint8_t *data, size_t len) {
    if (len == 0 || data == NULL) {
        return false;
    }
    
    esp_err_t ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (OV2640_ADDR_WRITE << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, start_reg, ACK_CHECK_EN);
    
    for (size_t i = 0; i < len; i++) {
        i2c_master_write_byte(cmd, data[i], ACK_CHECK_EN);
    }
    
    i2c_master_stop(cmd);
    
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "sccb_write_burst failed at 0x%02X (len=%d): %s",
                start_reg, len, esp_err_to_name(ret));
        return false;
    }
    
    return true;
}

// Read multiple bytes in burst mode
bool sccb_read_burst(uint8_t start_reg, uint8_t *buffer, size_t len) {
    if (len == 0 || buffer == NULL) {
        return false;
    }
    
    // First write the starting register address
    if (!sccb_write_byte(start_reg, 0x00)) {
        return false;
    }
    
    // Then read multiple bytes
    esp_err_t ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (OV2640_ADDR_READ << 1) | READ_BIT, ACK_CHECK_EN);
    
    // Read all bytes except the last with ACK
    for (size_t i = 0; i < len - 1; i++) {
        i2c_master_read_byte(cmd, &buffer[i], ACK_VAL);
    }
    
    // Read last byte with NACK
    i2c_master_read_byte(cmd, &buffer[len - 1], NACK_VAL);
    i2c_master_stop(cmd);
    
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "sccb_read_burst failed at 0x%02X (len=%d): %s",
                start_reg, len, esp_err_to_name(ret));
        return false;
    }
    
    return true;
}

// Test SCCB communication
bool sccb_test_connection(void) {
    uint8_t pidh, pidl;
    
    // Switch to sensor bank and read product ID
    if (!sccb_write_byte(REG_BANK_SELECT, REG_BANK_SENSOR)) {
        return false;
    }
    
    if (!sccb_read_byte(0x0A, &pidh) || !sccb_read_byte(0x0B, &pidl)) {
        return false;
    }
    
    ESP_LOGI(TAG, "OV2640 Product ID: 0x%02X%02X", pidh, pidl);
    
    // Check if it's OV2640 (should be 0x26)
    if (pidh != 0x26) {
        ESP_LOGE(TAG, "Invalid product ID, expected 0x26, got 0x%02X", pidh);
        return false;
    }
    
    return true;
}