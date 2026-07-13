#include "temperature.h"
#include "constant.h"
#include "driver/i2c.h"

void mcp9808_init() {
}

float mcp9808_read_temp() {
    uint8_t data[2];
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MCP9808_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, MCP9808_REG_TEMP, true);
    
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MCP9808_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, &data[0], I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &data[1], I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_OK) {
        data[0] = data[0] & 0x1F;
        float temp = (data[0] * 16.0f) + (data[1] / 16.0f);
        if (data[0] & 0x10) {
            temp = 256.0f - temp;
        }
        return temp;
    }
    
    return 0.0f;
}