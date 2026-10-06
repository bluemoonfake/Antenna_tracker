#include "usb_device.h"

#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "board.h"
#include "bridge_status.h"
#include "usbd_cdc.h"
#include "usbd_core.h"
#include "usbd_desc.h"

#define USB_RX_BUFFER_SIZE    CDC_DATA_FS_MAX_PACKET_SIZE
#define USB_TX_BUFFER_SIZE    CDC_DATA_FS_MAX_PACKET_SIZE
#define TRACKER_USB_DEVICE_ID 0U

static USBD_HandleTypeDef g_usb_device;
static uint8_t g_usb_rx_buffer[USB_RX_BUFFER_SIZE];
static uint8_t g_usb_tx_buffer[USB_TX_BUFFER_SIZE];
static volatile bool g_tx_busy;
static volatile uint32_t g_generation;
static uint32_t g_tx_length;
static uint8_t g_line_coding[7] = {(uint8_t)BOARD_FC_BAUD_RATE,
                                   (uint8_t)(BOARD_FC_BAUD_RATE >> 8),
                                   (uint8_t)(BOARD_FC_BAUD_RATE >> 16),
                                   (uint8_t)(BOARD_FC_BAUD_RATE >> 24),
                                   0U,
                                   0U,
                                   8U};

static void abort_tx(void)
{
  if (g_tx_busy)
    g_bridge_io_status.usb_tx_aborted_bytes += g_tx_length;
  g_tx_busy = false;
}

static int8_t cdc_init(void)
{
  abort_tx();
  g_generation++;
  (void)USBD_CDC_SetTxBuffer(&g_usb_device, g_usb_tx_buffer, 0U);
  (void)USBD_CDC_SetRxBuffer(&g_usb_device, g_usb_rx_buffer);
  return USBD_OK;
}

static int8_t cdc_deinit(void)
{
  abort_tx();
  g_generation++;
  return USBD_OK;
}

static int8_t cdc_control(uint8_t command, uint8_t *data, uint16_t length)
{
  if (length < sizeof(g_line_coding))
    return USBD_OK;
  /* CDC line coding describes the virtual port; physical UART baud is fixed
   * by BOARD_FC_BAUD_RATE, not changed asynchronously by a host request. */
  if (command == CDC_SET_LINE_CODING)
    memcpy(g_line_coding, data, sizeof(g_line_coding));
  else if (command == CDC_GET_LINE_CODING)
    memcpy(data, g_line_coding, sizeof(g_line_coding));
  return USBD_OK;
}

static int8_t cdc_receive(uint8_t *data, uint32_t *length)
{
  board_usb_rx_from_isr(data, *length);
  (void)USBD_CDC_SetRxBuffer(&g_usb_device, g_usb_rx_buffer);
  (void)USBD_CDC_ReceivePacket(&g_usb_device);
  return USBD_OK;
}

static int8_t cdc_transmit_complete(uint8_t *data, uint32_t *length, uint8_t endpoint)
{
  (void)data;
  g_bridge_io_status.usb_tx_completed_bytes += *length;
  (void)endpoint;
  g_tx_busy = false;
  return USBD_OK;
}

static USBD_CDC_ItfTypeDef g_cdc_interface = {cdc_init, cdc_deinit, cdc_control, cdc_receive,
                                              cdc_transmit_complete};

bool usb_device_init(void)
{
  configASSERT(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
  if (USBD_Init(&g_usb_device, &tracker_usb_descriptors, TRACKER_USB_DEVICE_ID) != USBD_OK ||
      USBD_RegisterClass(&g_usb_device, &USBD_CDC) != USBD_OK ||
      USBD_CDC_RegisterInterface(&g_usb_device, &g_cdc_interface) != USBD_OK ||
      USBD_Start(&g_usb_device) != USBD_OK)
  {
    return false;
  }
  return true;
}

bool usb_device_transmit(const uint8_t *data, size_t length)
{
  bool accepted = false;

  if (data == NULL || length == 0U || length > sizeof(g_usb_tx_buffer))
  {
    return false;
  }

  taskENTER_CRITICAL();
  if (!g_tx_busy && g_usb_device.dev_state == USBD_STATE_CONFIGURED &&
      g_usb_device.pClassDataCmsit[0] != NULL)
  {
    g_tx_busy = true;
    g_tx_length = (uint32_t)length;
    memcpy(g_usb_tx_buffer, data, length);
    (void)USBD_CDC_SetTxBuffer(&g_usb_device, g_usb_tx_buffer, (uint32_t)length);
    if (USBD_CDC_TransmitPacket(&g_usb_device) == USBD_OK)
    {
      accepted = true;
      g_bridge_io_status.usb_tx_bytes += (uint32_t)length;
    } else
    {
      g_tx_busy = false;
    }
  }
  taskEXIT_CRITICAL();
  return accepted;
}

bool usb_device_connected(uint32_t *generation)
{
  taskENTER_CRITICAL();
  *generation = g_generation;
  bool connected =
      g_usb_device.dev_state == USBD_STATE_CONFIGURED && g_usb_device.pClassDataCmsit[0] != NULL;
  taskEXIT_CRITICAL();
  return connected;
}

void usb_device_reset_from_isr(void)
{
  abort_tx();
  g_generation++;
  g_bridge_io_status.usb_reset_events++;
}
