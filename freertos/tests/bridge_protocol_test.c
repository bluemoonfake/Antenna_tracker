#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "bridge_protocol.h"

static bridge_protocol_t g_protocol;
static bridge_buffer_t g_buffer;

static size_t heartbeat(uint8_t *bytes, bool v1)
{
  mavlink_message_t message;
  mavlink_status_t *status = mavlink_get_channel_status(MAVLINK_COMM_0);
  status->flags = v1 ? MAVLINK_STATUS_FLAG_OUT_MAVLINK1 : 0U;
  mavlink_msg_heartbeat_pack_chan(1U, 1U, MAVLINK_COMM_0, &message, MAV_TYPE_QUADROTOR,
                                  MAV_AUTOPILOT_PX4, 0U, 0U, MAV_STATE_ACTIVE);
  return mavlink_msg_to_send_buffer(bytes, &message);
}

static void frame_boundaries(void)
{
  uint8_t bytes[MAVLINK_MAX_PACKET_LEN];
  mavlink_message_t message;
  for (unsigned mode = 0; mode < 4U; mode++)
  {
    memset(&g_buffer, 0, sizeof(g_buffer));
    bridge_protocol_init(&g_protocol);
    size_t size = heartbeat(bytes, mode == 0U);
    if (mode == 2U)
    {
      /* Signed v2 frame: CRC is deliberately invalid, but the 13-byte
       * signature must remain in the forwarded frame before injection. */
      bytes[2] |= MAVLINK_IFLAG_SIGNED;
      memset(bytes + size, 0xa5, MAVLINK_SIGNATURE_BLOCK_LEN);
      size += MAVLINK_SIGNATURE_BLOCK_LEN;
    }
    if (mode == 3U)
      bytes[2] |= 0x80U; /* Unsupported incompatibility flag. */
    for (size_t i = 0; i < size; i++)
    {
      assert(bridge_buffer_enqueue(&g_buffer, bytes + i, 1U));
      bool valid = bridge_protocol_receive(&g_protocol, bytes[i], 2000U, &message);
      if (i + 1U < size)
      {
        assert(!valid && !g_protocol.at_boundary);
        assert(!bridge_protocol_heartbeat(&g_protocol, &g_buffer, 2000U));
      } else
      {
        assert(g_protocol.at_boundary);
        assert(valid == (mode < 2U));
      }
    }
    assert(bridge_protocol_heartbeat(&g_protocol, &g_buffer, 2000U));
    assert(memcmp(g_buffer.data, bytes, size) == 0);
    assert(g_buffer.data[size] == MAVLINK_STX);
    assert(g_protocol.heartbeats == 1U);
  }
}

static void recovery_and_wraparound(void)
{
  uint8_t bytes[MAVLINK_MAX_PACKET_LEN];
  mavlink_message_t message;
  size_t size = heartbeat(bytes, false);
  bridge_protocol_init(&g_protocol);
  memset(&g_buffer, 0, sizeof(g_buffer));
  for (size_t i = 0; i < 5U; i++)
    assert(!bridge_protocol_receive(&g_protocol, bytes[i], UINT32_MAX - 10U, &message));
  bridge_protocol_expire(&g_protocol, BRIDGE_FRAME_GAP_MS - 11U);
  assert(g_protocol.resets == 1U && !g_protocol.at_boundary);
  assert(!bridge_protocol_heartbeat(&g_protocol, &g_buffer, 3000U));
  bytes[size - 1U] ^= 0x40U;
  for (size_t i = 0; i < size; i++)
    assert(!bridge_protocol_receive(&g_protocol, bytes[i], 4000U, &message));
  assert(g_protocol.bad_frames == 1U && g_protocol.at_boundary);
  bytes[size - 1U] ^= 0x40U;
  for (size_t i = 0; i < size; i++)
    assert(bridge_protocol_receive(&g_protocol, bytes[i], 4001U, &message) == (i + 1U == size));
  assert(g_protocol.valid_frames == 1U);
  assert(message.msgid == MAVLINK_MSG_ID_HEARTBEAT);
  g_protocol.last_heartbeat_ms = UINT32_MAX - 500U;
  assert(bridge_protocol_heartbeat(&g_protocol, &g_buffer, 499U));
  assert(!bridge_protocol_heartbeat(&g_protocol, &g_buffer, 500U));
}

