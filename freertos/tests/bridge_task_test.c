#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "board.h"
#include "app_state.h"
#include "bridge_status.h"
#include "bridge_protocol.h"
#include "tracker_tasks.h"

static jmp_buf g_done;
static void (*g_task)(void *);
static uint32_t g_now;
static uint32_t g_end;
static unsigned g_case;
static bool g_scheduler;
static bool g_connected = true;
static uint32_t g_generation = 1U;
static uint8_t g_fc_input[8192], g_usb_input[8192];
static size_t g_fc_count, g_usb_count;
static uint8_t g_fc_output[1000000], g_usb_output[1000000];
static size_t g_fc_out_count, g_usb_out_count;
static uint8_t g_frames[256];
static size_t g_frames_length;
static const uint8_t g_command[] = {0x12, 0x34, 0x56, 0x78};
static tracker_position_t g_target;
static unsigned g_heartbeat_updates;

BaseType_t xTaskCreate(void (*entry)(void *), const char *name, uint32_t stack, void *argument,
                       UBaseType_t priority, TaskHandle_t *handle)
{
  (void)argument;
  (void)handle;
  assert(strcmp(name, "mav_bridge") == 0 && stack == 1536U && priority == 5U);
  assert(!g_scheduler);
  g_task = entry;
  return pdPASS;
}

TickType_t xTaskGetTickCount(void)
{
  return g_now;
}
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t task)
{
  (void)task;
  return 900U;
}
size_t xPortGetFreeHeapSize(void)
{
  return 70000U;
}
size_t xPortGetMinimumEverFreeHeapSize(void)
{
  return 70000U;
}
uint32_t board_millis(void)
{
  return g_now;
}
void board_fatal(void)
{
  abort();
}
void board_poll_io(void) {}
void board_start_io(void)
{
  assert(g_scheduler);
}
bool usb_device_init(void)
{
  assert(g_scheduler);
  return true;
}
bool usb_device_connected(uint32_t *generation)
{
  *generation = g_generation;
  return g_connected;
}
void board_usb_reset_rx(void)
{
  g_usb_count = 0U;
}

static size_t take(uint8_t *input, size_t *pending, uint8_t *data, size_t capacity)
{
  size_t count = *pending < capacity ? *pending : capacity;
  memcpy(data, input, count);
  *pending -= count;
  memmove(input, input + count, *pending);
  return count;
}

size_t board_fc_read(uint8_t *data, size_t capacity, uint32_t wait_ms)
{
  (void)wait_ms;
  return take(g_fc_input, &g_fc_count, data, capacity);
}
size_t board_usb_read(uint8_t *data, size_t capacity, uint32_t wait_ms)
{
  (void)wait_ms;
  return take(g_usb_input, &g_usb_count, data, capacity);
}

bool board_fc_write(const uint8_t *data, size_t length, uint32_t timeout_ms)
{
  (void)timeout_ms;
  if ((g_case == 2U && g_now < 60U) || (g_case == 3U && g_now % 23U != 0U))
    return false;
  assert(g_fc_out_count + length <= sizeof(g_fc_output));
  memcpy(g_fc_output + g_fc_out_count, data, length);
  g_fc_out_count += length;
  return true;
}

bool board_usb_write(const uint8_t *data, size_t length)
{
  if (!g_connected || (g_case == 3U && g_now % 3U != 0U))
    return false;
  assert(g_usb_out_count + length <= sizeof(g_usb_output));
  memcpy(g_usb_output + g_usb_out_count, data, length);
  g_usb_out_count += length;
  return true;
}

void app_state_update_heartbeat(uint32_t now_ms)
{
  (void)now_ms;
  g_heartbeat_updates++;
}
void app_state_update_target(int32_t lat, int32_t lon, int32_t alt, uint32_t now_ms)
{
  g_target.latitude_e7 = lat;
  g_target.longitude_e7 = lon;
  g_target.altitude_mm = alt;
  g_target.timestamp_ms = now_ms;
}

static void inject_fc(const uint8_t *data, size_t length)
{
  assert(g_fc_count + length <= sizeof(g_fc_input));
  memcpy(g_fc_input + g_fc_count, data, length);
  g_fc_count += length;
}

static void inject_usb(const uint8_t *data, size_t length)
{
  assert(g_usb_count + length <= sizeof(g_usb_input));
  memcpy(g_usb_input + g_usb_count, data, length);
  g_usb_count += length;
}

void vTaskDelay(TickType_t ticks)
{
  assert(ticks == 1U);
  g_now += ticks;
  if (g_now >= g_end)
    longjmp(g_done, 1);
  if (g_case == 0U && g_now == 1U)
  {
    inject_fc(g_frames, g_frames_length);
    inject_usb(g_command, sizeof(g_command));
  }
  if (g_case == 1U && g_now == 1000U)
  {
    inject_fc(g_frames, 5U);
    inject_usb(g_command, sizeof(g_command));
  }
  if (g_case == 1U && g_now == 1100U)
    inject_fc(g_frames + 5U, g_frames_length - 5U);
  if (g_case == 2U && g_now == 1U)
  {
    uint8_t old[5000];
    memset(old, 0xa5, sizeof(old));
    inject_usb(old, sizeof(old));
  }
  if (g_case == 2U && g_now == 50U)
  {
    g_connected = false;
    g_generation++;
  }
  if (g_case == 2U && g_now == 60U)
  {
    g_connected = true;
    g_generation++;
  }
  if (g_case == 2U && g_now == 61U)
    inject_usb(g_command, sizeof(g_command));
  if (g_case == 3U)
  {
    inject_fc(g_frames, g_frames_length);
    for (unsigned i = 0; i < 8U; i++)
      inject_usb(g_command, sizeof(g_command));
  }
}

