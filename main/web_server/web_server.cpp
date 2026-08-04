#include "web_server.h"
#include "cJSON.h"
#include "constant.h"
#include "esp_http_server.h"
#include "rtc/rtc.h"
#include "sd_logger.h"
#include "wifi_ap/wifi_ap.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

static void format_time_with_offset(time_t now, int offset_min, char *out,
                                    size_t out_size) {
  time_t adjusted = now + ((time_t)offset_min * 60);
  struct tm timeinfo;
  gmtime_r(&adjusted, &timeinfo);
  strftime(out, out_size, "%Y-%m-%d %H:%M:%S", &timeinfo);
}

extern const char index_html_start[] asm("_binary_index_html_start");
extern const char style_css_start[] asm("_binary_style_css_start");
extern const char script_js_start[] asm("_binary_script_js_start");
extern const char jszip_min_js_start[] asm("_binary_jszip_min_js_start");

#define BASIC_AUTH_B64 "Basic YWRtaW46YWRtaW4="

static esp_err_t check_auth(httpd_req_t *req) {
  char buf[128];
  int len = httpd_req_get_hdr_value_len(req, "Authorization");
  if (len > 0 && len < sizeof(buf)) {
    if (httpd_req_get_hdr_value_str(req, "Authorization", buf, sizeof(buf)) ==
        ESP_OK) {
      if (strcmp(buf, BASIC_AUTH_B64) == 0)
        return ESP_OK;
    }
  }
  httpd_resp_set_hdr(req, "WWW-Authenticate",
                     "Basic realm=\"TRD Device Monitor\"");
  httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
  return ESP_FAIL;
}

static esp_err_t index_handler(httpd_req_t *req) {
  if (check_auth(req) != ESP_OK)
    return ESP_FAIL;
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

static esp_err_t jszip_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "application/javascript");
  httpd_resp_sendstr(req, jszip_min_js_start);
  return ESP_OK;
}

static esp_err_t data_handler(httpd_req_t *req) {
  if (check_auth(req) != ESP_OK)
    return ESP_FAIL;

  time_t now;
  time(&now);
  char rtc_time_str[32];
  format_time_with_offset(now, g_timezone_offset_min, rtc_time_str,
                          sizeof(rtc_time_str));

  char resp_str[750];
  snprintf(
      resp_str, sizeof(resp_str),
      "{\"vib_raw_g\": %.3f, \"vib_uncalib_ms2\": %.3f, \"vib_calib_ms2\": "
      "%.3f, \"accX\": %.3f, \"accY\": %.3f, \"accZ\": %.3f, \"pitch\": %.3f, "
      "\"roll\": %.3f, \"yaw\": %.3f, \"fuel_voltage\": %.3f, \"voltage\": "
      "%.3f, \"acc_voltage\": %.3f, \"temperature\": %.3f, \"is_logging\": %s, "
      "\"rate\": %d, \"sd_ready\": %s, \"sd_used_perc\": %.1f, \"batt_perc\": "
      "%.1f, \"ignition\": %s, \"rtc_time\": \"%s\", \"rtc_ready\": %s, \"wifi_on\": %s}",
      g_curr_vib_raw_g, g_curr_vib_uncalib_ms2, g_curr_vib_calib_ms2,
      g_curr_accX_ms2, g_curr_accY_ms2, g_curr_accZ_ms2, g_curr_pitch,
      g_curr_roll, g_curr_yaw, g_curr_fuel_raw, g_curr_voltage,
      g_curr_acc_voltage, g_curr_temp_c, g_is_logging ? "true" : "false",
      g_sampling_rate_ms, g_sd_card_ready ? "true" : "false", g_sd_used_perc,
      g_batt_perc, g_ignition ? "true" : "false", 
      g_rtc_ready ? rtc_time_str : "DISCONNECTED",
      g_rtc_ready ? "true" : "false",
      wifi_ap_is_enabled() ? "true" : "false");

  httpd_resp_set_type(req, "application/json");
  httpd_resp_sendstr(req, resp_str);
  return ESP_OK;
}

