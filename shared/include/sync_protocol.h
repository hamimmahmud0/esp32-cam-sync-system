#ifndef SYNC_PROTOCOL_H
#define SYNC_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

// Synchronization modes
typedef enum {
    MASTER_MODE,
    SLAVE_MODE
} sync_mode_t;

// Capture types
typedef enum {
    CAPTURE_SINGLE,
    CAPTURE_BURST,
    CAPTURE_VIDEO
} capture_type_t;

// Slave state
typedef struct {
    bool armed;
    capture_type_t capture_type;
    char filename[64];
    size_t file_size;
    uint32_t frame_count;
} slave_state_t;

// Register sync message
typedef struct {
    uint8_t bank;
    uint8_t start_reg;
    uint8_t count;
    uint8_t data[32];
} __attribute__((packed)) reg_sync_msg_t;

// Function prototypes
esp_err_t sync_init(sync_mode_t mode);
bool sync_registers_to_slave(camera_state_t *master, camera_state_t *slave);
bool capture_single_image(camera_state_t *master, camera_state_t *slave);
bool capture_burst(camera_state_t *master, camera_state_t *slave, uint32_t count);
bool capture_video(camera_state_t *master, camera_state_t *slave, uint32_t duration_ms);

// GPIO trigger control
void trigger_assert(void);
void trigger_deassert(void);
void trigger_pulse(uint32_t duration_us);

#endif // SYNC_PROTOCOL_H