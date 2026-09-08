#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include "esp_err.h"

#define WIFI_AP_SSID        "MyDevice-Config"
#define WIFI_AP_PASSWORD    "12345678"

#define WIFI_AP_IP          "192.168.4.1"

esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_start(void);
esp_err_t wifi_manager_start_provisioning(void);
esp_err_t wifi_manager_connect_sta(const char *ssid, const char *password);
esp_err_t wifi_manager_save_credentials(const char *ssid, const char *password);
esp_err_t wifi_manager_clear_credentials(void);
bool wifi_manager_is_connected(void);

#endif /* WIFI_MANAGER_H */