static esp_err_t config_handler(httpd_req_t *req) {
  if (check_auth(req) != ESP_OK)
    return ESP_FAIL;
  char buf[256];
  int ret, remaining = req->content_len;

  if (remaining >= sizeof(buf)) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Payload too large");
    return ESP_FAIL;
  }

  if ((ret = httpd_req_recv(req, buf, remaining)) <= 0)
    return ESP_FAIL;
  buf[ret] = '\0';

  cJSON *json = cJSON_Parse(buf);
  if (!json) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    return ESP_FAIL;
  }

  bool requested_logging = false;
  bool start_logging_failed = false;

  cJSON *is_log = cJSON_GetObjectItem(json, "is_logging");
  if (cJSON_IsBool(is_log)) {
    requested_logging = cJSON_IsTrue(is_log);
    if (requested_logging && !g_is_logging) {
      if (g_sd_card_ready && sd_start_new_log()) {
        g_is_logging = true;
      } else {
        g_is_logging = false;
        start_logging_failed = true;
      }
    }
    if (!requested_logging && g_is_logging) {
      sd_stop_log();
      g_is_logging = false;
    }
  }

  cJSON *rate = cJSON_GetObjectItem(json, "sampling_rate_ms");
  if (cJSON_IsNumber(rate))
    g_sampling_rate_ms = rate->valueint;

  cJSON *tz_offset = cJSON_GetObjectItem(json, "timezone_offset_min");
  int tz_min = 0;
  if (cJSON_IsNumber(tz_offset)) {
    tz_min = tz_offset->valueint;
  }
  g_timezone_offset_min = 0;

  cJSON *rtc_ts = cJSON_GetObjectItem(json, "rtc_timestamp");
  bool rtc_update_failed = false;

  if (cJSON_IsNumber(rtc_ts) && rtc_ts->valueint > 0) {
    time_t local_sec = (time_t)rtc_ts->valueint + ((time_t)tz_min * 60);
    struct tm local_timeinfo;
    gmtime_r(&local_sec, &local_timeinfo);
    bool rtc_ok = rtc_set_time(local_timeinfo.tm_year + 1900, local_timeinfo.tm_mon + 1,
                 local_timeinfo.tm_mday, local_timeinfo.tm_hour,
                 local_timeinfo.tm_min, local_timeinfo.tm_sec);
    if (rtc_ok) {
      struct timeval tv;
      tv.tv_sec = local_sec;
      tv.tv_usec = 0;
      settimeofday(&tv, NULL);
    } else {
      rtc_update_failed = true;
    }
  }

  cJSON_Delete(json);
  httpd_resp_set_type(req, "application/json");

  if (start_logging_failed) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_sendstr(
        req,
        "{\"status\":\"ERROR\",\"is_logging\":false,\"message\":\"Cannot "
        "start logging. SD Card is not ready or failed to create log file.\"}");
    return ESP_OK;
  } else if (rtc_update_failed) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_sendstr(
        req,
        "{\"status\":\"ERROR\",\"is_logging\":false,\"message\":\"Cannot "
        "update RTC. RTC module is disconnected or I2C communication failed.\"}");
    return ESP_OK;
  } else {
    char resp_buf[128];
    snprintf(resp_buf, sizeof(resp_buf), "{\"status\":\"OK\",\"is_logging\":%s}", g_is_logging ? "true" : "false");
    httpd_resp_sendstr(req, resp_buf);
    return ESP_OK;
  }
}

static esp_err_t files_handler(httpd_req_t *req) {
  if (check_auth(req) != ESP_OK)
    return ESP_FAIL;

  if (!sd_lock(1000)) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "SD Card busy");
    return ESP_FAIL;
  }

  cJSON *root = cJSON_CreateArray();
  if (!root) {
    sd_unlock();
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "Memory allocation failed");
    return ESP_FAIL;
  }

  DIR *dir = opendir("/sdcard");
  if (dir != NULL) {
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
      size_t len = strlen(entry->d_name);
      if (len > 4 && strcasecmp(entry->d_name + len - 4, ".csv") == 0) {
        char filepath[320];
        snprintf(filepath, sizeof(filepath), "/sdcard/%s", entry->d_name);

        struct stat st;
        char size_buf[32];
        if (stat(filepath, &st) == 0) {
          if (st.st_size >= 1024 * 1024) {
            snprintf(size_buf, sizeof(size_buf), "%.1f MB",
                     (double)st.st_size / (1024.0 * 1024.0));
          } else if (st.st_size >= 1024) {
            snprintf(size_buf, sizeof(size_buf), "%ld KB",
                     (long)(st.st_size / 1024));
          } else {
            snprintf(size_buf, sizeof(size_buf), "%ld B", (long)st.st_size);
          }
        } else {
          snprintf(size_buf, sizeof(size_buf), "0 B");
        }

        cJSON *item = cJSON_CreateObject();
        if (item) {
          cJSON_AddStringToObject(item, "name", entry->d_name);
          cJSON_AddStringToObject(item, "size", size_buf);
          cJSON_AddItemToArray(root, item);
        }
      }
    }
    closedir(dir);
  }

  sd_unlock();

  char *json_str = cJSON_PrintUnformatted(root);
  httpd_resp_set_type(req, "application/json");
  if (json_str) {
    httpd_resp_sendstr(req, json_str);
    cJSON_free(json_str);
  } else {
    httpd_resp_sendstr(req, "[]");
  }
  cJSON_Delete(root);

  return ESP_OK;
}

