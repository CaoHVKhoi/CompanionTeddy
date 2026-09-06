# Companion Teddy Firmware

Companion Teddy is an ESP32-S3 firmware prototype for a connected teddy bear. It provides the embedded device layer for the Companion ecosystem: BLE provisioning, Wi-Fi connectivity, OLED status reporting, battery monitoring, button input, I2S audio capture, and local audio feedback.

This repository currently contains the device firmware only. The Companion mobile application, backend services, voice API, authentication service, and manufacturing tools are not included here.

## Current Status

The project builds successfully with PlatformIO for the `esp32-s3-devkitc-1` board.

Implemented:

- ESP32-S3 Arduino firmware
- OLED display initialization and status screens
- BQ27441 battery gauge integration
- BLE service advertising and provisioning writes
- Persistent storage for Wi-Fi credentials and an access token
- Wi-Fi station-mode connection
- I2S microphone and amplifier initialization
- Toggle-based audio recording with chunk processing
- Local tone-based audio feedback
- BLE status notifications

Not yet implemented:

- HTTPS communication with the Companion voice API
- Uploading recorded PCM audio
- Downloading and playing server-generated audio
- QR-code generation or QR-code scanning on the device
- Production claim-code verification
- Secure credential encryption or secure provisioning
- OTA firmware updates
- Audio streaming and noise processing
- Automated hardware tests

## Repository Structure

```text
01_CompanionTeddy/
├── .gitignore
├── .pio/                  # PlatformIO-generated build and dependency data
├── .vscode/
│   └── settings.json      # VS Code IntelliSense integration
├── include/               # Public module interfaces and device configuration
├── platformio.ini         # PlatformIO project configuration
├── README.md              # This document
└── src/
    ├── action_manager.cpp
    ├── audio_manager.cpp
    ├── battery_manager.cpp
    ├── ble_manager.cpp
    ├── display_manager.cpp
    ├── main.cpp           # Application coordinator
    └── wifi_manager.cpp
```

### `src/main.cpp`

This is the application coordinator. It initializes the modules, handles the two physical buttons, applies record-button debouncing, and runs the periodic battery/status update.

### `include/`

This directory contains the public interfaces for the firmware modules and the shared device configuration.

### Firmware Modules

- `device_config`: GPIO assignments, UUIDs, display settings, and audio constants.
- `display_manager`: SSD1306 initialization and status rendering.
- `battery_manager`: BQ27441 initialization and battery measurements.
- `wifi_manager`: Wi-Fi credentials, Preferences storage, connection state, and access token.
- `ble_manager`: BLE service, provisioning characteristic, status characteristic, and notifications.
- `audio_manager`: I2S initialization, reusable audio chunk buffer, recording state, tone playback, and voice API streaming hook.
- `action_manager`: Local action-button response behavior.

### `platformio.ini`

This file defines the target board, framework, serial monitor speed, and external libraries. The project uses:

- PlatformIO Espressif32 platform
- `esp32-s3-devkitc-1` board definition
- Arduino framework
- `115200` baud serial monitor
- Adafruit GFX Library
- Adafruit SSD1306 Library
- SparkFun BQ27441 library from its official GitHub ZIP archive

### `.pio/`

PlatformIO creates this directory automatically. It contains downloaded libraries, framework data, intermediate object files, firmware images, and build metadata. It should not be edited manually or committed to source control.

### `.vscode/settings.json`

The VS Code configuration selects PlatformIO as the C/C++ configuration provider:

```json
{
    "C_Cpp.default.configurationProvider": "platformio.platformio-ide"
}
```

This allows IntelliSense to resolve Arduino core headers and PlatformIO library headers such as `Arduino.h`, `I2S.h`, `Adafruit_SSD1306.h`, and `SparkFunBQ27441.h`.

### `.gitignore`

Generated PlatformIO output and VS Code IntelliSense caches are ignored. Source files, project configuration, and documentation remain trackable.

## Hardware Target

The configured target is an ESP32-S3 DevKitC-1:

```ini
board = esp32-s3-devkitc-1
framework = arduino
```

The firmware assumes the following external hardware:

