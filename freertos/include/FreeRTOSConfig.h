#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

extern uint32_t SystemCoreClock;

/* Scheduler and time base. */
#define configUSE_PREEMPTION     1
#define configUSE_TIME_SLICING   1
#define configUSE_TICKLESS_IDLE  0
#define configCPU_CLOCK_HZ       SystemCoreClock
#define configTICK_RATE_HZ       1000U
#define configMAX_PRIORITIES     8U
#define configMINIMAL_STACK_SIZE 256U
#define configMAX_TASK_NAME_LEN  20U
#define configUSE_16_BIT_TICKS   0
#define configIDLE_SHOULD_YIELD  1

/* Kernel objects used by the application. */
#define configUSE_MUTEXES                     1
#define configUSE_RECURSIVE_MUTEXES           0
#define configUSE_COUNTING_SEMAPHORES         1
#define configUSE_TASK_NOTIFICATIONS          1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES 1
#define configUSE_EVENT_GROUPS                1
#define configUSE_STREAM_BUFFERS              1
#define configUSE_QUEUE_SETS                  0
#define configQUEUE_REGISTRY_SIZE             8U

/* Software timers provide the timer service task for future timed services. */
#define configUSE_TIMERS             1
#define configTIMER_TASK_PRIORITY    2U
#define configTIMER_QUEUE_LENGTH     8U
#define configTIMER_TASK_STACK_DEPTH 512U

/* Allocation and failure handling. All current objects are created at boot. */
#define configSUPPORT_STATIC_ALLOCATION  0
#define configSUPPORT_DYNAMIC_ALLOCATION 1
#define configTOTAL_HEAP_SIZE            (96U * 1024U)
#define configAPPLICATION_ALLOCATED_HEAP 0
#define configUSE_MALLOC_FAILED_HOOK     1
#define configCHECK_FOR_STACK_OVERFLOW   2
#define configUSE_TICK_HOOK              1
#define configUSE_IDLE_HOOK              0

/* Cortex-M7 interrupt priority model. IRQs that call FreeRTOS use priority 6. */
#define configPRIO_BITS                              4U
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY      15U
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5U
#define configKERNEL_INTERRUPT_PRIORITY                                                            \
  (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8U - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY                                                       \
  (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8U - configPRIO_BITS))

/* Cortex-M7 port settings. */
#define configENABLE_FPU               1
#define configENABLE_MPU               0
#define configENABLE_TRUSTZONE         0
#define configRUN_FREERTOS_SECURE_ONLY 0

/* Keep only APIs used by the firmware and its diagnostics. */
#define INCLUDE_vTaskDelay                  1
#define INCLUDE_vTaskDelayUntil             1
#define INCLUDE_xTaskGetSchedulerState      1
#define INCLUDE_xTaskGetTickCount           1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define INCLUDE_vTaskSuspend                1

#define vPortSVCHandler    SVC_Handler
#define xPortPendSVHandler PendSV_Handler

void tracker_assert_failed(void);
#define configASSERT(condition)                                                                    \
  do                                                                                               \
  {                                                                                                \
    if (!(condition))                                                                              \
      tracker_assert_failed();                                                                     \
  } while (0)

#endif
