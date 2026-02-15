#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "functions.h"
#include "driver/ledc.h"
#include "hal/ledc_types.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "ulp_app.h"



#define PWM_CHANNEL LEDC_CHANNEL_0

static void PWMSetup()
{

	gpio_deep_sleep_hold_dis();
    gpio_hold_dis(PWM_PIN);
    rtc_gpio_hold_dis(PWM_PIN);
    rtc_gpio_deinit(PWM_PIN);
    gpio_reset_pin(PWM_PIN);
	
	ledc_timer_config_t ledc_timer = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .timer_num = LEDC_TIMER_0,
    .duty_resolution = LEDC_TIMER_13_BIT,
    .freq_hz = 3000,
    .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .gpio_num = PWM_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&ledc_channel);
}

inline static void Pump(int speed)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, PWM_CHANNEL, speed);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, PWM_CHANNEL);
	printf("PWM Set to %d!\n", speed);

}

void PWMTask(void* param)
{
	RunInfo* runInfo = (RunInfo*)param;
	uint32_t dataCheckedCount = ulp_dataCheckedCount;

	PWMSetup();

	while(1)
	{
		printf("Entered PWM Loop\n");
		printf("%ld\n", dataCheckedCount);

		if((dataCheckedCount != ulp_dataCheckedCount) && (ulp_flags & DATA_VALID_BIT))
		{
			Pump(runInfo->dutyCycle);
		}
		
		if((dataCheckedCount != ulp_dataCheckedCount) && !(ulp_flags & DATA_VALID_BIT))
		{
			gpio_set_level(LED_PIN, 0);  
			ulp_flags |= TAKE_CONTROL_BIT;
			rtc_startup_reason = DATA_CORRUPTION;
			esp_sleep_enable_ulp_wakeup(); 
			esp_deep_sleep_start();
		}

		dataCheckedCount = ulp_dataCheckedCount;
		vTaskDelay(pdMS_TO_TICKS(ONE_SECOND));
	}
}