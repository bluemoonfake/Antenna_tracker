#ifndef TRACKER_SENSOR_DIAGNOSTICS_H
#define TRACKER_SENSOR_DIAGNOSTICS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  uint32_t task_iterations;
  uint32_t task_last_run_ms;
  uint32_t stack_high_water_words;
  uint32_t gps_rx_bytes;
  uint32_t gps_rx_dropped_bytes;
  uint32_t gps_uart_errors;
  uint32_t gps_rx_restart_errors;
  uint32_t gps_nav_pvt_frames;
  uint32_t gps_checksum_errors;
  uint32_t gps_oversized_frames;
  uint32_t gps_invalid_fixes;
  uint32_t gps_state_update_errors;
  uint32_t gps_last_nav_pvt_ms;
  uint32_t gps_last_fix_ms;
  uint32_t gps_fix_age_ms;
  int32_t gps_latitude_e7;
  int32_t gps_longitude_e7;
  int32_t gps_altitude_msl_mm;
  uint8_t gps_fix_type;
  uint8_t gps_satellites;
  bool gps_fix_valid;
  bool gps_state_valid;
  uint32_t compass_init_attempts;
  uint32_t compass_init_errors;
  uint32_t compass_samples;
  uint32_t compass_read_errors;
  uint32_t compass_not_ready;
  uint32_t compass_state_update_errors;
  uint32_t compass_last_sample_ms;
  uint32_t compass_sample_age_ms;
  uint32_t compass_max_period_ms;
  int16_t compass_x;
  int16_t compass_y;
  int16_t compass_z;
  int16_t compass_min_x;
  int16_t compass_max_x;
  int16_t compass_min_y;
  int16_t compass_max_y;
  float heading_raw_deg;
  bool compass_ready;
  bool heading_valid;
  bool heading_calibrated;
  uint32_t bmi088_init_attempts;
  uint32_t bmi088_init_errors;
  uint32_t bmi088_samples;
  uint32_t bmi088_read_errors;
  uint32_t bmi088_filter_rejects;
  uint32_t bmi088_state_update_errors;
  uint32_t bmi088_last_sample_ms;
  uint32_t bmi088_sample_age_ms;
  uint32_t bmi088_max_period_ms;
  uint8_t bmi088_accel_chip_id;
  uint8_t bmi088_gyro_chip_id;
  int16_t accel_x;
  int16_t accel_y;
  int16_t accel_z;
  int16_t gyro_x;
  int16_t gyro_y;
  int16_t gyro_z;
  float roll_rad;
  float pitch_rad;
  float gyro_yaw_rad;
  bool bmi088_ready;
  bool attitude_valid;
} sensor_diagnostics_t;

extern volatile sensor_diagnostics_t g_sensor_diagnostics;

#endif
