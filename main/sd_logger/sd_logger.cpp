#include "sd_logger.h"
#include "constant.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include <stdio.h>

static const char mount_point[] = "/sdcard";
static sdmmc_card_t *card;
static FILE *log_file = NULL;

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
    mount_config.max_files = 5;
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

    char filename[32];
    int index = 0;

    while (1) {
        snprintf(filename, sizeof(filename), "/sdcard/log_%d.csv", index);
        FILE *f = fopen(filename, "r");
        if (f == NULL) {
            break;
        }
        fclose(f);
        index++;
    }

    log_file = fopen(filename, "w");
    if (log_file == NULL) return false;
    
    fprintf(log_file, "Timestamp;Vib_Raw_g;Vib_Uncalib_ms2;Vib_Calib_ms2;Fuel_Raw;Fuel_Norm;Voltage\n");
    return true;
}

void sd_write_data_row(uint32_t timestamp, float vib_raw_g, float vib_uncalib_ms2, float vib_calib_ms2, float fuel_raw, float fuel_norm, float voltage) {
    if (log_file != NULL && g_sd_card_ready) {
        fprintf(log_file, "%lu;", timestamp);
        write_float_comma(log_file, vib_raw_g); fprintf(log_file, ";");
        write_float_comma(log_file, vib_uncalib_ms2); fprintf(log_file, ";");
        write_float_comma(log_file, vib_calib_ms2); fprintf(log_file, ";");
        write_float_comma(log_file, fuel_raw); fprintf(log_file, ";");
        write_float_comma(log_file, fuel_norm); fprintf(log_file, ";");
        write_float_comma(log_file, voltage); fprintf(log_file, "\n");
    }
}

void sd_stop_log() {
    if (log_file != NULL) {
        fclose(log_file);
        log_file = NULL;
    }
}