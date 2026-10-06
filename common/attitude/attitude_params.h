#ifndef __TRACKER_ATTITUDE_PARAMS_H
#define __TRACKER_ATTITUDE_PARAMS_H

#include <stdint.h>

#include "attitude_types.h"

/* The input transform is deliberately explicit.  Calibration and frame
 * rotation are applied before fusion, so the filter only handles SI data in
 * the tracker body frame. */
struct attitude_sensor_params_s
{
  float accel_m_s2_per_lsb;
  float gyro_rad_s_per_lsb;
  float accel_bias_lsb[3];
  float gyro_bias_lsb[3];
  uint8_t axis_map[3];
  int8_t axis_sign[3];
};

struct attitude_filter_params_s
{
  float accel_correction_gain;
  float gyro_bias_gain;
  float gyro_bias_limit_rad_s;
  float accel_min_m_s2;
  float accel_max_m_s2;
  float max_dt_s;
};

struct attitude_params_s
{
  struct attitude_sensor_params_s sensor;
  struct attitude_filter_params_s filter;
};

struct attitude_state_s
{
  struct attitude_params_s params;
  struct attitude_quaternion_s q;
  struct attitude_vec3_s gyro_bias_rad_s;
  uint64_t last_timestamp_us;
  float roll_rad;
  float pitch_rad;
  float yaw_gyro_rad;
  bool initialized;
  bool accel_correction_active;
};

#endif
