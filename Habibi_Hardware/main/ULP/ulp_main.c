#include <stdint.h>
#include "ulp_riscv.h"
#include "ulp_riscv_gpio.h"
#include "ulp_riscv_utils.h"
#include <stdint.h>

#define PWM_GPIO GPIO_NUM_13
#define LED_GPIO GPIO_NUM_12

#define EIGHTY_PERCENT_DUTY 3000
#define HUNDRED_PERCENT_DUTY 4000

#define DATA_VALID_BIT 1
#define DATA_CHECKED_BIT 2
#define TAKE_CONTROL_BIT 4

volatile uint32_t flags= 0;
volatile uint32_t dutyCycle;
volatile uint32_t dataCheckedCount = 0;
uint32_t validatedDutyCycle;
uint32_t ledVal = 0;

// Global lives in RTC_SLOW_MEM; main CPU can read it via ulp_app.h

int main(void)
{

    ulp_riscv_gpio_init(LED_GPIO);
    ulp_riscv_gpio_output_enable(LED_GPIO);
    ulp_riscv_gpio_set_output_mode(LED_GPIO, RTCIO_MODE_OUTPUT);
    ulp_riscv_gpio_input_disable(LED_GPIO);
    ulp_riscv_gpio_pullup_disable(LED_GPIO);
    ulp_riscv_gpio_pulldown_disable(LED_GPIO);


    if(dutyCycle <= EIGHTY_PERCENT_DUTY)
    {
        ulp_riscv_gpio_output_level(LED_GPIO, ledVal);
        flags |= DATA_VALID_BIT;
        validatedDutyCycle = dutyCycle;
    }
    else
    {
        flags &= ~DATA_VALID_BIT;
    }

    dataCheckedCount++;
    ledVal ^= 1;

    if(flags & TAKE_CONTROL_BIT)
    {
        ulp_riscv_gpio_output_level(LED_GPIO, 1);
        uint32_t timeOn = (validatedDutyCycle/HUNDRED_PERCENT_DUTY) * 1000;
        ulp_riscv_gpio_init(PWM_GPIO);
        ulp_riscv_gpio_output_enable(PWM_GPIO);
        ulp_riscv_gpio_set_output_mode(PWM_GPIO, RTCIO_MODE_OUTPUT);
        ulp_riscv_gpio_input_disable(PWM_GPIO);
        ulp_riscv_gpio_pullup_disable(PWM_GPIO);
        ulp_riscv_gpio_pulldown_disable(PWM_GPIO);
        ulp_riscv_wakeup_main_processor();
        while (1) 
        {

            if(!(flags & TAKE_CONTROL_BIT))
            {
                ulp_riscv_gpio_output_level(PWM_GPIO, 0);
                ulp_riscv_gpio_output_disable(PWM_GPIO);
                ulp_riscv_gpio_deinit(PWM_GPIO);

                break;
            }


            ulp_riscv_gpio_output_level(PWM_GPIO, 1);
            ulp_riscv_delay_cycles(timeOn * ULP_RISCV_CYCLES_PER_US); // 500 ms
            ulp_riscv_gpio_output_level(PWM_GPIO, 0);
            ulp_riscv_delay_cycles((100 - timeOn) * ULP_RISCV_CYCLES_PER_US); // 500 ms
        }

    }

    // Tell hardware we're done for this wake cycle (ULP will halt)
    ulp_riscv_halt();
    return 0;
}