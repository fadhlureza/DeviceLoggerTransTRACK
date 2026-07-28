#include "wifi_ap.h"
#include "constant.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include <string.h>
#include <stdio.h>

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
    }
}

void wifi_ap_init() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);

    char dynamic_ssid[32];
    snprintf(dynamic_ssid, sizeof(dynamic_ssid), "%s-%02X%02X", ESP_WIFI_SSID, mac[4], mac[5]);

    wifi_config_t wifi_config = {};
    strcpy((char *)wifi_config.ap.ssid, dynamic_ssid);
    wifi_config.ap.ssid_len = strlen(dynamic_ssid);
    strcpy((char *)wifi_config.ap.password, ESP_WIFI_PASS);
    wifi_config.ap.channel = ESP_WIFI_CHANNEL;
    wifi_config.ap.max_connection = ESP_MAX_STA_CONN;
    wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;

    if (strlen(ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    printf("[WiFi] AP Started with SSID: %s\n", dynamic_ssid);
}