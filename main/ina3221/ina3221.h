#pragma once

#include <stdint.h>
#include "esp_err.h"

esp_err_t ina3221_init();
esp_err_t ina3221_read_bus_voltage(uint8_t channel, float *voltage);