#ifndef TRACKER_FREERTOS_BOARD_H
#define TRACKER_FREERTOS_BOARD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "stm32h7xx_hal.h"

#ifndef BOARD_FC_BAUD_RATE
#define BOARD_FC_BAUD_RATE 57600U
#endif
#define BOARD_GPS_BAUD_RATE       230400U
#define BOARD_SERVO_FREQUENCY_HZ  330U
#define BOARD_SPI_CS_BMI088_GYRO  1U
#define BOARD_SPI_CS_BMI088_ACCEL 2U
#define BOARD_SPI_CS_BMI270       3U

bool board_init(void);
void board_start_io(void);
void board_poll_io(void);
void board_usb_reset_rx(void);
uint32_t board_millis(void);

size_t board_fc_read(uint8_t *data, size_t capacity,
                     uint32_t wait_ms); // board fc, board gps, board usb read data from uart,
                                        // return number of bytes read, wait_ms: timeout
size_t board_gps_read(uint8_t *data, size_t capacity, uint32_t wait_ms);
size_t board_usb_read(uint8_t *data, size_t capacity, uint32_t wait_ms);
bool board_fc_write(const uint8_t *data, size_t length, uint32_t timeout_ms);
bool board_usb_write(const uint8_t *data, size_t length);

void board_usb_rx_from_isr(const uint8_t *data, size_t length); // notifications from ISR

bool board_i2c_read(I2C_HandleTypeDef *bus, uint8_t address, uint8_t reg, uint8_t *data,
                    size_t length);
bool board_i2c_write(I2C_HandleTypeDef *bus, uint8_t address, uint8_t reg, uint8_t value);
bool board_spi_read(uint16_t chip_select, uint8_t reg, uint8_t *data, size_t length);
bool board_spi_write(uint16_t chip_select, uint8_t reg, uint8_t value);

bool board_servo_set(uint16_t yaw_us, uint16_t pitch_us);
void board_servo_stop(void);

I2C_HandleTypeDef *board_i2c_external(void);
I2C_HandleTypeDef *board_i2c_internal(void);
UART_HandleTypeDef *board_fc_uart_handle(void);
UART_HandleTypeDef *board_gps_uart_handle(void);

void board_fatal(void);

#endif
