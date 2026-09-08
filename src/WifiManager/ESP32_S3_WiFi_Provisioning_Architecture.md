# ESP32-S3 Wi-Fi Provisioning Architecture

## 1. Overview

This document describes a Wi-Fi Manager architecture for an ESP32-S3 device that supports:

- First-time Wi-Fi provisioning
- SoftAP-based configuration
- HTTP configuration server
- Storage of Wi-Fi credentials in NVS
- Automatic connection to the user's Wi-Fi network
- Fallback to provisioning mode when credentials are missing or unusable
- Retry and reconnection handling

The architecture is intended for an ESP-IDF firmware project and is designed so that the Wi-Fi Manager can later be extended with BLE provisioning, captive portal behavior, Wi-Fi scanning, cloud credentials, and device configuration.

---

## 2. Main Use Case

The device initially has no known Wi-Fi credentials.

It starts its own Wi-Fi access point:

```text
                    ESP32-S3
                       |
                 Start SoftAP
                       |
                       v
              +------------------+
              |  MyDevice-XXXX   |
              |                  |
              |  192.168.4.1     |
              +--------+---------+
                       |
                     Wi-Fi
                       |
                       v
                     Phone
                       |
                    Browser
                       |
                       v
             http://192.168.4.1
                       |
                       v
              Wi-Fi Configuration
              +------------------+
              | SSID: [________] |
              | PW:   [________] |
              |                  |
              |  [   Connect ]   |
              +------------------+
```

After receiving valid credentials, the ESP32:

1. Stores the credentials in NVS.
2. Stops provisioning mode.
3. Starts Wi-Fi Station mode.
4. Connects to the configured access point.
5. Reports the acquired IP address.
6. Enters normal application operation.

---

## 3. High-Level Architecture

```text
+-------------------------------------------------------------+
|                        Application                          |
|                                                             |
|  AI / Voice / Cloud / Mobile / Battery / Device Control    |
+-----------------------------+-------------------------------+
                              |
                              v
+-------------------------------------------------------------+
|                       Wi-Fi Manager                         |
|                                                             |
|  - Wi-Fi state machine                                      |
|  - Provisioning control                                     |
|  - Connection management                                    |
|  - Retry/reconnect policy                                   |
|  - Network status                                           |
+-----------------------------+-------------------------------+
                              |
                              v
+-------------------------------------------------------------+
|                    Wi-Fi Provisioning                       |
|                                                             |
|  - SoftAP                                                   |
|  - HTTP Server                                              |
|  - Configuration API                                        |
|  - Provisioning timeout                                     |
+-----------------------------+-------------------------------+
                              |
                              v
+-------------------------------------------------------------+
|                       Wi-Fi HAL / ESP-IDF                   |
|                                                             |
|  esp_wifi_*                                                 |
|  esp_netif_*                                                |
|  esp_event_*                                                |
+-----------------------------+-------------------------------+
                              |
                              v
+-------------------------------------------------------------+
|                       ESP32-S3 Wi-Fi                        |
+-------------------------------------------------------------+
```

NVS is used by the Wi-Fi Manager as persistent storage:

```text
                  +----------------+
                  |  Wi-Fi Manager |
                  +-------+--------+
                          |
                          v
                  +---------------+
                  |      NVS      |
                  |               |
                  | SSID          |
                  | Password      |
                  +---------------+
```

---

## 4. Boot-Time Decision

At startup, the Wi-Fi Manager checks whether valid Wi-Fi credentials exist.

```text
                         Boot
                           |
                           v
                  Read credentials
                       from NVS
                           |
                 +---------+---------+
                 |                   |
             Credentials          No credentials
               found                  |
                 |                    |
                 v                    v
             STA Mode           Provisioning Mode
                 |                    |
                 v                    v
        Connect to AP             Start SoftAP
                 |                    |
          +------+-------+            |
          |              |            |
       Success        Failure         |
          |              |            |
          v              v            v
       NORMAL          Retry       192.168.4.1
                         |
                         |
                    Retry limit /
                    provisioning
```

A practical policy is:

- No credentials -> enter provisioning mode.
- Credentials found -> try STA mode.
- Temporary connection failure -> retry.
- Repeated failure -> optionally enter provisioning mode.
- User explicitly requests reconfiguration -> enter provisioning mode.

---

## 5. Wi-Fi States

