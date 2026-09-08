#include "wifi_manager.h"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs.h"

static const char *TAG = "WIFI_MANAGER";

#define WIFI_NVS_NAMESPACE  "wifi"
#define WIFI_NVS_SSID       "ssid"
#define WIFI_NVS_PASSWORD   "password"

static bool s_wifi_connected = false;

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {

        case WIFI_EVENT_AP_START:
            ESP_LOGI(TAG, "SoftAP started");
            ESP_LOGI(TAG, "Open http://%s", WIFI_AP_IP);
            break;

        case WIFI_EVENT_AP_STACONNECTED:
            ESP_LOGI(TAG, "Station connected to SoftAP");
            break;

        case WIFI_EVENT_AP_STADISCONNECTED:
            ESP_LOGI(TAG, "Station disconnected from SoftAP");
            break;

        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "STA started");
            esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "STA connected");
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
            s_wifi_connected = false;
            ESP_LOGW(TAG, "STA disconnected");
            break;

        default:
            break;
        }
    }
    else if (event_base == IP_EVENT &&
             event_id == IP_EVENT_STA_GOT_IP) {

        ip_event_got_ip_t *event =
            (ip_event_got_ip_t *)event_data;

        s_wifi_connected = true;

        ESP_LOGI(TAG,
                 "STA got IP: " IPSTR,
                 IP2STR(&event->ip_info.ip));
    }
}

esp_err_t wifi_manager_init(void)
{
    esp_err_t ret;

    ret = esp_netif_init();

    if (ret != ESP_OK &&
        ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }

    ret = esp_event_loop_create_default();

    if (ret != ESP_OK &&
        ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ret = esp_wifi_init(&cfg);

    if (ret != ESP_OK &&
        ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }

    ESP_ERROR_CHECK(
        esp_event_handler_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            wifi_event_handler,
            NULL));

    ESP_ERROR_CHECK(
        esp_event_handler_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            wifi_event_handler,
            NULL));

    return ESP_OK;
}

esp_err_t wifi_manager_start_provisioning(void)
{
    esp_netif_t *ap_netif =
        esp_netif_create_default_wifi_ap();

    if (ap_netif == NULL) {
        return ESP_FAIL;
    }

    wifi_config_t ap_config = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .ssid_len = sizeof(WIFI_AP_SSID) - 1,
            .channel = 1,
            .password = WIFI_AP_PASSWORD,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK,
        }
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    ESP_ERROR_CHECK(
        esp_wifi_set_config(
            WIFI_IF_AP,
            &ap_config));

    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Provisioning mode started");
    ESP_LOGI(TAG, "SSID: %s", WIFI_AP_SSID);
    ESP_LOGI(TAG, "IP: %s", WIFI_AP_IP);

    return ESP_OK;
}

esp_err_t wifi_manager_save_credentials(const char *ssid,
                                        const char *password)
{
    if (ssid == NULL || password == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;

    esp_err_t ret = nvs_open(
        WIFI_NVS_NAMESPACE,
        NVS_READWRITE,
        &handle);

    if (ret != ESP_OK) {
        return ret;
    }

    ret = nvs_set_str(
        handle,
        WIFI_NVS_SSID,
        ssid);

    if (ret == ESP_OK) {
        ret = nvs_set_str(
            handle,
            WIFI_NVS_PASSWORD,
            password);
    }

    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }

    nvs_close(handle);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Wi-Fi credentials saved");
    }

    return ret;
}

esp_err_t wifi_manager_clear_credentials(void)
{
    nvs_handle_t handle;

    esp_err_t ret = nvs_open(
        WIFI_NVS_NAMESPACE,
        NVS_READWRITE,
        &handle);

    if (ret != ESP_OK) {
        return ret;
    }

    nvs_erase_key(handle, WIFI_NVS_SSID);
    nvs_erase_key(handle, WIFI_NVS_PASSWORD);

    ret = nvs_commit(handle);

    nvs_close(handle);

    return ret;
}

static esp_err_t wifi_manager_load_credentials(
    char *ssid,
    size_t ssid_size,
    char *password,
    size_t password_size)
{
    nvs_handle_t handle;

    esp_err_t ret = nvs_open(
        WIFI_NVS_NAMESPACE,
        NVS_READONLY,
        &handle);

    if (ret != ESP_OK) {
        return ret;
    }

    size_t required_ssid = ssid_size;
    size_t required_password = password_size;

    ret = nvs_get_str(
        handle,
        WIFI_NVS_SSID,
        ssid,
        &required_ssid);

    if (ret == ESP_OK) {
        ret = nvs_get_str(
            handle,
            WIFI_NVS_PASSWORD,
            password,
            &required_password);
    }

    nvs_close(handle);

    return ret;
}

esp_err_t wifi_manager_connect_sta(const char *ssid,
                                   const char *password)
{
    if (ssid == NULL || password == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    wifi_config_t sta_config = {0};

    strncpy(
        (char *)sta_config.sta.ssid,
        ssid,
        sizeof(sta_config.sta.ssid) - 1);

    strncpy(
        (char *)sta_config.sta.password,
        password,
        sizeof(sta_config.sta.password) - 1);

    esp_netif_t *sta_netif =
        esp_netif_create_default_wifi_sta();

    if (sta_netif == NULL) {
        return ESP_FAIL;
    }

    s_wifi_connected = false;

    ESP_ERROR_CHECK(
        esp_wifi_set_mode(WIFI_MODE_STA));

    ESP_ERROR_CHECK(
        esp_wifi_set_config(
            WIFI_IF_STA,
            &sta_config));

    ESP_ERROR_CHECK(
        esp_wifi_start());

    return ESP_OK;
}

esp_err_t wifi_manager_start(void)
{
    char ssid[33] = {0};
    char password[65] = {0};

    esp_err_t ret = wifi_manager_load_credentials(
        ssid,
        sizeof(ssid),
        password,
        sizeof(password));

    if (ret == ESP_OK) {
        ESP_LOGI(TAG,
                 "Stored Wi-Fi credentials found");

        return wifi_manager_connect_sta(
            ssid,
            password);
    }

    ESP_LOGI(TAG,
             "No stored Wi-Fi credentials");

    return wifi_manager_start_provisioning();
}

bool wifi_manager_is_connected(void)
{
    return s_wifi_connected;
}
