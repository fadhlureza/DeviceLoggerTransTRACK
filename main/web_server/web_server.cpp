#include "web_server.h"
#include "constant.h"
#include "sd_logger.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

extern const char index_html_start[] asm("_binary_index_html_start");
extern const char style_css_start[] asm("_binary_style_css_start");
extern const char script_js_start[] asm("_binary_script_js_start");

#define BASIC_AUTH_B64 "Basic YWRtaW46YWRtaW4="

static esp_err_t check_auth(httpd_req_t *req) {
    char buf[128];
    int len = httpd_req_get_hdr_value_len(req, "Authorization");
    if (len > 0 && len < sizeof(buf)) {
        if (httpd_req_get_hdr_value_str(req, "Authorization", buf, sizeof(buf)) == ESP_OK) {
            if (strcmp(buf, BASIC_AUTH_B64) == 0) return ESP_OK;
        }
    }
    httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"TRD Device Monitor\"");
    httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    return ESP_FAIL;
}

static esp_err_t index_handler(httpd_req_t *req) {
    if (check_auth(req) != ESP_OK) return ESP_FAIL;
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req, index_html_start);
    return ESP_OK;
}

static esp_err_t css_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/css");
    httpd_resp_sendstr(req, style_css_start);
    return ESP_OK;
}

static esp_err_t js_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_sendstr(req, script_js_start);
    return ESP_OK;
}

static esp_err_t data_handler(httpd_req_t *req) {
    if (check_auth(req) != ESP_OK) return ESP_FAIL;

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    char rtc_time_str[16];
    strftime(rtc_time_str, sizeof(rtc_time_str), "%H:%M:%S", &timeinfo);

    char resp_str[650];
    snprintf(resp_str, sizeof(resp_str), 
             "{\"vib_raw_g\": %.3f, \"vib_uncalib_ms2\": %.3f, \"vib_calib_ms2\": %.3f, \"accX\": %.3f, \"accY\": %.3f, \"accZ\": %.3f, \"pitch\": %.3f, \"roll\": %.3f, \"yaw\": %.3f, \"fuel_voltage\": %.3f, \"voltage\": %.3f, \"acc_voltage\": %.3f, \"temperature\": %.3f, \"is_logging\": %s, \"rate\": %d, \"sd_ready\": %s, \"sd_used_perc\": %.1f, \"batt_perc\": %.1f, \"ignition\": %s, \"rtc_time\": \"%s\"}", 
             g_curr_vib_raw_g, g_curr_vib_uncalib_ms2, g_curr_vib_calib_ms2, g_curr_accX_ms2, g_curr_accY_ms2, g_curr_accZ_ms2, g_curr_pitch, g_curr_roll, g_curr_yaw, g_curr_fuel_raw, g_curr_voltage, g_curr_acc_voltage, g_curr_temp_c,
             g_is_logging ? "true" : "false", g_sampling_rate_ms, g_sd_card_ready ? "true" : "false",
             g_sd_used_perc, g_batt_perc, g_ignition ? "true" : "false", rtc_time_str);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, resp_str);
    return ESP_OK;
}

static esp_err_t config_handler(httpd_req_t *req) {
    if (check_auth(req) != ESP_OK) return ESP_FAIL;
    char buf[256];
    int ret, remaining = req->content_len;
    
    if (remaining >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Payload too large");
        return ESP_FAIL;
    }
    
    if ((ret = httpd_req_recv(req, buf, remaining)) <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    cJSON *json = cJSON_Parse(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *is_log = cJSON_GetObjectItem(json, "is_logging");
    if (cJSON_IsBool(is_log)) {
        bool new_state = cJSON_IsTrue(is_log);
        if (new_state && !g_is_logging) {
            if (g_sd_card_ready && sd_start_new_log()) {
                g_is_logging = true;
            } else {
                g_is_logging = false;
            }
        }
        if (!new_state && g_is_logging) {
            sd_stop_log();
            g_is_logging = false;
        }
    }

    cJSON *rate = cJSON_GetObjectItem(json, "sampling_rate_ms");
    if (cJSON_IsNumber(rate)) g_sampling_rate_ms = rate->valueint;

    cJSON *rtc_ts = cJSON_GetObjectItem(json, "rtc_timestamp");
    if (cJSON_IsNumber(rtc_ts) && rtc_ts->valueint > 0) {
        struct timeval tv;
        tv.tv_sec = rtc_ts->valueint;
        tv.tv_usec = 0;
        settimeofday(&tv, NULL);
    }

    cJSON_Delete(json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"OK\"}");
    return ESP_OK;
}

static const httpd_uri_t uri_index = { .uri = "/", .method = HTTP_GET, .handler = index_handler, .user_ctx = NULL };
static const httpd_uri_t uri_css = { .uri = "/style.css", .method = HTTP_GET, .handler = css_handler, .user_ctx = NULL };
static const httpd_uri_t uri_js = { .uri = "/script.js", .method = HTTP_GET, .handler = js_handler, .user_ctx = NULL };
static const httpd_uri_t uri_data = { .uri = "/api/data", .method = HTTP_GET, .handler = data_handler, .user_ctx = NULL };
static const httpd_uri_t uri_config = { .uri = "/api/config", .method = HTTP_POST, .handler = config_handler, .user_ctx = NULL };

void start_webserver() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;
    config.stack_size = 8192;
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &uri_index);
        httpd_register_uri_handler(server, &uri_css);
        httpd_register_uri_handler(server, &uri_js);
        httpd_register_uri_handler(server, &uri_data);
        httpd_register_uri_handler(server, &uri_config);
    }
}