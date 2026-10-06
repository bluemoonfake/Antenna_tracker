#include "board.h"
#include "boot_task.h"

#define LED_PWM_FREQUENCY_HZ 1000U
#define LED_TIMER_TICK_HZ    1000000U
#define LED_PWM_DUTY_PERCENT 50U

static TIM_HandleTypeDef g_led_timer;

void boot_board_init(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOE_CLK_ENABLE();

  /* MicoAir H743: LEDs PE4/PE5/PE6 are active low. Set the output latch
   * before switching modes. PE13/PE14 remain GPIO low with TIM1 disabled. */
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_13 | GPIO_PIN_14, GPIO_PIN_RESET);
  gpio.Pin = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_13 | GPIO_PIN_14;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &gpio);
}

bool boot_board_led_pwm_start(void)
{
  GPIO_InitTypeDef gpio = {0};
  TIM_OC_InitTypeDef channel = {0};
  const uint32_t period_ticks = LED_TIMER_TICK_HZ / LED_PWM_FREQUENCY_HZ;

  __HAL_RCC_TIM15_CLK_ENABLE();
  __HAL_RCC_TIM15_FORCE_RESET();
  __HAL_RCC_TIM15_RELEASE_RESET();

  /* main configures APB2 = HCLK / 2, so TIM15 gets 2 * PCLK2 = 240 MHz. */
  g_led_timer.Instance = TIM15;
  g_led_timer.Init.Prescaler = (2U * HAL_RCC_GetPCLK2Freq()) / LED_TIMER_TICK_HZ - 1U;
  g_led_timer.Init.CounterMode = TIM_COUNTERMODE_UP;
  g_led_timer.Init.Period = period_ticks - 1U;
  g_led_timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  g_led_timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_PWM_Init(&g_led_timer) != HAL_OK)
  {
    return false;
  }

  channel.OCMode = TIM_OCMODE_PWM1;
  channel.Pulse = period_ticks * LED_PWM_DUTY_PERCENT / 100U;
  channel.OCPolarity = TIM_OCPOLARITY_LOW; /* LED is on while PE6 is low. */
  channel.OCIdleState = TIM_OCIDLESTATE_SET;
  if (HAL_TIM_PWM_ConfigChannel(&g_led_timer, &channel, TIM_CHANNEL_2) != HAL_OK ||
      HAL_TIM_PWM_Start(&g_led_timer, TIM_CHANNEL_2) != HAL_OK)
  {
    return false;
  }

  /* Attach PE6 only after PWM is running. No timer interrupt or DMA is used. */
  gpio.Pin = GPIO_PIN_6;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  gpio.Alternate = GPIO_AF4_TIM15;
  HAL_GPIO_Init(GPIOE, &gpio);
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_4, GPIO_PIN_SET); /* PWM ready: blue off. */
  return true;
}

void board_fatal(void)
{
  __disable_irq();
  __HAL_RCC_TIM15_FORCE_RESET();
  __HAL_RCC_TIM15_RELEASE_RESET();
  __HAL_RCC_TIM15_CLK_DISABLE();
  /* This also works if clock configuration failed before boot_board_init. */
  boot_board_init();
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_5, GPIO_PIN_RESET);
  for (;;)
  {
    __NOP();
  }
}
