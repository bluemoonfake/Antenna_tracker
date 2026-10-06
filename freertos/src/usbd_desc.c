#include "usbd_desc.h"

#include "stm32h7xx_hal.h"
#include "usbd_core.h"

#define TRACKER_USB_VID         0x1b8cU
#define TRACKER_USB_PID         0x0036U
#define TRACKER_USB_LANG_ID     0x0409U
#define TRACKER_USB_SERIAL_SIZE 26U

static uint8_t *device_descriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *language_descriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *manufacturer_descriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *product_descriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *serial_descriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *configuration_descriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *interface_descriptor(USBD_SpeedTypeDef speed, uint16_t *length);

USBD_DescriptorsTypeDef tracker_usb_descriptors = {
    device_descriptor, language_descriptor,      manufacturer_descriptor, product_descriptor,
    serial_descriptor, configuration_descriptor, interface_descriptor};

__ALIGN_BEGIN static uint8_t g_device_descriptor[USB_LEN_DEV_DESC] __ALIGN_END = {
    USB_LEN_DEV_DESC,
    USB_DESC_TYPE_DEVICE,
    0x00,
    0x02,
    0x02,
    0x02,
    0x00,
    USB_MAX_EP0_SIZE,
    LOBYTE(TRACKER_USB_VID),
    HIBYTE(TRACKER_USB_VID),
    LOBYTE(TRACKER_USB_PID),
    HIBYTE(TRACKER_USB_PID),
    0x00,
    0x01,
    USBD_IDX_MFC_STR,
    USBD_IDX_PRODUCT_STR,
    USBD_IDX_SERIAL_STR,
    USBD_MAX_NUM_CONFIGURATION};

__ALIGN_BEGIN static uint8_t g_language_descriptor[USB_LEN_LANGID_STR_DESC] __ALIGN_END = {
    USB_LEN_LANGID_STR_DESC, USB_DESC_TYPE_STRING, LOBYTE(TRACKER_USB_LANG_ID),
    HIBYTE(TRACKER_USB_LANG_ID)};

__ALIGN_BEGIN static uint8_t g_string_descriptor[USBD_MAX_STR_DESC_SIZ] __ALIGN_END;
__ALIGN_BEGIN static uint8_t g_serial_descriptor[TRACKER_USB_SERIAL_SIZE] __ALIGN_END = {
    TRACKER_USB_SERIAL_SIZE, USB_DESC_TYPE_STRING};

static void hex_to_unicode(uint32_t value, uint8_t *output, uint8_t digits)
{
  uint8_t index;
  for (index = 0; index < digits; index++)
  {
    uint8_t nibble = (uint8_t)(value >> 28);
    output[index * 2U] = nibble < 10U ? (uint8_t)('0' + nibble) : (uint8_t)('A' + nibble - 10U);
    output[index * 2U + 1U] = 0;
    value <<= 4;
  }
}

static uint8_t *device_descriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  (void)speed;
  *length = sizeof(g_device_descriptor);
  return g_device_descriptor;
}

static uint8_t *language_descriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  (void)speed;
  *length = sizeof(g_language_descriptor);
  return g_language_descriptor;
}

static uint8_t *make_string(const char *string, uint16_t *length)
{
  USBD_GetString((uint8_t *)string, g_string_descriptor, length);
  return g_string_descriptor;
}

static uint8_t *manufacturer_descriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  (void)speed;
  return make_string("MicoAir", length);
}

static uint8_t *product_descriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  (void)speed;
  return make_string("MicoAir H743 FreeRTOS Tracker", length);
}

static uint8_t *serial_descriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  uint32_t serial0 = HAL_GetUIDw0() + HAL_GetUIDw2();
  (void)speed;
  hex_to_unicode(serial0, &g_serial_descriptor[2], 8U);
  hex_to_unicode(HAL_GetUIDw1(), &g_serial_descriptor[18], 4U);
  *length = sizeof(g_serial_descriptor);
  return g_serial_descriptor;
}

static uint8_t *configuration_descriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  (void)speed;
  return make_string("MAVLink CDC", length);
}

static uint8_t *interface_descriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  (void)speed;
  return make_string("MAVLink Bridge", length);
}
