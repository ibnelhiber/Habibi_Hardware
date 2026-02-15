// wifi.c
// Drop-in replacement that keeps the same external behavior as your
// wifi_connect_blocking() (blocking connect with retries), but is more
// robust with iPhone hotspots.
//
// Key fixes vs your current file:
// - Proper NVS init w/ erase-on-version-mismatch
// - Explicit PMF config (capable=true, required=false)
// - Disable Wi-Fi power save (WIFI_PS_NONE) to avoid hotspot flakiness
// - Optional one-time scan to log RSSI/channel/auth for your target SSID
// - Better event logging (CONNECTED, GOT_IP with IP printed, DISCONNECTED with reason)
// - Uses esp_event_handler_instance_register() handles so you can unregister if desired
//
// This file should NOT interfere with your OTA logic as long as OTA
// is triggered only after you have connectivity (GOT_IP). We do not
// change certificates, HTTP, TLS, tasks, or anything outside Wi-Fi + netif.
//
// Requirements: ESP-IDF 4.4+ / 5.x (works on ESP32-S3).
// If your IDF is older and WIFI_AUTH_WPA2_WPA3_PSK is unavailable, it falls back.

#include "functions.h"

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#ifndef CONFIG_ESP_WIFI_SSID
#error "CONFIG_ESP_WIFI_SSID must be set (menuconfig)."
#endif
#ifndef CONFIG_ESP_WIFI_PASSWORD
#error "CONFIG_ESP_WIFI_PASSWORD must be set (menuconfig)."
#endif

static const char *TAG = "wifi";

static EventGroupHandle_t s_wifi_event_group;
static const int WIFI_CONNECTED_BIT = BIT0;
static const int WIFI_FAIL_BIT      = BIT1;

static int s_retry_num = 0;
static const int s_max_retries = 10;

static bool s_initialized = false;

static esp_netif_t *s_netif_sta = NULL;

static esp_event_handler_instance_t s_any_wifi_handler = NULL;
static esp_event_handler_instance_t s_got_ip_handler   = NULL;

static wifi_config_t s_wifi_config = { 0 };

static inline void log_ssid_password_lengths(void)
{
    // Note: SSID/password arrays are fixed-size; ensure they are null-terminated.
    s_wifi_config.sta.ssid[sizeof(s_wifi_config.sta.ssid) - 1] = '\0';
    s_wifi_config.sta.password[sizeof(s_wifi_config.sta.password) - 1] = '\0';

    ESP_LOGI(TAG, "SSID='%s' len=%u",
             (char*)s_wifi_config.sta.ssid,
             (unsigned)strlen((char*)s_wifi_config.sta.ssid));
    ESP_LOGI(TAG, "PASS len=%u", (unsigned)strlen((char*)s_wifi_config.sta.password));

    // Helpful for catching trailing/odd bytes in SSID buffer (spaces, non-ASCII, etc.)
    // Comment out if too noisy.
    ESP_LOGD(TAG, "SSID raw bytes (32):");
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, s_wifi_config.sta.ssid, 32, ESP_LOG_DEBUG);
}

