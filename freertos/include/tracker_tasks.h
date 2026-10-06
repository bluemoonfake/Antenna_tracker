#ifndef TRACKER_TASKS_H
#define TRACKER_TASKS_H

#include <stdbool.h>

bool mavlink_bridge_task_start(void);
bool sensor_task_start(void);
bool tracker_control_task_start(void);

#endif
