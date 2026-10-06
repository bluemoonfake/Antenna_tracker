#include "tracker_tasks.h"

#include "FreeRTOS.h"
#include "task.h"

#include "app_state.h"
#include "board.h"
#include "tracker_config.h"
#include "tracker_math.h"

static bool state_is_fresh(const tracker_state_snapshot_t *state, uint32_t now_ms)
{
  return state->fc_heartbeat_valid && state->target.valid && state->station.valid &&
         state->heading_valid &&
         now_ms - state->fc_heartbeat_timestamp_ms <= TRACKER_TELEMETRY_TIMEOUT_MS &&
         now_ms - state->target.timestamp_ms <= TRACKER_TELEMETRY_TIMEOUT_MS &&
         now_ms - state->station.timestamp_ms <= TRACKER_STATION_GPS_TIMEOUT_MS &&
         now_ms - state->heading_timestamp_ms <= TRACKER_HEADING_TIMEOUT_MS;
}

static void tracker_control_task(void *argument)
{
  const TickType_t period = pdMS_TO_TICKS(1000U / TRACKER_CONTROL_RATE_HZ);
  TickType_t wake_time = xTaskGetTickCount();
  (void)argument;

  board_servo_stop();
  for (;;)
  {
    tracker_state_snapshot_t state = {0};
    tracker_solution_t solution;
    uint16_t yaw_us;
    uint16_t pitch_us;
    uint32_t now_ms = board_millis();

    app_state_snapshot(&state);
    if (state_is_fresh(&state, now_ms) &&
        tracker_solution_compute(state.station.latitude_e7, state.station.longitude_e7,
                                 state.station.altitude_mm, state.target.latitude_e7,
                                 state.target.longitude_e7, state.target.altitude_mm, &solution) &&
        tracker_solution_to_servo(&solution, state.station_heading_deg, &yaw_us, &pitch_us))
    {
#if TRACKER_ENABLE_SERVO_OUTPUT
      if (!board_servo_set(yaw_us, pitch_us))
        board_servo_stop();
#else
      (void)yaw_us;
      (void)pitch_us;
      board_servo_stop();
#endif
    } else
    {
      board_servo_stop();
    }

    vTaskDelayUntil(&wake_time, period);
  }
}
bool tracker_control_task_start(void)
{
  return xTaskCreate(tracker_control_task, "control", TRACKER_CONTROL_TASK_STACK_WORDS, NULL,
                     TRACKER_CONTROL_TASK_PRIORITY, NULL) == pdPASS;
}
