/**
 * @file i2c_manager.c
 * @brief Implementation of I2C bus and device management.
 */

#include "i2c_manager.h"

#include "esp_check.h"

static const char *TAG = "I2C_MANAGER";

/**
 * @brief Initialize one I2C master bus.
 *
 * @details
 * Validates the caller-provided objects, creates the ESP-IDF master bus, and
 * stores a copy of the configuration for later device registration.
 */
esp_err_t i2c_manager_init(i2c_bus_t *bus, const i2c_bus_config_t *config)
{
    ESP_RETURN_ON_FALSE(bus != NULL, ESP_ERR_INVALID_ARG, TAG, "bus is NULL");
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "config is NULL");

    const i2c_master_bus_config_t esp_config = {
        .i2c_port = config->port,
        .sda_io_num = config->sda_gpio,
        .scl_io_num = config->scl_gpio,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_RETURN_ON_ERROR(
        i2c_new_master_bus(&esp_config, &bus->handle),
        TAG,
        "Failed to create I2C bus"
    );

    bus->config = *config;

    return ESP_OK;
}

/**
 * @brief Register one device on an I2C master bus.
 *
 * @details
 * Uses the requested device speed when it is nonzero. A zero speed selects
 * the default speed stored in the bus configuration.
 */
esp_err_t i2c_manager_add_device(const i2c_bus_t *bus, uint16_t device_address,
                                 uint32_t speed_hz, i2c_master_dev_handle_t *device)
{
    ESP_RETURN_ON_FALSE(bus != NULL, ESP_ERR_INVALID_ARG, TAG, "bus is NULL");
    ESP_RETURN_ON_FALSE(device != NULL, ESP_ERR_INVALID_ARG, TAG, "device is NULL");

    if (speed_hz == 0) {
        speed_hz = bus->config.default_speed_hz;
    }

    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = device_address,
        .scl_speed_hz = speed_hz,
    };

    return i2c_master_bus_add_device(bus->handle, &device_config, device);
}
