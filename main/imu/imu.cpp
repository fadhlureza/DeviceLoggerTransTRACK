#include "constant.h"
#include "imu.h"
#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "BMI160";

static const int I2C_RETRY_COUNT = 3;
static const int RECOVERY_COOLDOWN_MS = 5000; 
static const int HEALTH_CHECK_INTERVAL = 100; 
static const int RECOVERY_FAIL_THRESHOLD = 3;

static float baseX = 0;
static float baseY = 0;
static float baseZ = 0;

static float pitch_filtered = 0.0f;
static float roll_filtered = 0.0f;
static float yaw_gyro = 0.0f;
static uint64_t last_time = 0;

static struct {
    float raw_g, uncalib_ms2, calib_ms2;
    float accX, accY, accZ;
    float pitch, roll, yaw;
    bool valid;
} last_valid = {};

static int read_cycle_count = 0;
static int consecutive_failures = 0;
static int64_t last_recovery_attempt_us = 0;

static esp_err_t i2c_master_init(void) {
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = IMU_SDA_PIN; 
    conf.scl_io_num = IMU_SCL_PIN; 
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = IMU_I2C_FREQ;

    esp_err_t err = i2c_param_config(I2C_NUM_0, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(err));
        return err;
    }

    err = i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(err));
    }
    return err;
}

static esp_err_t writeRegister(uint8_t reg, uint8_t data) {
    uint8_t write_buf[2] = {reg, data};
    esp_err_t err = i2c_master_write_to_device(
        I2C_NUM_0, BMI160_ADDR, write_buf, sizeof(write_buf),
        pdMS_TO_TICKS(1000));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Write reg 0x%02X failed: %s", reg, esp_err_to_name(err));
    }
    return err;
}

static esp_err_t readRegister(uint8_t reg, uint8_t *buf, size_t len) {
    esp_err_t err = i2c_master_write_read_device(
        I2C_NUM_0, BMI160_ADDR, &reg, 1, buf, len,
        pdMS_TO_TICKS(1000));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Read reg 0x%02X failed: %s", reg, esp_err_to_name(err));
    }
    return err;
}

static float rawToG(int16_t raw) {
    return raw / IMU_LSB_PER_G;
}

static float rawToDPS(int16_t raw) {
    return raw / 16.4f;
}

static esp_err_t read_accel_gyro_burst(int16_t *ax, int16_t *ay, int16_t *az, int16_t *gx, int16_t *gy, int16_t *gz) {
    uint8_t data[12] = {0};
    esp_err_t err = readRegister(0x0C, data, sizeof(data));
    if (err != ESP_OK) return err;

    *gx = (int16_t)((data[1] << 8) | data[0]);
    *gy = (int16_t)((data[3] << 8) | data[2]);
    *gz = (int16_t)((data[5] << 8) | data[4]);
    *ax = (int16_t)((data[7] << 8) | data[6]);
    *ay = (int16_t)((data[9] << 8) | data[8]);
    *az = (int16_t)((data[11] << 8) | data[10]);
    return ESP_OK;
}

static esp_err_t read_burst_with_retry(int16_t *ax, int16_t *ay, int16_t *az, int16_t *gx, int16_t *gy, int16_t *gz) {
    esp_err_t err = ESP_FAIL;
    for (int attempt = 0; attempt < I2C_RETRY_COUNT; attempt++) {
        err = read_accel_gyro_burst(ax, ay, az, gx, gy, gz);
        if (err == ESP_OK) return ESP_OK;
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    return err;
}

static esp_err_t bmi160_check_who_am_i(void) {
    uint8_t chip_id = 0;
    esp_err_t err = readRegister(BMI160_REG_CHIP_ID, &chip_id, 1);
    if (err != ESP_OK) return err;
    if (chip_id != BMI160_CHIP_ID) {
        ESP_LOGE(TAG, "Unexpected CHIP_ID: 0x%02X", chip_id);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t bmi160_configure(void) {
    esp_err_t err = writeRegister(CMD_REG, 0x11);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(50));

    err = writeRegister(CMD_REG, 0x15);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(100));

    err = writeRegister(ACC_CONF, 0x2C);
    if (err != ESP_OK) return err;

    err = writeRegister(ACC_RANGE, 0x0C);
    if (err != ESP_OK) return err;

    err = writeRegister(GYR_RANGE, 0x00);
    if (err != ESP_OK) return err;

    vTaskDelay(pdMS_TO_TICKS(100));
    return ESP_OK;
}

static esp_err_t imu_recover(void) {
    int64_t now_us = esp_timer_get_time();
    int64_t cooldown_us = (int64_t)RECOVERY_COOLDOWN_MS * 1000;
    if ((now_us - last_recovery_attempt_us) < cooldown_us) return ESP_ERR_INVALID_STATE;
    last_recovery_attempt_us = now_us;

    ESP_LOGW(TAG, "Attempting BMI160 recovery (soft reset)...");
    esp_err_t err = writeRegister(CMD_REG, BMI160_CMD_SOFT_RESET);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(50));

    err = bmi160_configure();
    if (err != ESP_OK) return err;

    err = bmi160_check_who_am_i();
    if (err != ESP_OK) return err;

    imu_calibrate();
    consecutive_failures = 0;
    ESP_LOGI(TAG, "BMI160 recovery successful");
    return ESP_OK;
}

