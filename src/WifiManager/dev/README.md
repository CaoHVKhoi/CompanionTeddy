# ESP32-S3 Wi-Fi Provisioning Example

Minimal ESP-IDF example implementing:

- ESP32-S3 SoftAP
- Static/default SoftAP address `192.168.4.1`
- HTTP configuration page
- POST of Wi-Fi SSID/password
- NVS credential storage
- STA connection support
- Boot-time selection between stored STA credentials and provisioning mode

## Build

From the project directory:

```bash
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

## Current test mode

`main.c` currently starts provisioning mode directly:

```c
wifi_manager_start_provisioning();
wifi_provisioning_start();
```

Connect a phone/PC to:

```text
SSID: MyDevice-Config
Password: 12345678
```

Then open:

```text
http://192.168.4.1
```

Enter the target Wi-Fi SSID/password.

## Production boot flow

Replace the direct provisioning calls with:

```c
wifi_manager_start();
```

However, the example intentionally leaves the AP-to-STA transition as a next architecture step. A production implementation should coordinate that transition through the Wi-Fi Manager/state machine after the HTTP response is completed.

## Important

This is a reference implementation, not a production-ready provisioning system.

Before production use, add:

- Robust HTTP form parsing
- Complete URL decoding/validation
- AP credential generation
- Secure provisioning
- Connection timeout/retry policy
- AP-to-STA transition task/state machine
- Factory reset
- Provisioning timeout
- Captive portal if desired
- Credential encryption/protection as required by the product
- Avoid logging sensitive credentials
