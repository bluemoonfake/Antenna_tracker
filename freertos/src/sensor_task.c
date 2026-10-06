#include "tracker_tasks.h"

#include <math.h>

#include "FreeRTOS.h"
#include "task.h"

#include "app_state.h"
#include "bmi088_spi.h"
#include "board.h"
#include "attitude.h"
#include "gps_ubx.h"
#include "sensor_diagnostics.h"
#include "tracker_config.h"

#define QMC5883L_ADDRESS      0x0dU
#define QMC5883L_REG_DATA     0x00U
#define QMC5883L_REG_STATUS   0x06U
#define QMC5883L_REG_CONTROL1 0x09U
#define QMC5883L_REG_CONTROL2 0x0aU
#define QMC5883L_STATUS_READY 0x01U
#define SENSOR_SAMPLE_MS      20U
#define SENSOR_RETRY_MS       1000U
#define SENSOR_DIAG_MS        500U
#define RAD_TO_DEG            57.295779513082320876f

//Task thu thap cam bien: doc GPS, IMU, Compass
volatile sensor_diagnostics_t g_sensor_diagnostics;

static int16_t read_i16(const uint8_t *data)
{
  return (int16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static bool compass_init(void)
{
  I2C_HandleTypeDef *bus = board_i2c_external();
  g_sensor_diagnostics.compass_init_attempts++;
  g_sensor_diagnostics.compass_ready =
      board_i2c_write(bus, QMC5883L_ADDRESS, QMC5883L_REG_CONTROL2, 0x80U) &&
      board_i2c_write(bus, QMC5883L_ADDRESS, QMC5883L_REG_CONTROL1, 0x1dU);
  if (!g_sensor_diagnostics.compass_ready)
    g_sensor_diagnostics.compass_init_errors++;
  return g_sensor_diagnostics.compass_ready;
}

static bool compass_update(uint32_t now_ms)
{
  I2C_HandleTypeDef *bus = board_i2c_external();
  uint8_t status;
  uint8_t raw[6];
  int16_t x;
  int16_t y;
  int16_t z;
  float heading;

  if (!board_i2c_read(bus, QMC5883L_ADDRESS, QMC5883L_REG_STATUS, &status, 1U))
  {
    g_sensor_diagnostics.compass_read_errors++;
    return false;
  }
  if ((status & QMC5883L_STATUS_READY) == 0U)
  {
    g_sensor_diagnostics.compass_not_ready++;
    return false;
  }
  if (!board_i2c_read(bus, QMC5883L_ADDRESS, QMC5883L_REG_DATA, raw, sizeof(raw)))
  {
    g_sensor_diagnostics.compass_read_errors++;
    return false;
  }

  x = read_i16(&raw[0]);
  y = read_i16(&raw[2]);
  z = read_i16(&raw[4]);
  if (x == 0 && y == 0)
  {
    g_sensor_diagnostics.compass_read_errors++;
    return false;
  }

  heading = atan2f((float)y, (float)x) * RAD_TO_DEG;
  if (heading < 0.0f)
    heading += 360.0f;

  if (g_sensor_diagnostics.compass_samples == 0U)
  {
    g_sensor_diagnostics.compass_min_x = x;
    g_sensor_diagnostics.compass_max_x = x;
    g_sensor_diagnostics.compass_min_y = y;
    g_sensor_diagnostics.compass_max_y = y;
  } else
  {
    if (x < g_sensor_diagnostics.compass_min_x)
      g_sensor_diagnostics.compass_min_x = x;
    if (x > g_sensor_diagnostics.compass_max_x)
      g_sensor_diagnostics.compass_max_x = x;
    if (y < g_sensor_diagnostics.compass_min_y)
      g_sensor_diagnostics.compass_min_y = y;
    if (y > g_sensor_diagnostics.compass_max_y)
      g_sensor_diagnostics.compass_max_y = y;
    uint32_t period_ms = now_ms - g_sensor_diagnostics.compass_last_sample_ms;
    if (period_ms > g_sensor_diagnostics.compass_max_period_ms)
      g_sensor_diagnostics.compass_max_period_ms = period_ms;
  }

  g_sensor_diagnostics.compass_x = x;
  g_sensor_diagnostics.compass_y = y;
  g_sensor_diagnostics.compass_z = z;
  g_sensor_diagnostics.heading_raw_deg = heading;
  g_sensor_diagnostics.compass_last_sample_ms = now_ms;
  g_sensor_diagnostics.compass_samples++;
  if (!app_state_update_heading(heading, now_ms))
  {
    g_sensor_diagnostics.compass_state_update_errors++;
    return false;
  }
  return true;
}

static bool bmi088_init(void)
{
  uint8_t accel_chip_id;
  uint8_t gyro_chip_id;

  g_sensor_diagnostics.bmi088_init_attempts++;
  g_sensor_diagnostics.bmi088_ready = bmi088_spi_init(&accel_chip_id, &gyro_chip_id);
  g_sensor_diagnostics.bmi088_accel_chip_id = accel_chip_id;
  g_sensor_diagnostics.bmi088_gyro_chip_id = gyro_chip_id;
  if (!g_sensor_diagnostics.bmi088_ready)
    g_sensor_diagnostics.bmi088_init_errors++;
  return g_sensor_diagnostics.bmi088_ready;
}

static bool bmi088_update(struct attitude_state_s *attitude, uint32_t now_ms, uint64_t timestamp_us)
{
  bmi088_raw_sample_t sample;
  struct attitude_output_s output;

  if (!bmi088_spi_read(&sample))
  {
    g_sensor_diagnostics.bmi088_read_errors++;
    return false;
  }

  g_sensor_diagnostics.accel_x = sample.accel_x;
  g_sensor_diagnostics.accel_y = sample.accel_y;
  g_sensor_diagnostics.accel_z = sample.accel_z;
  g_sensor_diagnostics.gyro_x = sample.gyro_x;
  g_sensor_diagnostics.gyro_y = sample.gyro_y;
  g_sensor_diagnostics.gyro_z = sample.gyro_z;

  if (!attitude_update_raw(attitude, sample.accel_x, sample.accel_y, sample.accel_z, sample.gyro_x,
                           sample.gyro_y, sample.gyro_z, timestamp_us))
  {
    g_sensor_diagnostics.bmi088_filter_rejects++;
    return false;
  }

  attitude_get(attitude, &output);
  if (g_sensor_diagnostics.bmi088_samples > 0U)
  {
    uint32_t period_ms = now_ms - g_sensor_diagnostics.bmi088_last_sample_ms;
    if (period_ms > g_sensor_diagnostics.bmi088_max_period_ms)
      g_sensor_diagnostics.bmi088_max_period_ms = period_ms;
  }
  g_sensor_diagnostics.roll_rad = output.roll_rad;
  g_sensor_diagnostics.pitch_rad = output.pitch_rad;
  g_sensor_diagnostics.gyro_yaw_rad = output.yaw_gyro_rad;
  g_sensor_diagnostics.bmi088_last_sample_ms = now_ms;
  g_sensor_diagnostics.bmi088_samples++;
  g_sensor_diagnostics.attitude_valid = output.initialized;
  if (!app_state_update_attitude(output.roll_rad, output.pitch_rad, now_ms))
  {
    g_sensor_diagnostics.bmi088_state_update_errors++;
    return false;
  }
  return true;
}

static void gps_update(gps_ubx_parser_t *parser, gps_ubx_position_t *position, uint8_t byte,
                       uint32_t now_ms)
{
  bool nav_pvt = gps_ubx_parse(parser, byte, position);
  g_sensor_diagnostics.gps_nav_pvt_frames = parser->nav_pvt_frames;
  g_sensor_diagnostics.gps_checksum_errors = parser->checksum_errors;
  g_sensor_diagnostics.gps_oversized_frames = parser->oversized_frames;
  g_sensor_diagnostics.gps_invalid_fixes = parser->invalid_fixes;

  if (!nav_pvt)
    return;

  g_sensor_diagnostics.gps_last_nav_pvt_ms = now_ms;
  g_sensor_diagnostics.gps_fix_type = position->fix_type;
  g_sensor_diagnostics.gps_satellites = position->satellites;
  g_sensor_diagnostics.gps_latitude_e7 = position->latitude_e7;
  g_sensor_diagnostics.gps_longitude_e7 = position->longitude_e7;
  g_sensor_diagnostics.gps_altitude_msl_mm = position->altitude_msl_mm;
  g_sensor_diagnostics.gps_fix_valid = position->valid;

  if (position->valid)
  {
    g_sensor_diagnostics.gps_last_fix_ms = now_ms;
    if (app_state_update_station(position->latitude_e7, position->longitude_e7,
                                 position->altitude_msl_mm, now_ms))
    {
      g_sensor_diagnostics.gps_state_valid = true;
    } else
    {
      g_sensor_diagnostics.gps_state_update_errors++;
      if (app_state_invalidate_station())
        g_sensor_diagnostics.gps_state_valid = false;
      else
        g_sensor_diagnostics.gps_state_update_errors++;
      g_sensor_diagnostics.gps_fix_valid = false;
    }
  } else if (app_state_invalidate_station())
  {
    g_sensor_diagnostics.gps_state_valid = false;
  } else
  {
    g_sensor_diagnostics.gps_state_update_errors++;
  }
}

static void sensor_task(void *argument)
{
  //khoi tao UBX parser
  gps_ubx_parser_t parser;
  gps_ubx_position_t position = {0};
  struct attitude_params_s attitude_params;
  struct attitude_state_s attitude;
  uint8_t bytes[128];
  TickType_t last_sample_tick;
  uint32_t last_compass_init_ms;
  uint32_t last_bmi088_init_ms;
  uint32_t last_diagnostics_ms;
  uint32_t previous_ms;
  uint64_t timestamp_us = 0U;
  (void)argument;

  gps_ubx_init(&parser);
  attitude_params_bmi088_default(&attitude_params);
  attitude_init(&attitude, &attitude_params);
  //delay 100ms de init
  vTaskDelay(pdMS_TO_TICKS(100U));
  //chon thoi diem init
  last_compass_init_ms = board_millis();
  compass_init();
  last_bmi088_init_ms = board_millis();
  bmi088_init();
  last_sample_tick = xTaskGetTickCount(); //tra ve so tick da troi qua ke tu khi scheduler
  last_diagnostics_ms = board_millis();
  previous_ms = last_diagnostics_ms;
  for (;;)
  {
    size_t count = board_gps_read(bytes, sizeof(bytes), 10U);
    uint32_t now_ms = board_millis();
    size_t index;

    timestamp_us += (uint64_t)(uint32_t)(now_ms - previous_ms) * 1000U;
    previous_ms = now_ms;

    g_sensor_diagnostics.task_iterations++;
    g_sensor_diagnostics.task_last_run_ms = now_ms;
    for (index = 0; index < count; index++)
      gps_update(&parser, &position, bytes[index], now_ms);

    if (g_sensor_diagnostics.gps_last_fix_ms != 0U)
      g_sensor_diagnostics.gps_fix_age_ms = now_ms - g_sensor_diagnostics.gps_last_fix_ms;
    if (g_sensor_diagnostics.gps_fix_valid && g_sensor_diagnostics.gps_last_fix_ms != 0U &&
        g_sensor_diagnostics.gps_fix_age_ms > TRACKER_STATION_GPS_TIMEOUT_MS)
      g_sensor_diagnostics.gps_fix_valid = false;
    if (!g_sensor_diagnostics.gps_fix_valid && g_sensor_diagnostics.gps_state_valid)
    {
      if (app_state_invalidate_station())
        g_sensor_diagnostics.gps_state_valid = false;
      else
        g_sensor_diagnostics.gps_state_update_errors++;
    }

    if ((xTaskGetTickCount() - last_sample_tick) >= pdMS_TO_TICKS(SENSOR_SAMPLE_MS))
    {
      if (!g_sensor_diagnostics.compass_ready && now_ms - last_compass_init_ms >= SENSOR_RETRY_MS)
      {
        last_compass_init_ms = now_ms;
        compass_init();
      }
      bool heading_valid = g_sensor_diagnostics.compass_ready && compass_update(now_ms);
      g_sensor_diagnostics.heading_valid = heading_valid;
      if (!heading_valid && !app_state_invalidate_heading())
        g_sensor_diagnostics.compass_state_update_errors++;
      if (!heading_valid && now_ms - last_compass_init_ms >= SENSOR_RETRY_MS &&
          (g_sensor_diagnostics.compass_samples == 0U ||
           now_ms - g_sensor_diagnostics.compass_last_sample_ms >= TRACKER_HEADING_TIMEOUT_MS))
      {
        g_sensor_diagnostics.compass_ready = false;
        last_compass_init_ms = now_ms;
      }

      if (!g_sensor_diagnostics.bmi088_ready && now_ms - last_bmi088_init_ms >= SENSOR_RETRY_MS)
      {
        last_bmi088_init_ms = now_ms;
        if (bmi088_init())
          attitude_reset(&attitude);
      }
      now_ms = board_millis();
      timestamp_us += (uint64_t)(uint32_t)(now_ms - previous_ms) * 1000U;
      previous_ms = now_ms;
      bool imu_valid =
          g_sensor_diagnostics.bmi088_ready && bmi088_update(&attitude, now_ms, timestamp_us);
      g_sensor_diagnostics.attitude_valid = imu_valid;
      if (!imu_valid && !app_state_invalidate_attitude())
        g_sensor_diagnostics.bmi088_state_update_errors++;
      if (!imu_valid && now_ms - last_bmi088_init_ms >= SENSOR_RETRY_MS &&
          (g_sensor_diagnostics.bmi088_samples == 0U ||
           now_ms - g_sensor_diagnostics.bmi088_last_sample_ms >= TRACKER_IMU_TIMEOUT_MS))
      {
        g_sensor_diagnostics.bmi088_ready = false;
        last_bmi088_init_ms = now_ms;
      }
      last_sample_tick = xTaskGetTickCount();
    }

    g_sensor_diagnostics.compass_sample_age_ms =
        g_sensor_diagnostics.compass_samples ? now_ms - g_sensor_diagnostics.compass_last_sample_ms
                                             : UINT32_MAX;
    g_sensor_diagnostics.bmi088_sample_age_ms =
        g_sensor_diagnostics.bmi088_samples ? now_ms - g_sensor_diagnostics.bmi088_last_sample_ms
                                            : UINT32_MAX;

    if (now_ms - last_diagnostics_ms >= SENSOR_DIAG_MS)
    {
      last_diagnostics_ms = now_ms;
      g_sensor_diagnostics.stack_high_water_words = uxTaskGetStackHighWaterMark(NULL);
    }
  }
}

bool sensor_task_start(void)
{
  return xTaskCreate(sensor_task, "sensors", TRACKER_SENSOR_TASK_STACK_WORDS, NULL,
                     TRACKER_SENSOR_TASK_PRIORITY, NULL) == pdPASS;
}
