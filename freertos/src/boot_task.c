#include "boot_task.h"

#include "FreeRTOS.h"
#include "task.h"
#include "board.h"
#include "tracker_config.h"

#define BOOT_DIAGNOSTICS_PERIOD_MS 500U

volatile boot_status_t g_boot_status;

static void boot_task(void *argument)
{
  TickType_t wake_time = xTaskGetTickCount();
  const TickType_t period = pdMS_TO_TICKS(BOOT_DIAGNOSTICS_PERIOD_MS);
  (void)argument;

  g_boot_status.boot_stage = BOOT_STAGE_TASK_RUNNING;
  if (!boot_board_led_pwm_start())
  {
    board_fatal();
  }
  g_boot_status.boot_stage = BOOT_STAGE_PWM_RUNNING;

  for (;;)
  {
    g_boot_status.task_iterations++;
    g_boot_status.rtos_ticks = xTaskGetTickCount();
    g_boot_status.hal_millis = HAL_GetTick();
    g_boot_status.free_heap_bytes = xPortGetFreeHeapSize();
    g_boot_status.minimum_free_heap_bytes = xPortGetMinimumEverFreeHeapSize();
    g_boot_status.stack_high_water_words = uxTaskGetStackHighWaterMark(NULL);
    vTaskDelayUntil(&wake_time, period);
  }
}

bool boot_task_start(void)
{
  return xTaskCreate(boot_task, "boot_diag", TRACKER_BOOT_TASK_STACK_WORDS, NULL,
                     TRACKER_BOOT_TASK_PRIORITY, NULL) == pdPASS;
}
