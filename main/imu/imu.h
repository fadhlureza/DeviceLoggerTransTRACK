#pragma once

void imu_init();
void imu_calibrate();
void imu_read_vibration(float *out_raw_g, float *out_uncalib_ms2, float *out_calib_ms2);