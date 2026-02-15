\
#include "functions.h"

#include "esp_log.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_system.h"
#include "ulp_app.h"

static const char *TAG = "github_ota";

esp_err_t github_ota_flash_from_url(const github_ota_cfg_t *cfg)
{
    if (!cfg || !cfg->firmware_url) return ESP_ERR_INVALID_ARG;

    esp_http_client_config_t http_cfg = {
        .url = cfg->firmware_url,
        .timeout_ms = 20000,
        .keep_alive_enable = false,


        // GitHub release asset links redirect; allow redirects
        .disable_auto_redirect = false,
        .max_redirection_count = 8,

        // TLS server verification using the ESP certificate bundle
        .crt_bundle_attach = esp_crt_bundle_attach,

        .user_agent = "esp32s3-ota",
        .keep_alive_enable = true,
        .buffer_size = 12288,
        .buffer_size_tx = 4096,
        .disable_auto_redirect = false,
        .max_redirection_count = 8

    };

    esp_https_ota_config_t ota_cfg = {
        .http_config = &http_cfg,
        .partial_http_download = false,
        .max_http_request_size = 32768,
    };

    ESP_LOGW(TAG, "Starting HTTPS OTA from:\n%s", cfg->firmware_url);
    esp_err_t ret = esp_https_ota(&ota_cfg);

    if (ret == ESP_OK) {
        ESP_LOGW(TAG, "OTA successful");
        if (cfg->reboot_after_success) {
            ESP_LOGW(TAG, "Rebooting...");
            esp_restart();
        }
        return ESP_OK;
    }

    ESP_LOGE(TAG, "OTA failed: %s", esp_err_to_name(ret));
    return ret;
}
