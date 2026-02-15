#include "functions.h"
#include "driver/gpio.h"
#include "ulp_app.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ulp_riscv.h"



void Startup()
{
	printf("Startup rtc_startup_reason=%lu\n", rtc_startup_reason);
	
	ulp_flags = 0;
	ulp_dutyCycle = 0;
	ulp_dataCheckedCount = 0;

	vTaskDelay(pdMS_TO_TICKS(ONE_SECOND));
	gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
	gpio_set_level(LED_PIN, 1); 

	if(rtc_startup_reason == DATA_CORRUPTION)
	{
		printf("Recovering from corrupted data\n");
		if (wifi_connect_blocking() == ESP_OK) 
		{
			github_ota_cfg_t cfg = {
				.firmware_url = "https://github.com/ibnelhiber/Habibi_Hardware/releases/download/clean-code-version-2/app-template.bin",
				.reboot_after_success = true,
			};

			rtc_startup_reason = NORMAL_STARTUP;
    		github_ota_flash_from_url(&cfg);
		}
		
	}


}