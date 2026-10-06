#ifndef __TRACKER_ATTITUDE_TYPES_H
#define __TRACKER_ATTITUDE_TYPES_H

#include <stdbool.h>
#include <stdint.h>

struct attitude_vec3_s
{
  float x;
  float y;
  float z;
};

/* Hamilton quaternion, body-to-local NED rotation: w, x, y, z. */
struct attitude_quaternion_s
{
  float w;
  float x;
  float y;
  float z;
};

struct attitude_imu_sample_s
{
  uint64_t timestamp_us;
  struct attitude_vec3_s accel_m_s2;
  struct attitude_vec3_s gyro_rad_s;
};

struct attitude_output_s
{
  uint64_t timestamp_us;
  struct attitude_quaternion_s q;
  struct attitude_vec3_s gyro_bias_rad_s;
  float roll_rad;
  float pitch_rad;
  float yaw_gyro_rad;
  bool initialized;
  bool accel_correction_active;
};

#endif
