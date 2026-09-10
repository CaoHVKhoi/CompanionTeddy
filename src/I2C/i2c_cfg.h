/**
 * @file i2c_cfg.h
 * @brief Board-specific I2C bus assignments and operating speeds.
 *
 * @details
 * Defines the logical I2C buses used by the OLED display and battery monitor.
 * The configuration is consumed by the I2C service during initialization.
 */

#ifndef BOARD_I2C_CONFIG_H
#define BOARD_I2C_CONFIG_H

#include "i2c_manager.h"

/**
 * @brief Identifies a configured I2C bus.
 */
typedef enum {
    I2C_BUS_OLED,
    I2C_BUS_BATTERY,
    I2C_BUS_COUNT
} i2c_bus_id_t;

/**
 * @brief Board-specific I2C settings indexed by i2c_bus_id_t.
 *
 */
static const i2c_bus_config_t g_i2c_bus_config[I2C_BUS_COUNT] = {
    [I2C_BUS_OLED] = {
        .port = I2C_NUM_0,
        .sda_gpio = GPIO_NUM_8,
        .scl_gpio = GPIO_NUM_9,
        .default_speed_hz = 400000,
    },

    [I2C_BUS_BATTERY] = {
        .port = I2C_NUM_1,
        .sda_gpio = GPIO_NUM_10,
        .scl_gpio = GPIO_NUM_11,
        .default_speed_hz = 100000,
    },
};

#endif /* BOARD_I2C_CONFIG_H */
