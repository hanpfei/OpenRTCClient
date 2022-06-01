
#pragma once

#include <stdint.h>

void put_le32(unsigned char* _dst, uint32_t _x);

uint32_t get_le32(const unsigned char* _dst);

void put_le16(unsigned char* _dst, uint16_t _x);

uint16_t get_le16(const unsigned char* _dst);
