#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_adc/adc_oneshot.h"

// Micro SD card pins
#define SD_MISO_PIN 19
#define SD_MOSI_PIN 20
#define SD_CLK_PIN  21
#define SD_CS_PIN   47

// IMU pins
#define IMU_SDA_PIN 42
#define IMU_SCL_PIN 41

// IMU Registers
#define BMI160_ADDR      0x68 
#define CMD_REG          0x7E
#define ACC_CONF         0x40
#define ACC_RANGE        0x41
#define ACC_X_LSB        0x12
#define GYR_RANGE        0x43
#define GYR_X_LSB        0x0C
#define BMI160_CHIP_ID    0xD1
#define BMI160_REG_CHIP_ID 0x00
#define BMI160_CMD_SOFT_RESET 0xB6

// IMU Parameters
#define IMU_I2C_FREQ     100000
#define IMU_LSB_PER_G    2048.0f
#define GRAVITY_EARTH    9.80665f
#define ALPHA 0.98f

// Kalibrasi IMU
#define VIB_CALIB_MULT   1.226f
#define VIB_CALIB_OFFSET 0.145f

// Fuel and voltage sensor parameters
#define FUEL_ADC_CHAN     ADC_CHANNEL_4
#define VOLT_ADC_CHAN     ADC_CHANNEL_5
#define ADC_MAX_VAL       4095.0f
#define FUEL_PERCENT_MAX  100.0f
#define VOLT_MAX_VAL      36.0f

// WiFi Settings
#define ESP_WIFI_SSID      "DeviceLogger"
#define ESP_WIFI_PASS      "12345678"
#define ESP_WIFI_CHANNEL   1
#define ESP_MAX_STA_CONN   4

// Logging parameters
inline volatile bool g_sd_card_ready = false;
inline volatile bool g_is_logging = false;
inline volatile int g_sampling_rate_ms = 50;

// Sensor data variables
inline volatile float g_curr_vib_raw_g = 0.0;
inline volatile float g_curr_vib_uncalib_ms2 = 0.0;
inline volatile float g_curr_vib_calib_ms2 = 0.0;
inline volatile float g_curr_fuel_raw = 0.0;
inline volatile float g_curr_fuel_norm = 0.0;
inline volatile float g_curr_voltage = 0.0;
extern float g_curr_accX_ms2;
extern float g_curr_accY_ms2;
extern float g_curr_accZ_ms2;
extern float g_curr_pitch;
extern float g_curr_roll;
extern float g_curr_yaw;

extern volatile float g_batt_perc;
extern volatile float g_sd_used_perc;
extern volatile bool g_ignition;

inline adc_oneshot_unit_handle_t g_adc1_handle = NULL;