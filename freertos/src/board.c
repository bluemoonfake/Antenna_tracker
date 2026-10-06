#include "board.h"

#include "FreeRTOS.h"
#include "stream_buffer.h"
#include "task.h"

#include "usb_device.h"
#include "bridge_status.h"
#include "sensor_diagnostics.h"

#define RX_STREAM_SIZE     4096U
#define UART_RX_TIMEOUT_MS 20U
#define I2C_TIMEOUT_MS     20U
#define SPI_TIMEOUT_MS     20U

static UART_HandleTypeDef g_fc_uart;
static UART_HandleTypeDef g_gps_uart;
static I2C_HandleTypeDef g_i2c1;
static I2C_HandleTypeDef g_i2c2;
static SPI_HandleTypeDef g_spi2;
static TIM_HandleTypeDef g_tim1;
static StreamBufferHandle_t g_fc_rx;
static StreamBufferHandle_t g_gps_rx;
static StreamBufferHandle_t g_usb_rx;
static uint8_t g_fc_rx_byte;
static uint8_t g_gps_rx_byte;
static bool g_servo_started;

static void gpio_init(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_4 | GPIO_PIN_5, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);
  gpio.Pin = GPIO_PIN_4 | GPIO_PIN_5;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOD, &gpio);
  gpio.Pin = GPIO_PIN_15;
  HAL_GPIO_Init(GPIOA, &gpio);

  gpio.Pin = GPIO_PIN_9 | GPIO_PIN_10;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio.Alternate = GPIO_AF7_USART1;
  HAL_GPIO_Init(GPIOA, &gpio);

  gpio.Pin = GPIO_PIN_8 | GPIO_PIN_9;
  gpio.Alternate = GPIO_AF7_USART3;
  HAL_GPIO_Init(GPIOD, &gpio);

  gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
  gpio.Mode = GPIO_MODE_AF_OD;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio.Alternate = GPIO_AF4_I2C1;
  HAL_GPIO_Init(GPIOB, &gpio);
  gpio.Pin = GPIO_PIN_10 | GPIO_PIN_11;
  gpio.Alternate = GPIO_AF4_I2C2;
  HAL_GPIO_Init(GPIOB, &gpio);

  gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio.Alternate = GPIO_AF5_SPI2;
  HAL_GPIO_Init(GPIOC, &gpio);
  gpio.Pin = GPIO_PIN_3;
  HAL_GPIO_Init(GPIOD, &gpio);

  gpio.Pin = GPIO_PIN_13 | GPIO_PIN_14;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio.Alternate = GPIO_AF1_TIM1;
  HAL_GPIO_Init(GPIOE, &gpio);
}

static bool uart_init(void)
{
  __HAL_RCC_USART1_CLK_ENABLE();
  __HAL_RCC_USART3_CLK_ENABLE();

  g_fc_uart.Instance = USART1;
  g_fc_uart.Init.BaudRate = BOARD_FC_BAUD_RATE;
  g_fc_uart.Init.WordLength = UART_WORDLENGTH_8B;
  g_fc_uart.Init.StopBits = UART_STOPBITS_1;
  g_fc_uart.Init.Parity = UART_PARITY_NONE;
  g_fc_uart.Init.Mode = UART_MODE_TX_RX;
  g_fc_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  g_fc_uart.Init.OverSampling = UART_OVERSAMPLING_16;
  g_fc_uart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  g_fc_uart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

  g_gps_uart = g_fc_uart;
  g_gps_uart.Instance = USART3;
  g_gps_uart.Init.BaudRate = BOARD_GPS_BAUD_RATE;

  if (HAL_UART_Init(&g_fc_uart) != HAL_OK || HAL_UART_Init(&g_gps_uart) != HAL_OK)
  {
    return false;
  }

  HAL_NVIC_SetPriority(USART1_IRQn, 6, 0);
  HAL_NVIC_SetPriority(USART3_IRQn, 6, 0);
  return true;
}

