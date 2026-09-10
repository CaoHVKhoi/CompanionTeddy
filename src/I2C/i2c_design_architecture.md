# ESP32-S3 I2C Design and Architecture

## 1. Overview

This document describes the I2C software architecture implemented for the ESP32-S3 firmware.

The current hardware uses two independent I2C buses:

| Bus | Device | SDA | SCL | Controller | Default Speed |
|---|---|---:|---:|---|---:|
| OLED | OLED display | GPIO8 | GPIO9 | I2C_NUM_0 | 400 kHz |
| Battery | Battery monitor | GPIO10 | GPIO11 | I2C_NUM_1 | 100 kHz |

The implementation intentionally separates:

- Board-specific hardware configuration
- Generic I2C bus management
- I2C service / bus ownership
- Device-specific drivers

This prevents OLED or battery-monitor logic from becoming coupled to ESP32-S3 GPIO or I2C-controller assignments.

---

## 2. Design Goals

The I2C implementation is designed around the following goals:

1. **Hardware configurability**
   - I2C controller, SDA, SCL, and default speed are configuration data.
   - The generic I2C manager contains no OLED- or battery-specific GPIO definitions.

2. **Device independence**
   - Device drivers receive an I2C device handle.
   - Device protocol logic is independent from the physical bus pins.

3. **Separation of responsibilities**
   - Board configuration defines hardware.
   - I2C manager creates buses and devices.
   - I2C service owns initialized bus instances.
   - Device drivers implement device-specific protocols.

4. **Scalability**
   - Additional I2C buses or devices can be added without modifying the generic I2C manager.

5. **Maintainability**
   - Configuration changes should normally affect only the board configuration layer.

---

# 3. Software Architecture

## 3.1 Layered Architecture

```text
+------------------------------------------------------+
|                   Application                        |
|                                                      |
|   Application Logic / Tasks / State Machines         |
+---------------------------+--------------------------+
                            |
                            v
+------------------------------------------------------+
|                 Device Driver Layer                  |
|                                                      |
|        +----------------+    +----------------+      |
|        |  OLED Driver   |    | Battery Driver |      |
|        +----------------+    +----------------+      |
+---------------------------+--------------------------+
                            |
                            v
+------------------------------------------------------+
|                 I2C Service Layer                    |
|                                                      |
|   Provides access to configured I2C bus instances    |
+---------------------------+--------------------------+
                            |
                            v
+------------------------------------------------------+
|                 I2C Manager Layer                    |
|                                                      |
|   Generic bus creation and device registration       |
+---------------------------+--------------------------+
                            |
                            v
+------------------------------------------------------+
|                 ESP-IDF I2C Driver                   |
|                                                      |
|      i2c_new_master_bus()                            |
|      i2c_master_bus_add_device()                     |
|      i2c_master_transmit()                           |
|      i2c_master_receive()                            |
|      i2c_master_transmit_receive()                   |
+---------------------------+--------------------------+
                            |
                            v
+------------------------------------------------------+
|                  ESP32-S3 Hardware                   |
|                                                      |
| I2C_NUM_0                         I2C_NUM_1           |
| GPIO8  SDA                        GPIO10 SDA          |
| GPIO9  SCL                        GPIO11 SCL          |
+-------------------+                    +-------------+
                    |                    |
                    v                    v
                  OLED              Battery Monitor
```

---

# 4. Module Responsibilities

## 4.1 `board_i2c_config.h`

This module contains board-specific configuration.

Example:

```c
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
```

### Responsibility

It defines:

- Which ESP32-S3 I2C controller is used
- SDA GPIO
- SCL GPIO
- Default bus speed
- Logical bus identifiers

### It should not contain

- OLED initialization sequences
- Battery-monitor register definitions
- I2C transaction implementation
- Application logic

---

# 5. I2C Manager

Files:

```text
i2c_manager.h
i2c_manager.c
```

The I2C manager is the generic low-level bus abstraction.

## 5.1 Bus Configuration

```c
typedef struct {
    i2c_port_num_t port;
    gpio_num_t sda_gpio;
    gpio_num_t scl_gpio;
    uint32_t default_speed_hz;
} i2c_bus_config_t;
```

This makes the hardware assignment data-driven.

The manager does not contain code such as:

