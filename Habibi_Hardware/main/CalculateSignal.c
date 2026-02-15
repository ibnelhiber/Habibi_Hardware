#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "functions.h"
#include <stdint.h>
#include "ulp_app.h"




void CalculateDutyCycleTask(void* param)
{
	RunInfo* runInfo = (RunInfo*)param;
	const int injectFaultTime = 45;

	runInfo->dutyCycle = FIFTY_PERCENT_DUTY;
	ulp_dutyCycle = runInfo->dutyCycle;

	xSemaphoreGive(pwm_sem); 

	while(1)
	{
		printf("Entered CalculateDutyCycleTask!\n");
		if(--runInfo->runTime == 0)
		{
			runInfo->dutyCycle = 0;
			ulp_dutyCycle = runInfo->dutyCycle;
			xSemaphoreGive(pwm_sem); 
		}

		if(runInfo->runTime == injectFaultTime)
		{
			runInfo->dutyCycle = HUNDRED_PERCENT_DUTY;
			ulp_dutyCycle = runInfo->dutyCycle;
			xSemaphoreGive(pwm_sem); 
		}

		printf("Current Duty Cycle: %ld\n", runInfo->dutyCycle);


		vTaskDelay(pdMS_TO_TICKS(ONE_SECOND));
	}
}