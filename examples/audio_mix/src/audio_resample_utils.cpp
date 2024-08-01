
#include "audio_resample_utils.h"

#include "api/audio/audio_frame.h"
#include "audio/utility/audio_frame_operations.h"

// Resample audio in |frame| to given sample rate preserving the
// channel count and place the result in |destination|.
int AudioResampleUtils::Resample(size_t src_channels, int src_sample_rate,
                                 int dest_sample_rate, const int16_t *data,
                                 size_t samples_per_channel,
                                 webrtc::PushResampler<int16_t> *resampler,
                                 int16_t *destination) {
  const int target_samples_per_channel = dest_sample_rate / 100;
  if (resampler->InitializeIfNeeded(src_sample_rate, dest_sample_rate,
                                    src_channels) != 0) {
    printf("InitializeIfNeeded (%d, %d, %d) failed.\n");
    return -1;
  }

  return resampler->Resample(data, samples_per_channel * src_channels,
                             destination,
                             src_channels * target_samples_per_channel);
}

int AudioResampleUtils::Resample(size_t src_channels, int src_sample_rate,
                                 int dest_channels, int dest_sample_rate,
                                 const int16_t *data,
                                 size_t samples_per_channel,
                                 webrtc::PushResampler<int16_t> *resampler,
                                 int16_t *destination) {
  int totalSamples = 0;
  if (src_sample_rate == dest_sample_rate && src_channels != dest_channels) {
    if (src_channels > dest_channels) {
      webrtc::AudioFrameOperations::DownmixChannels(
          data, src_channels, samples_per_channel, dest_channels, destination);
    } else {
      for (size_t i = 0; i < samples_per_channel; ++i) {
        for (size_t j = 0; j < dest_channels; ++i) {
          destination[i * dest_channels + j] =
              data[i * src_channels + (j % src_channels)];
        }
      }
    }
    totalSamples = samples_per_channel * dest_channels;
  } else if (src_channels == dest_channels &&
             src_sample_rate != dest_sample_rate) {
    totalSamples = Resample(src_channels, src_sample_rate, dest_sample_rate,
                            data, samples_per_channel, resampler, destination);
  } else if (src_channels != dest_channels &&
             src_sample_rate != dest_sample_rate) {
    int16_t tmpbuffer[webrtc::AudioFrame::kMaxDataSizeSamples] = {0};
    totalSamples = Resample(src_channels, src_sample_rate, dest_sample_rate,
                            data, samples_per_channel, resampler, tmpbuffer);

    if (src_channels < dest_channels) {
      for (size_t i = 0; i < totalSamples / src_channels; ++i) {
        for (size_t j = 0; j < dest_channels; ++i) {
          destination[i * dest_channels + j] =
              tmpbuffer[i * src_channels + (j % src_channels)];
        }
      }
    } else {
      webrtc::AudioFrameOperations::DownmixChannels(tmpbuffer, src_channels,
                                                    totalSamples / src_channels,
                                                    dest_channels, destination);
    }
    totalSamples = totalSamples / src_channels * dest_channels;
  } else {
    totalSamples = samples_per_channel * src_channels;
    memcpy(destination, data, sizeof(int16_t) * totalSamples);
  }
  return totalSamples;
}

int AudioResampleUtils::Resample(const webrtc::AudioFrame &frame,
                                 int dest_sample_rate,
                                 webrtc::PushResampler<int16_t> *resampler,
                                 int16_t *destination) {
  const int src_channels = static_cast<int>(frame.num_channels_);
  const int src_sample_rate = frame.sample_rate_hz_;
  const int16_t *data = frame.data();
  const size_t samples_per_channel = frame.samples_per_channel_;

  return Resample(src_channels, src_sample_rate, dest_sample_rate, data,
                  samples_per_channel, resampler, destination);
}

std::unique_ptr<webrtc::AudioFrame>
AudioResampleUtils::Resample(const void *samples, size_t samples_count,
                             size_t src_channels, uint32_t src_sample_rate,
                             size_t dest_channels, uint32_t dest_sample_rate,
                             webrtc::PushResampler<int16_t> *resampler) {
  auto audio_frame = std::make_unique<webrtc::AudioFrame>();
  if (src_channels != 0) {
    size_t samples_per_channel = samples_count / src_channels;
    int samples_out =
        Resample(src_channels, src_sample_rate, dest_channels, dest_sample_rate,
                 static_cast<const int16_t *>(samples), samples_per_channel,
                 resampler, audio_frame->mutable_data());

    audio_frame->timestamp_ = 0;
    audio_frame->samples_per_channel_ = samples_out / dest_channels;
    audio_frame->sample_rate_hz_ = dest_sample_rate;
    audio_frame->speech_type_ = webrtc::AudioFrame::SpeechType::kNormalSpeech;
    audio_frame->vad_activity_ = webrtc::AudioFrame::VADActivity::kVadUnknown;
    audio_frame->num_channels_ = dest_channels;
  }

  return audio_frame;
}

std::unique_ptr<webrtc::AudioFrame>
AudioResampleUtils::Resample(const webrtc::AudioFrame &frame, int dest_channels,
                             int dest_sample_rate,
                             webrtc::PushResampler<int16_t> *resampler) {
  const int src_channels = static_cast<int>(frame.num_channels_);
  const int src_sample_rate = frame.sample_rate_hz_;
  const int16_t *data = frame.data();
  const size_t samples_per_channel = frame.samples_per_channel_;

  auto audio_frame = std::make_unique<webrtc::AudioFrame>();

  int samples_out = Resample(src_channels, src_sample_rate, dest_channels,
                             dest_sample_rate, data, samples_per_channel,
                             resampler, audio_frame->mutable_data());

  audio_frame->timestamp_ = 0;
  audio_frame->samples_per_channel_ = samples_out / dest_channels;
  audio_frame->sample_rate_hz_ = dest_sample_rate;
  audio_frame->speech_type_ = webrtc::AudioFrame::SpeechType::kNormalSpeech;
  audio_frame->vad_activity_ = webrtc::AudioFrame::VADActivity::kVadUnknown;
  audio_frame->num_channels_ = dest_channels;

  return audio_frame;
}

int AudioResampleUtils::ExpandChannels(const size_t samples_per_channel,
                                       const size_t source_num_channels,
                                       const size_t expand_to_num_channels,
                                       int16_t *data) {
  if (expand_to_num_channels == 0 || source_num_channels == 0 ||
      expand_to_num_channels == source_num_channels) {
    return 0;
  }

  if (expand_to_num_channels > source_num_channels) {
    for (int i = samples_per_channel - 1; i >= 0; --i) {
      for (int j = 0; j < expand_to_num_channels; ++j) {
        data[expand_to_num_channels * i + j] =
            data[source_num_channels * i + j % source_num_channels];
      }
    }
  } else {
    for (int i = 0; i < samples_per_channel; ++i) {
      for (int j = 0; j < expand_to_num_channels; ++j) {
        data[expand_to_num_channels * i + j] =
            data[source_num_channels * i + j % source_num_channels];
      }
    }

    int16_t *memsetData = &data[samples_per_channel * expand_to_num_channels];
    size_t len = (source_num_channels - expand_to_num_channels) *
                 samples_per_channel * sizeof(int16_t);
    memset(memsetData, 0, len);
  }

  return (static_cast<int64_t>(expand_to_num_channels) -
          static_cast<int64_t>(source_num_channels)) *
         samples_per_channel;
}
