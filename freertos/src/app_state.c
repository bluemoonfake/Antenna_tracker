#include "app_state.h"

#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"

static tracker_state_snapshot_t g_state;
static SemaphoreHandle_t g_state_mutex;

bool app_state_init(void) // chuan bi state va tao khoa mutex de bao ve state
{
  memset(&g_state, 0, sizeof(g_state)); // khoi tao state vao mot dia chi vung nho can ghi
  g_state_mutex = xSemaphoreCreateMutex();
  return g_state_mutex != NULL;
}

void app_state_update_heartbeat(uint32_t now_ms)
{
  if (xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
  {
    g_state.fc_heartbeat_valid = true;
    g_state.fc_heartbeat_timestamp_ms = now_ms;
    xSemaphoreGive(g_state_mutex);
  }
}

static void update_position(tracker_position_t *position, int32_t latitude_e7, int32_t longitude_e7,
                            int32_t altitude_mm, uint32_t now_ms)
{
  position->latitude_e7 = latitude_e7;
  position->longitude_e7 = longitude_e7;
  position->altitude_mm = altitude_mm;
  position->timestamp_ms = now_ms;
  position->valid = latitude_e7 != 0 || longitude_e7 != 0;
}

void app_state_update_target(int32_t latitude_e7, int32_t longitude_e7, int32_t altitude_mm,
                             uint32_t now_ms)
{
  if (xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
  {
    update_position(&g_state.target, latitude_e7, longitude_e7, altitude_mm, now_ms);
    xSemaphoreGive(g_state_mutex);
  }
}

bool app_state_update_station(int32_t latitude_e7, int32_t longitude_e7, int32_t altitude_mm,
                              uint32_t now_ms)
{
  if (xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
  {
    update_position(&g_state.station, latitude_e7, longitude_e7, altitude_mm, now_ms);
    xSemaphoreGive(g_state_mutex);
    return true;
  }
  return false;
}

bool app_state_invalidate_station(void)
{
  if (xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
  {
    g_state.station.valid = false;
    xSemaphoreGive(g_state_mutex);
    return true;
  }
  return false;
}

bool app_state_update_heading(float heading_deg, uint32_t now_ms)
{
  if (xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
  {
    g_state.station_heading_deg = heading_deg;
    g_state.heading_timestamp_ms = now_ms;
    g_state.heading_valid = true;
    xSemaphoreGive(g_state_mutex);
    return true;
  }
  return false;
}

bool app_state_invalidate_heading(void)
{
  if (xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(10)) != pdTRUE)
    return false;
  g_state.heading_valid = false;
  xSemaphoreGive(g_state_mutex);
  return true;
}

bool app_state_update_attitude(float roll_rad, float pitch_rad, uint32_t now_ms)
{
  if (xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
  {
    g_state.imu_roll_rad = roll_rad;
    g_state.imu_pitch_rad = pitch_rad;
    g_state.imu_timestamp_ms = now_ms;
    g_state.imu_valid = true;
    xSemaphoreGive(g_state_mutex);
    return true;
  }
  return false;
}

bool app_state_invalidate_attitude(void)
{
  if (xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(10)) != pdTRUE)
    return false;
  g_state.imu_valid = false;
  xSemaphoreGive(g_state_mutex);
  return true;
}

void app_state_snapshot(tracker_state_snapshot_t *snapshot)
{
  if (snapshot == NULL)
  {
    return;
  }
  // mutex de bao ve state khi copy vao snapshot
  if (xSemaphoreTake(g_state_mutex, portMAX_DELAY) == pdTRUE)
  {
    *snapshot = g_state;
    xSemaphoreGive(g_state_mutex);
  }
}
