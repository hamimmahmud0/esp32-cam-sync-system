#!/usr/bin/env python3
import os
import sys
import shutil
import subprocess
from pathlib import Path

# Project configuration
PROJECT_DIR = Path(__file__).parent
MASTER_DIR = PROJECT_DIR / "master"
SLAVE_DIR = PROJECT_DIR / "slave"
SHARED_DIR = PROJECT_DIR / "shared"
BUILD_DIR = PROJECT_DIR / "build"

def copy_shared_files(target_dir, is_master=True):
    """Copy shared files to target directory"""
    shared_include = SHARED_DIR / "include"
    target_include = target_dir / "include"
    
    # Create include directory if it doesn't exist
    target_include.mkdir(parents=True, exist_ok=True)
    
    # Copy shared header files
    for header_file in shared_include.glob("*.h"):
        shutil.copy2(header_file, target_include / header_file.name)
    
    # Copy shared components if they exist
    shared_components = SHARED_DIR / "components"
    if shared_components.exists():
        target_components = target_dir / "components"
        target_components.mkdir(parents=True, exist_ok=True)
        for component in shared_components.iterdir():
            if component.is_dir():
                shutil.copytree(component, target_components / component.name, 
                              dirs_exist_ok=True)

def create_component_mk(target_dir):
    """Create component.mk file"""
    component_mk = target_dir / "component.mk"
    
    with open(component_mk, 'w') as f:
        f.write("""# Component Makefile for ESP32-CAM System

COMPONENT_ADD_INCLUDEDIRS := include

COMPONENT_SRCDIRS := src

ifdef CONFIG_IS_MASTER
    COMPONENT_ADD_INCLUDEDIRS += include/master
    COMPONENT_SRCDIRS += src/master
    CFLAGS += -DIS_MASTER=1
else
    COMPONENT_ADD_INCLUDEDIRS += include/slave
    COMPONENT_SRCDIRS += src/slave
    CFLAGS += -DIS_MASTER=0
endif

# Enable all warnings
CFLAGS += -Wall -Werror -Wno-error=unused-variable
""")

def create_cmakelists(target_dir, project_name):
    """Create CMakeLists.txt file"""
    cmakelists = target_dir / "CMakeLists.txt"
    
    with open(cmakelists, 'w') as f:
        f.write(f"""cmake_minimum_required(VERSION 3.16)
include($ENV{{IDF_PATH}}/tools/cmake/project.cmake)
project({project_name})

# Add components directory
set(EXTRA_COMPONENT_DIRS ${{CMAKE_CURRENT_SOURCE_DIR}}/components)

# Required components
set(COMPONENTS 
    app_update
    bootloader
    bt
    driver
    esp-tls
    esp32
    esp_adc_cal
    esp_common
    esp_eth
    esp_event
    esp_gdbstub
    esp_http_client
    esp_http_server
    esp_https_ota
    esp_https_server
    esp_local_ctrl
    esp_netif
    esp_ringbuf
    esp_rom
    esp_serial_slave_link
    esp_system
    esp_timer
    esp_websocket_client
    esp_wifi
    espcoredump
    esptool_py
    fatfs
    freertos
    hal
    heap
    json
    log
    lwip
    mbedtls
    mdns
    newlib
    nvs_flash
    openssl
    partition_table
    protobuf-c
    protocomm
    pthread
    sdmmc
    soc
    spi_flash
    tcp_transport
    tcpip_adapter
    ulp
    unity
    vfs
    wear_levelling
    wifi_provisioning
    xtensa
)

# Add main component
idf_component_register(SRCS "src/main.c"
                       INCLUDE_DIRS "include"
                       REQUIRES ${{COMPONENTS}})

# Set target properties
set_target_properties(${{PROJECT_NAME}}.elf PROPERTIES
    LINK_FLAGS "-Wl,--gc-sections"
)

# Set partition table
set(PARTITION_TABLE_PATH "partitions.csv")

# Additional settings
set(EXTRA_CFLAGS "-Wall -Werror -Wno-error=unused-variable")

# SD Card support
set(SDCARD_SUPPORT 1)

# Camera support
set(CAMERA_SUPPORT 1)
""")

def create_partitions_csv(target_dir):
    """Create partitions.csv file"""
    partitions = target_dir / "partitions.csv"
    
    with open(partitions, 'w') as f:
        f.write("""# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x140000,
app1,     app,  ota_1,   0x150000,0x140000,
spiffs,   data, spiffs,  0x290000,0x160000,
coredump, data, coredump,0x3F0000,0x10000,
""")