- ESP32-S3 development board
- 128 x 64 SSD1306 OLED display
- BQ27441 LiPo fuel gauge
- I2S microphone
- I2S amplifier or DAC
- Record button
- Action button
- Battery and suitable power circuitry

The exact electrical behavior depends on the selected microphone, amplifier, display module, pull-up resistors, power supply, and board revision. The GPIO assignments in the firmware must be verified against the actual PCB before production.

## Pin Mapping

| Function | GPIO | Notes |
|---|---:|---|
| I2S bit clock | 4 | Shared I2S clock |
| I2S word select / frame sync | 5 | Shared I2S clock |
| I2S microphone data | 6 | Input data |
| I2S amplifier data | 7 | Output data |
| I2C SDA | 8 | OLED and BQ27441 bus |
| I2C SCL | 9 | OLED and BQ27441 bus |
| Record button | 14 | Configured with `INPUT_PULLUP` |
| Action button | 15 | Configured with `INPUT_PULLUP` |
| OLED I2C address | `0x3C` | SSD1306 address |

The buttons are treated as active-low inputs. A button is considered pressed when `digitalRead()` returns `LOW`.

## Firmware Components

### Arduino and ESP32 Core

The following headers come from the Arduino-ESP32 framework or ESP32 libraries:

- `Arduino.h`
- `Wire.h`
- `WiFi.h`
- `Preferences.h`
- `BLEDevice.h`
- `BLEServer.h`
- `BLEUtils.h`
- `BLE2902.h`
- `I2S.h`

The installed framework version provides the Arduino `I2SClass` API. The code uses the framework's `I2S.h`, `I2SClass` constructor, `setDataOutPin()`, `setDuplex()`, and `begin()` methods.

### OLED Display

The firmware uses:

- `Adafruit_GFX.h`
- `Adafruit_SSD1306.h`

The `drawStatus()` function renders:

- The product name
- A separator line
- Two status lines
- Battery percentage and voltage when the gauge is available

The display is initialized through the I2C bus at address `0x3C`.

### Battery Gauge

The firmware uses the SparkFun BQ27441 library through:

```cpp
#include <SparkFunBQ27441.h>
```

The library provides the global `lipo` instance. The firmware reads:

- State of charge through `lipo.soc()`
- Battery voltage through `lipo.voltage()`

The battery capacity is currently configured as `1000` through `lipo.setCapacity(1000)`. This value must be changed to match the actual battery capacity before production.

Battery status is refreshed every five seconds and is also included in BLE status notifications.

### Audio

The firmware initializes one `I2SClass` instance for duplex microphone and amplifier operation:

```cpp
I2SClass audioBus(0, 0, I2S_MIC_DATA_PIN, I2S_BCLK_PIN, I2S_WS_PIN);
```

Audio configuration:

- Sample rate: `16000 Hz`
- Sample format: signed 16-bit samples
- Intended channel layout: mono
- Audio chunk size: `512` bytes
- Recording duration is controlled by record-button release rather than a fixed RAM buffer

The firmware uses one reusable `int16_t` chunk buffer. It no longer allocates a three-second global recording buffer:

```text
512 bytes / 2 bytes per sample = 256 samples per chunk
```

`playTone()` generates local sine-wave tones and writes them to the I2S output. These tones are currently a placeholder for server-generated voice responses.

During recording, `captureAudioChunk()` reads one chunk and passes it to `streamAudioChunk()`. The current function is an integration hook and does not yet transmit data. Until an HTTPS or WebSocket transport is implemented, chunks are discarded after the callback returns. This prevents RAM overflow but does not preserve an unlimited recording locally.

## BLE Provisioning Contract

The device advertises using a name based on the device identifier:

```text
Companion-TEDDY-PROTO-001
```

The firmware creates one BLE service and two characteristics.

### UUIDs

| Item | UUID | Properties |
|---|---|---|
| BLE service | `8d9e0c10-8c53-4d08-a0c0-7a7eb45fc001` | Service |
| Provisioning characteristic | `8d9e0c10-8c53-4d08-a0c0-7a7eb45fc002` | Write |
| Status characteristic | `8d9e0c10-8c53-4d08-a0c0-7a7eb45fc003` | Read and notify |

