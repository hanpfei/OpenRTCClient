#pragma once

#include "api/audio/audio_mixer.h"
#include "common_audio/resampler/include/push_resampler.h"

#include "wav_pcm_file_parser.h"

class AudioMixerSourceWavFile : public webrtc::AudioMixer::Source {
public:
  static std::unique_ptr<AudioMixerSourceWavFile> Create(const char *file_path);

  AudioMixerSourceWavFile(std::unique_ptr<WavPcmFileParser> &&wav_file_parser);
  virtual ~AudioMixerSourceWavFile();

  AudioFrameInfo
  GetAudioFrameWithInfo(int sample_rate_hz,
                        webrtc::AudioFrame *audio_frame) override;

  // A way for a mixer implementation to distinguish participants.
  int Ssrc() const override;

  // A way for this source to say that GetAudioFrameWithInfo called
  // with this sample rate or higher will not cause quality loss.
  int PreferredSampleRate() const override;

  bool IsEof() const;

private:
  enum : size_t {
    // Stereo, 32 kHz, 120 ms (2 * 32 * 120)
    // Stereo, 192 kHz, 20 ms (2 * 192 * 20)
    kMaxDataSizeSamples = 7680,
    kMaxDataSizeBytes = kMaxDataSizeSamples * sizeof(int16_t),
  };

private:
  int id_;
  int sample_rate_;
  int channels_;
  std::unique_ptr<WavPcmFileParser> wav_file_parser_;
  webrtc::PushResampler<int16_t> resampler_;
  webrtc::AudioFrame audio_frame_;
};
