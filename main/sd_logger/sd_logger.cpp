#include "sd_logger.h"
#include "constant.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include <time.h>
#include <sys/time.h>
#include <stdio.h>

static const char mount_point[] = "/sdcard";
static sdmmc_card_t *card;
static FILE *log_file = NULL;
static int current_log_day = -1;

static void write_float_comma(FILE* f, float val) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.3f", val);
    for(int i = 0; buf[i] != '\0'; i++) {
        if(buf[i] == '.') buf[i] = ',';
    }
    fprintf(f, "%s", buf);
}

void sd_init() {
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
    mount_config.format_if_mount_failed = false;
    mount_config.max_files = 10;
    mount_config.allocation_unit_size = 16 * 1024;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;

    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = SD_MOSI_PIN;
    bus_cfg.miso_io_num = SD_MISO_PIN;
    bus_cfg.sclk_io_num = SD_CLK_PIN;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = 4000;

    spi_bus_initialize(SPI2_HOST, &bus_cfg, SDSPI_DEFAULT_DMA);

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = (gpio_num_t)SD_CS_PIN;
    slot_config.host_id = SPI2_HOST;

    esp_err_t ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        printf("\n[ERROR] SD Card tidak terdeteksi atau gagal di-mount! (Code: %s)\n", esp_err_to_name(ret));
        g_sd_card_ready = false;
    } else {
        printf("\n[SUCCESS] SD Card berhasil di-mount!\n");
        g_sd_card_ready = true;
    }
}

bool sd_start_new_log() {
    if (!g_sd_card_ready) return false;
    if (log_file != NULL) return false;

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    char filename[64];
    snprintf(filename, sizeof(filename), "%s/%04d-%02d-%02d.csv", mount_point, timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
    
    log_file = fopen(filename, "a");
    if (log_file == NULL) return false;

    current_log_day = timeinfo.tm_mday;

    fseek(log_file, 0, SEEK_END);
    if (ftell(log_file) == 0) {
        fprintf(log_file, "Timestamp;Veh_Voltage;Acc_Voltage;Fuel_Volt;Ignition;AccX;AccY;AccZ;Pitch;Roll;Yaw;Temperature\n");
    }
    return true;
}

    void sd_write_data_row(const char* rtc_timestamp, float veh_voltage, float acc_voltage, float fuel_volt, int ignition, 
                        float accX, float accY, float accZ, float pitch, float roll, float yaw, float temp_c) {
    if (log_file != NULL && g_sd_card_ready) {
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);

        if (timeinfo.tm_mday != current_log_day) {
            sd_stop_log();
            sd_start_new_log();
        }

        if (log_file == NULL) return;

        fprintf(log_file, "%s;", rtc_timestamp);
        write_float_comma(log_file, veh_voltage); fprintf(log_file, ";");
        write_float_comma(log_file, acc_voltage); fprintf(log_file, ";");
        write_float_comma(log_file, fuel_volt); fprintf(log_file, ";");
        fprintf(log_file, "%d;", ignition);
        write_float_comma(log_file, accX); fprintf(log_file, ";");
        write_float_comma(log_file, accY); fprintf(log_file, ";");
        write_float_comma(log_file, accZ); fprintf(log_file, ";");
        write_float_comma(log_file, pitch); fprintf(log_file, ";");
        write_float_comma(log_file, roll); fprintf(log_file, ";");
        write_float_comma(log_file, yaw); fprintf(log_file, ";");
        write_float_comma(log_file, temp_c); fprintf(log_file, "\n");

        fflush(log_file);
    }
}

void sd_stop_log() {
    if (log_file != NULL) {
        fclose(log_file);
        log_file = NULL;
    }
}

float sd_get_used_percentage() {
    if (!g_sd_card_ready) return 0.0f;

    uint64_t total = 0;
    uint64_t free = 0;

    esp_err_t err = esp_vfs_fat_info(mount_point, &total, &free);

    if (err == ESP_OK && total > 0) {
        uint64_t used = total - free;
        float percentage = ((float)used / (float)total) * 100.0f;

        return percentage;
    }
    return 0.0f;
}