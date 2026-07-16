#include "accumulator.h"

#include "constant.h"
#include "../ina3221/ina3221.h"

void accumulator_sensor_init() {
    (void)ina3221_init();
}

float accumulator_read_actual() {
    float voltage = 0.0f;

    if (ina3221_read_bus_voltage(INA3221_ACCUMULATOR_CHANNEL, &voltage) != ESP_OK) {
        return 0.0f;
    }

    return voltage;
}