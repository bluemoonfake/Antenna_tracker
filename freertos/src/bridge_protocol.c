#include "bridge_protocol.h"

#include <string.h>
#include "tracker_config.h"

bool bridge_buffer_enqueue(bridge_buffer_t *buffer, const uint8_t *data, size_t length)
{
  // kiem tra buffer co du dung luong de enqueue khong
  if (length > BRIDGE_BUFFER_SIZE - buffer->count)
  {
    buffer->dropped_bytes += (uint32_t)length;
    return false;
  }
  if (length == 0U)
    return true;
  size_t first = BRIDGE_BUFFER_SIZE - buffer->write_index; // so o con lai tu vi tri ghi den het
                                                           // mang
  if (first > length)
    first = length;
  // copy du lieu vao buffer. memcpy(dich, nguon, so byte)
  memcpy(&buffer->data[buffer->write_index], data, first);
  memcpy(buffer->data, data + first, length - first);
  // update va ghi index
  buffer->write_index = (buffer->write_index + length) % BRIDGE_BUFFER_SIZE;
  buffer->count += length;
  return true;
}

size_t bridge_buffer_peek(const bridge_buffer_t *buffer, const uint8_t **data, size_t limit)
{
  size_t length = BRIDGE_BUFFER_SIZE - buffer->read_index;
  if (length > buffer->count)
    length = buffer->count;
  if (length > limit)
    length = limit;
  *data = &buffer->data[buffer->read_index];
  return length;
}

void bridge_buffer_consume(bridge_buffer_t *buffer, size_t length)
{
  buffer->read_index = (buffer->read_index + length) % BRIDGE_BUFFER_SIZE;
  buffer->count -= length;
}

void bridge_buffer_clear(bridge_buffer_t *buffer)
{
  buffer->dropped_bytes += (uint32_t)buffer->count;
  buffer->count = 0U;
  buffer->read_index = 0U;
  buffer->write_index = 0U;
}

void bridge_protocol_init(bridge_protocol_t *protocol)
{
  // tinh do dai cua frame
  memset(protocol, 0, sizeof(*protocol));
  protocol->at_boundary = true;
}

void bridge_protocol_reset(bridge_protocol_t *protocol)
{
  memset(&protocol->rx_status, 0,
         sizeof(protocol->rx_status)); // dat lai cac gia tri cua rx_status ve 0
  protocol->frame_index = 0U;
  /* After a truncated/lost frame, suppress injection until a whole subsequent
   * wire frame has passed. Never append a heartbeat to a timed-out fragment. */
  protocol->at_boundary = false;
  protocol->resets++;
}

void bridge_protocol_expire(bridge_protocol_t *protocol, uint32_t now_ms)
{
  if (protocol->frame_index != 0U && now_ms - protocol->last_rx_ms >= BRIDGE_FRAME_GAP_MS)
  {
    bridge_protocol_reset(protocol);
  }
}

bool bridge_protocol_receive(bridge_protocol_t *protocol, uint8_t byte, uint32_t now_ms,
                             mavlink_message_t *message)
{
  if (protocol->frame_index == 0U)
  {
    if (byte != MAVLINK_STX && byte != MAVLINK_STX_MAVLINK1)
      return false;
    // khoi tao lai cac gia tri cua protocol khi nhan duoc byte dau tien cua frame
    memset(&protocol->rx_status, 0, sizeof(protocol->rx_status));
    protocol->mavlink2 = byte == MAVLINK_STX;
    protocol->unsupported_flags = false;
    protocol->frame_length =
        protocol->mavlink2 ? 12U : 8U; // byte co dinh ngoai payload: header + CRC
    protocol->at_boundary = false;
  }
  if (protocol->frame_index == 1U) // mavlink 1
    protocol->frame_length += byte;
  if (protocol->frame_index == 2U && protocol->mavlink2 && // mavlink2
      (byte & MAVLINK_IFLAG_SIGNED) != 0U) // kiem tra xem co flag sign hay khong
  {
    protocol->frame_length +=
        MAVLINK_SIGNATURE_BLOCK_LEN; // header 10 byte + payload 28 byte + CRC (2 byte) + 13 (ID(1
                                     // byte), timestamp(6 byte), signature(6 byte))
  }
  if (protocol->frame_index == 2U &&
      protocol->mavlink2 && // kiem tra co flag khong tuong thich khong
      (byte & ~MAVLINK_IFLAG_SIGNED) != 0U)
    protocol->unsupported_flags = true; // neu co => true
  protocol->frame_index++;
  protocol->last_rx_ms = now_ms;
  uint8_t result = MAVLINK_FRAMING_INCOMPLETE;
  if (!protocol->unsupported_flags)
    result =
        mavlink_frame_char_buffer(&protocol->rx_message, &protocol->rx_status, byte, message, NULL);
  if (protocol->frame_index < protocol->frame_length)
    return false;
  protocol->frame_index = 0U;
  protocol->at_boundary = true;
  if (result == MAVLINK_FRAMING_OK)
  {
    protocol->valid_frames++;
    return true;
  }
  protocol->bad_frames++;
  return false;
}

// tao ban tin heartbeat va gui di neu du dieu kien
bool bridge_protocol_heartbeat(bridge_protocol_t *protocol, bridge_buffer_t *to_usb,
                               uint32_t now_ms)
{
  if (!protocol->at_boundary || // neu boundary = false => frame chua ket thuc
      BRIDGE_BUFFER_SIZE - to_usb->count <
          MAVLINK_MSG_ID_HEARTBEAT_LEN +
              MAVLINK_NUM_NON_PAYLOAD_BYTES || // queue khong du chua heartbeat => false
      now_ms - protocol->last_heartbeat_ms < 1000U) // chua du 1s tu lan them heartbeat
    return false;
  uint8_t frame[MAVLINK_MAX_PACKET_LEN];
  mavlink_message_t message;
  mavlink_msg_heartbeat_pack_chan(TRACKER_SYSTEM_ID, TRACKER_COMPONENT_ID, MAVLINK_COMM_1, &message,
                                  MAV_TYPE_ANTENNA_TRACKER, MAV_AUTOPILOT_INVALID, 0, 0,
                                  MAV_STATE_ACTIVE); // tao heartbeat message
  uint16_t length = mavlink_msg_to_send_buffer(frame, &message); // ghi frame vao buffer
  if (!bridge_buffer_enqueue(to_usb, frame, length))
    return false;
  protocol->last_heartbeat_ms = now_ms;
  protocol->heartbeats++; // ghi nhan thanh cong 1 heartbeat
  return true;
}