static bool i2c_init(void)
{
  __HAL_RCC_I2C1_CLK_ENABLE();
  __HAL_RCC_I2C2_CLK_ENABLE();

  g_i2c1.Instance = I2C1;
  g_i2c1.Init.Timing = 0x10c0ecffU;
  g_i2c1.Init.OwnAddress1 = 0;
  g_i2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  g_i2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  g_i2c1.Init.OwnAddress2 = 0;
  g_i2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  g_i2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  g_i2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  g_i2c2 = g_i2c1;
  g_i2c2.Instance = I2C2;

  return HAL_I2C_Init(&g_i2c1) == HAL_OK && HAL_I2C_Init(&g_i2c2) == HAL_OK &&
         HAL_I2CEx_ConfigAnalogFilter(&g_i2c1, I2C_ANALOGFILTER_ENABLE) == HAL_OK &&
         HAL_I2CEx_ConfigAnalogFilter(&g_i2c2, I2C_ANALOGFILTER_ENABLE) == HAL_OK;
}

static bool spi_init(void)
{
  __HAL_RCC_SPI2_CLK_ENABLE();
  g_spi2.Instance = SPI2;
  g_spi2.Init.Mode = SPI_MODE_MASTER;
  g_spi2.Init.Direction = SPI_DIRECTION_2LINES;
  g_spi2.Init.DataSize = SPI_DATASIZE_8BIT;
  g_spi2.Init.CLKPolarity = SPI_POLARITY_HIGH;
  g_spi2.Init.CLKPhase = SPI_PHASE_2EDGE;
  g_spi2.Init.NSS = SPI_NSS_SOFT;
  g_spi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128;
  g_spi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  g_spi2.Init.TIMode = SPI_TIMODE_DISABLE;
  g_spi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  g_spi2.Init.CRCPolynomial = 7;
  g_spi2.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  g_spi2.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
  g_spi2.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  g_spi2.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  g_spi2.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  g_spi2.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  g_spi2.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  g_spi2.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  g_spi2.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE;
  g_spi2.Init.IOSwap = SPI_IO_SWAP_DISABLE;
  return HAL_SPI_Init(&g_spi2) == HAL_OK;
}

static bool pwm_init(void)
{
  TIM_OC_InitTypeDef channel = {0};

  __HAL_RCC_TIM1_CLK_ENABLE();
  g_tim1.Instance = TIM1;
  g_tim1.Init.Prescaler = 239U;
  g_tim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  g_tim1.Init.Period = (1000000U / BOARD_SERVO_FREQUENCY_HZ) - 1U;
  g_tim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  g_tim1.Init.RepetitionCounter = 0;
  g_tim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&g_tim1) != HAL_OK)
    return false;

  channel.OCMode = TIM_OCMODE_PWM1;
  channel.Pulse = 0;
  channel.OCPolarity = TIM_OCPOLARITY_HIGH;
  channel.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  channel.OCFastMode = TIM_OCFAST_DISABLE;
  channel.OCIdleState = TIM_OCIDLESTATE_RESET;
  channel.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  return HAL_TIM_PWM_ConfigChannel(&g_tim1, &channel, TIM_CHANNEL_3) == HAL_OK &&
         HAL_TIM_PWM_ConfigChannel(&g_tim1, &channel, TIM_CHANNEL_4) == HAL_OK;
}

bool board_init(void)
{
  gpio_init();
  g_fc_rx = xStreamBufferCreate(RX_STREAM_SIZE, 1U);
  g_gps_rx = xStreamBufferCreate(RX_STREAM_SIZE, 1U);
  g_usb_rx = xStreamBufferCreate(RX_STREAM_SIZE, 1U);
  return g_fc_rx != NULL && g_gps_rx != NULL && g_usb_rx != NULL && uart_init() && i2c_init() &&
         spi_init() && pwm_init();
}

