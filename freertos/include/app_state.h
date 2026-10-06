#ifndef TRACKER_APP_STATE_H
#define TRACKER_APP_STATE_H

#include <stdbool.h>
#include <stdint.h>

// Global position target: lat, lon, altitude, timestamp
typedef struct
{
  int32_t latitude_e7;
  int32_t longitude_e7;
  int32_t altitude_mm;
  uint32_t timestamp_ms;
  bool valid;
} tracker_position_t;

typedef struct
{
  tracker_position_t target;
  tracker_position_t station;
  float station_heading_deg;
  uint32_t heading_timestamp_ms;
  bool heading_valid;
  float imu_roll_rad;
  float imu_pitch_rad;
  uint32_t imu_timestamp_ms;
  bool imu_valid;
  bool fc_heartbeat_valid;
  uint32_t fc_heartbeat_timestamp_ms;
} tracker_state_snapshot_t;

bool app_state_init(void);
void app_state_update_heartbeat(uint32_t now_ms);
void app_state_update_target(int32_t latitude_e7, int32_t longitude_e7, int32_t altitude_mm,
                             uint32_t now_ms);
bool app_state_update_station(int32_t latitude_e7, int32_t longitude_e7, int32_t altitude_mm,
                              uint32_t now_ms);
bool app_state_invalidate_station(void);
bool app_state_update_heading(float heading_deg, uint32_t now_ms);
bool app_state_invalidate_heading(void);
bool app_state_update_attitude(float roll_rad, float pitch_rad, uint32_t now_ms);
bool app_state_invalidate_attitude(void);
void app_state_snapshot(tracker_state_snapshot_t *snapshot);

#endif