The Wi-Fi Manager should have an explicit state machine.

Recommended states:

```text
WIFI_STATE_INIT
WIFI_STATE_PROVISIONING
WIFI_STATE_CONNECTING
WIFI_STATE_CONNECTED
WIFI_STATE_DISCONNECTED
WIFI_STATE_ERROR
```

Example state transitions:

```text
                 +------+
                 | INIT |
                 +--+---+
                    |
          +---------+---------+
          |                   |
     No credentials      Credentials exist
          |                   |
          v                   v
 +----------------+     +-------------+
 | PROVISIONING   |     | CONNECTING  |
 +-------+--------+     +------+------+
         |                     |
         | credentials         |
         | received            |
         v                     |
 +----------------+            |
 | CONNECTING     |<-----------+
 +-------+--------+
         |
    +----+----+
    |         |
 Success    Failure
    |         |
    v         v
CONNECTED   DISCONNECTED
    |         |
    |         +----> RETRY
    |
    v
 NORMAL
```

The exact state names can be adjusted to match the project's coding standard.

---

## 6. Provisioning Mode

Provisioning mode uses ESP32 SoftAP.

Example:

```text
SSID:     MyDevice-XXXX
Password: device-specific password
IP:       192.168.4.1
```

The phone connects directly to the ESP32.

The ESP32 acts as:

- Wi-Fi access point
- DHCP server
- HTTP server

The phone then accesses:

```text
http://192.168.4.1
```

The configuration page can provide:

- Wi-Fi SSID
- Wi-Fi password
- Device name
- Optional cloud configuration
- Optional device-specific settings

---

## 7. HTTP Configuration Flow

```text
Phone                              ESP32-S3
 |                                    |
 |---- Connect to SoftAP ------------>|
 |                                    |
 |---- GET / ------------------------>|
 |                                    |
 |<--- Configuration HTML ------------|
 |                                    |
 | User enters SSID/password          |
 |                                    |
 |---- POST /save ------------------->|
 |     ssid=...                       |
 |     password=...                   |
 |                                    |
 |                         Validate data
 |                                    |
 |                         Save to NVS
 |                                    |
 |<--- Configuration accepted --------|
 |                                    |
 |                         Stop SoftAP
 |                         Start STA
 |                                    |
 |                         Connect AP
 |                                    |
 |                         GOT_IP
 |                                    |
```

---

## 8. Recommended HTTP Endpoints

A simple implementation can use:

| Method | Endpoint | Purpose |
|---|---|---|
| GET | `/` | Configuration web page |
| POST | `/save` | Save Wi-Fi configuration |
| GET | `/status` | Return Wi-Fi state |
| POST | `/reset` | Clear Wi-Fi configuration |
| GET | `/scan` | Return available Wi-Fi networks |

Example:

```text
GET /status

{
    "state": "connected",
    "ip": "192.168.1.25"
}
```

The `/scan` endpoint can be added later if the UI should allow the user to select an available network instead of manually entering the SSID.

---

## 9. NVS Storage

The Wi-Fi credentials should persist across resets.

Example logical NVS namespace:

```text
Namespace: wifi

Key              Example
--------------------------------
ssid              MyHomeWiFi
password          MyPassword
configured        true
```

The boot sequence becomes:

```text
Boot
 |
 v
NVS init
 |
 v
Read "configured"
 |
 +---- false ----> Provisioning
 |
 +---- true -----> Read SSID/password
                    |
                    v
                  STA
```

For production firmware, avoid logging the Wi-Fi password.

Bad:

```text
ESP_LOGI(TAG, "Password: %s", password);
```

Good:

```text
ESP_LOGI(TAG, "Wi-Fi credentials received");
```

---

## 10. Wi-Fi Manager API

A clean public interface can be kept small:

```c
esp_err_t wifi_manager_init(void);

esp_err_t wifi_manager_start(void);

esp_err_t wifi_manager_start_provisioning(void);

esp_err_t wifi_manager_connect_sta(const char *ssid,
                                   const char *password);

esp_err_t wifi_manager_clear_credentials(void);

bool wifi_manager_is_connected(void);
```

The application should not need to directly call `esp_wifi_*()` functions.

For example:

```c
wifi_manager_start();
```

rather than:

```c
esp_wifi_set_mode(...);
esp_wifi_set_config(...);
esp_wifi_start();
```

