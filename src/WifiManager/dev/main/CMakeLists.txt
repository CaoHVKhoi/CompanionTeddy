idf_component_register(
    SRCS
        "main.c"
        "wifi_manager.c"
        "wifi_provisioning.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_wifi
        esp_event
        esp_netif
        esp_http_server
        nvs_flash
)
