
#include "audio_wav_file_writer.h"

#include <errno.h>
#include <string.h>

static const char MODULE_NAME[] = "[AWFW]";

std::unique_ptr<AudioWavFileWriter>
AudioWavFileWriter::CreateFileWrite(const std::string &filePath,
                                    size_t nChannels, uint32_t sampleRateHz) {
  auto file_writer =
      std::make_unique<AudioWavFileWriter>(filePath, nChannels, sampleRateHz);
  if (file_writer->preWriteWavFile()) {
    return file_writer;
  }
  return nullptr;
}

AudioWavFileWriter::AudioWavFileWriter(const std::string &filePath,
                                       size_t nChannels, uint32_t sampleRateHz)
    : file_path_(filePath), number_of_channels_(nChannels),
      sample_rate_hz_(sampleRateHz), wav_file_(nullptr), recv_sample_count_(0) {
}

AudioWavFileWriter::~AudioWavFileWriter() {
  if (wav_file_) {
    postWriteWavFile();
  }
}

bool AudioWavFileWriter::initWAVHeader() {
  if (number_of_channels_ == 0 || sample_rate_hz_ == 0) {
    printf(
        "%s: Init WAV header failed: number_of_channels %zu, sample_rate_hz %u.\n",
        MODULE_NAME, number_of_channels_, sample_rate_hz_);
    return false;
  }
  wav_header_.numberOfChannels = number_of_channels_;
  wav_header_.bytesPerSample = sizeof(int16_t);
  wav_header_.dataLength =
      recv_sample_count_ * wav_header_.numberOfChannels * sizeof(int16_t);
  wav_header_.streamLength = wav_header_.dataLength + 36;
  wav_header_.sampleRateHz = sample_rate_hz_;
  wav_header_.bitsPerSample = sizeof(int16_t) * 8;
  wav_header_.bytesPerSecond =
      wav_header_.sampleRateHz * sizeof(int16_t) * wav_header_.numberOfChannels;

  return true;
}

bool AudioWavFileWriter::preWriteWavFile() {
  if (wav_file_) {
    printf("%s: file %s already existed\n", MODULE_NAME, file_path_.c_str());
    return true;
  }

  if (!initWAVHeader()) {
    return false;
  }

  wav_file_ = fopen(file_path_.c_str(), "wb+");
  if (!wav_file_) {
    printf("%s: Open file %s failed\n", MODULE_NAME, file_path_.c_str());
    return false;
  }

  unsigned char wav_header[44];
  makeWAVHeader(wav_header, wav_header_);

  if (!fwrite(wav_header, sizeof(wav_header), 1, wav_file_)) {
    printf("%s: Write WAV header failed: %s\n", MODULE_NAME, strerror(errno));
    return false;
  }

  return true;
}

bool AudioWavFileWriter::writeWavAudioPcmData(
    const void *payload_data, const AudioPcmDataInfo &audioFrameInfo) {
  if (!payload_data || audioFrameInfo.samplesOut == 0) {
    printf("%s: Write wav audio data failed: invalid parameter\n", MODULE_NAME);
    return false;
  }
  if (!wav_file_) {
    printf("%s: Write wav audio data failed: no opened file\n", MODULE_NAME);
    return false;
  }
  recv_sample_count_ += audioFrameInfo.samplesOut;
  if (!fwrite(payload_data, audioFrameInfo.samplesOut * sizeof(int16_t), 1,
              wav_file_)) {
    printf("%s: Write wav audio data failed: %s\n", MODULE_NAME,
           strerror(errno));
    return false;
  }
  fflush(wav_file_);
  return true;
}

bool AudioWavFileWriter::postWriteWavFile() {
  if (!wav_file_) {
    printf("%s: Write wav audio data failed: no opened file\n", MODULE_NAME);
    return false;
  }

  unsigned char wav_header[44];
  wav_header_.dataLength =
      recv_sample_count_ * wav_header_.numberOfChannels * sizeof(int16_t);
  makeWAVHeader(wav_header, wav_header_);
  if (fseek(wav_file_, 0, SEEK_SET) ||
      !fwrite(wav_header, sizeof(wav_header), 1, wav_file_)) {
    printf("%s: Rewrite WAV header failed: %s\n", MODULE_NAME, strerror(errno));
  }
  fclose(wav_file_);
  wav_file_ = nullptr;

  return true;
}