static esp_err_t download_handler(httpd_req_t *req) {
  if (check_auth(req) != ESP_OK)
    return ESP_FAIL;

  char query[256];
  char filename[128];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
      httpd_query_key_value(query, "file", filename, sizeof(filename)) !=
          ESP_OK) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'file' parameter");
    return ESP_FAIL;
  }

  char filepath[320];
  if (strncmp(filename, "/sdcard/", 8) == 0) {
    snprintf(filepath, sizeof(filepath), "%s", filename);
  } else if (filename[0] == '/') {
    snprintf(filepath, sizeof(filepath), "/sdcard%s", filename);
  } else {
    snprintf(filepath, sizeof(filepath), "/sdcard/%s", filename);
  }

  if (!sd_lock(2000)) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "SD Card busy");
    return ESP_FAIL;
  }

  FILE *f = fopen(filepath, "r");
  if (!f) {
    sd_unlock();
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "text/csv");
  const char *basename = strrchr(filepath, '/');
  basename = basename ? basename + 1 : filepath;
  char disposition[512];
  snprintf(disposition, sizeof(disposition), "attachment; filename=\"%s\"",
           basename);
  httpd_resp_set_hdr(req, "Content-Disposition", disposition);

  char *chunk = (char *)malloc(4096);
  if (!chunk) {
    fclose(f);
    sd_unlock();
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "Memory allocation failed");
    return ESP_FAIL;
  }

  size_t read_bytes;
  esp_err_t res = ESP_OK;
  while ((read_bytes = fread(chunk, 1, 4096, f)) > 0) {
    if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
      res = ESP_FAIL;
      break;
    }
  }
  fclose(f);
  sd_unlock();
  free(chunk);

  httpd_resp_send_chunk(req, NULL, 0);
  return res;
}

static const httpd_uri_t uri_index = {
    .uri = "/", .method = HTTP_GET, .handler = index_handler, .user_ctx = NULL};
static const httpd_uri_t uri_css = {.uri = "/style.css",
                                    .method = HTTP_GET,
                                    .handler = css_handler,
                                    .user_ctx = NULL};
static const httpd_uri_t uri_js = {.uri = "/script.js",
                                   .method = HTTP_GET,
                                   .handler = js_handler,
                                   .user_ctx = NULL};
static const httpd_uri_t uri_jszip = {.uri = "/jszip.min.js",
                                      .method = HTTP_GET,
                                      .handler = jszip_handler,
                                      .user_ctx = NULL};
static const httpd_uri_t uri_data = {.uri = "/api/data",
                                     .method = HTTP_GET,
                                     .handler = data_handler,
                                     .user_ctx = NULL};
static const httpd_uri_t uri_config = {.uri = "/api/config",
                                       .method = HTTP_POST,
                                       .handler = config_handler,
                                       .user_ctx = NULL};
static const httpd_uri_t uri_files = {.uri = "/api/files",
                                      .method = HTTP_GET,
                                      .handler = files_handler,
                                      .user_ctx = NULL};
static const httpd_uri_t uri_download = {.uri = "/download",
                                         .method = HTTP_GET,
                                         .handler = download_handler,
                                         .user_ctx = NULL};

void start_webserver() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_uri_handlers = 12;
  config.stack_size = 8192;
  httpd_handle_t server = NULL;

  if (httpd_start(&server, &config) == ESP_OK) {
    httpd_register_uri_handler(server, &uri_index);
    httpd_register_uri_handler(server, &uri_css);
    httpd_register_uri_handler(server, &uri_js);
    httpd_register_uri_handler(server, &uri_jszip);
    httpd_register_uri_handler(server, &uri_data);
    httpd_register_uri_handler(server, &uri_config);
    httpd_register_uri_handler(server, &uri_files);
    httpd_register_uri_handler(server, &uri_download);
  }
}