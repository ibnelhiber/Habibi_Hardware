#include "functions.h"
#include "driver/gpio.h"
#include "ulp_app.h"
#include "ulp_riscv.h"



void Startup()
{
	gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
	gpio_set_level(LED_PIN, 1); 

	printf("Startup rtc_startup_reason=%lu\n", rtc_startup_reason);

	if(rtc_startup_reason == DATA_CORRUPTION)
	{
		printf("Recovering from corrupted data\n");
		if (wifi_connect_blocking() == ESP_OK) 
		{
			github_ota_cfg_t cfg = {
				.firmware_url = "https://github.com/ibnelhiber/Habibi_Hardware/releases/download/uncorrupted-clean-code/app-template.bin",
				.reboot_after_success = true,
			};
    		github_ota_flash_from_url(&cfg);
		}

		ulp_flags &= ~TAKE_CONTROL_BIT;
		ulp_riscv_halt();
		
	}

	rtc_startup_reason = NORMAL_STARTUP;

}