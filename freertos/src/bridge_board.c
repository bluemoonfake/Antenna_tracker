#include "board.h"

#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"
#include "bridge_status.h"
#include "usb_device.h"

#define RX_STREAM_SIZE                                                                             \
  4096U // luong nhan byte lien tuc, den bat cu luc nao, task bridge lay ra khi duoc CPU chay
#define TX_CHUNK_SIZE                                                                              \
  128U // kich thuoc chunk de gui data qua USB: doan du lieu duoc driver nhan de gui trong 1 lan

static UART_HandleTypeDef
    g_uart; // dia chi ngoai vi: cau hinh UART, baud, trang thai, buffer, so byte phai xu ly va CRC
static StreamBufferHandle_t g_fc_rx; // luong nhan byte lien tuc tu FC
static StreamBufferHandle_t g_usb_rx; // luong nhan byte lien tuc tu USB
static uint8_t g_rx_byte; // mot byte nhan duoc tu UART, duoc gui vao stream buffer g_fc_rx
static uint8_t g_tx_buffer[TX_CHUNK_SIZE]; // buffer de luu du lieu truoc khi gui qua USB
static volatile bool g_tx_busy; // bool TX co dang ban khong ?
static volatile uint16_t g_tx_length; // so byte duoc gui qua USB
static volatile bool g_rx_restart; // khoi dong lai RX hay khong, neu co loi
// volatile de task va ISR cung truy cap cap, gia tri co the thay doi khi task, CPU se kiem tra gia
// tri moi nhat cua bien volatile, khong dung gia tri luu trong thanh ghi CPU
bool board_init(void)
{
  GPIO_InitTypeDef gpio = {0};
  g_fc_rx = xStreamBufferCreate(RX_STREAM_SIZE, 1U);
  g_usb_rx = xStreamBufferCreate(RX_STREAM_SIZE, 1U);
  if (g_fc_rx == NULL || g_usb_rx == NULL)
    return false;
  // RCC: Reset and Clock Control, cau hinh clock cho cac ngoai vi, bo nho, timer, UART, SPI, I2C
  __HAL_RCC_GPIOA_CLK_ENABLE(); // bat cap clock cho GPIOA, de cau hinh chan UART1
  __HAL_RCC_USART1_CLK_ENABLE(); // bat cap clock cho ngoai vi USART1
  // Bat dau khai bao
  HAL_NVIC_DisableIRQ(
      USART1_IRQn); // yeu cau NVIC tat ngat UART1, de cau hinh UART1 truoc khi bat ngat
  gpio.Pin = GPIO_PIN_9 | GPIO_PIN_10;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio.Alternate = GPIO_AF7_USART1;
  HAL_GPIO_Init(GPIOA, &gpio);
  g_uart.Instance = USART1;
  g_uart.Init.BaudRate = BOARD_FC_BAUD_RATE;
  g_uart.Init.WordLength = UART_WORDLENGTH_8B;
  g_uart.Init.StopBits = UART_STOPBITS_1;
  g_uart.Init.Parity = UART_PARITY_NONE;
  g_uart.Init.Mode = UART_MODE_TX_RX;
  g_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  g_uart.Init.OverSampling = UART_OVERSAMPLING_16;
  g_uart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  g_uart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  return HAL_UART_Init(&g_uart) == HAL_OK;
}

