#ifndef TRACKER_BMI088_SPI_H
#define TRACKER_BMI088_SPI_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  int16_t accel_x;
  int16_t accel_y;
  int16_t accel_z;
  int16_t gyro_x;
  int16_t gyro_y;
  int16_t gyro_z;
} bmi088_raw_sample_t;

bool bmi088_spi_init(uint8_t *accel_chip_id, uint8_t *gyro_chip_id);
bool bmi088_spi_read(bmi088_raw_sample_t *sample);

#endif