void board_start_io(void)
{
  configASSERT(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
  if (HAL_UART_Receive_IT(&g_fc_uart, &g_fc_rx_byte, 1U) != HAL_OK ||
      HAL_UART_Receive_IT(&g_gps_uart, &g_gps_rx_byte, 1U) != HAL_OK)
  {
    board_fatal();
  }
  HAL_NVIC_EnableIRQ(USART1_IRQn);
  HAL_NVIC_EnableIRQ(USART3_IRQn);
}

uint32_t board_millis(void)
{
  return HAL_GetTick();
}

static size_t stream_read(StreamBufferHandle_t stream, uint8_t *data, size_t capacity,
                          uint32_t wait_ms)
{
  return xStreamBufferReceive(stream, data, capacity, pdMS_TO_TICKS(wait_ms));
}

size_t board_fc_read(uint8_t *data, size_t capacity, uint32_t wait_ms)
{
  return stream_read(g_fc_rx, data, capacity, wait_ms);
}

size_t board_gps_read(uint8_t *data, size_t capacity, uint32_t wait_ms)
{
  return stream_read(g_gps_rx, data, capacity, wait_ms);
}

size_t board_usb_read(uint8_t *data, size_t capacity, uint32_t wait_ms)
{
  return stream_read(g_usb_rx, data, capacity, wait_ms);
}

void board_usb_reset_rx(void)
{
  taskENTER_CRITICAL();
  size_t pending = xStreamBufferBytesAvailable(g_usb_rx);
  if (xStreamBufferReset(g_usb_rx) == pdPASS)
  {
    g_bridge_io_status.usb_rx_dropped_bytes += (uint32_t)pending;
  }
  taskEXIT_CRITICAL();
}

bool board_fc_write(const uint8_t *data, size_t length, uint32_t timeout_ms)
{
  if (length > UINT16_MAX ||
      HAL_UART_Transmit(&g_fc_uart, (uint8_t *)data, (uint16_t)length, timeout_ms) != HAL_OK)
  {
    g_bridge_io_status.uart_tx_start_errors++;
    return false;
  }
  g_bridge_io_status.uart_tx_bytes += (uint32_t)length;
  g_bridge_io_status.uart_tx_completed_bytes += (uint32_t)length;
  return true;
}

bool board_usb_write(const uint8_t *data, size_t length)
{
  return usb_device_transmit(data, length);
}

void board_usb_rx_from_isr(const uint8_t *data, size_t length)
{
  BaseType_t wake = pdFALSE;
  size_t sent = xStreamBufferSendFromISR(g_usb_rx, data, length, &wake);
  g_bridge_io_status.usb_rx_bytes += (uint32_t)length;
  g_bridge_io_status.usb_rx_dropped_bytes += (uint32_t)(length - sent);
  portYIELD_FROM_ISR(wake);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
  BaseType_t wake = pdFALSE;

  if (uart->Instance == USART1)
  {
    g_bridge_io_status.uart_rx_bytes++;
    size_t sent = xStreamBufferSendFromISR(g_fc_rx, &g_fc_rx_byte, 1U, &wake);
    g_bridge_io_status.uart_rx_dropped_bytes += (uint32_t)(1U - sent);
    if (HAL_UART_Receive_IT(&g_fc_uart, &g_fc_rx_byte, 1U) != HAL_OK)
      g_bridge_io_status.uart_rx_restart_errors++;
  } else if (uart->Instance == USART3)
  {
    size_t sent;
    g_sensor_diagnostics.gps_rx_bytes++;
    sent = xStreamBufferSendFromISR(g_gps_rx, &g_gps_rx_byte, 1U, &wake);
    g_sensor_diagnostics.gps_rx_dropped_bytes += (uint32_t)(1U - sent);
    if (HAL_UART_Receive_IT(&g_gps_uart, &g_gps_rx_byte, 1U) != HAL_OK)
      g_sensor_diagnostics.gps_rx_restart_errors++;
  }

  portYIELD_FROM_ISR(wake);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
  if (uart->Instance == USART1)
  {
    g_bridge_io_status.uart_errors++;
    if (HAL_UART_Receive_IT(&g_fc_uart, &g_fc_rx_byte, 1U) != HAL_OK)
      g_bridge_io_status.uart_rx_restart_errors++;
  } else if (uart->Instance == USART3)
  {
    g_sensor_diagnostics.gps_uart_errors++;
    if (HAL_UART_Receive_IT(&g_gps_uart, &g_gps_rx_byte, 1U) != HAL_OK)
      g_sensor_diagnostics.gps_rx_restart_errors++;
  }
}

bool board_i2c_read(I2C_HandleTypeDef *bus, uint8_t address, uint8_t reg, uint8_t *data,
                    size_t length)
{
  return length <= UINT16_MAX &&
         HAL_I2C_Mem_Read(bus, (uint16_t)address << 1, reg, I2C_MEMADD_SIZE_8BIT, data,
                          (uint16_t)length, I2C_TIMEOUT_MS) == HAL_OK;
}

bool board_i2c_write(I2C_HandleTypeDef *bus, uint8_t address, uint8_t reg, uint8_t value)
{
  return HAL_I2C_Mem_Write(bus, (uint16_t)address << 1, reg, I2C_MEMADD_SIZE_8BIT, &value, 1U,
                           I2C_TIMEOUT_MS) == HAL_OK;
}

static GPIO_TypeDef *chip_select_port(uint16_t chip_select, uint16_t *pin)
{
  if (chip_select == BOARD_SPI_CS_BMI088_GYRO)
  {
    *pin = GPIO_PIN_5;
    return GPIOD;
  }
  if (chip_select == BOARD_SPI_CS_BMI088_ACCEL)
  {
    *pin = GPIO_PIN_4;
    return GPIOD;
  }
  *pin = GPIO_PIN_15;
  return GPIOA;
}

bool board_spi_read(uint16_t chip_select, uint8_t reg, uint8_t *data, size_t length)
{
  GPIO_TypeDef *port;
  uint16_t pin;
  uint8_t command = reg | 0x80U;
  uint8_t dummy;
  HAL_StatusTypeDef status;

  if (data == NULL || length == 0U || length > UINT16_MAX ||
      chip_select < BOARD_SPI_CS_BMI088_GYRO || chip_select > BOARD_SPI_CS_BMI270)
  {
    return false;
  }

  port = chip_select_port(chip_select, &pin);
  HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
  status = HAL_SPI_Transmit(&g_spi2, &command, 1U, SPI_TIMEOUT_MS);
  if (status == HAL_OK && chip_select == BOARD_SPI_CS_BMI088_ACCEL)
    status = HAL_SPI_Receive(&g_spi2, &dummy, 1U, SPI_TIMEOUT_MS);
  if (status == HAL_OK)
  {
    status = HAL_SPI_Receive(&g_spi2, data, (uint16_t)length, SPI_TIMEOUT_MS);
  }
  HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
  return status == HAL_OK;
}

bool board_spi_write(uint16_t chip_select, uint8_t reg, uint8_t value)
{
  GPIO_TypeDef *port;
  uint16_t pin;
  uint8_t data[2] = {reg & 0x7fU, value};
  HAL_StatusTypeDef status;

  if (chip_select < BOARD_SPI_CS_BMI088_GYRO || chip_select > BOARD_SPI_CS_BMI270)
    return false;

  port = chip_select_port(chip_select, &pin);
  HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
  status = HAL_SPI_Transmit(&g_spi2, data, sizeof(data), SPI_TIMEOUT_MS);
  HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
  return status == HAL_OK;
}

bool board_servo_set(uint16_t yaw_us, uint16_t pitch_us)
{
  if (yaw_us < 800U)
    yaw_us = 800U;
  if (yaw_us > 2200U)
    yaw_us = 2200U;
  if (pitch_us < 1100U)
    pitch_us = 1100U;
  if (pitch_us > 2200U)
    pitch_us = 2200U;
  __HAL_TIM_SET_COMPARE(&g_tim1, TIM_CHANNEL_4, yaw_us);
  __HAL_TIM_SET_COMPARE(&g_tim1, TIM_CHANNEL_3, pitch_us);
  if (!g_servo_started)
  {
    if (HAL_TIM_PWM_Start(&g_tim1, TIM_CHANNEL_3) != HAL_OK ||
        HAL_TIM_PWM_Start(&g_tim1, TIM_CHANNEL_4) != HAL_OK)
    {
      return false;
    }
    g_servo_started = true;
  }
  return true;
}

void board_servo_stop(void)
{
  (void)HAL_TIM_PWM_Stop(&g_tim1, TIM_CHANNEL_3);
  (void)HAL_TIM_PWM_Stop(&g_tim1, TIM_CHANNEL_4);
  g_servo_started = false;
}

I2C_HandleTypeDef *board_i2c_external(void)
{
  return &g_i2c1;
}

I2C_HandleTypeDef *board_i2c_internal(void)
{
  return &g_i2c2;
}

UART_HandleTypeDef *board_fc_uart_handle(void)
{
  return &g_fc_uart;
}

UART_HandleTypeDef *board_gps_uart_handle(void)
{
  return &g_gps_uart;
}

void board_fatal(void)
{
  __disable_irq();
  for (;;)
  {
    __NOP();
  }
}
