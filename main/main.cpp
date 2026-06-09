#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "constant.h"
#include "imu/imu.h"
#include "sd_logger/sd_logger.h"
#include "fuel/fuel.h"
#include "voltage/voltage.h"
#include "wifi_ap/wifi_ap.h"
#include "web_server/web_server.h"

void sensor_read_task(void *pvParameters) {
    while (1) {
        float raw_g = 0.0, uncalib_ms2 = 0.0, calib_ms2 = 0.0;
        imu_read_vibration(&raw_g, &uncalib_ms2, &calib_ms2);
        
        g_curr_vib_raw_g = raw_g;
        g_curr_vib_uncalib_ms2 = uncalib_ms2;
        g_curr_vib_calib_ms2 = calib_ms2;
        g_curr_fuel_raw = fuel_read_raw();
        g_curr_fuel_norm = fuel_read_normalized();
        g_curr_voltage = voltage_read_actual();

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void logging_task(void *pvParameters) {
    while (1) {
        if (g_is_logging) {
            uint32_t timestamp = (uint32_t)(esp_timer_get_time() / 1000ULL);
            sd_write_data_row(timestamp, g_curr_vib_raw_g, g_curr_vib_uncalib_ms2, g_curr_vib_calib_ms2, g_curr_fuel_raw, g_curr_fuel_norm, g_curr_voltage);
        }
        
        int delay_ms = g_sampling_rate_ms;
        if (delay_ms < 10) delay_ms = 10; 
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

extern "C" void app_main(void) {
    adc_oneshot_unit_init_cfg_t adc_init_config = {};
    adc_init_config.unit_id = ADC_UNIT_1;
    adc_oneshot_new_unit(&adc_init_config, &g_adc1_handle);

    imu_init();
    imu_calibrate();
    fuel_sensor_init();
    voltage_sensor_init();
    sd_init();
    wifi_ap_init();
    start_webserver();

    xTaskCreatePinnedToCore(sensor_read_task, "sensor_task", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(logging_task, "log_task", 4096, NULL, 4, NULL, 1);
}