```c
if (device == OLED) {
    use I2C_NUM_0;
}
```

Instead, it receives the configuration.

---

## 5.2 Runtime Bus Object

```c
typedef struct {
    i2c_master_bus_handle_t handle;
    i2c_bus_config_t config;
} i2c_bus_t;
```

The object contains:

- ESP-IDF bus handle
- Original bus configuration

This lets higher layers keep the logical bus object without knowing how the bus was created.

---

# 6. I2C Manager Responsibilities

The manager performs two primary operations.

## 6.1 Create a Bus

```c
esp_err_t i2c_manager_init(i2c_bus_t *bus, const i2c_bus_config_t *config);
```

Flow:

```text
i2c_bus_config_t
       |
       v
i2c_manager_init()
       |
       v
i2c_master_bus_config_t
       |
       v
i2c_new_master_bus()
       |
       v
i2c_master_bus_handle_t
```

The manager translates the project-level configuration into ESP-IDF configuration.

---

## 6.2 Add a Device

```c
esp_err_t i2c_manager_add_device(const i2c_bus_t *bus,
                                 uint16_t device_address,
                                 uint32_t speed_hz,
                                 i2c_master_dev_handle_t *device);
```

Flow:

```text
Bus handle
    +
Device address
    +
Bus speed
    |
    v
i2c_device_config_t
    |
    v
i2c_master_bus_add_device()
    |
    v
Device handle
```

The resulting device handle is passed to the device driver.

---

# 7. I2C Service Layer

Files:

```text
i2c_service.h
i2c_service.c
```

The service layer owns the actual runtime bus instances.

```c
static i2c_bus_t s_i2c_buses[I2C_BUS_COUNT];
```

Initialization:

```c
esp_err_t i2c_service_init(void);
```

The service loops over the configured buses:

```text
g_i2c_bus_config[]
        |
        v
+------------------+
| I2C Service      |
+------------------+
        |
        +----> Bus 0 -> I2C_NUM_0 -> GPIO8/9
        |
        +----> Bus 1 -> I2C_NUM_1 -> GPIO10/11
```

This means application code does not need to manage global I2C handles itself.

---

# 8. Logical Bus Identification

The implementation uses:

```c
typedef enum {
    I2C_BUS_OLED,
    I2C_BUS_BATTERY,
    I2C_BUS_COUNT
} i2c_bus_id_t;
```

The application therefore refers to:

```c
I2C_BUS_OLED
I2C_BUS_BATTERY
```

instead of directly referring to:

```c
I2C_NUM_0
I2C_NUM_1
```

This is an important abstraction boundary.

For example, this hardware change:

```text
Before:
OLED -> I2C_NUM_0 -> GPIO8/9

After:
OLED -> I2C_NUM_1 -> GPIO10/11
```

requires a configuration change rather than a change to the generic I2C manager.

---

# 9. Device Driver Architecture

The device drivers should sit above the I2C service.

Example:

```text
+--------------------+
| OLED Driver        |
|                    |
| oled_init()        |
| oled_write()       |
| oled_clear()       |
+---------+----------+
          |
          | I2C device handle
          v
+--------------------+
| I2C Service        |
+--------------------+
```

Similarly:

```text
+---------------------------+
| Battery Monitor Driver    |
|                           |
| battery_init()            |
| battery_read_voltage()    |
| battery_read_soc()        |
+------------+--------------+
             |
             | I2C device handle
             v
+---------------------------+
| I2C Service               |
+---------------------------+
```

The drivers should not need to know:

```text
GPIO8
GPIO9
GPIO10
GPIO11
I2C_NUM_0
I2C_NUM_1
```

Those belong to the board configuration layer.

---

# 10. Example Initialization Flow

At boot:

```text
app_main()
    |
    v
i2c_service_init()
    |
    +----------------------------+
    |                            |
    v                            v
Initialize OLED bus       Initialize battery bus
    |                            |
    v                            v
I2C_NUM_0                  I2C_NUM_1
GPIO8/9                    GPIO10/11
    |                            |
    +-------------+--------------+
                  |
                  v
             I2C Ready
```

After the buses are initialized:

```text
oled_init()
      |
      v
Get I2C_BUS_OLED
      |
      v
Add OLED device
      |
      v
OLED device handle
```

