#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "gps_ubx.h"

static uint8_t g_frame[100];

static void put_i32(uint8_t *data, int32_t value)
{
  uint32_t raw = (uint32_t)value;
  data[0] = (uint8_t)raw;
  data[1] = (uint8_t)(raw >> 8);
  data[2] = (uint8_t)(raw >> 16);
  data[3] = (uint8_t)(raw >> 24);
}

static size_t make_nav_pvt(uint8_t fix_type, bool fix_ok)
{
  uint8_t checksum_a = 0U;
  uint8_t checksum_b = 0U;
  size_t index;

  for (index = 0; index < sizeof(g_frame); index++)
    g_frame[index] = 0U;
  g_frame[0] = 0xb5U;
  g_frame[1] = 0x62U;
  g_frame[2] = 0x01U;
  g_frame[3] = 0x07U;
  g_frame[4] = 92U;
  g_frame[26] = fix_type;
  g_frame[27] = fix_ok ? 1U : 0U;
  g_frame[29] = 14U;
  put_i32(&g_frame[30], 1061234567);
  put_i32(&g_frame[34], 105234567);
  put_i32(&g_frame[42], 12345);
  for (index = 2U; index < 98U; index++)
  {
    checksum_a = (uint8_t)(checksum_a + g_frame[index]);
    checksum_b = (uint8_t)(checksum_b + checksum_a);
  }
  g_frame[98] = checksum_a;
  g_frame[99] = checksum_b;
  return sizeof(g_frame);
}

static bool feed(gps_ubx_parser_t *parser, gps_ubx_position_t *position, size_t length)
{
  size_t index;
  bool complete = false;

  for (index = 0; index < length; index++)
    complete = gps_ubx_parse(parser, g_frame[index], position) || complete;
  return complete;
}

int main(void)
{
  gps_ubx_parser_t parser;
  gps_ubx_position_t position = {0};
  size_t length = make_nav_pvt(3U, true);
  size_t index;

  gps_ubx_init(&parser);
  for (index = 0; index < length; index++)
  {
    bool complete = gps_ubx_parse(&parser, g_frame[index], &position);
    assert(complete == (index == length - 1U));
  }
  assert(position.valid && position.fix_type == 3U && position.satellites == 14U);
  assert(position.latitude_e7 == 105234567 && position.longitude_e7 == 1061234567);
  assert(position.altitude_msl_mm == 12345 && parser.nav_pvt_frames == 1U);

  length = make_nav_pvt(2U, false);
  assert(feed(&parser, &position, length));
  assert(!position.valid && parser.invalid_fixes == 1U);

  length = make_nav_pvt(3U, true);
  g_frame[40] ^= 0x80U;
  assert(!feed(&parser, &position, length));
  assert(parser.checksum_errors == 1U && parser.nav_pvt_frames == 2U);

  gps_ubx_init(&parser);
  (void)gps_ubx_parse(&parser, 0xb5U, &position);
  (void)gps_ubx_parse(&parser, 0x62U, &position);
  (void)gps_ubx_parse(&parser, 0x01U, &position);
  (void)gps_ubx_parse(&parser, 0x07U, &position);
  (void)gps_ubx_parse(&parser, 93U, &position);
  (void)gps_ubx_parse(&parser, 0U, &position);
  assert(parser.oversized_frames == 1U);
  assert(!gps_ubx_parse(NULL, 0U, &position));
  assert(!gps_ubx_parse(&parser, 0U, NULL));
  puts("PASS: fragmented NAV-PVT, invalid fix, checksum and oversize diagnostics");
  return 0;
}
