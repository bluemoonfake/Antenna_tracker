#ifndef TEST_FREERTOS_H
#define TEST_FREERTOS_H
#include <stddef.h>
#include <stdint.h>
typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef unsigned UBaseType_t;
typedef void *TaskHandle_t;
#define pdFALSE               0
#define configASSERT(x)       ((void)(x))
#define portYIELD_FROM_ISR(x) ((void)(x))
#define pdPASS                1
#define pdMS_TO_TICKS(ms)     (ms)
size_t xPortGetFreeHeapSize(void);
size_t xPortGetMinimumEverFreeHeapSize(void);
#endif
