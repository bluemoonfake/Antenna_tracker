#ifndef TRACKER_BRIDGE_PROTOCOL_H
#define TRACKER_BRIDGE_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <mavlink/common/mavlink.h>

#define BRIDGE_BUFFER_SIZE  4096U
#define BRIDGE_FRAME_GAP_MS 250U

typedef struct
{
  uint8_t data[BRIDGE_BUFFER_SIZE];
  size_t read_index;
  size_t write_index;
  size_t count;
  uint32_t dropped_bytes;
} bridge_buffer_t;

typedef struct
{
  mavlink_message_t rx_message;
  mavlink_status_t rx_status;
  uint16_t frame_index;
  uint16_t frame_length;
  bool mavlink2;
  bool unsupported_flags;
  bool at_boundary;
  uint32_t last_rx_ms;
  uint32_t last_heartbeat_ms;
  uint32_t valid_frames;
  uint32_t bad_frames;
  uint32_t resets;
  uint32_t heartbeats;
} bridge_protocol_t;

bool bridge_buffer_enqueue(bridge_buffer_t *buffer, const uint8_t *data, size_t length);
size_t bridge_buffer_peek(const bridge_buffer_t *buffer, const uint8_t **data, size_t limit);
void bridge_buffer_consume(bridge_buffer_t *buffer, size_t length);
void bridge_buffer_clear(bridge_buffer_t *buffer);
void bridge_protocol_init(bridge_protocol_t *protocol);
void bridge_protocol_reset(bridge_protocol_t *protocol);
void bridge_protocol_expire(bridge_protocol_t *protocol, uint32_t now_ms);
bool bridge_protocol_receive(bridge_protocol_t *protocol, uint8_t byte, uint32_t now_ms,
                             mavlink_message_t *message);
bool bridge_protocol_heartbeat(bridge_protocol_t *protocol, bridge_buffer_t *to_usb,
                               uint32_t now_ms);

#endif
