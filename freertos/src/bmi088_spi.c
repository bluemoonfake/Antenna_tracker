#include "bmi088_spi.h"

#include "FreeRTOS.h"
#include "task.h"

#include "board.h"

#define BMI088_ACCEL_CHIP_ID  0x00U
#define BMI088_ACCEL_STATUS   0x03U
#define BMI088_ACCEL_DATA     0x12U
#define BMI088_ACCEL_CONF     0x40U
#define BMI088_ACCEL_RANGE    0x41U
#define BMI088_ACCEL_PWR_CONF 0x7cU
#define BMI088_ACCEL_PWR_CTRL 0x7dU
#define BMI088_ACCEL_RESET    0x7eU
#define BMI088_ACCEL_ID       0x1eU
#define BMI088_GYRO_CHIP_ID   0x00U
#define BMI088_GYRO_DATA      0x02U
#define BMI088_GYRO_RANGE     0x0fU
#define BMI088_GYRO_BANDWIDTH 0x10U
#define BMI088_GYRO_RESET     0x14U
#define BMI088_GYRO_ID        0x0fU

static int16_t read_i16(const uint8_t *data)
{
  return (int16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static bool register_matches(uint16_t cs, uint8_t reg, uint8_t expected, uint8_t mask)
{
  uint8_t value;
  return board_spi_read(cs, reg, &value, 1U) && (value & mask) == expected;
}

static bool configuration_valid(void)
{
  return register_matches(BOARD_SPI_CS_BMI088_ACCEL, BMI088_ACCEL_CHIP_ID, BMI088_ACCEL_ID,
                          0xffU) &&
         register_matches(BOARD_SPI_CS_BMI088_GYRO, BMI088_GYRO_CHIP_ID, BMI088_GYRO_ID, 0xffU) &&
         register_matches(BOARD_SPI_CS_BMI088_ACCEL, BMI088_ACCEL_PWR_CONF, 0U, 0xffU) &&
         register_matches(BOARD_SPI_CS_BMI088_ACCEL, BMI088_ACCEL_PWR_CTRL, 4U, 0xffU) &&
         register_matches(BOARD_SPI_CS_BMI088_ACCEL, BMI088_ACCEL_CONF, 0xacU, 0xffU) &&
         register_matches(BOARD_SPI_CS_BMI088_ACCEL, BMI088_ACCEL_RANGE, 0U, 3U) &&
         register_matches(BOARD_SPI_CS_BMI088_GYRO, BMI088_GYRO_RANGE, 0U, 7U) &&
         register_matches(BOARD_SPI_CS_BMI088_GYRO, BMI088_GYRO_BANDWIDTH, 7U, 0x0fU);
}

bool bmi088_spi_init(uint8_t *accel_chip_id, uint8_t *gyro_chip_id)
{
  uint8_t accel_id = 0U;
  uint8_t gyro_id = 0U;

  if (accel_chip_id == NULL || gyro_chip_id == NULL)
    return false;
  *accel_chip_id = 0U;
  *gyro_chip_id = 0U;

  /* The first accel read switches the part from I2C to SPI; discard its ID. */
  if (!board_spi_read(BOARD_SPI_CS_BMI088_ACCEL, BMI088_ACCEL_CHIP_ID, &accel_id, 1U))
    return false;
  vTaskDelay(pdMS_TO_TICKS(1U));
  if (!board_spi_read(BOARD_SPI_CS_BMI088_ACCEL, BMI088_ACCEL_CHIP_ID, &accel_id, 1U) ||
      !board_spi_read(BOARD_SPI_CS_BMI088_GYRO, BMI088_GYRO_CHIP_ID, &gyro_id, 1U))
    return false;

  *accel_chip_id = accel_id;
  *gyro_chip_id = gyro_id;
  if (accel_id != BMI088_ACCEL_ID || gyro_id != BMI088_GYRO_ID)
    return false;

  if (!board_spi_write(BOARD_SPI_CS_BMI088_ACCEL, BMI088_ACCEL_RESET, 0xb6U) ||
      !board_spi_write(BOARD_SPI_CS_BMI088_GYRO, BMI088_GYRO_RESET, 0xb6U))
    return false;
  vTaskDelay(pdMS_TO_TICKS(50U));
  /* Re-establish accel SPI access after reset, as in Bosch SensorAPI. */
  if (!board_spi_read(BOARD_SPI_CS_BMI088_ACCEL, BMI088_ACCEL_CHIP_ID, &accel_id, 1U))
    return false;

  if (!board_spi_write(BOARD_SPI_CS_BMI088_ACCEL, BMI088_ACCEL_PWR_CONF, 0x00U))
    return false;
  vTaskDelay(pdMS_TO_TICKS(5U));
  if (!board_spi_write(BOARD_SPI_CS_BMI088_ACCEL, BMI088_ACCEL_PWR_CTRL, 0x04U))
    return false;
  vTaskDelay(pdMS_TO_TICKS(5U));

  return board_spi_write(BOARD_SPI_CS_BMI088_ACCEL, BMI088_ACCEL_CONF, 0xacU) &&
         board_spi_write(BOARD_SPI_CS_BMI088_ACCEL, BMI088_ACCEL_RANGE, 0x00U) &&
         board_spi_write(BOARD_SPI_CS_BMI088_GYRO, BMI088_GYRO_RANGE, 0x00U) &&
         board_spi_write(BOARD_SPI_CS_BMI088_GYRO, BMI088_GYRO_BANDWIDTH, 0x07U) &&
         configuration_valid();
}

bool bmi088_spi_read(bmi088_raw_sample_t *sample)
{
  uint8_t accel[6];
  uint8_t gyro[6];
  uint8_t status;

  if (sample == NULL || !configuration_valid() ||
      !board_spi_read(BOARD_SPI_CS_BMI088_ACCEL, BMI088_ACCEL_STATUS, &status, 1U) ||
      (status & 0x80U) == 0U ||
      !board_spi_read(BOARD_SPI_CS_BMI088_ACCEL, BMI088_ACCEL_DATA, accel, sizeof(accel)) ||
      !board_spi_read(BOARD_SPI_CS_BMI088_GYRO, BMI088_GYRO_DATA, gyro, sizeof(gyro)))
    return false;

  sample->accel_x = read_i16(&accel[0]);
  sample->accel_y = read_i16(&accel[2]);
  sample->accel_z = read_i16(&accel[4]);
  sample->gyro_x = read_i16(&gyro[0]);
  sample->gyro_y = read_i16(&gyro[2]);
  sample->gyro_z = read_i16(&gyro[4]);
  return true;
}
