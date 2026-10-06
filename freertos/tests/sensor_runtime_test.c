#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "semphr.h"
#include "app_state.h"
#include "board.h"
#include "sensor_diagnostics.h"
#include "tracker_tasks.h"

static jmp_buf done;
static void (*task_entry)(void *);
static uint32_t clock_ms = UINT32_MAX - 600U;
static uint32_t elapsed;
static uint8_t regs[3][256];
static bool mutex_busy;
static bool disconnected;
static bool recovering;
static uint32_t last_good_imu;
static uint32_t last_good_heading;
static unsigned scenario;
static unsigned accel_resets;
static unsigned compass_resets;
static bool saw_wrap;
static bool saw_invalid;
static bool saw_recovery;

SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
  return &clock_ms;
}
BaseType_t xSemaphoreTake(SemaphoreHandle_t mutex, TickType_t ticks)
{
  (void)mutex;
  (void)ticks;
  return !mutex_busy;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t mutex)
{
  (void)mutex;
  return 1;
}
BaseType_t xTaskCreate(void (*entry)(void *), const char *name, uint32_t stack, void *argument,
                       UBaseType_t priority, TaskHandle_t *handle)
{
  (void)name;
  (void)stack;
  (void)argument;
  (void)priority;
  (void)handle;
  task_entry = entry;
  return pdPASS;
}
TickType_t xTaskGetTickCount(void)
{
  return clock_ms;
}
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t task)
{
  (void)task;
  return 400;
}
void vTaskDelay(TickType_t ticks)
{
  clock_ms += ticks;
  elapsed += ticks;
}
uint32_t board_millis(void)
{
  return clock_ms;
}
I2C_HandleTypeDef *board_i2c_external(void)
{
  return NULL;
}
bool board_i2c_write(I2C_HandleTypeDef *bus, uint8_t address, uint8_t reg, uint8_t value)
{
  (void)bus;
  (void)address;
  if (disconnected && scenario == 0)
    return false;
  if (reg == 0x0a && value == 0x80)
    compass_resets++;
  regs[0][reg] = value;
  return true;
}
bool board_i2c_read(I2C_HandleTypeDef *bus, uint8_t address, uint8_t reg, uint8_t *data,
                    size_t length)
{
  (void)bus;
  (void)address;
  if (disconnected && scenario == 0)
    return false;
  memset(data, 0, length);
  if (reg == 6)
    data[0] = !disconnected && regs[0][9] == 0x1d ? 1 : 0;
  else
  {
    data[0] = 100;
    data[2] = 100;
  }
  return true;
}
bool board_spi_write(uint16_t cs, uint8_t reg, uint8_t value)
{
  if (disconnected && scenario == 0)
    return false;
  if (cs == BOARD_SPI_CS_BMI088_ACCEL && reg == 0x7e)
  {
    memset(regs[cs], 0, 256);
    accel_resets++;
  }
  regs[cs][reg] = value;
  return true;
}
bool board_spi_read(uint16_t cs, uint8_t reg, uint8_t *data, size_t length)
{
  /* Missing SPI device can still complete a transfer successfully. */
  memset(data, 0xff, length);
  if (disconnected && scenario == 0)
    return true;
  memset(data, 0, length);
  if (reg == 0)
    data[0] = cs == BOARD_SPI_CS_BMI088_ACCEL ? 0x1e : 0x0f;
  else if (cs == BOARD_SPI_CS_BMI088_ACCEL && reg == 3)
    data[0] = disconnected && scenario == 2 ? 0 : 0x80;
  else if (cs == BOARD_SPI_CS_BMI088_ACCEL && reg == 0x12)
  {
    data[4] = (uint16_t)-10920 & 0xff;
    data[5] = (uint16_t)-10920 >> 8;
  } else if (cs == BOARD_SPI_CS_BMI088_GYRO && reg == 2)
  { /* stationary */
  } else
    data[0] = disconnected && scenario == 1 ? 0 : regs[cs][reg];
  return true;
}
size_t board_gps_read(uint8_t *data, size_t capacity, uint32_t wait_ms)
{
  tracker_state_snapshot_t state;
  (void)data;
  (void)capacity;
  app_state_snapshot(&state);
  if (elapsed > 800 && elapsed < 1500)
  {
    assert(state.imu_valid && state.heading_valid);
    assert(g_sensor_diagnostics.bmi088_filter_rejects == 0);
    saw_wrap = true;
  }
  if (!disconnected && !recovering && elapsed >= 1500)
  {
    disconnected = true;
    last_good_imu = g_sensor_diagnostics.bmi088_last_sample_ms;
    last_good_heading = g_sensor_diagnostics.compass_last_sample_ms;
  }
  if (disconnected && elapsed >= 1600)
  {
    assert(!state.imu_valid && !state.heading_valid);
    assert(!g_sensor_diagnostics.attitude_valid && !g_sensor_diagnostics.heading_valid);
    assert(g_sensor_diagnostics.bmi088_last_sample_ms == last_good_imu);
    assert(g_sensor_diagnostics.compass_last_sample_ms == last_good_heading);
    saw_invalid = true;
  }
  if (disconnected && elapsed >= 3800)
  {
    disconnected = false;
    recovering = true;
    memset(regs, 0, sizeof(regs)); /* power-on configuration */
  }
  if (recovering && elapsed >= 6500)
  {
    assert(state.imu_valid && state.heading_valid);
    assert(g_sensor_diagnostics.bmi088_ready && g_sensor_diagnostics.compass_ready);
    assert(g_sensor_diagnostics.bmi088_sample_age_ms < 30);
    assert(accel_resets >= 2 && compass_resets >= 2);
    assert(g_sensor_diagnostics.bmi088_filter_rejects == 0);
    saw_recovery = true;
    longjmp(done, 1);
  }
  clock_ms += wait_ms;
  elapsed += wait_ms;
  return 0;
}
int main(int argc, char **argv)
{
  assert(argc == 2);
  scenario = (unsigned)atoi(argv[1]);
  assert(scenario < 3);
  assert(app_state_init());
  assert(sensor_task_start());
  if (setjmp(done) == 0)
    task_entry(NULL);
  assert(saw_wrap && saw_invalid && saw_recovery);
  tracker_state_snapshot_t before, after;
  app_state_snapshot(&before);
  assert(app_state_invalidate_attitude());
  assert(app_state_invalidate_heading());
  app_state_snapshot(&after);
  assert(!after.imu_valid && !after.heading_valid);
  assert(after.imu_timestamp_ms == before.imu_timestamp_ms);
  assert(after.heading_timestamp_ms == before.heading_timestamp_ms);
  mutex_busy = true;
  assert(!app_state_invalidate_attitude() && !app_state_invalidate_heading());
  puts("PASS: real sensor task, SPI missing device, stale validity, recovery and millis rollover");
  return 0;
}
