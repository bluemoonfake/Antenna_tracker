  /****************************************************************************
   * Project-owned 6-axis quaternion complementary filter.
   *
   * The initialization, gravity correction and bounded gyro-bias update are
   * ported in simplified form from PX4 AttitudeEstimatorQ.  PX4 scheduling,
   * uORB, parameter storage, GPS and magnetometer fusion are intentionally not
   * included: the QMC5883L is on the yaw-only structure, not the IMU frame.
   ****************************************************************************/

  #include <nuttx/config.h>

  #include <math.h>
  #include <string.h>

  #include "attitude.h"

  #define ATTITUDE_PI 3.14159265358979323846f
  #define ATTITUDE_ONE_G 9.80665f
  #define ATTITUDE_STILL_RATE_RAD_S 0.175f
  #define ATTITUDE_EPSILON 0.000001f

  static float vector_norm(FAR const struct attitude_vec3_s *v)
  {
    return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
  }

  static struct attitude_vec3_s vector_cross(FAR const struct attitude_vec3_s *a,
                                            FAR const struct attitude_vec3_s *b)
  {
    struct attitude_vec3_s result;

    result.x = a->y * b->z - a->z * b->y;
    result.y = a->z * b->x - a->x * b->z;
    result.z = a->x * b->y - a->y * b->x;
    return result;
  }

  static void normalize_quaternion(FAR struct attitude_quaternion_s *q)
  {
    const float norm = sqrtf(q->w * q->w + q->x * q->x +
                            q->y * q->y + q->z * q->z);

    if (norm > ATTITUDE_EPSILON && isfinite(norm))
      {
        q->w /= norm;
        q->x /= norm;
        q->y /= norm;
        q->z /= norm;
      }
    else
      {
        q->w = 1.0f;
        q->x = 0.0f;
        q->y = 0.0f;
        q->z = 0.0f;
      }
  }

  static void update_euler(FAR struct attitude_state_s *state)
  {
    const float w = state->q.w;
    const float x = state->q.x;
    const float y = state->q.y;
    const float z = state->q.z;
    float sin_pitch = 2.0f * (w * y - z * x);

    if (sin_pitch > 1.0f)
      {
        sin_pitch = 1.0f;
      }
    else if (sin_pitch < -1.0f)
      {
        sin_pitch = -1.0f;
      }

    state->roll_rad = atan2f(2.0f * (w * x + y * z),
                              1.0f - 2.0f * (x * x + y * y));
    state->pitch_rad = asinf(sin_pitch);
    state->yaw_gyro_rad = atan2f(2.0f * (w * z + x * y),
                                  1.0f - 2.0f * (y * y + z * z));
  }

  static void reset_state(FAR struct attitude_state_s *state)
  {
    const struct attitude_params_s params = state->params;

    memset(state, 0, sizeof(*state));
    state->params = params;
    state->q.w = 1.0f;
  }

  void attitude_params_bmi088_default(FAR struct attitude_params_s *params)
  {
    memset(params, 0, sizeof(*params));

    /* PX4 starts the MicoAir BMI088 with ROTATION_YAW_270. */
    params->sensor.axis_map[0] = 1;
    params->sensor.axis_map[1] = 0;
    params->sensor.axis_map[2] = 2;
    params->sensor.axis_sign[0] = 1;
    params->sensor.axis_sign[1] = -1;
    params->sensor.axis_sign[2] = 1;
    params->sensor.accel_m_s2_per_lsb =
      (3.0f * ATTITUDE_ONE_G) / 32768.0f; /* BMI088 +/-3 g */
    params->sensor.gyro_rad_s_per_lsb =
      (2000.0f * ATTITUDE_PI / 180.0f) / 32768.0f; /* +/-2000 dps */

    params->filter.accel_correction_gain = 0.2f;
    params->filter.gyro_bias_gain = 0.1f;
    params->filter.gyro_bias_limit_rad_s = 0.05f;
    params->filter.accel_min_m_s2 = 0.9f * ATTITUDE_ONE_G;
    params->filter.accel_max_m_s2 = 1.1f * ATTITUDE_ONE_G;
    params->filter.max_dt_s = 0.05f;
  }

  void attitude_init(FAR struct attitude_state_s *state,
                    FAR const struct attitude_params_s *params)
  {
    memset(state, 0, sizeof(*state));
    state->params = *params;
    state->q.w = 1.0f;
  }

  void attitude_reset(FAR struct attitude_state_s *state)
  {
    reset_state(state);
  }

  static bool initialize_from_accel(FAR struct attitude_state_s *state,
                                    FAR const struct attitude_imu_sample_s *sample,
                                    float accel_norm)
  {
    struct attitude_vec3_s down;
    float roll;
    float pitch;
    float cr;
    float sr;
    float cp;
    float sp;

    if (accel_norm < state->params.filter.accel_min_m_s2 ||
        accel_norm > state->params.filter.accel_max_m_s2)
      {
        return false;
      }

    /* Accelerometers measure specific force.  At rest they point opposite to
    * NED down, therefore the gravity/down vector is -acceleration. */
    down.x = -sample->accel_m_s2.x / accel_norm;
    down.y = -sample->accel_m_s2.y / accel_norm;
    down.z = -sample->accel_m_s2.z / accel_norm;
    roll = atan2f(down.y, down.z);
    pitch = atan2f(-down.x, sqrtf(down.y * down.y + down.z * down.z));
    cr = cosf(roll * 0.5f);
    sr = sinf(roll * 0.5f);
    cp = cosf(pitch * 0.5f);
    sp = sinf(pitch * 0.5f);

    state->q.w = cr * cp;
    state->q.x = sr * cp;
    state->q.y = cr * sp;
    state->q.z = -sr * sp;
    normalize_quaternion(&state->q);
    state->last_timestamp_us = sample->timestamp_us;
    state->initialized = true;
    state->accel_correction_active = true;
    update_euler(state);
    return true;
  }

  bool attitude_update(FAR struct attitude_state_s *state,
                      FAR const struct attitude_imu_sample_s *sample)
  {
    float accel_norm = vector_norm(&sample->accel_m_s2);
    float dt;
    struct attitude_vec3_s rate = sample->gyro_rad_s;

    if (!state->initialized)
      {
        return initialize_from_accel(state, sample, accel_norm);
      }

    if (sample->timestamp_us <= state->last_timestamp_us)
      {
        return false;
      }

    dt = (sample->timestamp_us - state->last_timestamp_us) / 1000000.0f;
    state->last_timestamp_us = sample->timestamp_us;
    if (dt > state->params.filter.max_dt_s || !isfinite(dt))
      {
        state->accel_correction_active = false;
        return false;
      }

    state->accel_correction_active =
      accel_norm >= state->params.filter.accel_min_m_s2 &&
      accel_norm <= state->params.filter.accel_max_m_s2;

    if (state->accel_correction_active)
      {
        struct attitude_vec3_s estimated_down;
        struct attitude_vec3_s measured_down;
        struct attitude_vec3_s correction;

        /* Optimized quaternion rotation of NED down into the body frame. */
        estimated_down.x = 2.0f * (state->q.x * state->q.z -
                                    state->q.w * state->q.y);
        estimated_down.y = 2.0f * (state->q.y * state->q.z +
                                    state->q.w * state->q.x);
        estimated_down.z = state->q.w * state->q.w - state->q.x * state->q.x -
                            state->q.y * state->q.y + state->q.z * state->q.z;
        measured_down.x = -sample->accel_m_s2.x / accel_norm;
        measured_down.y = -sample->accel_m_s2.y / accel_norm;
        measured_down.z = -sample->accel_m_s2.z / accel_norm;
        correction = vector_cross(&measured_down, &estimated_down);

        rate.x += correction.x * state->params.filter.accel_correction_gain;
        rate.y += correction.y * state->params.filter.accel_correction_gain;
        rate.z += correction.z * state->params.filter.accel_correction_gain;

        if (vector_norm(&sample->gyro_rad_s) < ATTITUDE_STILL_RATE_RAD_S)
          {
            state->gyro_bias_rad_s.x += correction.x *
              state->params.filter.gyro_bias_gain * dt;
            state->gyro_bias_rad_s.y += correction.y *
              state->params.filter.gyro_bias_gain * dt;
            state->gyro_bias_rad_s.z += correction.z *
              state->params.filter.gyro_bias_gain * dt;

  #define LIMIT_BIAS(axis) do { \
    if (state->gyro_bias_rad_s.axis > state->params.filter.gyro_bias_limit_rad_s) \
      state->gyro_bias_rad_s.axis = state->params.filter.gyro_bias_limit_rad_s; \
    else if (state->gyro_bias_rad_s.axis < -state->params.filter.gyro_bias_limit_rad_s) \
      state->gyro_bias_rad_s.axis = -state->params.filter.gyro_bias_limit_rad_s; \
  } while (0)
            LIMIT_BIAS(x);
            LIMIT_BIAS(y);
            LIMIT_BIAS(z);
  #undef LIMIT_BIAS
          }
      }

    rate.x += state->gyro_bias_rad_s.x;
    rate.y += state->gyro_bias_rad_s.y;
    rate.z += state->gyro_bias_rad_s.z;

    {
      const float w = state->q.w;
      const float x = state->q.x;
      const float y = state->q.y;
      const float z = state->q.z;
      const float half_dt = 0.5f * dt;

      state->q.w += (-x * rate.x - y * rate.y - z * rate.z) * half_dt;
      state->q.x += ( w * rate.x + y * rate.z - z * rate.y) * half_dt;
      state->q.y += ( w * rate.y - x * rate.z + z * rate.x) * half_dt;
      state->q.z += ( w * rate.z + x * rate.y - y * rate.x) * half_dt;
    }

    normalize_quaternion(&state->q);
    update_euler(state);
    return true;
  }

  bool attitude_update_raw(FAR struct attitude_state_s *state,
                          int16_t accel_x, int16_t accel_y, int16_t accel_z,
                          int16_t gyro_x, int16_t gyro_y, int16_t gyro_z,
                          uint64_t timestamp_us)
  {
    const float accel_raw[3] = {(float)accel_x, (float)accel_y, (float)accel_z};
    const float gyro_raw[3] = {(float)gyro_x, (float)gyro_y, (float)gyro_z};
    float accel_body[3];
    float gyro_body[3];
    struct attitude_imu_sample_s sample;
    int axis;

    for (axis = 0; axis < 3; axis++)
      {
        const uint8_t source = state->params.sensor.axis_map[axis];
        const float sign = state->params.sensor.axis_sign[axis];

        if (source > 2 || (sign != 1.0f && sign != -1.0f))
          {
            return false;
          }

        accel_body[axis] = sign *
          (accel_raw[source] - state->params.sensor.accel_bias_lsb[source]) *
          state->params.sensor.accel_m_s2_per_lsb;
        gyro_body[axis] = sign *
          (gyro_raw[source] - state->params.sensor.gyro_bias_lsb[source]) *
          state->params.sensor.gyro_rad_s_per_lsb;
      }

    sample.timestamp_us = timestamp_us;
    sample.accel_m_s2.x = accel_body[0];
    sample.accel_m_s2.y = accel_body[1];
    sample.accel_m_s2.z = accel_body[2];
    sample.gyro_rad_s.x = gyro_body[0];
    sample.gyro_rad_s.y = gyro_body[1];
    sample.gyro_rad_s.z = gyro_body[2];
    return attitude_update(state, &sample);
  }

  void attitude_get(FAR const struct attitude_state_s *state,
                    FAR struct attitude_output_s *output)
  {
    output->timestamp_us = state->last_timestamp_us;
    output->q = state->q;
    output->gyro_bias_rad_s = state->gyro_bias_rad_s;
    output->roll_rad = state->roll_rad;
    output->pitch_rad = state->pitch_rad;
    output->yaw_gyro_rad = state->yaw_gyro_rad;
    output->initialized = state->initialized;
    output->accel_correction_active = state->accel_correction_active;
  }
