#ifndef TRACKER_USB_DEVICE_H
#define TRACKER_USB_DEVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool usb_device_init(void);
bool usb_device_transmit(const uint8_t *data, size_t length);
bool usb_device_connected(uint32_t *generation);
void usb_device_reset_from_isr(void);

#endif
