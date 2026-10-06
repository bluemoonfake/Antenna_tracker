#ifndef TRACKER_BRIDGE_STATUS_H
#define TRACKER_BRIDGE_STATUS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  bool io_started; // khoi dong IO (UART, USB) thanh cong hay khong
  bool usb_configured; // trang thai USB ma task dung de giao tiep
  uint32_t task_iterations; // so vong lap task bridge thuc hien
  uint32_t rtos_ticks; // so tick cua RTOS
  uint32_t hal_millis; // moc thoi gian HAL
  uint32_t free_heap_bytes; // so byte con lai cua heap
  uint32_t minimum_free_heap_bytes; // luong heap trong thap nhat xPortGetMinimumEverFreeHeapSize
  uint32_t stack_high_water_words; // luong stack con lai trong thap nhat
                                   // uxTaskGetStackHighWaterMark
  uint32_t fc_valid_frames; // so frame hop le nhan duoc tu flight controller
  uint32_t fc_bad_frames;
  uint32_t parser_resets; // so lan parser reset
  uint32_t target_updates; // so lan cap nhat muc tieu
  uint32_t tracker_heartbeats; // nhan heartbeat tu tracker
  uint32_t usb_link_changes; // so lan task thay doi
  uint32_t to_fc_pending_bytes;
  uint32_t to_usb_pending_bytes;
  uint32_t to_fc_dropped_bytes;
  uint32_t to_usb_dropped_bytes;
} bridge_status_t;

/* ISR/driver counters are separate from task-owned counters. Read both while
 * the MCU is halted to obtain a coherent diagnostic snapshot. */
typedef struct
{
  uint32_t uart_rx_bytes;
  uint32_t uart_rx_dropped_bytes;
  uint32_t uart_errors;
  uint32_t uart_rx_restart_errors;
  uint32_t uart_tx_bytes;
  uint32_t uart_tx_completed_bytes;
  uint32_t uart_tx_start_errors;
  uint32_t usb_rx_bytes;
  uint32_t usb_rx_dropped_bytes;
  uint32_t usb_tx_bytes;
  uint32_t usb_tx_completed_bytes;
  uint32_t usb_tx_aborted_bytes;
  uint32_t usb_reset_events;
} bridge_io_status_t;

extern volatile bridge_status_t g_bridge_status;
extern volatile bridge_io_status_t g_bridge_io_status;

#endif
