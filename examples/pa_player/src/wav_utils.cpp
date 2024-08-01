
#include "wav_utils.h"

#include <string.h>

static void put_le32(unsigned char *_dst, uint32_t _x) {
  _dst[0] = (unsigned char)(_x & 0xFF);
  _dst[1] = (unsigned char)(_x >> 8 & 0xFF);
  _dst[2] = (unsigned char)(_x >> 16 & 0xFF);
  _dst[3] = (unsigned char)(_x >> 24 & 0xFF);
}

static uint32_t get_le32(const unsigned char *_dst) {
  uint32_t value = _dst[0];
  value |= ((uint32_t)_dst[1]) << 8;
  value |= ((uint32_t)_dst[2]) << 16;
  value |= ((uint32_t)_dst[3]) << 24;
  return value;
}

static void put_le16(unsigned char *_dst, uint16_t _x) {
  _dst[0] = (unsigned char)(_x & 0xFF);
  _dst[1] = (unsigned char)(_x >> 8 & 0xFF);
}

static uint16_t get_le16(const unsigned char *_dst) {
  uint16_t value = _dst[0];
  value |= ((uint16_t)_dst[1]) << 8;
  return value;
}

/*Make a header for a 48 kHz, stereo, signed, 16-bit little-endian PCM WAV.*/
void makeWAVHeader(unsigned char _dst[44], const WavHeader &header) {
  /*The chunk sizes are set to 0x7FFFFFFF by default.
   Many, though not all, programs will interpret this to mean the duration is
   "undefined", and continue to read from the file so long as there is actual
   data.*/
  static const unsigned char WAV_HEADER_TEMPLATE[44] = {
      'R',  'I',  'F',  'F',  0xFF, 0xFF, 0xFF, 0x7F, 'W',  'A',  'V',
      'E',  'f',  'm',  't',  ' ',  0x10, 0x00, 0x00, 0x00, 0x01, 0x00,
      0x02, 0x00, 0x44, 0xAC, 0x00, 0x00, 0x10, 0xB1, 0x02, 0x00, 0x04,
      0x00, 0x10, 0x00, 'd',  'a',  't',  'a',  0xFF, 0xFF, 0xFF, 0x7F};
  memcpy(_dst, WAV_HEADER_TEMPLATE, sizeof(WAV_HEADER_TEMPLATE));
  put_le32(_dst + 4, header.dataLength + 36);
  put_le16(_dst + 20, header.codec);
  put_le16(_dst + 22, header.numberOfChannels);
  put_le32(_dst + 24, header.sampleRateHz);
  put_le32(_dst + 28, header.bytesPerSecond);
  put_le16(_dst + 32, header.bytesPerSample);
  put_le16(_dst + 34, header.bitsPerSample);
  put_le32(_dst + 40, header.dataLength);
}

void parseWAVHeader(const unsigned char _dst[44], WavHeader &header) {
  strncpy(reinterpret_cast<char *>(header.tag),
          reinterpret_cast<const char *>(_dst), 4);
  strncpy(reinterpret_cast<char *>(header.typeTag),
          reinterpret_cast<const char *>(_dst + 8), 4);
  strncpy(reinterpret_cast<char *>(header.fmtTag),
          reinterpret_cast<const char *>(_dst + 12), 4);
  strncpy(reinterpret_cast<char *>(header.dataTag),
          reinterpret_cast<const char *>(_dst + 36), 4);

  header.dataLength = get_le32(_dst + 4) - 36;
  header.codec = get_le16(_dst + 20);
  header.numberOfChannels = get_le16(_dst + 22);
  header.sampleRateHz = get_le32(_dst + 24);
  header.bytesPerSecond = get_le32(_dst + 28);
  header.bytesPerSample = get_le16(_dst + 32);
  header.bitsPerSample = get_le16(_dst + 34);
  header.dataLength = get_le32(_dst + 40);
}

std::unique_ptr<WavHeader> createWAVHeader(int16_t numberOfChannels,
                                           uint32_t sampleRateHz) {
  auto wavHeader = std::make_unique<WavHeader>();
  wavHeader->numberOfChannels = numberOfChannels;
  wavHeader->bytesPerSample = sizeof(int16_t);

  int recvSampleCount = 0;
  wavHeader->dataLength =
      recvSampleCount * wavHeader->numberOfChannels * sizeof(int16_t);
  wavHeader->streamLength = wavHeader->dataLength + 36;
  wavHeader->sampleRateHz = sampleRateHz;
  wavHeader->bitsPerSample = sizeof(int16_t) * 8;
  wavHeader->bytesPerSecond =
      wavHeader->sampleRateHz * sizeof(int16_t) * wavHeader->numberOfChannels;

  return wavHeader;
}
