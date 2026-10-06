#include "tracker_tasks.h"

#include "FreeRTOS.h"
#include "task.h"
#include "app_state.h"
#include "board.h"
#include "bridge_protocol.h"
#include "bridge_status.h"
#include "tracker_config.h"
#include "usb_device.h"

#define BRIDGE_IO_CHUNK 128U

volatile bridge_status_t g_bridge_status;
volatile bridge_io_status_t g_bridge_io_status;
static bridge_buffer_t g_to_fc;
static bridge_buffer_t g_to_usb;
static bridge_protocol_t g_protocol;

static void flush_outputs(void) // lay du lieu dang cho queue gui di (FC)
{
  const uint8_t *data;
  size_t length = bridge_buffer_peek(&g_to_fc, &data, BRIDGE_IO_CHUNK); // FC: max 128 byte
  if (length != 0U && board_fc_write(data, length, 30U))
    bridge_buffer_consume(&g_to_fc, length); // dich vi tri doc cua queue, giam so byte con cho
  length = bridge_buffer_peek(&g_to_usb, &data, 64U); // USB: max 64 byte
  if (length != 0U && board_usb_write(data, length))
    bridge_buffer_consume(&g_to_usb, length); // dich vi tri doc cua queue, giam so byte con cho
}

static void handle_fc_message(const mavlink_message_t *message, uint32_t now_ms)
{
  if (message->sysid != TRACKER_TARGET_SYSTEM_ID)
    return;
  if (message->msgid == MAVLINK_MSG_ID_HEARTBEAT)
    app_state_update_heartbeat(now_ms); // update target
  else if (message->msgid == MAVLINK_MSG_ID_GLOBAL_POSITION_INT)
  {
    mavlink_global_position_int_t position;
    mavlink_msg_global_position_int_decode(message, &position); // giai ma thanh position
    app_state_update_target(position.lat, position.lon, position.alt, now_ms); // update target
    g_bridge_status.target_updates++;
  }
}

static void // task nahn byte tu Fc roi chuyen tiep
receive_fc(uint32_t now_ms, bool connected)
{
  uint8_t data[BRIDGE_IO_CHUNK];
  mavlink_message_t message;
  size_t count = board_fc_read(data, sizeof(data), 0U);
  for (size_t index = 0; index < count; index++)
  {
    if (connected)
      (void)bridge_buffer_enqueue(&g_to_usb, &data[index], 1U);
    else
      g_to_usb.dropped_bytes++;
    if (bridge_protocol_receive(&g_protocol, data[index], now_ms, &message))
      handle_fc_message(&message, now_ms);
    /* The queue tail now matches this byte's wire boundary. Inject here,
     * including between two frames received in the same UART chunk. */
    if (connected)
      (void)bridge_protocol_heartbeat(&g_protocol, &g_to_usb, now_ms);
  }
}

static void receive_usb(void)
{
  uint8_t data[BRIDGE_IO_CHUNK];
  size_t count = board_usb_read(data, sizeof(data), 0U);
  (void)bridge_buffer_enqueue(&g_to_fc, data, count);
}

static void update_diagnostics(uint32_t now_ms)
{
  static uint32_t last_diagnostics_ms;
  g_bridge_status.task_iterations++;
  g_bridge_status.fc_valid_frames = g_protocol.valid_frames;
  g_bridge_status.fc_bad_frames = g_protocol.bad_frames;
  g_bridge_status.parser_resets = g_protocol.resets;
  g_bridge_status.tracker_heartbeats = g_protocol.heartbeats;
  g_bridge_status.to_fc_pending_bytes = (uint32_t)g_to_fc.count;
  g_bridge_status.to_usb_pending_bytes = (uint32_t)g_to_usb.count;
  g_bridge_status.to_fc_dropped_bytes = g_to_fc.dropped_bytes;
  g_bridge_status.to_usb_dropped_bytes = g_to_usb.dropped_bytes;
  if (now_ms - last_diagnostics_ms < 500U)
    return;
  last_diagnostics_ms = now_ms;
  g_bridge_status.rtos_ticks = xTaskGetTickCount();
  g_bridge_status.hal_millis = now_ms;
  g_bridge_status.free_heap_bytes = xPortGetFreeHeapSize();
  g_bridge_status.minimum_free_heap_bytes = xPortGetMinimumEverFreeHeapSize();
  g_bridge_status.stack_high_water_words = uxTaskGetStackHighWaterMark(NULL);
}

static void // khoi tao -> kiem tra -> nhan du lieu, gui lieu -> tick
mavlink_bridge_task(void *argument)
{
  uint32_t generation = 0U;
  uint32_t rx_loss = 0U;
  (void)argument; // task khong su dung argument
  bridge_protocol_init(&g_protocol); // chuan bi trang thai nhan vao gui di
  /* These calls can enable RTOS-aware interrupt callbacks. All queues/tasks
   * exist and the scheduler is running before USB or UART RX starts. */
  if (!usb_device_init())
    board_fatal();
  board_start_io();
  g_bridge_status.io_started = true;
  for (;;)
  {
    uint32_t now_ms = board_millis();
    uint32_t new_generation;
    bool connected = usb_device_connected(&new_generation);
    if (new_generation != generation || connected != g_bridge_status.usb_configured)
    {
      generation = new_generation;
      bridge_buffer_clear(&g_to_fc);
      bridge_buffer_clear(&g_to_usb);
      board_usb_reset_rx();
      g_bridge_status.usb_link_changes++;
    }
    g_bridge_status.usb_configured = connected;
    uint32_t new_loss = g_bridge_io_status.uart_rx_dropped_bytes + g_bridge_io_status.uart_errors;  //loss = so byte drop + so byte loi UART
    if (new_loss != rx_loss)
    {
      rx_loss = new_loss;
      bridge_protocol_reset(&g_protocol);
    }
#if TRACKER_BRIDGE_ONLY
    board_poll_io();
#endif
    bridge_protocol_expire(&g_protocol, now_ms);
    receive_fc(now_ms, connected);
    if (connected)
    {
      receive_usb();
      (void)bridge_protocol_heartbeat(&g_protocol, &g_to_usb, now_ms);
    }
    flush_outputs(); // gui du lieu
    update_diagnostics(now_ms); // cap nhat bo dem, luong byte cho, drop
    vTaskDelay(pdMS_TO_TICKS(1U)); // block state 1 tick => nhuong CPU
  }
}

bool mavlink_bridge_task_start(void)
{
  return xTaskCreate(mavlink_bridge_task, "mav_bridge", TRACKER_BRIDGE_TASK_STACK_WORDS, NULL,
                     TRACKER_BRIDGE_TASK_PRIORITY, NULL) == pdPASS;
}
