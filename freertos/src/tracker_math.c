#include "tracker_math.h"

#include <math.h>
#include <stddef.h>

#define DEG_TO_RAD     0.01745329251994329577f
#define RAD_TO_DEG     57.295779513082320876f
#define EARTH_RADIUS_M 6371000.0f
#define YAW_MIN_US     800U
#define YAW_MAX_US     2200U
#define PITCH_MIN_US   1100U
#define PITCH_MAX_US   2200U

static float clampf(float value, float low, float high)
{
  if (value < low)
    return low;
  if (value > high)
    return high;
  return value;
}

static float wrap_180(float angle)
{
  while (angle > 180.0f)
    angle -= 360.0f;
  while (angle < -180.0f)
    angle += 360.0f;
  return angle;
}

bool tracker_solution_compute(int32_t station_latitude_e7, int32_t station_longitude_e7,
                              int32_t station_altitude_mm, int32_t target_latitude_e7,
                              int32_t target_longitude_e7, int32_t target_altitude_mm,
                              tracker_solution_t *solution)
{
  float station_lat;
  float target_lat;
  float delta_lat;
  float delta_lon;
  float a;
  float horizontal;
  float altitude_delta;
  float bearing;

  if (solution == NULL || (station_latitude_e7 == 0 && station_longitude_e7 == 0) ||
      (target_latitude_e7 == 0 && target_longitude_e7 == 0))
  {
    return false;
  }

  station_lat = station_latitude_e7 * 1.0e-7f * DEG_TO_RAD;
  target_lat = target_latitude_e7 * 1.0e-7f * DEG_TO_RAD;
  delta_lat = target_lat - station_lat;
  delta_lon = (target_longitude_e7 - station_longitude_e7) * 1.0e-7f * DEG_TO_RAD;
  a = sinf(delta_lat * 0.5f) * sinf(delta_lat * 0.5f) +
      cosf(station_lat) * cosf(target_lat) * sinf(delta_lon * 0.5f) * sinf(delta_lon * 0.5f);
  a = clampf(a, 0.0f, 1.0f);
  horizontal = EARTH_RADIUS_M * 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
  altitude_delta = (target_altitude_mm - station_altitude_mm) * 0.001f;
  bearing = atan2f(sinf(delta_lon) * cosf(target_lat),
                   cosf(station_lat) * sinf(target_lat) -
                       sinf(station_lat) * cosf(target_lat) * cosf(delta_lon)) *
            RAD_TO_DEG;
  if (bearing < 0.0f)
    bearing += 360.0f;

  solution->bearing_deg = bearing;
  solution->horizontal_distance_m = horizontal;
  solution->elevation_deg = atan2f(altitude_delta, horizontal) * RAD_TO_DEG;
  solution->slant_distance_m = sqrtf(horizontal * horizontal + altitude_delta * altitude_delta);
  return isfinite(solution->bearing_deg) && isfinite(solution->elevation_deg);
}

bool tracker_solution_to_servo(const tracker_solution_t *solution, float station_heading_deg,
                               uint16_t *yaw_us, uint16_t *pitch_us)
{
  float relative_yaw;
  float elevation;

  if (solution == NULL || yaw_us == NULL || pitch_us == NULL || !isfinite(station_heading_deg))
  {
    return false;
  }

  relative_yaw = wrap_180(solution->bearing_deg - station_heading_deg);
  elevation = clampf(solution->elevation_deg, 0.0f, 90.0f);
  *yaw_us = (uint16_t)(YAW_MIN_US + ((relative_yaw + 180.0f) / 360.0f) * (YAW_MAX_US - YAW_MIN_US) +
                       0.5f);
  *pitch_us = (uint16_t)(PITCH_MIN_US + (elevation / 90.0f) * (PITCH_MAX_US - PITCH_MIN_US) + 0.5f);
  return true;
}
