#include "wifi_ap.h"
#include "constant.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include <string.h>
#include <stdio.h>

static bool s_wifi_ap_enabled = false;
static uint8_t s_connected_clients = 0;

static TimerHandle_t s_no_client_timer = NULL;
static TimerHandle_t s_disconnect_timer = NULL;

static void no_client_timer_cb(TimerHandle_t xTimer) {
    printf("[WiFi] 5-minute no-client timeout expired. Disabling AP.\n");
    wifi_ap_disable();
}

static void disconnect_timer_cb(TimerHandle_t xTimer) {
    printf("[WiFi] 2-minute disconnect timeout expired. Disabling AP.\n");
    wifi_ap_disable();
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        s_connected_clients++;
        printf("[WiFi] Client connected. Total: %d\n", s_connected_clients);
        
        // Stop both timers when a client connects
        if (s_no_client_timer != NULL) {
            xTimerStop(s_no_client_timer, 0);
        }
        if (s_disconnect_timer != NULL) {
            xTimerStop(s_disconnect_timer, 0);
        }
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        if (s_connected_clients > 0) {
            s_connected_clients--;
        }
        printf("[WiFi] Client disconnected. Total: %d\n", s_connected_clients);

        // When all clients disconnect and AP is enabled, start 2-minute disconnect timer
        if (s_connected_clients == 0 && s_wifi_ap_enabled) {
            if (s_disconnect_timer != NULL) {
                xTimerStart(s_disconnect_timer, 0);
                printf("[WiFi] 2-minute disconnect timer started.\n");
            }
        }
    }
}

void wifi_ap_init(void) {
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
    wifi_config.ap.max_connection = 1; // Strictly limit max clients to 1
    wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;

    if (strlen(ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));

    // Create FreeRTOS Timers (5 min & 2 min)
    s_no_client_timer = xTimerCreate("wifi_5min_timer", pdMS_TO_TICKS(5 * 60 * 1000), pdFALSE, NULL, no_client_timer_cb);
    s_disconnect_timer = xTimerCreate("wifi_2min_timer", pdMS_TO_TICKS(2 * 60 * 1000), pdFALSE, NULL, disconnect_timer_cb);

    printf("[WiFi] Initialized AP configuration. Max conn: 1. SSID: %s\n", dynamic_ssid);

    // =========================================================================
    // BOOT BEHAVIOR CONFIGURATION (Controlled by WIFI_AP_ENABLE_ON_BOOT in constant.h)
    // Set WIFI_AP_ENABLE_ON_BOOT to true for bench testing without ignition signal.
    // Set to false for in-vehicle mode (AP starts ONLY when Ignition turns ON).
    // =========================================================================
#if WIFI_AP_ENABLE_ON_BOOT
    wifi_ap_enable();
#endif
}

void wifi_ap_enable(void) {
    if (s_wifi_ap_enabled) {
        return;
    }

    s_connected_clients = 0;
    esp_err_t err = esp_wifi_start();
    if (err == ESP_OK) {
        s_wifi_ap_enabled = true;
        printf("[WiFi] AP Enabled & Started broadcasting.\n");

        // Start 5-minute no-client timer
        if (s_no_client_timer != NULL) {
            xTimerStop(s_no_client_timer, 0);
            xTimerStart(s_no_client_timer, 0);
            printf("[WiFi] 5-minute no-client timer started.\n");
        }
        if (s_disconnect_timer != NULL) {
            xTimerStop(s_disconnect_timer, 0);
        }
    } else {
        printf("[WiFi] Failed to start AP: %s\n", esp_err_to_name(err));
    }
}

void wifi_ap_disable(void) {
    if (!s_wifi_ap_enabled) {
        return;
    }

    if (s_no_client_timer != NULL) {
        xTimerStop(s_no_client_timer, 0);
    }
    if (s_disconnect_timer != NULL) {
        xTimerStop(s_disconnect_timer, 0);
    }

    esp_err_t err = esp_wifi_stop();
    if (err == ESP_OK || err == ESP_ERR_WIFI_NOT_STARTED) {
        s_wifi_ap_enabled = false;
        s_connected_clients = 0;
        printf("[WiFi] AP Disabled.\n");
    } else {
        printf("[WiFi] Failed to stop AP: %s\n", esp_err_to_name(err));
    }
}

bool wifi_ap_is_enabled(void) {
    return s_wifi_ap_enabled;
}