def create_sdkconfig(target_dir, is_master=True):
    """Create sdkconfig.defaults file"""
    sdkconfig = target_dir / "sdkconfig.defaults"
    
    with open(sdkconfig, 'w') as f:
        f.write(f"""# ESP32-CAM Configuration
CONFIG_ESP32_DEFAULT_CPU_FREQ_240=y
CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ=240

# FreeRTOS
CONFIG_FREERTOS_UNICORE=n
CONFIG_FREERTOS_HZ=1000
CONFIG_FREERTOS_ASSERT_ON_UNTESTED_FUNCTION=y

# WiFi
CONFIG_ESP32_WIFI_STATIC_RX_BUFFER_NUM=16
CONFIG_ESP32_WIFI_DYNAMIC_RX_BUFFER_NUM=32
CONFIG_ESP32_WIFI_TX_BUFFER_TYPE=1
CONFIG_ESP32_WIFI_STATIC_TX_BUFFER_NUM=16
CONFIG_ESP32_WIFI_DYNAMIC_TX_BUFFER_NUM=32
CONFIG_ESP32_WIFI_AMPDU_TX_ENABLED=y
CONFIG_ESP32_WIFI_TX_BA_WIN=6
CONFIG_ESP32_WIFI_AMPDU_RX_ENABLED=y
CONFIG_ESP32_WIFI_RX_BA_WIN=6
CONFIG_ESP32_WIFI_NVS_ENABLED=y
CONFIG_ESP32_WIFI_TASK_PINNED_TO_CORE_0=y

# Camera
CONFIG_CAMERA_MODULE_WROVER_KIT=y
CONFIG_CAMERA_MODULE_SUPPORT_OV2640=y
CONFIG_OV2640_SUPPORT=y
CONFIG_SCCB_HARDWARE_I2C_PORT1=y

# SD Card
CONFIG_SDMMC_HOST_SUPPORTED=y
CONFIG_SDMMC_DEFAULT_BUS_WIDTH=4
CONFIG_SDSPI_HOST_DEFAULT_INPUT_DELAY_NS=0

# File System
CONFIG_FATFS_CODEPAGE=437
CONFIG_FATFS_LFN_NONE=y
CONFIG_FATFS_MAX_LFN=255
CONFIG_FATFS_API_ENCODING_ANSI_OEM=y
CONFIG_FATFS_PER_FILE_CACHE=y

# HTTP Server
CONFIG_HTTPD_MAX_REQ_HDR_LEN=512
CONFIG_HTTPD_MAX_URI_LEN=512
CONFIG_HTTPD_ERR_RESP_NO_DELAY=y
CONFIG_HTTPD_PURGE_BUF_LEN=32

# Logging
CONFIG_LOG_DEFAULT_LEVEL_INFO=y
CONFIG_LOG_TIMESTAMP_SOURCE_RTOS=y

# Core dump
CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y

# Partition Table
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_PARTITION_TABLE_OFFSET=0x8000

# SPIFFS
CONFIG_SPIFFS_USE_MAGIC=y
CONFIG_SPIFFS_USE_MAGIC_LENGTH=y
CONFIG_SPIFFS_PAGE_SIZE=256
CONFIG_SPIFFS_OBJ_NAME_LEN=64

# Master/Slave configuration
CONFIG_IS_MASTER={"y" if is_master else "n"}
CONFIG_SYNC_GPIO_TRIGGER=y
CONFIG_SYNC_GPIO_TRIGGER_PIN=12
CONFIG_SYNC_GPIO_RECEIVE_PIN=13

# Memory configuration
CONFIG_ESP32_WIFI_TX_BUFFER_TYPE=1
CONFIG_ESP32_WIFI_DYNAMIC_TX_BUFFER_NUM=32
CONFIG_ESP32_WIFI_STATIC_RX_BUFFER_NUM=16
CONFIG_ESP32_WIFI_DYNAMIC_RX_BUFFER_NUM=32

# Optimizations
CONFIG_COMPILER_OPTIMIZATION_PERF=y
CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_ENABLE=y
""")

def build_project(project_dir, project_name, is_master=True):
    """Build the ESP-IDF project"""
    print(f"Building {project_name}...")
    
    # Set IDF_PATH if not set
    if "IDF_PATH" not in os.environ:
        print("ERROR: IDF_PATH not set. Please source esp-idf export.sh first.")
        sys.exit(1)
    
    # Change to project directory
    os.chdir(project_dir)
    
    # Clean previous build
    if (project_dir / "build").exists():
        shutil.rmtree(project_dir / "build")
    
    # Configure project
    cmd = ["idf.py", "set-target", "esp32"]
    subprocess.run(cmd, check=True)
    
    # Build project
    cmd = ["idf.py", "build"]
    result = subprocess.run(cmd)
    
    if result.returncode != 0:
        print(f"Build failed for {project_name}")
        return False
    
    print(f"Build successful for {project_name}")
    return True

