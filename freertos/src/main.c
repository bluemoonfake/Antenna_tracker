#include "FreeRTOS.h"
#include "task.h"

#include "board.h"

// Cung cap khai bao cho boot_board_init(), boot_task_start() va g_boot_status --> chuan doan boot
#if TRACKER_BOOT_ONLY || TRACKER_BRIDGE_ONLY
#include "boot_task.h"
#endif
#if !TRACKER_BOOT_ONLY // Khi 0, compiler doc app state va tracker_tasks
#include "app_state.h"
#include "tracker_tasks.h"
#endif

// dua gia g_bootloader_delay_signature vao section .bootloader_signature de bootloader co the kiem
// tra
__attribute__((section(".bootloader_signature"),
               used)) static const uint64_t g_bootloader_delay_signature =
    UINT64_C(0xffecc2925d7d05c5);

static void system_clock_config(void)
{

  // khai bao nguon HSE bo PLL1, clock SYSCLK, HCLK, PCLK1, PCLK2, PCLK3, PCLK4 va nguon PLL3 bo
  // USB, SPI2, I2C1, I2C2, USART1, USART32
  RCC_OscInitTypeDef oscillator = {0};
  RCC_ClkInitTypeDef clock = {0};
  RCC_PeriphCLKInitTypeDef peripheral = {0};

  if (HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY) != HAL_OK)
  {
    board_fatal();
  }

  /* STM32H743 VOS0 uses the SYSCFG overdrive control. */
  __HAL_RCC_SYSCFG_CLK_ENABLE(); // bat clock cho sys cfg
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0); // cau hinh ap dien VOS
  while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY))
  {
  }
  // bat cau hinh PLL1
  oscillator.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  oscillator.HSEState = RCC_HSE_ON;
  oscillator.PLL.PLLState = RCC_PLL_ON;
  oscillator.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  // PLL / M * N / P = 120 MHz SYSCLK
  oscillator.PLL.PLLM = 1;
  oscillator.PLL.PLLN = 120;
  oscillator.PLL.PLLP = 2;
  oscillator.PLL.PLLQ = 4;
  oscillator.PLL.PLLR = 8;
  oscillator.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
  oscillator.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  oscillator.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&oscillator) != HAL_OK)
  {
    board_fatal();
  }

  clock.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_D1PCLK1 |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_D3PCLK1;
  clock.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  clock.SYSCLKDivider = RCC_SYSCLK_DIV1;
  clock.AHBCLKDivider = RCC_HCLK_DIV2;
  clock.APB3CLKDivider = RCC_APB3_DIV2;
  clock.APB1CLKDivider = RCC_APB1_DIV2;
  clock.APB2CLKDivider = RCC_APB2_DIV2;
  clock.APB4CLKDivider = RCC_APB4_DIV2;
  if (HAL_RCC_ClockConfig(&clock, FLASH_LATENCY_4) != HAL_OK)
  {
    board_fatal();
  }

  peripheral.PeriphClockSelection = RCC_PERIPHCLK_USB | RCC_PERIPHCLK_SPI2 | RCC_PERIPHCLK_I2C1 |
                                    RCC_PERIPHCLK_I2C2 | RCC_PERIPHCLK_USART1 |
                                    RCC_PERIPHCLK_USART3;
  peripheral.PLL3.PLL3M = 2;
  peripheral.PLL3.PLL3N = 48;
  peripheral.PLL3.PLL3P = 2;
  peripheral.PLL3.PLL3Q = 4;
  peripheral.PLL3.PLL3R = 2;
  peripheral.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_1;
  peripheral.PLL3.PLL3VCOSEL = RCC_PLL3VCOWIDE;
  peripheral.PLL3.PLL3FRACN = 0;
  peripheral.UsbClockSelection = RCC_USBCLKSOURCE_PLL3;
  peripheral.Spi123ClockSelection = RCC_SPI123CLKSOURCE_PLL;
  peripheral.I2c123ClockSelection = RCC_I2C123CLKSOURCE_D2PCLK1;
  peripheral.Usart16ClockSelection = RCC_USART16CLKSOURCE_D2PCLK2;
  peripheral.Usart234578ClockSelection = RCC_USART234578CLKSOURCE_D2PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&peripheral) != HAL_OK)
  {
    board_fatal();
  }
}

int main(void)
{
#if TRACKER_BOOT_ONLY || TRACKER_BRIDGE_ONLY
  boot_board_init();
  g_boot_status.boot_stage = BOOT_STAGE_MAIN;
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_4, GPIO_PIN_RESET); /* Blue: main entered. */
#endif

  SCB_EnableICache(); // bat cache instruction
  SCB_EnableDCache(); // bat cache data
  HAL_Init();
#if TRACKER_BOOT_ONLY || TRACKER_BRIDGE_ONLY
  g_boot_status.boot_stage = BOOT_STAGE_HAL_READY;
#endif
  system_clock_config();

#if TRACKER_BOOT_ONLY || TRACKER_BRIDGE_ONLY
  g_boot_status.boot_stage = BOOT_STAGE_CLOCK_READY;
  /* Blue + green: clock ready, waiting for the task to start PWM. */
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_6, GPIO_PIN_RESET);
  if (!boot_task_start())
  {
    board_fatal();
  }
#if TRACKER_BRIDGE_ONLY
  if (!board_init() || !app_state_init() || !mavlink_bridge_task_start())
  {
    board_fatal();
  }
#endif
  g_boot_status.boot_stage = BOOT_STAGE_TASK_CREATED;
#else // check dieu kien de task co the du dieu kien de tao
  if (!board_init() || !app_state_init())
  {
    board_fatal();
  }
  // neu mavlink || sensor || tracker control task chua tao duoc, bao loi
  if (!mavlink_bridge_task_start() || !sensor_task_start() || !tracker_control_task_start())
  {
    board_fatal();
  }
#endif

  vTaskStartScheduler();
  // neu Scheduler khong du heap, goi board_fatal() de bao loi
  board_fatal();
}

void vApplicationTickHook(void)
{
  HAL_IncTick();
}

void vApplicationMallocFailedHook(void)
{
  board_fatal();
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
  (void)task;
  (void)name;
  board_fatal();
}

void tracker_assert_failed(void)
{
  board_fatal();
}