static void maybe_scan_and_log_target_ap(const char *target_ssid)
{
    // Optional: one-time scan to show RSSI/channel/authmode for the hotspot.
    // Useful to verify antenna is fine and to see what authmode iPhone is advertising.
    wifi_scan_config_t scan_cfg = { 0 };
    esp_err_t err = esp_wifi_scan_start(&scan_cfg, true /* block */);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Scan start failed: %s", esp_err_to_name(err));
        return;
    }

    uint16_t ap_num = 0;
    err = esp_wifi_scan_get_ap_num(&ap_num);
    if (err != ESP_OK || ap_num == 0) {
        ESP_LOGW(TAG, "Scan got no APs or failed: %s (ap_num=%u)", esp_err_to_name(err), (unsigned)ap_num);
        return;
    }

    wifi_ap_record_t *recs = (wifi_ap_record_t *)calloc(ap_num, sizeof(wifi_ap_record_t));
    if (!recs) {
        ESP_LOGW(TAG, "Scan alloc failed (ap_num=%u)", (unsigned)ap_num);
        return;
    }

    uint16_t n = ap_num;
    err = esp_wifi_scan_get_ap_records(&n, recs);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Scan get records failed: %s", esp_err_to_name(err));
        free(recs);
        return;
    }

    for (uint16_t i = 0; i < n; i++) {
        if (strcmp((char*)recs[i].ssid, target_ssid) == 0) {
            ESP_LOGI(TAG, "Target AP found: SSID='%s' RSSI=%d dBm ch=%d auth=%d",
                     (char*)recs[i].ssid, recs[i].rssi, recs[i].primary, recs[i].authmode);
            break;
        }
    }

    free(recs);
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "WIFI_EVENT_STA_START");
            s_retry_num = 0;
            log_ssid_password_lengths();
            esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_CONNECTED: {
            wifi_event_sta_connected_t *e = (wifi_event_sta_connected_t *)event_data;
            ESP_LOGI(TAG, "WIFI_EVENT_STA_CONNECTED: channel=%d, authmode=%d",
                     e ? e->channel : -1, e ? e->authmode : -1);
            break;
        }

        case WIFI_EVENT_STA_DISCONNECTED: {
            wifi_event_sta_disconnected_t *d = (wifi_event_sta_disconnected_t *)event_data;
            int reason = d ? (int)d->reason : -1;

            // Common reasons you saw:
            // 2   = AUTH_EXPIRE
            // 205 = (Espressif) CONNECTION_FAIL
            ESP_LOGW(TAG, "WIFI_EVENT_STA_DISCONNECTED: reason=%d (retry %d/%d)",
                     reason, s_retry_num, s_max_retries);

            if (s_retry_num < s_max_retries) {
                s_retry_num++;
                // small backoff helps with hotspots
                vTaskDelay(pdMS_TO_TICKS(300));
                esp_wifi_connect();
            } else {
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            }
            break;
        }

        default:
            // keep quiet for other events
            break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)event_data;
        if (e) {
            ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP: " IPSTR, IP2STR(&e->ip_info.ip));
        } else {
            ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP");
        }
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_once(void)
{
    if (s_initialized) return;

    // --- NVS init (robust) ---
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS init issue (%s), erasing NVS...", esp_err_to_name(ret));
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(ret);
    }

    // --- Netif + event loop ---
    ESP_ERROR_CHECK(esp_netif_init());

    // esp_event_loop_create_default() returns ESP_ERR_INVALID_STATE if already created.
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(ret);
    }

    // Create default Wi-Fi STA netif once
    if (!s_netif_sta) {
        s_netif_sta = esp_netif_create_default_wifi_sta();
    }

    // --- Wi-Fi init ---
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Create event group once
    if (!s_wifi_event_group) {
        s_wifi_event_group = xEventGroupCreate();
    }

    // Register handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &s_any_wifi_handler));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &s_got_ip_handler));

    // Fill config from menuconfig
    memset(&s_wifi_config, 0, sizeof(s_wifi_config));
    snprintf((char*)s_wifi_config.sta.ssid, sizeof(s_wifi_config.sta.ssid), "%s", CONFIG_ESP_WIFI_SSID);
    snprintf((char*)s_wifi_config.sta.password, sizeof(s_wifi_config.sta.password), "%s", CONFIG_ESP_WIFI_PASSWORD);

    // Hotspot-friendly settings:
    // - PMF: allow if AP supports it, but never require (many hotspot combos get weird otherwise)
    s_wifi_config.sta.pmf_cfg.capable = true;
    s_wifi_config.sta.pmf_cfg.required = false;

    // - Auth threshold:
    // Prefer WPA2/WPA3 transition if supported, else WPA2.
#if defined(WIFI_AUTH_WPA2_WPA3_PSK)
    s_wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_WPA3_PSK;
#else
    s_wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
#endif

    // Recommended to reduce edge cases with roaming (not important for hotspot but harmless)
#if defined(WIFI_ALL_CHANNEL_SCAN)
    s_wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
#endif

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &s_wifi_config));

    // Disable Wi-Fi power save (helps a lot with phone hotspots)
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    ESP_ERROR_CHECK(esp_wifi_start());

    // Optional: scan and log target AP details (RSSI/channel/auth)
    maybe_scan_and_log_target_ap((char*)s_wifi_config.sta.ssid);

    s_initialized = true;
}

esp_err_t wifi_connect_blocking(void)
{
    wifi_init_once();

    // Clear previous results (so repeated calls work as expected)
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

    // If already connected (got IP earlier), return quickly.
    // (We still allow the event bits to drive the result; this is just a fast-path.)
    // You can remove this if you prefer always forcing a reconnect.
    esp_netif_ip_info_t ip;
    if (s_netif_sta && esp_netif_get_ip_info(s_netif_sta, &ip) == ESP_OK && ip.ip.addr != 0) {
        ESP_LOGI(TAG, "Already has IP: " IPSTR, IP2STR(&ip.ip));
        return ESP_OK;
    }

    // Trigger connect (if start event already did it, this is harmless; ESP-IDF will handle state)
    esp_wifi_connect();

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(20000)
    );

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi connected (GOT_IP)");
        return ESP_OK;
    }

    ESP_LOGE(TAG, "WiFi connect failed (no IP after retries)");
    return ESP_FAIL;
}
