/**
 * @file i2c_manager.h
 * @brief Public interface for creating I2C buses and registering devices.
 *
 * @details
 * Provides the data types and operations required to configure an ESP-IDF
 * I2C master bus and attach 7-bit addressed devices to that bus.
 */

#ifndef I2C_MANAGER_H
#define I2C_MANAGER_H

#include <stdint.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"

/**
 * @brief Hardware and default timing configuration for one I2C bus.
 */
typedef struct {
    i2c_port_num_t port;
    gpio_num_t sda_gpio;
    gpio_num_t scl_gpio;
    uint32_t default_speed_hz;
} i2c_bus_config_t;

/**
 * @brief Runtime state and retained configuration for one I2C bus.
 */
typedef struct {
    i2c_master_bus_handle_t handle;
    i2c_bus_config_t config;
} i2c_bus_t;

/**
 * @brief Initialize one I2C master bus.
 *
 * @details
 * Creates an ESP-IDF I2C master bus using the hardware configuration
 * provided by the board configuration layer and retains that configuration
 * in the runtime bus object.
 *
 * @param[out] bus Runtime I2C bus object to initialize.
 * @param[in]  config I2C hardware configuration.
 *
 * @return ESP_OK on success.
 * @return ESP_ERR_INVALID_ARG if bus or config is NULL.
 * @return ESP-IDF error code if bus creation fails.
 */
esp_err_t i2c_manager_init(i2c_bus_t *bus, const i2c_bus_config_t *config);

/**
 * @brief Register a device on an initialized I2C bus.
 *
 * @details
 * Registers a 7-bit addressed device. If speed_hz is zero, the default bus
 * speed retained in the bus configuration is used.
 *
 * @param[in]  bus Initialized I2C bus object.
 * @param[in]  device_address 7-bit I2C device address.
 * @param[in]  speed_hz Device clock speed in hertz, or zero for the bus default.
 * @param[out] device Output handle for the registered device.
 *
 * @return ESP_OK on success.
 * @return ESP_ERR_INVALID_ARG if bus or device is NULL.
 * @return ESP-IDF error code if device registration fails.
 */
esp_err_t i2c_manager_add_device(const i2c_bus_t *bus, uint16_t device_address,
                                 uint32_t speed_hz, i2c_master_dev_handle_t *device);

#endif /* I2C_MANAGER_H */
