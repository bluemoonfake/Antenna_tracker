#ifndef TRACKER_BOOT_TASK_H
#define TRACKER_BOOT_TASK_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  BOOT_STAGE_MAIN = 1,
  BOOT_STAGE_HAL_READY,
  BOOT_STAGE_CLOCK_READY,
  BOOT_STAGE_TASK_CREATED,
  BOOT_STAGE_TASK_RUNNING,
  BOOT_STAGE_PWM_RUNNING
} boot_stage_t;

/* Startup writes the stage before scheduling; the boot task then updates
 * this structure. Read through the debugger while the MCU is halted.
 * Stack high-water marks are in StackType_t words, not bytes. */
typedef struct
{
  boot_stage_t boot_stage;
  uint32_t task_iterations;
  uint32_t rtos_ticks;
  uint32_t hal_millis;
  uint32_t free_heap_bytes;
  uint32_t minimum_free_heap_bytes;
  uint32_t stack_high_water_words;
} boot_status_t;

extern volatile boot_status_t g_boot_status;

void boot_board_init(void);
bool boot_board_led_pwm_start(void);
bool boot_task_start(void);

#endif
