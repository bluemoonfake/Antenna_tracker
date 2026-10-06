#ifndef TEST_BOARD_H
#define TEST_BOARD_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
void board_start_io(void);
void board_poll_io(void);
void board_usb_reset_rx(void);
void board_fatal(void);
uint32_t board_millis(void);
size_t board_fc_read(uint8_t *data, size_t capacity, uint32_t wait_ms);
size_t board_usb_read(uint8_t *data, size_t capacity, uint32_t wait_ms);
bool board_fc_write(const uint8_t *data, size_t length, uint32_t timeout_ms);
bool board_usb_write(const uint8_t *data, size_t length);
#endif