and:

```text
battery_monitor_init()
      |
      v
Get I2C_BUS_BATTERY
      |
      v
Add battery device
      |
      v
Battery device handle
```

---

# 11. Runtime Transaction Flow

For a battery register read:

```text
Application
    |
    v
battery_read_voltage()
    |
    v
Battery driver
    |
    v
i2c_master_transmit_receive()
    |
    v
ESP-IDF I2C driver
    |
    v
I2C_NUM_1
    |
    +---- SDA GPIO10
    |
    +---- SCL GPIO11
    |
    v
Battery Monitor IC
```

For OLED communication:

```text
Application
    |
    v
oled_write()
    |
    v
OLED driver
    |
    v
i2c_master_transmit()
    |
    v
ESP-IDF I2C driver
    |
    v
I2C_NUM_0
    |
    +---- SDA GPIO8
    |
    +---- SCL GPIO9
    |
    v
OLED
```

---

# 12. Error Handling

The current manager uses ESP-IDF error propagation.

Example:

```c
ESP_RETURN_ON_FALSE(bus != NULL, ESP_ERR_INVALID_ARG, TAG, "bus is NULL");
ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "config is NULL");
```

and:

```c
ESP_RETURN_ON_ERROR(
    i2c_new_master_bus(&esp_config, &bus->handle),
    TAG,
    "Failed to create I2C bus"
);
```

The philosophy is:

```text
Lower layer detects error
        |
        v
Return esp_err_t
        |
        v
Caller decides recovery policy
```

The low-level manager should not unexpectedly restart the system or decide application-level recovery.

---

# 13. Pull-Up Consideration

I2C uses open-drain signaling, so SDA and SCL require pull-up resistors.

The configuration currently enables ESP-IDF internal pull-ups:

```c
.flags.enable_internal_pullup = true,
```

For the final hardware, external pull-ups may be preferable depending on:

- Bus capacitance
- PCB trace length
- Number of connected devices
- Bus speed
- Device electrical requirements

The hardware design should confirm whether the OLED and battery-monitor modules already contain pull-up resistors.

Avoid unintentionally placing many pull-ups in parallel.

---

# 14. Bus Speed Strategy

Current configuration:

```text
OLED            400 kHz
Battery monitor 100 kHz
```

These speeds are independent because the devices are placed on separate I2C controllers.

This is one advantage of the two-bus architecture.

For example:

```text
I2C_NUM_0
    |
    +-- OLED
    +-- 400 kHz

I2C_NUM_1
    |
    +-- Battery monitor
    +-- 100 kHz
```

A change to the OLED bus speed does not change the battery-monitor bus speed.

---

# 15. Why Two Independent I2C Buses

Using separate controllers provides isolation:

```text
OLED bus problem
      |
      X
I2C_NUM_0
      |
      X
OLED unavailable

I2C_NUM_1
      |
      |
      +---- Battery monitor still available
```

This is useful when the OLED and battery-monitor devices have different:

- Timing requirements
- Bus speeds
- Transaction rates
- Failure/recovery behavior

---

# 16. Directory Structure

Recommended project structure:

```text
project/
├── CMakeLists.txt
│
└── main/
    ├── CMakeLists.txt
    ├── main.c
    │
    ├── board/
    │   └── board_i2c_config.h
    │
    └── i2c/
        ├── i2c_manager.c
        ├── i2c_manager.h
        ├── i2c_service.c
        └── i2c_service.h
```

When the actual device drivers are added:

```text
project/
├── CMakeLists.txt
└── main/
    ├── CMakeLists.txt
    ├── main.c
    │
    ├── board/
    │   └── board_i2c_config.h
    │
    ├── i2c/
    │   ├── i2c_manager.c
    │   ├── i2c_manager.h
    │   ├── i2c_service.c
    │   └── i2c_service.h
    │
    ├── oled/
    │   ├── oled.c
    │   └── oled.h
    │
    └── battery/
        ├── battery_monitor.c
        └── battery_monitor.h
```

---

# 17. Dependency Direction

The intended dependency direction is:

```text
Application
    |
    v
Device Drivers
    |
    v
I2C Service
    |
    v
I2C Manager
    |
    v
ESP-IDF
```