static void reject_nested_message(void)
{
  uint8_t inner[MAVLINK_MAX_PACKET_LEN];
  uint8_t outer[MAVLINK_MAX_PACKET_LEN] = {0};
  size_t size = heartbeat(inner, false);
  outer[0] = MAVLINK_STX;
  outer[1] = (uint8_t)(size - 2U);
  outer[2] = 0x80U;
  memcpy(outer + 10U, inner, size);
  bridge_protocol_init(&g_protocol);
  mavlink_message_t message;
  for (size_t i = 0; i < size + 10U; i++)
    assert(!bridge_protocol_receive(&g_protocol, outer[i], 2000U, &message));
  assert(g_protocol.bad_frames == 1U && g_protocol.valid_frames == 0U);
  assert(g_protocol.at_boundary);
}

static void bounded_buffers(void)
{
  uint8_t bytes[BRIDGE_BUFFER_SIZE];
  uint8_t output[BRIDGE_BUFFER_SIZE];
  for (size_t i = 0; i < sizeof(bytes); i++)
    bytes[i] = (uint8_t)i;
  memset(&g_buffer, 0, sizeof(g_buffer));
  assert(bridge_buffer_enqueue(&g_buffer, bytes, sizeof(bytes)));
  assert(!bridge_buffer_enqueue(&g_buffer, bytes, 1U));
  assert(g_buffer.dropped_bytes == 1U);
  bridge_buffer_consume(&g_buffer, 3000U);
  assert(bridge_buffer_enqueue(&g_buffer, bytes, 3000U));
  size_t offset = 0U;
  while (g_buffer.count != 0U)
  {
    const uint8_t *data;
    size_t length = bridge_buffer_peek(&g_buffer, &data, 63U);
    memcpy(output + offset, data, length);
    offset += length;
    bridge_buffer_consume(&g_buffer, length);
  }
  assert(offset == sizeof(bytes));
  assert(memcmp(output, bytes + 3000U, sizeof(bytes) - 3000U) == 0);
  assert(memcmp(output + sizeof(bytes) - 3000U, bytes, 3000U) == 0);
  assert(bridge_buffer_enqueue(&g_buffer, bytes, 50U));
  bridge_buffer_clear(&g_buffer);
  assert(g_buffer.count == 0U && g_buffer.dropped_bytes == 51U);
  bridge_protocol_init(&g_protocol);
  assert(bridge_buffer_enqueue(&g_buffer, bytes, sizeof(bytes)));
  assert(!bridge_protocol_heartbeat(&g_protocol, &g_buffer, 2000U));
  assert(g_buffer.dropped_bytes == 51U);
}

int main(int argc, char **argv)
{
  if (argc == 2)
  {
    FILE *file = fopen(argv[1], "rb");
    assert(file != NULL);
    bridge_protocol_init(&g_protocol);
    mavlink_message_t message;
    int byte;
    while ((byte = fgetc(file)) != EOF)
      (void)bridge_protocol_receive(&g_protocol, (uint8_t)byte, 1U, &message);
    fclose(file);
    assert(g_protocol.valid_frames == 1U && g_protocol.bad_frames == 0U);
    mavlink_global_position_int_t position;
    mavlink_msg_global_position_int_decode(&message, &position);
    assert(position.lat == 110000000 && position.lon == 1060000000);
    assert(position.alt == 23700);
  }
  frame_boundaries();
  recovery_and_wraparound();
  reject_nested_message();
  bounded_buffers();
  puts("PASS: MAVLink v1/v2/signed/bad frame boundaries, loss recovery, tick "
       "wrap, bounded queues");
  return 0;
}
