#pragma once
#include <stdbool.h>
#include <stdint.h>

void sd_init();
bool sd_lock(uint32_t timeout_ms);
void sd_unlock(void);
bool sd_try_remount();
void sd_handle_card_removal();
bool sd_start_new_log();
bool sd_write_data_row(const char *rtc_timestamp, float dev_voltage,
                       float acc_voltage, float fuel_volt, int ignition,
                       float accX, float accY, float accZ, float pitch,
                       float roll, float yaw, float temp_c);
void sd_stop_log();
float sd_get_used_percentage();