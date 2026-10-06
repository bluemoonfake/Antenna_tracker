#ifndef TRACKER_MATH_H
#define TRACKER_MATH_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  float bearing_deg;
  float elevation_deg;
  float horizontal_distance_m;
  float slant_distance_m;
} tracker_solution_t;

bool tracker_solution_compute(int32_t station_latitude_e7, int32_t station_longitude_e7,
                              int32_t station_altitude_mm, int32_t target_latitude_e7,
                              int32_t target_longitude_e7, int32_t target_altitude_mm,
                              tracker_solution_t *solution);

bool tracker_solution_to_servo(const tracker_solution_t *solution, float station_heading_deg,
                               uint16_t *yaw_us, uint16_t *pitch_us);

#endif