void board_start_io(void)
{
  configASSERT(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
  HAL_NVIC_SetPriority(USART1_IRQn, 6U, 0U); // uu tien ngat trong NVIC
  HAL_NVIC_ClearPendingIRQ(USART1_IRQn); // xoa cac ngat dang cho xu ly trong NVIC
  if (HAL_UART_Receive_IT(&g_uart, &g_rx_byte, 1U) != HAL_OK)
    board_fatal();
  HAL_NVIC_EnableIRQ(USART1_IRQn);
}

// neu de static void, linker se hieu ham nay chi dung trong file nay toi
static void restart_rx(void)
{
  if (HAL_UART_Receive_IT(&g_uart, &g_rx_byte, 1U) != HAL_OK)
  {
    g_rx_restart = true;
    g_bridge_io_status.uart_rx_restart_errors++;
  } else
  {
    g_rx_restart = false;
  }
}

// dang ky lai
void board_poll_io(void)
{
  if (!g_rx_restart)
    return;
  taskENTER_CRITICAL(); // tranh uart chen vao giua luc kiem tra trang thai va dang ky nhan lai
  if (g_uart.RxState == HAL_UART_STATE_READY)
    restart_rx();
  taskEXIT_CRITICAL();
}

uint32_t board_millis(void)
{
  return HAL_GetTick();
}

size_t board_fc_read(uint8_t *data, size_t capacity, uint32_t wait_ms)
{
  return xStreamBufferReceive(g_fc_rx, data, capacity, pdMS_TO_TICKS(wait_ms));
}

size_t board_usb_read(uint8_t *data, size_t capacity, uint32_t wait_ms)
{
  return xStreamBufferReceive(g_usb_rx, data, capacity, pdMS_TO_TICKS(wait_ms));
}

void board_usb_reset_rx(void)
{
  taskENTER_CRITICAL();
  size_t pending = xStreamBufferBytesAvailable(g_usb_rx);
  if (xStreamBufferReset(g_usb_rx) == pdPASS)
    g_bridge_io_status.usb_rx_dropped_bytes += (uint32_t)pending;
  taskEXIT_CRITICAL();
}

bool board_fc_write(const uint8_t *data, size_t length, uint32_t timeout_ms)
{
  (void)timeout_ms;
  if (length == 0U || length > sizeof(g_tx_buffer))
    return false;
  bool accepted = false;
  /* Copy into driver-owned memory: the bridge queue may wrap/reuse its bytes
   * immediately after acceptance, while the TX interrupt is still sending. */
  taskENTER_CRITICAL();
  if (!g_tx_busy)
  {
    memcpy(g_tx_buffer, data, length);
    g_tx_length = (uint16_t)length;
    g_tx_busy = true;
    if (HAL_UART_Transmit_IT(&g_uart, g_tx_buffer, g_tx_length) == HAL_OK)
    {
      accepted = true;
      g_bridge_io_status.uart_tx_bytes += (uint32_t)length;
    } else
    {
      g_tx_busy = false;
      g_bridge_io_status.uart_tx_start_errors++;
    }
  }
  taskEXIT_CRITICAL();
  return accepted;
}

bool board_usb_write(const uint8_t *data, size_t length)
{
  return usb_device_transmit(data, length);
}

void board_usb_rx_from_isr(const uint8_t *data, size_t length)
{
  BaseType_t wake = pdFALSE;
  size_t accepted = xStreamBufferSendFromISR(g_usb_rx, data, length, &wake);
  g_bridge_io_status.usb_rx_bytes += (uint32_t)length;
  g_bridge_io_status.usb_rx_dropped_bytes += (uint32_t)(length - accepted);
  portYIELD_FROM_ISR(wake);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
  if (uart->Instance != USART1)
    return;
  BaseType_t wake = pdFALSE;
  g_bridge_io_status.uart_rx_bytes++;
  if (xStreamBufferSendFromISR(g_fc_rx, &g_rx_byte, 1U, &wake) != 1U)
    g_bridge_io_status.uart_rx_dropped_bytes++;
  restart_rx();
  portYIELD_FROM_ISR(wake);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
  if (uart->Instance != USART1)
    return;
  g_bridge_io_status.uart_errors++;
  /* ORE aborts RX in HAL. Non-blocking framing/noise errors leave RX active. */
  if (uart->RxState == HAL_UART_STATE_READY)
    restart_rx();
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart)
{
  if (uart->Instance != USART1)
    return;
  g_bridge_io_status.uart_tx_completed_bytes += g_tx_length;
  g_tx_busy = false;
}

UART_HandleTypeDef *board_fc_uart_handle(void)
{
  return &g_uart;
}
