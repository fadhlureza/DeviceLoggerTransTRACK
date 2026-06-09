#include "voltage.h"
#include "constant.h"

void voltage_sensor_init() {
    adc_oneshot_chan_cfg_t config = {};
    config.bitwidth = ADC_BITWIDTH_DEFAULT;
    config.atten = ADC_ATTEN_DB_12;
    adc_oneshot_config_channel(g_adc1_handle, VOLT_ADC_CHAN, &config);
}

float voltage_read_actual() {
    int val = 0;
    adc_oneshot_read(g_adc1_handle, VOLT_ADC_CHAN, &val);
    return ((float)val / ADC_MAX_VAL) * VOLT_MAX_VAL; 
}