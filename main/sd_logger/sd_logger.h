#pragma once
#include <stdint.h>
#include <stdbool.h>

void sd_init();
bool sd_start_new_log();
void sd_write_data_row(uint32_t timestamp, float vib_raw_g, float vib_uncalib_ms2, float vib_calib_ms2, float fuel_raw, float fuel_norm, float voltage);
void sd_stop_log();