static void make_frames(void)
{
  mavlink_message_t message;
  mavlink_msg_heartbeat_pack(1U, 1U, &message, MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_PX4, 0U, 0U,
                             MAV_STATE_ACTIVE);
  g_frames_length = mavlink_msg_to_send_buffer(g_frames, &message);
  if (g_case == 1U || g_case == 3U)
    return;
  for (unsigned i = 0; i < 3U; i++)
  {
    mavlink_msg_global_position_int_pack(i == 1U ? 2U : 1U, 1U, &message, 100U, 123456789,
                                         -456789012, i == 2U ? 99999 : 12345, 0, 0, 0, 0, 0);
    size_t length = mavlink_msg_to_send_buffer(g_frames + g_frames_length, &message);
    if (i == 2U)
      g_frames[g_frames_length + length - 1U] ^= 0x40U;
    g_frames_length += length;
  }
}

static void verify_forwarding(void)
{
  assert(g_fc_out_count == sizeof(g_command));
  assert(memcmp(g_fc_output, g_command, sizeof(g_command)) == 0);
  assert(memcmp(g_usb_output, g_frames, g_frames_length) == 0);
  bridge_protocol_t parser;
  bridge_protocol_init(&parser);
  mavlink_message_t message;
  unsigned tracker = 0U;
  for (size_t i = g_frames_length; i < g_usb_out_count; i++)
    if (bridge_protocol_receive(&parser, g_usb_output[i], g_now, &message))
    {
      assert(message.compid == 191U);
      tracker++;
    }
  assert(tracker == 1U && g_bridge_status.tracker_heartbeats == 1U);
  if (g_case == 0U)
  {
    assert(g_bridge_status.fc_valid_frames == 3U && g_bridge_status.fc_bad_frames == 1U);
    assert(g_bridge_status.target_updates == 1U && g_heartbeat_updates == 1U);
    assert(g_target.latitude_e7 == 123456789 && g_target.longitude_e7 == -456789012);
    assert(g_target.altitude_mm == 12345);
  }
}

int main(int argc, char **argv)
{
  assert(argc == 2);
  g_case = (unsigned)strtoul(argv[1], NULL, 10);
  g_now = g_case == 1U ? 999U : 0U;
  g_end = g_case == 2U ? 100U : (g_case == 3U ? 20000U : 1200U);
  make_frames();
  assert(mavlink_bridge_task_start() && g_task != NULL);
  g_scheduler = true;
  if (setjmp(g_done) == 0)
    g_task(NULL);
  assert(g_bridge_status.io_started);
  assert(g_bridge_status.to_fc_pending_bytes <= BRIDGE_BUFFER_SIZE);
  assert(g_bridge_status.to_usb_pending_bytes <= BRIDGE_BUFFER_SIZE);
  if (g_case < 2U)
  {
    verify_forwarding();
  }
  if (g_case == 2U)
  {
    assert(g_bridge_status.usb_link_changes == 3U);
    /* All pre-disconnect commands must have been discarded, not replayed. */
    assert(g_fc_out_count == sizeof(g_command));
    assert(memcmp(g_fc_output, g_command, sizeof(g_command)) == 0);
    assert(g_bridge_status.to_fc_dropped_bytes == 5000U);
  }
  if (g_case == 3U)
  {
    assert(g_bridge_status.to_fc_dropped_bytes > 0U);
    assert(g_bridge_status.to_usb_dropped_bytes == 0U);
    assert(g_fc_out_count + g_bridge_status.to_fc_pending_bytes +
               g_bridge_status.to_fc_dropped_bytes ==
           8U * sizeof(g_command) * (g_end - 1U));
    for (size_t i = 0; i < g_fc_out_count; i++)
      assert(g_fc_output[i] == g_command[i % sizeof(g_command)]);
    assert(g_usb_out_count + g_bridge_status.to_usb_pending_bytes ==
           g_frames_length * (g_end - 1U) + 21U * g_bridge_status.tracker_heartbeats);
    bridge_protocol_t parser;
    bridge_protocol_init(&parser);
    mavlink_message_t message;
    for (size_t i = 0; i < g_usb_out_count; i++)
      (void)bridge_protocol_receive(&parser, g_usb_output[i], g_now, &message);
    assert(parser.bad_frames == 0U && parser.valid_frames > 19000U);
    assert(g_bridge_status.tracker_heartbeats >= 19U);
  }
  printf("PASS: production bridge task scenario %u, iterations=%u drops=%u/%u\n", g_case,
         g_bridge_status.task_iterations, g_bridge_status.to_fc_dropped_bytes,
         g_bridge_status.to_usb_dropped_bytes);
  return 0;
}
