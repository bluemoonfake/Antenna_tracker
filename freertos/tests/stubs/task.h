#ifndef TEST_TASK_H
#define TEST_TASK_H
#include "FreeRTOS.h"
BaseType_t xTaskCreate(void (*entry)(void *), const char *name, uint32_t stack, void *argument,
                       UBaseType_t priority, TaskHandle_t *handle);
TickType_t xTaskGetTickCount(void);
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t task);
void vTaskDelay(TickType_t ticks);
#endif
