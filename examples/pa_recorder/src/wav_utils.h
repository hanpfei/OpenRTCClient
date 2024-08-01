
#pragma once

#include <stdint.h>

#include <memory>

struct WavHeader {
  char tag[5] = {'R', 'I', 'F', 'F', 0};
  int32_t streamLength;
  char typeTag[5] = {'W', 'A', 'V', 'E', 0};
  char fmtTag[5] = {'f', 'm', 't', ' ', 0};
  int32_t cksize = 0x10;
  int16_t codec = 1; // 1 -> pcm
  int16_t numberOfChannels = 0;
  int32_t sampleRateHz = 0;
  int32_t bytesPerSecond = 0;
  int16_t bytesPerSample = 0;
  int16_t bitsPerSample = 16;
  char dataTag[5] = {'d', 'a', 't', 'a', 0};
  int32_t dataLength = 0;
};

void makeWAVHeader(unsigned char _dst[44], const WavHeader &header);
void parseWAVHeader(const unsigned char _dst[44], WavHeader &header);

std::unique_ptr<WavHeader> createWAVHeader(int16_t numberOfChannels,
                                           uint32_t sampleRateHz);
