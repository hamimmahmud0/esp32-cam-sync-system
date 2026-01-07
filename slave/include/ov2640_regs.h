#ifndef OV2640_CONTROL_H
#define OV2640_CONTROL_H

#include <stdint.h>
#include <stdbool.h>

// Register banks
typedef enum {
    REG_BANK_DSP = 0x00,
    REG_BANK_SENSOR = 0x01
} ov2640_bank_t;

// Resolution types
typedef enum {
    RES_UXGA = 0,   // 1600x1200
    RES_SVGA = 1,   // 800x600
    RES_CIF = 2     // 400x296
} resolution_t;

// Output formats
typedef enum {
    OUTPUT_FORMAT_RAW = 0,
    OUTPUT_FORMAT_YUV = 1,
    OUTPUT_FORMAT_RGB = 2
} output_format_t;

// Register structure
typedef struct {
    ov2640_bank_t bank;
    uint8_t addr;
    uint8_t value;
} ov2640_reg_t;

// Camera state
typedef struct {
    bool initialized;
    resolution_t resolution;
    output_format_t format;
    uint8_t frame_rate;
    uint8_t exposure;
    uint8_t gain;
    bool auto_wb;
    char ip_address[16];
    bool connected;
    uint8_t register_cache[2][256];  // Cache for both banks
} camera_state_t;

// Function prototypes
bool ov2640_set_bank(ov2640_bank_t bank);
bool ov2640_read_reg(uint8_t reg_addr, uint8_t *value, ov2640_bank_t bank);
bool ov2640_write_reg(uint8_t reg_addr, uint8_t value, ov2640_bank_t bank);
bool ov2640_modify_reg(uint8_t reg_addr, uint8_t mask, uint8_t value, ov2640_bank_t bank);
bool ov2640_init_default(void);
bool ov2640_set_resolution(resolution_t res);
bool ov2640_set_output_format(output_format_t fmt);
bool ov2640_set_exposure(uint16_t exposure_lines);
bool ov2640_set_gain(uint8_t gain_value);
bool ov2640_set_white_balance(bool auto_wb);
bool ov2640_set_window(uint16_t x_start, uint16_t y_start, uint16_t width, uint16_t height);
void ov2640_dump_registers(void);

// Camera initialization
esp_err_t camera_init(camera_state_t *camera, bool is_master);

#endif // OV2640_CONTROL_H