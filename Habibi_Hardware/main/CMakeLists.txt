idf_component_register(
    SRCS "main.c"
    "Startup.c"
    "PWM.c"
    "CalculateSignal.c"
    "wifi.c"
    "github_ota.c"
    REQUIRES 
        ulp
        esp_https_ota
        esp_http_client
        app_update
        nvs_flash
        esp_wifi
        mbedtls
)

set(ulp_app_name ulp_app)
set(ulp_sources "ULP/ulp_main.c")
set(ulp_exp_dep_srcs "main.c")

ulp_embed_binary(${ulp_app_name} "${ulp_sources}" "${ulp_exp_dep_srcs}")