### Provisioning Payload

The Companion application is expected to write this format to the provisioning characteristic:

```text
SSID|WIFI_PASSWORD|DEVICE_ACCESS_TOKEN
```

The firmware splits the payload at the first two pipe characters. It rejects the payload when:

- The SSID is empty
- The password is empty
- The access token is empty
- The required separators are missing

The current parser does not support pipe characters inside the individual values. A future protocol should use a structured and escaped format, such as JSON with strict length limits, or a binary framed protocol.

### Stored Preferences

The firmware stores values in the `companion` Preferences namespace:

| Key | Value |
|---|---|
| `ssid` | Wi-Fi network name |
| `password` | Wi-Fi password |
| `token` | Companion device access token |

The token is currently stored but is not yet used by an HTTPS client.

### BLE Status Payload

The status characteristic publishes a semicolon-separated string:

```text
device_id=TEDDY-PROTO-001;provisioned=1;wifi=1;battery=85
```

Fields:

- `device_id`: device identifier
- `provisioned`: `1` when Wi-Fi configuration exists, otherwise `0`
- `wifi`: `1` when connected, otherwise `0`
- `battery`: battery percentage

This is a prototype protocol. A versioned structured payload should be introduced before the mobile app and firmware are released independently.

## Runtime Behavior

### Boot Sequence

`setup()` performs the following operations:

1. Configures both buttons as active-low inputs with internal pull-ups.
2. Starts the I2C bus on GPIO 8 and GPIO 9.
3. Initializes the SSD1306 display.
4. Initializes the BQ27441 gauge.
5. Reads the initial battery percentage and voltage.
6. Configures the I2S output data pin.
7. Enables duplex I2S operation.
8. Starts BLE advertising.
9. Loads saved Wi-Fi credentials.
10. Attempts to connect to Wi-Fi for up to ten seconds.
11. Displays the initial device state.

If I2S initialization fails, the firmware displays an audio error and returns from `setup()` without starting BLE or Wi-Fi initialization.

### Main Loop

`loop()` continuously:

- Checks the record button.
- Starts recording when the record button is pressed.
- Reads audio chunks while recording.
- Stops recording when the button is released or the three-second limit is reached.
- Detects a new action-button press.
- Plays a local response tone for an action-button press.
- Refreshes battery state every five seconds.
- Publishes updated status over BLE.

### Recording Flow

```text
First record-button press
    -> Verify Wi-Fi is connected
    -> Clear recording position
    -> Set recording state
    -> Read I2S data in 512-byte chunks
Second record-button press
    -> Stop recording
    -> Show recorded status
    -> Finalize the streaming request
    -> Play local placeholder response
```

The record button uses a press-to-toggle interaction. A 40 ms debounce window prevents mechanical button bounce from starting and stopping a recording accidentally. The current recording data is processed chunk by chunk and is not written to flash.

### Wi-Fi Flow

On boot, the device reads the saved SSID and password from Preferences. If an SSID exists, it enters station mode and calls `WiFi.begin()`.

The connection attempt is synchronous and waits up to ten seconds. This is acceptable for a prototype but should be replaced with a non-blocking state machine in a production firmware.

## Security and Privacy Notes

The current source contains prototype identifiers and a fixed device ID. Before production:

- Assign a unique device ID during manufacturing.
- Do not embed production claim codes in firmware.
- Do not embed firmware-signing private keys in firmware.
- Protect BLE provisioning with authentication and authorization.
- Avoid transmitting Wi-Fi passwords in an unauthenticated BLE characteristic.
- Use encrypted transport for all backend communication.
- Store access tokens using an appropriate secure-storage strategy.
- Validate input length before copying or storing values.
- Consider secure boot and flash encryption on ESP32-S3.
- Add token rotation and device revocation support.
- Avoid logging credentials, audio, transcripts, or access tokens.

The current BLE provisioning characteristic accepts writes without an application-level authentication step. It should be treated as development-only functionality.

## Build and Development

### Prerequisites

Install:

