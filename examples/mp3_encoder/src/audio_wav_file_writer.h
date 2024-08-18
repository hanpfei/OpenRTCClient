
#pragma once

#include <string>

#include "wav_utils.h"

struct AudioPcmDataInfo {
  AudioPcmDataInfo()
      : sampleCount(0), samplesOut(0), elapsedTimeMs(0), ntpTimeMs(0) {}

  AudioPcmDataInfo(const AudioPcmDataInfo &rhs)
      : sampleCount(rhs.sampleCount), samplesOut(rhs.samplesOut),
        elapsedTimeMs(rhs.elapsedTimeMs), ntpTimeMs(rhs.ntpTimeMs) {}

  // The samples count you expect.
  size_t sampleCount;

  // Output
  size_t samplesOut;
  int64_t elapsedTimeMs;
  int64_t ntpTimeMs;
};

class AudioWavFileWriter {
public:
  static std::unique_ptr<AudioWavFileWriter>
  CreateFileWrite(const std::string &filePath, size_t nChannels,
                  uint32_t sampleRateHz);

public:
  AudioWavFileWriter(const std::string &filePath, size_t nChannels,
                     uint32_t sampleRateHz);
  ~AudioWavFileWriter();

  bool preWriteWavFile();
  bool writeWavAudioPcmData(const void *payload_data,
                            const AudioPcmDataInfo &audioFrameInfo);
  bool postWriteWavFile();

private:
  bool initWAVHeader();

private:
  const std::string file_path_;
  const size_t number_of_channels_;
  const uint32_t sample_rate_hz_;
  WavHeader wav_header_;
  FILE *wav_file_;
  int64_t recv_sample_count_;
};
