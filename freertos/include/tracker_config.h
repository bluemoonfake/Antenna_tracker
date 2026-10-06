#ifndef TRACKER_CONFIG_H
#define TRACKER_CONFIG_H

#define TRACKER_SYSTEM_ID              1U
#define TRACKER_COMPONENT_ID           191U
#define TRACKER_CONTROL_RATE_HZ        50U
#define TRACKER_TELEMETRY_TIMEOUT_MS   2000U
#define TRACKER_STATION_GPS_TIMEOUT_MS 3000U
#define TRACKER_HEADING_TIMEOUT_MS     1000U
#define TRACKER_IMU_TIMEOUT_MS         1000U

/* Task stack sizes are StackType_t words; one word is four bytes on Cortex-M7. */
#define TRACKER_BOOT_TASK_STACK_WORDS    256U
#define TRACKER_BRIDGE_TASK_STACK_WORDS  1536U
#define TRACKER_SENSOR_TASK_STACK_WORDS  768U
#define TRACKER_CONTROL_TASK_STACK_WORDS 768U

/* Larger values run first when tasks are ready. */
#define TRACKER_BOOT_TASK_PRIORITY    1U
#define TRACKER_BRIDGE_TASK_PRIORITY  5U
#define TRACKER_SENSOR_TASK_PRIORITY  4U
#define TRACKER_CONTROL_TASK_PRIORITY 6U

/* Deliberately disabled until yaw/pitch direction, mechanical stops and
 * emergency stop behavior have been validated with the antenna disconnected. */
#ifndef TRACKER_ENABLE_SERVO_OUTPUT
#define TRACKER_ENABLE_SERVO_OUTPUT 0
#endif

#ifndef TRACKER_TARGET_SYSTEM_ID
#define TRACKER_TARGET_SYSTEM_ID 1U
#endif

#endif