This keeps ESP-IDF-specific details inside the Wi-Fi Manager.

---

## 11. Recommended Module Separation

A production implementation can be divided into:

```text
main/
|
+-- wifi_manager/
|   |
|   +-- wifi_manager.c
|   +-- wifi_manager.h
|   |
|   +-- wifi_state_machine.c
|   +-- wifi_state_machine.h
|   |
|   +-- wifi_storage.c
|   +-- wifi_storage.h
|   |
|   +-- wifi_provisioning.c
|   +-- wifi_provisioning.h
|
+-- application/
|   |
|   +-- app_main.c
|
+-- components/
    |
    +-- ...
```

Responsibilities:

### `wifi_manager`

Coordinates the entire Wi-Fi subsystem.

### `wifi_state_machine`

Owns state transitions.

### `wifi_storage`

Handles NVS operations.

### `wifi_provisioning`

Handles:

- SoftAP
- HTTP server
- Configuration requests

This separation makes the system easier to test and maintain.

---

## 12. Recommended Runtime Architecture

```text
                         Application
                              |
                              | Events / API
                              v
                    +--------------------+
                    |   Wi-Fi Manager    |
                    +---------+----------+
                              |
              +---------------+---------------+
              |               |               |
              v               v               v
       State Machine       Storage       Provisioning
              |               |               |
              |              NVS          HTTP + SoftAP
              |                               |
              +---------------+---------------+
                              |
                              v
                         ESP-IDF Wi-Fi
                              |
                              v
                          ESP32-S3
```

---

## 13. Error Handling

Important failure scenarios should be explicitly handled.

### AP cannot be started

```text
Start SoftAP
     |
     v
Failure
     |
     v
WIFI_STATE_ERROR
     |
     v
Retry / report error
```

### STA connection fails

```text
CONNECTING
    |
    v
Connection failed
    |
    v
Retry
    |
    +----> Success
    |
    +----> Retry limit reached
                |
                v
          Provisioning Mode
```

### Phone disconnects during provisioning

The ESP32 should normally remain in provisioning mode.

```text
SoftAP
  |
  +--- Phone connected
  |
  +--- Phone disconnected
            |
            v
       Keep SoftAP
```

The user can reconnect without restarting the device.

---

## 14. Provisioning Timeout

A production device should consider a provisioning timeout.

Example:

```text
Boot
 |
 v
Provisioning
 |
 |---- 5 minutes ----|
 |
 v
No configuration
 |
 v
Sleep / retry / normal fallback
```

The exact behavior depends on the product requirements.

For a device that is expected to remain available for setup, the SoftAP can instead remain active indefinitely.

---

## 15. Reset / Reconfiguration

A mechanism should exist to erase stored credentials.

Possible triggers:

- Physical button
- Long button press
- Mobile application command
- Factory reset
- Configuration command

Example:

```text
Long press button
       |
       v
Clear NVS Wi-Fi credentials
       |
       v
Restart Wi-Fi
       |
       v
Start SoftAP
       |
       v
192.168.4.1
```

This is particularly useful during development and manufacturing.

---

## 16. Security Considerations

For development:

```text
SSID:     MyDevice-Config
Password: 12345678
```

may be acceptable.

For production, use a stronger strategy.

Possible options:

### Option A: Device-specific SoftAP password

```text
MyDevice-A83F
Password: <device-specific>
```

### Option B: Temporary provisioning credential

Generate a temporary credential when provisioning starts.

### Option C: BLE-assisted provisioning

Use BLE to authenticate/configure the device and avoid exposing a permanent SoftAP password.

The provisioning interface should also validate:

- SSID length
- Password length
- HTTP input length
- URL encoding
- Invalid characters
- Request size
- Concurrent requests

Never log Wi-Fi passwords.

---

## 17. Captive Portal Extension

A better user experience can be implemented later.

Instead of requiring the user to manually type:

```text
http://192.168.4.1
```

the device can provide captive portal behavior.

Flow:

```text
Phone connects to:

MyDevice-XXXX

        |
        v

Phone detects captive portal

        |
        v

Configuration page automatically opens
```

This requires additional DNS handling and HTTP redirection.

It is an enhancement rather than a requirement for the initial implementation.

---

## 18. Wi-Fi Scanning Extension

