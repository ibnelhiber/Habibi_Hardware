#include "functions.h"
#include "driver/gpio.h"
#include "ulp_app.h"
#include "ulp_riscv.h"



void Startup()
{
	printf("Startup rtc_startup_reason=%lu\n", rtc_startup_reason);

	gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
	gpio_set_level(LED_PIN, 1); 

	if(rtc_startup_reason == DATA_CORRUPTION)
	{
		printf("Recovering from corrupted data\n");
		if (wifi_connect_blocking() == ESP_OK) 
		{
			github_ota_cfg_t cfg = {
				.firmware_url = "https://github.com/ibnelhiber/Habibi_Hardware/clean-code-version-2/app-template.bin",
				.reboot_after_success = true,
			};
    		github_ota_flash_from_url(&cfg);
		}

		ulp_flags = 0;;
		ulp_riscv_halt();
		
	}

	rtc_startup_reason = NORMAL_STARTUP;

}