void imu_init() {
    i2c_master_init();
    bmi160_configure();
    bmi160_check_who_am_i();
    ESP_LOGI(TAG, "BMI160 Initialized & Configured");
}

void imu_calibrate() {
    const int sampleCount = 100;
    float sumX = 0, sumY = 0, sumZ = 0;
    int validSamples = 0;

    for (int i = 0; i < sampleCount; i++) {
        int16_t ax, ay, az, gx, gy, gz;
        esp_err_t err = read_burst_with_retry(&ax, &ay, &az, &gx, &gy, &gz);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Calibration sample %d failed, skipping", i);
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        sumX += rawToG(ax);
        sumY += rawToG(ay);
        sumZ += rawToG(az);
        validSamples++;
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (validSamples > 0) {
        baseX = sumX / validSamples;
        baseY = sumY / validSamples;
        baseZ = sumZ / validSamples;
        ESP_LOGI(TAG, "Baseline updated with %d samples", validSamples);
    }
    last_time = esp_timer_get_time();
}

void imu_read_vibration_and_orientation(float *out_raw_g, float *out_uncalib_ms2, float *out_calib_ms2, 
                                        float *out_accX, float *out_accY, float *out_accZ,
                                        float *out_pitch, float *out_roll, float *out_yaw) {
    read_cycle_count++;
    if (read_cycle_count >= HEALTH_CHECK_INTERVAL) {
        read_cycle_count = 0;
        if (bmi160_check_who_am_i() != ESP_OK) ESP_LOGW(TAG, "Health check failed");
    }

    int16_t ax, ay, az, gx, gy, gz;
    esp_err_t err = read_burst_with_retry(&ax, &ay, &az, &gx, &gy, &gz);

    if (err != ESP_OK) {
        consecutive_failures++;
        if (consecutive_failures >= RECOVERY_FAIL_THRESHOLD) imu_recover();

        if (last_valid.valid) {
            *out_raw_g = last_valid.raw_g;
            *out_uncalib_ms2 = last_valid.uncalib_ms2;
            *out_calib_ms2 = last_valid.calib_ms2;
            *out_accX = last_valid.accX;
            *out_accY = last_valid.accY;
            *out_accZ = last_valid.accZ;
            *out_pitch = last_valid.pitch;
            *out_roll = last_valid.roll;
            *out_yaw = last_valid.yaw;
        }
        return;
    }

    consecutive_failures = 0;

    // Proses Data Akselerometer
    float accX_g = rawToG(ax);
    float accY_g = rawToG(ay);
    float accZ_g = rawToG(az);
    float deltaX = accX_g - baseX;
    float deltaY = accY_g - baseY;
    float deltaZ = accZ_g - baseZ;

    float vibration_g = sqrt((deltaX * deltaX) + (deltaY * deltaY) + (deltaZ * deltaZ));
    float vibration_ms2 = (vibration_g * GRAVITY_EARTH);
    float vibration_ms2_calibrated = (vibration_ms2 * VIB_CALIB_MULT) + VIB_CALIB_OFFSET;

    // Proses Data Orientasi
    float gyro_rate_pitch = rawToDPS(gx);
    float gyro_rate_roll  = rawToDPS(gy);
    float gyro_rate_yaw   = rawToDPS(gz);
    float accel_pitch = atan2(-accX_g, sqrt(accY_g * accY_g + accZ_g * accZ_g)) * 180.0 / M_PI;
    float accel_roll  = atan2(accY_g, accZ_g) * 180.0 / M_PI;

    uint64_t current_time = esp_timer_get_time();
    float dt = (current_time - last_time) / 1000000.0f; 
    last_time = current_time;

    pitch_filtered = ALPHA * (pitch_filtered + gyro_rate_pitch * dt) + (1.0f - ALPHA) * accel_pitch;
    roll_filtered  = ALPHA * (roll_filtered + gyro_rate_roll * dt) + (1.0f - ALPHA) * accel_roll;
    yaw_gyro = yaw_gyro + (gyro_rate_yaw * dt); 

    // Update Data Caching
    last_valid.accX = accX_g;
    last_valid.accY = accY_g;
    last_valid.accZ = accZ_g;
    last_valid.raw_g = vibration_g;
    last_valid.uncalib_ms2 = vibration_ms2;
    last_valid.calib_ms2 = vibration_ms2_calibrated;
    last_valid.pitch = pitch_filtered;
    last_valid.roll = roll_filtered;
    last_valid.yaw = yaw_gyro;
    last_valid.valid = true;

    // Kirim Output
    *out_accX = accX_g;
    *out_accY = accY_g;
    *out_accZ = accZ_g;
    *out_raw_g = vibration_g;
    *out_uncalib_ms2 = vibration_ms2;
    *out_calib_ms2 = vibration_ms2_calibrated;
    *out_pitch = pitch_filtered;
    *out_roll = roll_filtered;
    *out_yaw = yaw_gyro;
}