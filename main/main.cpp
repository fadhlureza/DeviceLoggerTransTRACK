#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "constant.h"
#include "imu/imu.h"
#include "rtc/rtc.h"
#include "temperature/temperature.h"
#include "sd_logger/sd_logger.h"
#include "fuel/fuel.h"
#include "voltage/voltage.h"
#include "wifi_ap/wifi_ap.h"
#include "web_server/web_server.h"
#include <time.h>
#include <sys/time.h>

volatile float g_ema_alpha = 0.2f;
volatile bool first_ema_read = true;

void sensor_read_task(void *pvParameters) {
    const uint64_t SAMPLING_PERIOD_US = 625;
    uint64_t next_sample_time = esp_timer_get_time();
    uint32_t adc_counter = 0;

    while (1) {
        float raw_g = 0.0, uncalib_ms2 = 0.0, calib_ms2 = 0.0;
        float accX = 0.0, accY = 0.0, accZ = 0.0;
        float pitch = 0.0, roll = 0.0, yaw = 0.0;
        float batt_perc = 0.0;
        bool ignition = false;
        
        imu_read_vibration_and_orientation(&raw_g, &uncalib_ms2, &calib_ms2, &accX, &accY, &accZ, &pitch, &roll, &yaw);
        
        g_curr_vib_raw_g = raw_g;
        g_curr_vib_uncalib_ms2 = uncalib_ms2;
        g_curr_accX_ms2 = accX;
        g_curr_accY_ms2 = accY;
        g_curr_accZ_ms2 = accZ;
        g_curr_pitch = pitch;
        g_curr_roll = roll;
        g_curr_yaw = yaw;
        g_batt_perc = batt_perc;
        g_ignition = ignition;

        if (first_ema_read) {
            g_curr_vib_calib_ms2 = calib_ms2;
            first_ema_read = false;
        } else {
            g_curr_vib_calib_ms2 = (g_ema_alpha * calib_ms2) + ((1.0f - g_ema_alpha) * g_curr_vib_calib_ms2);
        }

        adc_counter++;
        if (adc_counter >= 80) {
            g_curr_fuel_raw = fuel_read_raw();
            g_curr_fuel_norm = fuel_read_normalized();
            g_curr_voltage = voltage_read_actual();
            g_curr_temp_c = mcp9808_read_temp();
            adc_counter = 0;
        }

        next_sample_time += SAMPLING_PERIOD_US;
        while (esp_timer_get_time() < next_sample_time) {
            taskYIELD();
        }
    }
}

void logging_task(void *pvParameters) {
    uint32_t last_log_time = 0;
    uint32_t last_sd_check_time = 0;

    while(1){
        uint32_t current_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);

        if (current_ms - last_sd_check_time >= 5000) {
            g_sd_used_perc = sd_get_used_percentage();
            last_sd_check_time = current_ms;
        }

        if (g_is_logging && (current_ms - last_log_time >= g_sampling_rate_ms)) {
            time_t now;
            struct tm timeinfo;
            time(&now);
            localtime_r(&now, &timeinfo);

            char rtc_time_str[32];
            strftime(rtc_time_str, sizeof(rtc_time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);

            sd_write_data_row(rtc_time_str, g_curr_voltage, g_curr_voltage, g_curr_fuel_raw, g_ignition ? 1 : 0,
                              g_curr_accX_ms2, g_curr_accY_ms2, g_curr_accZ_ms2,
                              g_curr_pitch, g_curr_roll, g_curr_yaw, g_curr_temp_c);
            
            last_log_time = current_ms;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

extern "C" void app_main(void) {
    adc_oneshot_unit_init_cfg_t adc_init_config = {};
    adc_init_config.unit_id = ADC_UNIT_1;
    adc_oneshot_new_unit(&adc_init_config, &g_adc1_handle);

    imu_init();
    imu_calibrate();
    mcp9808_init();
    rtc_init();
    fuel_sensor_init();
    voltage_sensor_init();
    sd_init();
    wifi_ap_init();
    start_webserver();

    xTaskCreatePinnedToCore(sensor_read_task, "sensor_task", 8192, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(logging_task, "log_task", 4096, NULL, 4, NULL, 0);
}