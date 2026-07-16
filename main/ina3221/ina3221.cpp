#include "ina3221.h"

#include "constant.h"
#include "driver/i2c.h"

namespace {

constexpr uint8_t INA3221_REG_CONFIG = 0x00;
constexpr uint8_t INA3221_REG_BUS_VOLTAGE_BASE = 0x02;

esp_err_t read_register(uint8_t reg, uint16_t *value) {
    if (value == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t data[2] = {};
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (INA3221_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (INA3221_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, &data[0], I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &data[1], I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        return ret;
    }

    *value = (static_cast<uint16_t>(data[0]) << 8) | data[1];
    return ESP_OK;
}

}  // namespace

esp_err_t ina3221_init() {
    uint16_t config = 0;
    return read_register(INA3221_REG_CONFIG, &config);
}

esp_err_t ina3221_read_bus_voltage(uint8_t channel, float *voltage) {
    if (voltage == nullptr || channel < 1 || channel > 3) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t raw = 0;
    uint8_t reg = INA3221_REG_BUS_VOLTAGE_BASE + ((channel - 1) * 2);
    esp_err_t ret = read_register(reg, &raw);
    if (ret != ESP_OK) {
        return ret;
    }

    raw >>= 3;
    *voltage = static_cast<float>(raw) * INA3221_BUS_VOLT_LSB;
    return ESP_OK;
}