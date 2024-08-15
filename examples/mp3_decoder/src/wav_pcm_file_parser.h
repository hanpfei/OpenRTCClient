
#pragma once
#include <stdint.h>
#include <stdio.h>

#include "wav_utils.h"

class WavPcmFileParser {
public:
  explicit WavPcmFileParser(const char *filepath, size_t frameDuration = 10);
  ~WavPcmFileParser();
  bool open();
  bool hasNext();
  bool reset();

  int getNumberOfChannels();
  int getSampleRate();
  int getBitsPerSample();

  void getNext(char *buffer, int *length);

private:
  void readData();

private:
  static constexpr int BufferSize = 40960;

  char *wavFilePath_;
  FILE *wavFile_;
  unsigned char dataBuffer_[BufferSize] = {0};
  WavHeader wavHeader_;
  int32_t readedLength_;
  int32_t bufDataLength_;
  bool isEof_;
  size_t frameDuration_;
};
