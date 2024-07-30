
#pragma once
#include "common_audio/resampler/include/push_resampler.h"

namespace webrtc {
class AudioFrame;
}

class AudioResampleUtils {
public:
  static int Resample(const webrtc::AudioFrame &frame, int dest_sample_rate,
                      webrtc::PushResampler<int16_t> *resampler,
                      int16_t *destination);

  static int Resample(size_t num_channels, int src_sample_rate_hz,
                      int dest_sample_rate, const int16_t *data,
                      size_t samples_per_channel,
                      webrtc::PushResampler<int16_t> *resampler,
                      int16_t *destination);

  static int Resample(size_t src_num_channels, int src_sample_rate_hz,
                      int dest_channels, int dest_sample_rate,
                      const int16_t *data, size_t samples_per_channel,
                      webrtc::PushResampler<int16_t> *resampler,
                      int16_t *destination);

  static std::unique_ptr<webrtc::AudioFrame>
  Resample(const void *samples, size_t samples_count, size_t src_channels,
           uint32_t src_sample_rate, size_t dest_channels,
           uint32_t dest_sample_rate,
           webrtc::PushResampler<int16_t> *resampler);

  static std::unique_ptr<webrtc::AudioFrame>
  Resample(const webrtc::AudioFrame &frame, int dest_channels,
           int dest_sample_rate, webrtc::PushResampler<int16_t> *resampler);

  static int ExpandChannels(const size_t samples_per_channel,
                            const size_t source_num_channels,
                            const size_t expand_to_num_channels, int16_t *data);

private:
  AudioResampleUtils() = delete;
  ~AudioResampleUtils() = delete;
};