The provisioning UI can also scan for nearby networks.

Example:

```text
Available networks:

[ HomeWiFi        ] -75 dBm
[ OfficeWiFi       ] -61 dBm
[ MyPhone          ] -80 dBm
[ GuestWiFi        ] -70 dBm
```

The phone selects:

```text
HomeWiFi
```

and only needs to enter the password.

The ESP32 can expose:

```text
GET /scan
```

with a response such as:

```json
{
    "networks": [
        {
            "ssid": "HomeWiFi",
            "rssi": -75
        },
        {
            "ssid": "OfficeWiFi",
            "rssi": -61
        }
    ]
}
```

---

## 19. Recommended Final Architecture for the AI Toy

For the planned ESP32-S3 device, a scalable architecture is:

```text
+-------------------------------------------------------------+
|                     Application Layer                       |
|                                                             |
|  Voice | AI | Battery | Device | Cloud | Mobile           |
+-------------------------------+-----------------------------+
                                |
                                v
+-------------------------------------------------------------+
|                   Connectivity Manager                      |
|                                                             |
|        Wi-Fi Manager          BLE Manager                  |
+------------------+-------------------------+----------------+
                   |                         |
                   v                         v
          Wi-Fi Provisioning          BLE Provisioning
                   |                         |
                   v                         v
          SoftAP + HTTP                 BLE GATT
                   |                         |
                   +------------+------------+
                                |
                                v
                         Configuration
                                |
                                v
                              NVS
                                |
                                v
                      Network Credentials
```

Later, the Connectivity Manager can decide whether the device should use:

```text
Wi-Fi
  |
  +-- Provisioning
  +-- Normal STA
  +-- Reconnect
  +-- Cloud connection

BLE
  |
  +-- Provisioning
  +-- Mobile communication
  +-- Device control
```

This avoids coupling the application directly to ESP-IDF networking APIs.

---

## 20. Development Strategy

Recommended development order:

### Phase 1 — SoftAP

Implement:

```text
ESP32-S3
   |
   +-- SoftAP
   |
   +-- 192.168.4.1
```

Verify that a phone can connect.

### Phase 2 — HTTP server

Implement:

```text
GET /
```

and return a simple HTML page.

### Phase 3 — Configuration

Implement:

```text
POST /save
```

and parse SSID/password.

### Phase 4 — NVS

Store:

```text
SSID
Password
Configured flag
```

### Phase 5 — STA

After configuration:

```text
SoftAP
   |
   v
Stop AP
   |
   v
STA
   |
   v
Connect
   |
   v
GOT_IP
```

### Phase 6 — State machine

Add explicit state handling and retries.

### Phase 7 — Recovery

Implement:

- Connection retry
- Provisioning fallback
- Factory reset
- Provisioning timeout

### Phase 8 — Production security

Add:

- Device-specific provisioning credentials
- Input validation
- Credential protection
- Secure provisioning policy

### Phase 9 — User experience

Add:

- Wi-Fi scanning
- Captive portal
- Device identification
- Better configuration UI

---

## 21. Key Design Principle

The most important architectural rule is:

```text
Application
     |
     |  Do not directly call ESP-IDF Wi-Fi APIs
     v
Wi-Fi Manager
     |
     v
ESP-IDF / Wi-Fi HAL
```

This gives you a clean separation between:

- Application logic
- Connectivity management
- Provisioning
- Persistent storage
- ESP32-S3 hardware/ESP-IDF implementation

It also makes the Wi-Fi Manager easier to simulate and unit-test on a PC before integrating it with the real ESP32-S3 hardware.

---

## 22. Initial Implementation Scope

For the first implementation, keep the scope to:

```text
+---------------------------------------+
| Wi-Fi Manager                         |
|                                       |
| 1. Initialize Wi-Fi                  |
| 2. Read credentials from NVS          |
| 3. Start STA if credentials exist     |
| 4. Start SoftAP otherwise              |
| 5. Start HTTP server                  |
| 6. Receive SSID/password              |
| 7. Store credentials                  |
| 8. Switch to STA                      |
| 9. Connect to configured AP           |
| 10. Retry when disconnected           |
+---------------------------------------+
```

Do not add BLE provisioning, captive portal, cloud connectivity, or advanced security until this basic flow is stable.

This provides a solid foundation for the ESP32-S3 firmware architecture.