- Visual Studio Code
- PlatformIO IDE extension
- An ESP32-S3 board compatible with `esp32-s3-devkitc-1`
- USB data cable

A local PlatformIO executable is available in the current development environment at:

```text
C:\Users\ADMIN\.platformio\penv\Scripts\platformio.exe
```

### Build from VS Code

Use the PlatformIO build command from the PlatformIO toolbar or command palette.

### Build from PowerShell

If the PlatformIO executable is not in `PATH`, run:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
```

For the current environment, the equivalent explicit path is:

```powershell
& "C:\Users\ADMIN\.platformio\penv\Scripts\platformio.exe" run
```

### Clean Build

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run --target clean
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
```

### Serial Monitor

The monitor speed is configured as `115200` baud:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor --baud 115200
```

The current firmware does not yet initialize `Serial`, so the serial monitor will not provide meaningful runtime logs until logging is added.

### Upload

After connecting the ESP32-S3 board, use the PlatformIO Upload command. The correct upload port may need to be selected manually depending on the board and operating system.

## Current Architecture

The firmware is now organized into focused modules:

```text
include/
├── audio_manager.h
├── battery_manager.h
├── ble_manager.h
├── device_config.h
├── display_manager.h
└── wifi_manager.h

src/
├── audio_manager.cpp
├── battery_manager.cpp
├── ble_manager.cpp
├── display_manager.cpp
├── main.cpp
└── wifi_manager.cpp
```

Recommended ownership:

- `device_config`: GPIO assignments, UUIDs, device constants, limits
- `display_manager`: OLED initialization and status rendering
- `battery_manager`: BQ27441 initialization and measurements
- `ble_manager`: BLE service, characteristics, payload validation, status publishing
- `wifi_manager`: credential loading, connection state, reconnect logic
- `audio_manager`: I2S initialization, recording, playback, buffer management
- `main.cpp`: lifecycle coordination and high-level state machine

## Recommended Product Roadmap

### Phase 1: Hardware Prototype

- Confirm every GPIO against the physical board.
- Confirm I2S microphone and amplifier wiring.
- Confirm OLED address and I2C pull-ups.
- Confirm BQ27441 calibration and battery capacity.
- Add serial diagnostics.
- Test button debounce.

### Phase 2: Companion Connectivity

- Define a versioned BLE protocol.
- Add authenticated provisioning.
- Implement device registration and claim-code verification.
- Add reliable reconnect behavior.
- Add explicit BLE error codes.

### Phase 3: Voice API

- Load the stored access token safely.
- Add HTTPS client support.
- Send 16-bit, 16 kHz mono PCM.
- Define request timeout and retry rules.
- Receive and decode returned audio.
- Add network and API error states.

### Phase 4: Production Hardening

- Enable secure boot where appropriate.
- Enable flash encryption where appropriate.
- Add OTA updates with signed firmware.
- Remove hardcoded prototype identifiers.
- Add watchdog and brownout recovery behavior.
- Add memory, power, and thermal measurements.
- Add manufacturing configuration tools.
- Add automated build and hardware-in-the-loop tests.

## Known Limitations

- The firmware is split into focused modules, while the voice transport remains an extension point.
- The voice API integration is currently a chunk-streaming extension point; the transport is not implemented yet.
- The access token is stored in Preferences without an additional protection layer.
- BLE provisioning is not authenticated at the application level.
- Wi-Fi connection is blocking for up to ten seconds.
- There is no reconnect state machine for Wi-Fi loss.
- There is no audio compression, noise reduction, or voice activity detection.
- The payload format cannot safely represent pipe characters in values.
- Audio chunks are discarded until the voice API transport is implemented.
- The board and external hardware wiring are not validated by this repository.
- Serial logging is not currently enabled in the firmware.

## Summary

`01_CompanionTeddy` is the embedded hardware foundation for Companion Teddy. It is responsible for making the physical teddy discoverable, configurable, connected, and able to capture and play audio.

The current implementation is a working firmware prototype with successful PlatformIO compilation. Its most important missing feature is the backend voice path: recorded audio is captured locally but is not yet uploaded to a Companion service, and responses are currently simulated with tones.
