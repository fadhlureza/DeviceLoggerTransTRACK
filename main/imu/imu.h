#pragma once

void imu_init();
void imu_calibrate();
void imu_read_vibration_and_orientation(float *out_raw_g, float *out_uncalib_ms2, float *out_calib_ms2, 
                                        float *out_accX, float *out_accY, float *out_accZ,
                                        float *out_pitch, float *out_roll, float *out_yaw);