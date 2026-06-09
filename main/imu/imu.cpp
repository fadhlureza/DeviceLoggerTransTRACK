#include "constant.h"
#include "imu.h"
#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"

static float baseX = 0;
static float baseY = 0;
static float baseZ = 0;

static esp_err_t i2c_master_init(void) {
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = IMU_SDA_PIN; 
    conf.scl_io_num = IMU_SCL_PIN; 
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = IMU_I2C_FREQ;
    
    i2c_param_config(I2C_NUM_0, &conf);
    return i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
}

static void writeRegister(uint8_t reg, uint8_t data) {
    uint8_t write_buf[2] = {reg, data};
    i2c_master_write_to_device(I2C_NUM_0, BMI160_ADDR, write_buf, sizeof(write_buf), 1000 / portTICK_PERIOD_MS);
}

static int16_t read16(uint8_t reg) {
    uint8_t data[2] = {0, 0};
    i2c_master_write_read_device(I2C_NUM_0, BMI160_ADDR, &reg, 1, data, 2, 1000 / portTICK_PERIOD_MS);
    return (int16_t)((data[1] << 8) | data[0]);
}

static float rawToG(int16_t raw) {
    return raw / IMU_LSB_PER_G;
}

void imu_init() {
    i2c_master_init();
    
    writeRegister(CMD_REG, 0x11);
    writeRegister(ACC_RANGE, 0x05);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    printf("BMI160 Started\n");
}

void imu_calibrate() {
    const int sampleCount = 100;
    float sumX = 0, sumY = 0, sumZ = 0;

    for (int i = 0; i < sampleCount; i++) {
        int16_t accX_raw = read16(ACC_X_LSB);
        int16_t accY_raw = read16(ACC_X_LSB + 2);
        int16_t accZ_raw = read16(ACC_X_LSB + 4);

        sumX += rawToG(accX_raw);
        sumY += rawToG(accY_raw);
        sumZ += rawToG(accZ_raw);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    baseX = sumX / sampleCount;
    baseY = sumY / sampleCount;
    baseZ = sumZ / sampleCount;
}

void imu_read_vibration(float *out_raw_g, float *out_uncalib_ms2, float *out_calib_ms2) {
    int16_t accX_raw = read16(ACC_X_LSB);
    int16_t accY_raw = read16(ACC_X_LSB + 2);
    int16_t accZ_raw = read16(ACC_X_LSB + 4);

    float accX_g = rawToG(accX_raw);
    float accY_g = rawToG(accY_raw);
    float accZ_g = rawToG(accZ_raw);

    float deltaX = accX_g - baseX;
    float deltaY = accY_g - baseY;
    float deltaZ = accZ_g - baseZ;

    float vibration_g = sqrt((deltaX * deltaX) + (deltaY * deltaY) + (deltaZ * deltaZ));
    float vibration_ms2 = (vibration_g * GRAVITY_EARTH);
    float vibration_ms2_calibrated = (vibration_ms2 * VIB_CALIB_MULT) + VIB_CALIB_OFFSET;

    *out_raw_g = vibration_g;
    *out_uncalib_ms2 = vibration_ms2;
    *out_calib_ms2 = vibration_ms2_calibrated;
}