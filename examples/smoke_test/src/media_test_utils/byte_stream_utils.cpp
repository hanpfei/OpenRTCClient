
#include "byte_stream_utils.h"

#include <stdint.h>

void put_le32(unsigned char* _dst, uint32_t _x) {
  _dst[0] = (unsigned char)(_x & 0xFF);
  _dst[1] = (unsigned char)(_x >> 8 & 0xFF);
  _dst[2] = (unsigned char)(_x >> 16 & 0xFF);
  _dst[3] = (unsigned char)(_x >> 24 & 0xFF);
}

uint32_t get_le32(const unsigned char* _dst) {
  uint32_t value = _dst[0];
  value |= ((uint32_t)_dst[1]) << 8;
  value |= ((uint32_t)_dst[2]) << 16;
  value |= ((uint32_t)_dst[3]) << 24;
  return value;
}

void put_le16(unsigned char* _dst, uint16_t _x) {
  _dst[0] = (unsigned char)(_x & 0xFF);
  _dst[1] = (unsigned char)(_x >> 8 & 0xFF);
}

uint16_t get_le16(const unsigned char* _dst) {
  uint16_t value = _dst[0];
  value |= ((uint16_t)_dst[1]) << 8;
  return value;
}
