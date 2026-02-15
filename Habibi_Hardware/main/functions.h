#ifndef FUNCTIONS_H_
#define FUNCTIONS_H_
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_err.h"


#define ONE_SECOND 1000
#define PWM_PIN GPIO_NUM_13
#define LED_PIN GPIO_NUM_11


#define FIFTY_PERCENT_DUTY 2000
#define EIGHTY_PERCENT_DUTY 3000
#define HUNDRED_PERCENT_DUTY 4000

#define DATA_VALID_BIT 1
#define DATA_CHECKED_BIT 2
#define TAKE_CONTROL_BIT 4

extern SemaphoreHandle_t pwm_sem;
extern uint32_t rtc_startup_reason;


typedef struct 
{
    int runTime;
    uint32_t dutyCycle;
} RunInfo;

typedef enum
{
    DATA_CORRUPTION,
    NORMAL_STARTUP

} ResetReason;

typedef struct {
    const char *firmware_url;
    bool reboot_after_success;
} github_ota_cfg_t;

esp_err_t github_ota_flash_from_url(const github_ota_cfg_t *cfg);

void Startup();
void PWMTask(void* param);
void CalculateDutyCycleTask(void* param);
esp_err_t wifi_connect_blocking(void);


#endif