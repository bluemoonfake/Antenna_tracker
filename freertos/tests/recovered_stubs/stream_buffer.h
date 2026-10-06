#ifndef TEST_STREAM_BUFFER_H
#define TEST_STREAM_BUFFER_H
#include "FreeRTOS.h"
typedef void *StreamBufferHandle_t;
StreamBufferHandle_t xStreamBufferCreate(size_t capacity, size_t trigger);
size_t xStreamBufferReceive(StreamBufferHandle_t stream, void *data, size_t capacity,
                            TickType_t wait);
size_t xStreamBufferSendFromISR(StreamBufferHandle_t stream, const void *data, size_t count,
                                BaseType_t *wake);
size_t xStreamBufferBytesAvailable(StreamBufferHandle_t stream);
BaseType_t xStreamBufferReset(StreamBufferHandle_t stream);
#endif