def package_firmware(project_dir, project_name):
    """Package the firmware for flashing"""
    firmware_dir = project_dir / "firmware"
    firmware_dir.mkdir(exist_ok=True)
    
    # Copy firmware files
    files_to_copy = [
        ("build/bootloader/bootloader.bin", "bootloader.bin"),
        ("build/partition_table/partition-table.bin", "partition_table.bin"),
        ("build/ota_data_initial.bin", "ota_data_initial.bin"),
        ("build/{project_name}.bin", "{project_name}.bin"),
    ]
    
    for src, dst in files_to_copy:
        src_path = project_dir / src.format(project_name=project_name)
        dst_path = firmware_dir / dst.format(project_name=project_name)
        
        if src_path.exists():
            shutil.copy2(src_path, dst_path)
    
    # Create flash command file
    flash_cmd = firmware_dir / "flash.sh"
    with open(flash_cmd, 'w') as f:
        f.write(f"""#!/bin/bash
# Flash command for {project_name}
# Usage: ./flash.sh <serial_port>

PORT=$1
if [ -z "$PORT" ]; then
    PORT=/dev/ttyUSB0
fi

echo "Flashing {project_name} to $PORT"

esptool.py --chip esp32 --port $PORT --baud 921600 \\
    --before default_reset --after hard_reset write_flash -z \\
    --flash_mode dio --flash_freq 40m --flash_size 4MB \\
    0x1000 bootloader.bin \\
    0x8000 partition_table.bin \\
    0xd000 ota_data_initial.bin \\
    0x10000 {project_name}.bin

echo "Flashing complete!"
""")
    
    # Make flash script executable
    flash_cmd.chmod(0o755)
    
    print(f"Firmware packaged in {firmware_dir}")

def main():
    """Main build function"""
    print("Dual ESP32-CAM System Build Tool")
    print("=" * 40)
    
    # Create build directory
    BUILD_DIR.mkdir(exist_ok=True)
    
    # Build master
    print("\nBuilding Master Device...")
    master_build_dir = BUILD_DIR / "master"
    master_build_dir.mkdir(exist_ok=True)
    
    # Copy master files
    shutil.copytree(MASTER_DIR / "src", master_build_dir / "src", dirs_exist_ok=True)
    shutil.copytree(MASTER_DIR / "include", master_build_dir / "include", dirs_exist_ok=True)
    
    # Copy shared files
    copy_shared_files(master_build_dir, is_master=True)
    
    # Create build files
    create_component_mk(master_build_dir)
    create_cmakelists(master_build_dir, "master")
    create_partitions_csv(master_build_dir)
    create_sdkconfig(master_build_dir, is_master=True)
    
    # Copy web files to spiffs image directory
    web_dir = master_build_dir / "data" / "www"
    web_dir.mkdir(parents=True, exist_ok=True)
    shutil.copytree(MASTER_DIR / "data" / "www", web_dir, dirs_exist_ok=True)
    
    # Build master project
    if build_project(master_build_dir, "master", is_master=True):
        package_firmware(master_build_dir, "master")
    
    # Build slave
    print("\nBuilding Slave Device...")
    slave_build_dir = BUILD_DIR / "slave"
    slave_build_dir.mkdir(exist_ok=True)
    
    # Copy slave files
    shutil.copytree(SLAVE_DIR / "src", slave_build_dir / "src", dirs_exist_ok=True)
    shutil.copytree(SLAVE_DIR / "include", slave_build_dir / "include", dirs_exist_ok=True)
    
    # Copy shared files
    copy_shared_files(slave_build_dir, is_master=False)
    
    # Create build files
    create_component_mk(slave_build_dir)
    create_cmakelists(slave_build_dir, "slave")
    create_partitions_csv(slave_build_dir)
    create_sdkconfig(slave_build_dir, is_master=False)
    
    # Build slave project
    if build_project(slave_build_dir, "slave", is_master=False):
        package_firmware(slave_build_dir, "slave")
    
    print("\nBuild process complete!")
    print(f"Master firmware: {BUILD_DIR / 'master' / 'firmware'}")
    print(f"Slave firmware: {BUILD_DIR / 'slave' / 'firmware'}")
    
    # Create README
    readme = BUILD_DIR / "README.md"
    with open(readme, 'w') as f:
        f.write("""# Dual ESP32-CAM System Firmware

## Directory Structure
- master/firmware/ - Master device firmware
- slave/firmware/ - Slave device firmware

## Flashing Instructions

### Master Device
1. Connect ESP32-CAM to USB
2. Enter bootloader mode (hold BOOT, press EN, release EN, release BOOT)
3. Run: `cd master/firmware && ./flash.sh /dev/ttyUSB0`

### Slave Device
1. Connect second ESP32-CAM to USB
2. Enter bootloader mode
3. Run: `cd slave/firmware && ./flash.sh /dev/ttyUSB1`

## Configuration
1. Power on Master device first
2. Connect to Wi-Fi network: "ESP32-CAM-Master"
3. Open browser to: http://192.168.4.1
4. Configure slave IP address in web interface
5. Power on Slave device (it will connect automatically)

## Usage
- Access web interface at http://192.168.4.1
- Configure camera settings
- Perform synchronized captures
- Download files from SD card

## Troubleshooting
1. Ensure SD cards are formatted FAT32
2. Check Wi-Fi connection between devices
3. Verify GPIO connections for sync trigger
4. Check power supply (both devices need stable 5V)
""")

if __name__ == "__main__":
    main()