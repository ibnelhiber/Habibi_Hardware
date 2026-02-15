#include <stdint.h>
#include "ulp_app.h"
#include "ulp_riscv.h"
#include "ulp_common.h"   // for ulp_set_wakeup_perio
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "functions.h"
#include "freertos/semphr.h"

extern const uint8_t bin_start[] asm("_binary_ulp_app_bin_start");
extern const uint8_t bin_end[]   asm("_binary_ulp_app_bin_end");

SemaphoreHandle_t pwm_sem;
RTC_DATA_ATTR uint32_t rtc_startup_reason = NORMAL_STARTUP;

void app_main(void)
{
    Startup();

    pwm_sem = xSemaphoreCreateBinary();
    static RunInfo runInfo;
    runInfo.runTime = 60;

    ESP_ERROR_CHECK(ulp_riscv_load_binary(bin_start, (bin_end - bin_start)));
    ulp_set_wakeup_period(0, 500000); // 500 ms
    ESP_ERROR_CHECK(ulp_riscv_run());

    xTaskCreate(CalculateDutyCycleTask, "Decides the duty cycle", 4096, &runInfo, 18, NULL);
    xTaskCreate(PWMTask, "Generates PWM signal", 4096, &runInfo, 14, NULL);

}
