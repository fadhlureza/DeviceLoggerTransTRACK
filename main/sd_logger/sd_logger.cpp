#include "sd_logger.h"
#include "constant.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

static const char mount_point[] = "/sdcard";
static sdmmc_card_t *card = NULL;
static FILE *log_file = NULL;
static int current_log_day = -1;
static bool bus_initialized = false;
static sdmmc_host_t host = SDSPI_HOST_DEFAULT();
static sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
static esp_vfs_fat_sdmmc_mount_config_t mount_config = {};

static SemaphoreHandle_t s_sd_mutex = NULL;

bool sd_lock(uint32_t timeout_ms) {
    if (s_sd_mutex == NULL) return false;
    return xSemaphoreTakeRecursive(s_sd_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void sd_unlock(void) {
    if (s_sd_mutex != NULL) {
        xSemaphoreGiveRecursive(s_sd_mutex);
    }
}

static void sd_prepare_configs() {
    host.slot = SPI2_HOST;

    mount_config.format_if_mount_failed = false;
    mount_config.max_files = 10;
    mount_config.allocation_unit_size = 16 * 1024;

    slot_config.gpio_cs = (gpio_num_t)SD_CS_PIN;
    slot_config.host_id = SPI2_HOST;
}

static void sd_bus_init_once() {
    if (bus_initialized)
        return;

    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = SD_MOSI_PIN;
    bus_cfg.miso_io_num = SD_MISO_PIN;
    bus_cfg.sclk_io_num = SD_CLK_PIN;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = 4000;

    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret == ESP_OK) {
        bus_initialized = true;
    } else {
        printf("[SD] SPI init failed: %s\n", esp_err_to_name(ret));
    }
}

void sd_handle_card_removal() {
    if (sd_lock(500)) {
        g_sd_card_ready = false;
        if (log_file != NULL) {
            fclose(log_file);
            log_file = NULL;
        }
        if (card != NULL) {
            esp_vfs_fat_sdcard_unmount(mount_point, card);
            card = NULL;
        }
        printf("[SD Error] Unexpected SD card removal detected. Card unmounted and bus released.\n");
        sd_unlock();
    }
}

bool sd_try_remount() {
    if (!sd_lock(1000)) return false;

    if (g_sd_card_ready) {
        sd_unlock();
        return true;
    }

    if (card != NULL) {
        esp_vfs_fat_sdcard_unmount(mount_point, card);
        card = NULL;
    }

    if (!bus_initialized) {
        sd_bus_init_once();
        if (!bus_initialized) {
            sd_unlock();
            return false;
        }
    }

    esp_err_t ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);
    if (ret == ESP_OK) {
        g_sd_card_ready = true;
        printf("[SD] Mount succeeded.\n");
        sd_unlock();
        return true;
    } else {
        g_sd_card_ready = false;
        sd_unlock();
        return false;
    }
}

void sd_init() {
    if (s_sd_mutex == NULL) {
        s_sd_mutex = xSemaphoreCreateRecursiveMutex();
    }
    sd_prepare_configs();
    sd_bus_init_once();

    if (!bus_initialized) {
        g_sd_card_ready = false;
        return;
    }

    sd_try_remount();
}

bool sd_start_new_log() {
    if (!sd_lock(1000)) return false;

    if (!g_sd_card_ready) {
        sd_unlock();
        return false;
    }
    if (log_file != NULL) {
        sd_unlock();
        return true;
    }

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    char filename[64];
    snprintf(filename, sizeof(filename), "%s/%04d-%02d-%02d.csv", mount_point,
            timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);

    log_file = fopen(filename, "a");
    if (log_file == NULL) {
        printf("[SD Error] Failed to open CSV log file: %s\n", filename);
        sd_handle_card_removal();
        sd_unlock();
        return false;
    }

    current_log_day = timeinfo.tm_mday;

    if (fseek(log_file, 0, SEEK_END) != 0) {
        sd_handle_card_removal();
        sd_unlock();
        return false;
    }

    if (ftell(log_file) == 0) {
        if (fprintf(log_file, "Timestamp,Dev_Voltage,Acc_Voltage,Fuel_Volt,Ignition,AccX,AccY,AccZ,Pitch,Roll,Yaw,Temperature\n") < 0 || fflush(log_file) != 0) {
            sd_handle_card_removal();
            sd_unlock();
            return false;
        }
    }
    printf("[SD] Logging active on file: %s\n", filename);
    sd_unlock();
    return true;
}

bool sd_write_data_row(const char *rtc_timestamp, float dev_voltage,
                    float acc_voltage, float fuel_volt, int ignition,
                    float accX, float accY, float accZ, float pitch,
                    float roll, float yaw, float temp_c) {
    if (!sd_lock(200)) return false;

    if (!g_sd_card_ready) {
        sd_unlock();
        return false;
    }

    // Auto-resume logging if card is ready but file was closed (e.g., after hot-swap remount)
    if (log_file == NULL) {
        if (!sd_start_new_log()) {
            sd_unlock();
            return false;
        }
    }

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    if (timeinfo.tm_mday != current_log_day) {
        sd_stop_log();
        if (!sd_start_new_log()) {
            sd_unlock();
            return false;
        }
    }

    if (log_file == NULL) {
        sd_unlock();
        return false;
    }

    int written = fprintf(log_file, "%s,%.3f,%.3f,%.3f,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n",
        rtc_timestamp, dev_voltage, acc_voltage, fuel_volt, ignition, accX, accY,
        accZ, pitch, roll, yaw, temp_c);

    if (written < 0 || fflush(log_file) != 0) {
        printf("flush or write failed\r\n");
        sd_handle_card_removal();
        sd_unlock();
        return false;
    }

    // data sync every 20hz, to prevent data loss
    static int sync_counter = 0;
    sync_counter++;
    if (sync_counter >= 20) {
        if (fsync(fileno(log_file)) != 0) {
            printf("fsync failed\r\n");
            sd_handle_card_removal();
            sd_unlock();
            return false;
        }
        sync_counter = 0;
    }

    sd_unlock();
    return true;
}

void sd_stop_log() {
    if (sd_lock(500)) {
        if (log_file != NULL) {
            printf("[SD] Log stopped.\n");
            fclose(log_file);
            log_file = NULL;
        }
        sd_unlock();
    }
}

float sd_get_used_percentage() {
    if (!sd_lock(500)) return 0.0f;

    if (!g_sd_card_ready) {
        sd_unlock();
        return 0.0f;
    }

    uint64_t total = 0;
    uint64_t free = 0;

    esp_err_t err = esp_vfs_fat_info(mount_point, &total, &free);

    if (err == ESP_OK && total > 0) {
        uint64_t used = total - free;
        float perc = ((float)used / (float)total) * 100.0f;
        sd_unlock();
        return perc;
    }

    sd_handle_card_removal();
    sd_unlock();
    return 0.0f;
}