Board configuration is injected into the I2C layer:

```text
Board Configuration
        |
        v
I2C Service / Manager
```

The important rule is:

```text
Device drivers must not depend on board GPIO definitions.
```

---

# 18. Adding Another I2C Bus

To add another bus, only the configuration and logical identifier need to change.

For example:

```c
typedef enum {
    I2C_BUS_OLED,
    I2C_BUS_BATTERY,
    I2C_BUS_SENSOR,
    I2C_BUS_COUNT
} i2c_bus_id_t;
```

Then add its configuration:

```c
[I2C_BUS_SENSOR] = {
    .port = I2C_NUM_0,
    .sda_gpio = GPIO_NUM_6,
    .scl_gpio = GPIO_NUM_7,
    .default_speed_hz = 400000,
},
```

The generic I2C manager does not need to be modified.

---

# 19. Adding Multiple Devices to One Bus

The architecture also supports multiple devices on a single I2C bus.

For example:

```text
I2C_NUM_0
    |
    +---- OLED       0x3C
    |
    +---- EEPROM     0x50
    |
    +---- Sensor     0x68
```

Each device gets a separate ESP-IDF device handle:

```text
Bus Handle
   |
   +---- Device Handle A -> 0x3C
   |
   +---- Device Handle B -> 0x50
   |
   +---- Device Handle C -> 0x68
```

This is why the manager provides both:

- Bus creation
- Device registration

---

# 20. Design Principles

The implementation follows these principles:

### Single Responsibility

`i2c_manager` manages I2C infrastructure.

It does not implement OLED or battery protocols.

### Configuration over hardcoding

Hardware-specific parameters are represented as configuration data.

### Encapsulation

ESP-IDF bus/device handles are not required throughout the application.

### Reusability

The same manager can be used for additional I2C devices.

### Separation of concerns

Hardware configuration and device protocol implementation remain independent.

---

# 21. Current Scope

The current implementation provides:

- Two configurable I2C master buses
- ESP32-S3 I2C controller selection
- Configurable SDA/SCL GPIO
- Configurable default bus speed
- I2C bus initialization
- I2C device registration
- Device-handle based communication
- Logical bus IDs
- Error propagation through `esp_err_t`

The following are intentionally outside the current I2C manager scope:

- OLED protocol implementation
- Battery IC register protocol
- Battery percentage calculation
- OLED graphics/font rendering
- Automatic I2C bus recovery
- Device-specific retry policy
- Application-level task scheduling

These should be implemented in their respective layers.

---

# 22. Future Enhancements

For a production version, the following can be considered:

1. **I2C bus recovery**
   - Detect SDA/SCL stuck conditions.
   - Generate recovery clocks if supported by the hardware design.
   - Reinitialize the controller.

2. **Device health checking**
   - Probe or identify devices during startup.
   - Report unavailable devices without necessarily stopping the entire application.

3. **Timeout policy**
   - Define standard transaction timeout values.
   - Keep device-specific policy outside the generic manager.

4. **Concurrency protection**
   - Required if multiple FreeRTOS tasks can access the same logical I2C resource outside the ESP-IDF driver's own synchronization guarantees.

5. **Diagnostic logging**
   - Add structured device/bus error logging.
   - Avoid excessive logs in high-frequency periodic transactions.

6. **Board variants**
   - Keep separate board configuration files when multiple PCB revisions use different GPIO mappings.

---

# 23. Summary

The implemented architecture deliberately separates **what the hardware is** from **how I2C is operated** and from **what each device's protocol means**.

```text
                    Board Configuration
                            |
                            v
                    +---------------+
                    |  I2C Service  |
                    +-------+-------+
                            |
                            v
                    +---------------+
                    | I2C Manager   |
                    +-------+-------+
                            |
                 +----------+----------+
                 |                     |
                 v                     v
              I2C_NUM_0            I2C_NUM_1
               GPIO8/9             GPIO10/11
                 |                     |
                 v                     v
                OLED             Battery Monitor
```

The main benefit is that a hardware change such as moving a device to another I2C controller or changing its GPIO pins is primarily a **configuration change**, not a change to the generic I2C infrastructure or device protocol implementation.

This architecture is intended to be the foundation for the next layer: implementing the actual OLED and battery-monitor drivers.
