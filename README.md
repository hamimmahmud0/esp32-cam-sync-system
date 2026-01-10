# dual-esp32cam-sync (ESP-IDF v5.5)

Single firmware that can run as MASTER or SLAVE (set via `idf.py menuconfig`).

## Features
- Independent MJPEG streaming on each board: `GET /stream`
- Web UI served from SD card (`/sdcard/www`)
- OV2640 SCCB register control APIs (single/range/dump)
- Presets saved/loaded from SD card (`/sdcard/reg_profiles`)
- Synchronized capture:
  - MASTER arms SLAVE via HTTP (mDNS), then pulses TRIGGER GPIO
  - Both boards stop stream -> re-init camera for capture -> capture -> save to SD -> return to stream

## Hardware: AI-Thinker ESP32-CAM + SDIO 4-bit
**Trigger GPIO must be SDIO-safe.** Default is GPIO16.

Wiring:
- Master GPIO16 -> Slave GPIO16
- GND common

## Dependencies
This project expects the `esp32-camera` component in:
`components/esp32-camera`

You can add it as a git submodule or copy it there.

## Build (ESP32 target)
```bash
idf.py set-target esp32
idf.py menuconfig   # set role MASTER/SLAVE and Wi-Fi SSID/PASS
idf.py build flash monitor
```

## SD card layout
Copy the contents of `sdcard/www/` to the SD card at `/www/`:
- `/sdcard/www/index.html`
- `/sdcard/www/registers.html`
- `/sdcard/www/app.js`
- `/sdcard/www/registers.js`
- `/sdcard/www/app.css`

The firmware will create `/sdcard/captures` and `/sdcard/reg_profiles` if missing.

