#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "attitude.h"

static void check_near(float actual, float expected)
{
  assert(fabsf(actual - expected) < 0.01f);
}

static void initialize_pose(struct attitude_state_s *state, float roll, float pitch)
{
  struct attitude_params_s params;
  struct attitude_imu_sample_s sample = {};

  attitude_params_bmi088_default(&params);
  attitude_init(state, &params);
  sample.timestamp_us = 1000000U;
  sample.accel_m_s2.x = sinf(pitch) * 9.80665f;
  sample.accel_m_s2.y = -sinf(roll) * cosf(pitch) * 9.80665f;
  sample.accel_m_s2.z = -cosf(roll) * cosf(pitch) * 9.80665f;
  assert(attitude_update(state, &sample));
  check_near(state->roll_rad, roll);
  check_near(state->pitch_rad, pitch);
}

static void check_convergence(float roll, float pitch)
{
  attitude_state_s state;
  attitude_imu_sample_s sample = {};
  initialize_pose(&state, roll, pitch);
  sample.timestamp_us = 1000000U;
  sample.accel_m_s2.z = -9.80665f;
  for (unsigned i = 0; i < 3000; ++i)
  {
    sample.timestamp_us += 20000U;
    assert(attitude_update(&state, &sample));
  }
  check_near(state.roll_rad, 0.0f);
  check_near(state.pitch_rad, 0.0f);
}

int main()
{
  struct attitude_params_s params;
  struct attitude_state_s state;
  struct attitude_output_s output;
  struct attitude_imu_sample_s sample = {};

  attitude_params_bmi088_default(&params);
  attitude_init(&state, &params);
  assert(attitude_update_raw(&state, 0, 0, -10920, 0, 0, 0, 1000000U));
  attitude_get(&state, &output);
  assert(output.initialized && output.accel_correction_active);
  check_near(output.roll_rad, 0.0f);
  check_near(output.pitch_rad, 0.0f);
  assert(!attitude_update_raw(&state, 0, 0, -10920, 0, 0, 0, 1000000U));

  initialize_pose(&state, 0.0f, 0.35f);
  initialize_pose(&state, -0.5f, 0.0f);

  attitude_init(&state, &params);
  sample.timestamp_us = 1000000U;
  sample.accel_m_s2.z = -9.80665f;
  assert(attitude_update(&state, &sample));
  sample.timestamp_us += 100000U;
  assert(!attitude_update(&state, &sample));
  sample.timestamp_us += 20000U;
  assert(attitude_update(&state, &sample));
  check_convergence(0.0872665f, 0.0f);
  check_convergence(-0.0872665f, 0.0f);
  check_convergence(0.0f, 0.0872665f);
  check_convergence(0.0f, -0.0872665f);
  puts(
      "PASS: BMI088 scaling, level pose, roll/pitch poses, sample timing and feedback convergence");
  return 0;
}
