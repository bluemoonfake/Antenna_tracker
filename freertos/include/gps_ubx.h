#ifndef TRACKER_GPS_UBX_H
#define TRACKER_GPS_UBX_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  uint8_t state;
  uint8_t message_class;
  uint8_t message_id;
  uint16_t length;
  uint16_t offset;
  uint8_t checksum_a;
  uint8_t checksum_b;
  uint8_t received_checksum_a;
  uint32_t nav_pvt_frames;
  uint32_t checksum_errors;
  uint32_t oversized_frames;
  uint32_t invalid_fixes;
  uint8_t payload[92];
} gps_ubx_parser_t;

typedef struct
{
  int32_t latitude_e7;
  int32_t longitude_e7;
  int32_t altitude_msl_mm;
  uint8_t fix_type;
  uint8_t satellites;
  bool valid;
} gps_ubx_position_t;

void gps_ubx_init(gps_ubx_parser_t *parser);
bool gps_ubx_parse(gps_ubx_parser_t *parser, uint8_t byte, gps_ubx_position_t *position);

#endif
