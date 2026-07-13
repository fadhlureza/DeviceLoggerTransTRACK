#include "rtc.h"
#include "driver/i2c.h"
#include <sys/time.h>
#include <stdio.h>

uint8_t bcd2dec(uint8_t val) {
    return (val >> 4) * 10 + (val & 0x0F);
}

uint8_t dec2bcd(uint8_t val) {
    return ((val / 10) << 4) + (val % 10);
}

void rtc_init() {
    uint8_t data[7];
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x00, true);

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, &data[0], I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &data[1], I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &data[2], I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &data[3], I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &data[4], I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &data[5], I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &data[6], I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_OK) {
        struct tm timeinfo = {0};

        timeinfo.tm_sec = bcd2dec(data[0] & 0x7F);
        timeinfo.tm_min = bcd2dec(data[1]);
        timeinfo.tm_hour = bcd2dec(data[2] & 0x3F);
        timeinfo.tm_mday = bcd2dec(data[4]);
        timeinfo.tm_mon = bcd2dec(data[5] & 0x7F) - 1;
        timeinfo.tm_year = bcd2dec(data[6]) + 2000 - 1900;

        struct timeval tv;
        tv.tv_sec = mktime(&timeinfo);
        tv.tv_usec = 0;
        settimeofday(&tv, NULL);

        printf("[RTC] Sync successful!\n");
    } else {
        printf("[RTC] Error failed to read HW-084!\n");
    }
}

void rtc_set_time(int year, int month, int day, int hour, int min, int sec) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_WRITE, true);

    i2c_master_write_byte(cmd, 0x00, true);
    i2c_master_write_byte(cmd, dec2bcd(sec), true);
    i2c_master_write_byte(cmd, dec2bcd(min), true);
    i2c_master_write_byte(cmd, dec2bcd(hour), true);
    i2c_master_write_byte(cmd, 1, true);
    i2c_master_write_byte(cmd, dec2bcd(day), true);
    i2c_master_write_byte(cmd, dec2bcd(month), true);
    i2c_master_write_byte(cmd, dec2bcd(year - 2000), true);
    
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    printf("[RTC] Waktu berhasil diset manual ke HW-084!\n");
}