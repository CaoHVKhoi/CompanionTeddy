/**
 * @file i2c_service.c
 * @brief Implementation of board-level I2C service management.
 */

#include "i2c_service.h"

#include "esp_check.h"

static const char *TAG = "I2C_SERVICE";

/**
 * @brief Runtime state for every configured I2C bus.
 */
static i2c_bus_t s_i2c_buses[I2C_BUS_COUNT];

/**
 * @brief Initialize each configured I2C bus.
 *
 * @details
 * Processes the board configuration in enum order and returns immediately
 * when an individual bus cannot be created.
 */
esp_err_t i2c_service_init(void)
{
    for (i2c_bus_id_t bus_id = 0; bus_id < I2C_BUS_COUNT; ++bus_id) {
        ESP_RETURN_ON_ERROR(
            i2c_manager_init(&s_i2c_buses[bus_id], &g_i2c_bus_config[bus_id]),
            TAG,
            "Failed to initialize I2C bus %d",
            bus_id
        );
    }

    return ESP_OK;
}

/**
 * @brief Return the runtime state for a valid bus identifier.
 *
 * @details
 * Performs a range check before indexing the runtime bus array.
 */
i2c_bus_t *i2c_service_get_bus(i2c_bus_id_t bus_id)
{
    if (bus_id >= I2C_BUS_COUNT) {
        return NULL;
    }

    return &s_i2c_buses[bus_id];
}
