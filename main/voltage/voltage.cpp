#include "voltage.h"
#include "constant.h"
#include "../ina3221/ina3221.h"

void voltage_sensor_init() {
    (void)ina3221_init();
}

float voltage_read_actual() {
    float voltage = 0.0f;

    if (ina3221_read_bus_voltage(INA3221_VOLTAGE_CHANNEL, &voltage) != ESP_OK) {
        return 0.0f;
    }

    return voltage;
}