#include "wifi_provisioning.h"
#include "wifi_manager.h"

#include <string.h>
#include <stdlib.h>

#include "esp_http_server.h"
#include "esp_log.h"

static const char *TAG = "WIFI_PROVISION";

static httpd_handle_t s_server = NULL;

static const char *HTML_PAGE =
"<!DOCTYPE html>"
"<html>"
"<head>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>Device Wi-Fi Setup</title>"
"<style>"
"body{font-family:Arial;margin:40px;max-width:500px}"
"input{width:100%;padding:10px;margin:8px 0 16px;box-sizing:border-box}"
"button{padding:12px 20px}"
"</style>"
"</head>"
"<body>"
"<h2>Wi-Fi Configuration</h2>"
"<form action='/save' method='POST'>"
"<label>Wi-Fi SSID</label>"
"<input name='ssid' type='text' maxlength='32' required>"
"<label>Password</label>"
"<input name='password' type='password' maxlength='63'>"
"<button type='submit'>Connect</button>"
"</form>"
"</body>"
"</html>";

static void url_decode(char *dst,
                       size_t dst_size,
                       const char *src)
{
    size_t di = 0;

    while (*src != '\0' &&
           di + 1 < dst_size) {

        if (*src == '+') {
            dst[di++] = ' ';
            src++;
        }
        else if (*src == '%' &&
                 src[1] != '\0' &&
                 src[2] != '\0') {

            char hex[3] = {
                src[1],
                src[2],
                '\0'
            };

            char *endptr = NULL;
            long value = strtol(hex, &endptr, 16);

            if (endptr == hex + 2) {
                dst[di++] = (char)value;
                src += 3;
            }
            else {
                dst[di++] = *src++;
            }
        }
        else {
            dst[di++] = *src++;
        }
    }

    dst[di] = '\0';
}

static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");

    return httpd_resp_send(
        req,
        HTML_PAGE,
        HTTPD_RESP_USE_STRLEN);
}

static esp_err_t save_post_handler(httpd_req_t *req)
{
    if (req->content_len <= 0 ||
        req->content_len >= 256) {

        httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Invalid request");

        return ESP_OK;
    }

    char buffer[256] = {0};

    int received = httpd_req_recv(
        req,
        buffer,
        req->content_len);

    if (received <= 0) {
        httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Failed to receive request");

        return ESP_OK;
    }

    buffer[received] = '\0';

    char ssid_encoded[100] = {0};
    char password_encoded[150] = {0};

    int matched = sscanf(
        buffer,
        "ssid=%99[^&]&password=%149[^\r\n]",
        ssid_encoded,
        password_encoded);

    if (matched != 2) {
        httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Invalid form data");

        return ESP_OK;
    }

    char ssid[33] = {0};
    char password[65] = {0};

    url_decode(
        ssid,
        sizeof(ssid),
        ssid_encoded);

    url_decode(
        password,
        sizeof(password),
        password_encoded);

    if (strlen(ssid) == 0) {
        httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "SSID cannot be empty");

        return ESP_OK;
    }

    ESP_LOGI(TAG,
             "Received Wi-Fi configuration for SSID: %s",
             ssid);

    esp_err_t ret =
        wifi_manager_save_credentials(
            ssid,
            password);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG,
                 "Failed to save credentials: %s",
                 esp_err_to_name(ret));

        httpd_resp_send_err(
            req,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "Failed to save configuration");

        return ESP_OK;
    }

    const char *response =
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta name='viewport' "
        "content='width=device-width,initial-scale=1'>"
        "</head>"
        "<body>"
        "<h2>Configuration saved</h2>"
        "<p>The device is now connecting to your Wi-Fi.</p>"
        "</body>"
        "</html>";

    httpd_resp_set_type(req, "text/html");

    httpd_resp_send(
        req,
        response,
        HTTPD_RESP_USE_STRLEN);

    /*
     * For a production implementation, do not switch immediately
     * from AP to STA before the HTTP response has been transmitted.
     *
     * A dedicated Wi-Fi Manager task/event can perform the transition.
     */
    return ESP_OK;
}

esp_err_t wifi_provisioning_start(void)
{
    httpd_config_t config =
        HTTPD_DEFAULT_CONFIG();

    esp_err_t ret =
        httpd_start(&s_server, &config);

    if (ret != ESP_OK) {
        return ret;
    }

    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler,
        .user_ctx = NULL
    };

    httpd_uri_t save = {
        .uri = "/save",
        .method = HTTP_POST,
        .handler = save_post_handler,
        .user_ctx = NULL
    };

    ESP_ERROR_CHECK(
        httpd_register_uri_handler(
            s_server,
            &root));

    ESP_ERROR_CHECK(
        httpd_register_uri_handler(
            s_server,
            &save));

    ESP_LOGI(TAG,
             "HTTP provisioning server started");

    return ESP_OK;
}
