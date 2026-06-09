#include "fuel.h"
#include "constant.h"

static float kalman_p = 1.0f;
static float kalman_q = 0.05f;
static float kalman_r = 10.0f;
static float kalman_k = 0.0f;
static float kalman_x = 0.0f;
static bool is_kalman_init = false;

void fuel_sensor_init() {
    adc_oneshot_chan_cfg_t config = {};
    config.bitwidth = ADC_BITWIDTH_DEFAULT;
    config.atten = ADC_ATTEN_DB_12;
    adc_oneshot_config_channel(g_adc1_handle, FUEL_ADC_CHAN, &config);
}

float fuel_read_raw() {
    int val = 0;
    adc_oneshot_read(g_adc1_handle, FUEL_ADC_CHAN, &val);
    float raw = (float)val;

    if (!is_kalman_init) {
        kalman_x = raw;
        is_kalman_init = true;
    }

    kalman_p = kalman_p + kalman_q;
    kalman_k = kalman_p / (kalman_p + kalman_r);
    kalman_x = kalman_x + kalman_k * (raw - kalman_x);
    kalman_p = (1.0f - kalman_k) * kalman_p;

    return kalman_x;
}

float fuel_read_normalized() {
    return (fuel_read_raw() / ADC_MAX_VAL) * FUEL_PERCENT_MAX;
}