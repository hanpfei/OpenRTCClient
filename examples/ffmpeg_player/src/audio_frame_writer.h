#pragma once

#include <functional>
extern "C" {
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

#include "audio_encoded_frame_decoder.h"

typedef std::function<
    int(const int16_t *data, int sample_rate, int channels,
        int samples_per_channel)> callback_t;

class AudioFrameWriter: public IAudiorameProcessing {
public:
  AudioFrameWriter();
  virtual ~AudioFrameWriter();
  int initialize(const char *dstPath);
  int close();

  int setDestSampleRate(int dest_sample_rate);
  int setDestChannelLayout(int dest_channel_layout);
  int setDestSampleFormat(enum AVSampleFormat dest_sample_format);

  int setCallback(callback_t &&callback);

public:
  void processAudioFrame(AVFrame *frame) override;

private:
  int resample(uint8_t *audio_data, AVFrame *frame);

  template<typename SampleType>
  void interleave(SampleType *dst, int nb_channels, int nb_samples,
      uint8_t **data);
  int get_format_from_sample_fmt(const char **fmt,
      enum AVSampleFormat sample_fmt);

private:
  std::string file_path_;
  FILE *out_file_ = nullptr;

  long src_data_size_;
  std::unique_ptr<uint8_t[]> src_data_;

  int dest_sample_rate_;
  int dest_channel_layout_;
  enum AVSampleFormat dest_sample_format_;

  SwrContext *swr_ctx_;
  uint8_t **sw_src_data_;
  uint8_t **sw_dst_data_;
  int dest_samples_per_frame_;
  std::unique_ptr<uint8_t[]> resample_buffer_data_;
  int left_samples_;

  callback_t callback_;
};

