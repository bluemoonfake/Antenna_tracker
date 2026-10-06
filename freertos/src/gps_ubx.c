#include "gps_ubx.h"

#include <string.h>

enum
{
  UBX_SYNC1,
  UBX_SYNC2,
  UBX_CLASS,
  UBX_ID,
  UBX_LENGTH_LOW,
  UBX_LENGTH_HIGH,
  UBX_PAYLOAD,
  UBX_CHECKSUM_A,
  UBX_CHECKSUM_B
};

static void checksum(gps_ubx_parser_t *parser, uint8_t byte)
{
  parser->checksum_a = (uint8_t)(parser->checksum_a + byte);
  parser->checksum_b = (uint8_t)(parser->checksum_b + parser->checksum_a);
}

static int32_t read_i32(const uint8_t *data)
{
  return (int32_t)((uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
                   ((uint32_t)data[3] << 24));
}

void gps_ubx_init(gps_ubx_parser_t *parser)
{
  memset(parser, 0, sizeof(*parser));
  parser->state = UBX_SYNC1;
}

bool gps_ubx_parse(gps_ubx_parser_t *parser, uint8_t byte, gps_ubx_position_t *position)
{
  bool complete = false;

  if (parser == NULL || position == NULL)
    return false;

  switch (parser->state)
  {
    case UBX_SYNC1:
      if (byte == 0xb5U)
        parser->state = UBX_SYNC2;
      break;
    case UBX_SYNC2:
      parser->state = byte == 0x62U ? UBX_CLASS : UBX_SYNC1;
      parser->checksum_a = 0;
      parser->checksum_b = 0;
      break;
    case UBX_CLASS:
      parser->message_class = byte;
      checksum(parser, byte);
      parser->state = UBX_ID;
      break;
    case UBX_ID:
      parser->message_id = byte;
      checksum(parser, byte);
      parser->state = UBX_LENGTH_LOW;
      break;
    case UBX_LENGTH_LOW:
      parser->length = byte;
      checksum(parser, byte);
      parser->state = UBX_LENGTH_HIGH;
      break;
    case UBX_LENGTH_HIGH:
      parser->length |= (uint16_t)byte << 8;
      checksum(parser, byte);
      parser->offset = 0;
      if (parser->length > sizeof(parser->payload))
      {
        parser->oversized_frames++;
        parser->state = UBX_SYNC1;
      } else
      {
        parser->state = parser->length == 0 ? UBX_CHECKSUM_A : UBX_PAYLOAD;
      }
      break;
    case UBX_PAYLOAD:
      parser->payload[parser->offset++] = byte;
      checksum(parser, byte);
      if (parser->offset == parser->length)
        parser->state = UBX_CHECKSUM_A;
      break;
    case UBX_CHECKSUM_A:
      parser->received_checksum_a = byte;
      parser->state = UBX_CHECKSUM_B;
      break;
    case UBX_CHECKSUM_B:
      if (parser->received_checksum_a != parser->checksum_a || byte != parser->checksum_b)
      {
        parser->checksum_errors++;
      } else if (parser->message_class == 0x01U && parser->message_id == 0x07U &&
                 parser->length >= sizeof(parser->payload))
      {
        position->fix_type = parser->payload[20];
        position->satellites = parser->payload[23];
        position->longitude_e7 = read_i32(&parser->payload[24]);
        position->latitude_e7 = read_i32(&parser->payload[28]);
        position->altitude_msl_mm = read_i32(&parser->payload[36]);
        position->valid = position->fix_type >= 3U && (parser->payload[21] & 0x01U) != 0U;
        parser->nav_pvt_frames++;
        if (!position->valid)
          parser->invalid_fixes++;
        complete = true;
      }
      parser->state = UBX_SYNC1;
      break;
    default:
      parser->state = UBX_SYNC1;
      break;
  }

  return complete;
}
