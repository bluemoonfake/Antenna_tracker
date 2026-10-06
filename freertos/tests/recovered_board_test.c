#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/board.c"

volatile bridge_io_status_t g_bridge_io_status;
volatile sensor_diagnostics_t g_sensor_diagnostics;
static size_t accepted;
static HAL_StatusTypeDef rx_status;
static HAL_StatusTypeDef tx_status;
static unsigned tx_calls;
size_t xStreamBufferSendFromISR(StreamBufferHandle_t stream, const void *data, size_t length,
                                BaseType_t *wake)
{
  (void)stream;
  (void)data;
  (void)wake;
  return accepted < length ? accepted : length;
}
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *uart, uint8_t *data, uint16_t length)
{
  (void)uart;
  (void)data;
  assert(length == 1);
  return rx_status;
}
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *uart, uint8_t *data, uint16_t length,
                                    uint32_t timeout)
{
  (void)uart;
  (void)data;
  (void)length;
  (void)timeout;
  tx_calls++;
  return tx_status;
}
int main(void)
{
  UART_HandleTypeDef fc = {.Instance = USART1};
  UART_HandleTypeDef gps = {.Instance = USART3};
  accepted = 1;
  HAL_UART_RxCpltCallback(&fc);
  assert(g_bridge_io_status.uart_rx_bytes == 1 && g_bridge_io_status.uart_rx_dropped_bytes == 0);
  accepted = 0;
  rx_status = HAL_ERROR;
  HAL_UART_RxCpltCallback(&fc);
  assert(g_bridge_io_status.uart_rx_bytes == 2 && g_bridge_io_status.uart_rx_dropped_bytes == 1);
  assert(g_bridge_io_status.uart_rx_restart_errors == 1);
  HAL_UART_ErrorCallback(&fc);
  assert(g_bridge_io_status.uart_errors == 1 && g_bridge_io_status.uart_rx_restart_errors == 2);
  HAL_UART_RxCpltCallback(&gps);
  HAL_UART_ErrorCallback(&gps);
  assert(g_sensor_diagnostics.gps_rx_bytes == 1 && g_sensor_diagnostics.gps_rx_dropped_bytes == 1);
  assert(g_sensor_diagnostics.gps_uart_errors == 1 &&
         g_sensor_diagnostics.gps_rx_restart_errors == 2);
  uint8_t bytes[64] = {0};
  accepted = 10;
  board_usb_rx_from_isr(bytes, sizeof(bytes));
  assert(g_bridge_io_status.usb_rx_bytes == 64 && g_bridge_io_status.usb_rx_dropped_bytes == 54);
  assert(board_fc_write(bytes, sizeof(bytes), 10));
  assert(g_bridge_io_status.uart_tx_bytes == 64 &&
         g_bridge_io_status.uart_tx_completed_bytes == 64);
  tx_status = HAL_ERROR;
  assert(!board_fc_write(bytes, sizeof(bytes), 10));
  assert(!board_fc_write(bytes, UINT16_MAX + (size_t)1, 10));
  assert(tx_calls == 2 && g_bridge_io_status.uart_tx_start_errors == 2);
  assert(g_bridge_io_status.uart_tx_completed_bytes == 64);
  puts("PASS: recovered board actual UART/USB callbacks, drops, rearm/errors and TX counters");
  return 0;
}
