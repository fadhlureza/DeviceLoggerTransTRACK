#include "fuel.h"
#include "constant.h"

int ina3221_init();
int ina3221_read_bus_voltage(uint8_t channel, float *voltage);

void fuel_sensor_init() {
    (void)ina3221_init();
}

float fuel_read_raw() {
    float voltage = 0.0f;

    if (ina3221_read_bus_voltage(INA3221_FUEL_CHANNEL, &voltage) != 0) {
        return 0.0f;
    }

    return voltage;
}