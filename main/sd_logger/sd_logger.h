#pragma once
#include <stdint.h>
#include <stdbool.h>

void sd_init();
bool sd_start_new_log();
void sd_write_data_row(const char* rtc_timestamp, float veh_voltage, float int_voltage, float fuel_volt, int ignition, float accX, float accY, float accZ, float pitch, float roll, float yaw, float temp_c);
void sd_stop_log();
float sd_get_used_percentage();