/****************************************************************************
 * Lightweight 6-axis attitude estimator for the MicoAir tracker pitch frame.
 *
 * The public API is C-compatible for FreeRTOS tasks and host tests, while the
 * implementation remains C++ for the tracker application.
 ****************************************************************************/

#ifndef __TRACKER_ATTITUDE_H
#define __TRACKER_ATTITUDE_H

#include <nuttx/config.h>

#include <stdbool.h>
#include <stdint.h>

#include "attitude_params.h"
#include "attitude_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

void attitude_params_bmi088_default(FAR struct attitude_params_s *params);

void attitude_init(FAR struct attitude_state_s *state,
                   FAR const struct attitude_params_s *params);

void attitude_reset(FAR struct attitude_state_s *state);

/* Convert raw sensor words, apply the configured bias and axis rotation, then
 * update the filter.  Acceleration is treated as specific force, so the
 * gravity/down reference used by the filter is -acceleration. */
bool attitude_update_raw(FAR struct attitude_state_s *state,
                         int16_t accel_x, int16_t accel_y, int16_t accel_z,
                         int16_t gyro_x, int16_t gyro_y, int16_t gyro_z,
                         uint64_t timestamp_us);

/* Update using an already calibrated and rotated SI-unit IMU sample. */
bool attitude_update(FAR struct attitude_state_s *state,
                     FAR const struct attitude_imu_sample_s *sample);

void attitude_get(FAR const struct attitude_state_s *state,
                  FAR struct attitude_output_s *output);

#ifdef __cplusplus
}
#endif

#endif
