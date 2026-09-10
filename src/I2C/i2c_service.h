/**
 * @file i2c_service.h
 * @brief Public interface for board-level I2C service management.
 *
 * @details
 * Coordinates initialization and lookup of all I2C buses declared by the
 * board configuration layer.
 */

#ifndef I2C_SERVICE_H
#define I2C_SERVICE_H

#include "i2c_cfg.h"
#include "i2c_manager.h"
#include "esp_err.h"

/**
 * @brief Initialize all board-configured I2C buses.
 *
 * @details
 * Initializes buses in ascending logical bus identifier order and stops at
 * the first initialization failure.
 *
 * @return ESP_OK when all configured buses are initialized.
 * @return ESP-IDF error code from the first failed bus initialization.
 */
esp_err_t i2c_service_init(void);

/**
 * @brief Get the runtime state for a configured I2C bus.
 *
 * @param[in] bus_id Logical identifier of the requested I2C bus.
 *
 * @return Pointer to the bus state when bus_id is valid.
 * @return NULL when bus_id is outside the configured bus range.
 */
i2c_bus_t *i2c_service_get_bus(i2c_bus_id_t bus_id);

#endif /* I2C_SERVICE_H */
