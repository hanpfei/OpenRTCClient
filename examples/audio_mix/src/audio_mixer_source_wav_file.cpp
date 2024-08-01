
#include "audio_mixer_source_wav_file.h"

#include "audio/remix_resample.h"

static std::atomic<int> wav_file_mixer_source_id = {0};

static std::unique_ptr<WavPcmFileParser>
createWavPcmFileParser(const char *filepath) {
  std::unique_ptr<WavPcmFileParser> parser(new WavPcmFileParser(filepath));
  return parser;
}

static uint32_t supported_rates[] = {32000, 44100, 48000, 64000, 88200, 96000};

static bool check_sample_rate(uint32_t sample_rate) {
  for (size_t i = 0; i < sizeof(supported_rates) / sizeof(supported_rates[0]);
       ++i) {
    if (sample_rate == supported_rates[i]) {
      return true;
    }
  }
  return false;
}

void reset_audio_frame(webrtc::AudioFrame *audio_frame) {
  audio_frame->Mute();
  audio_frame->mutable_data();
}

std::unique_ptr<AudioMixerSourceWavFile>
AudioMixerSourceWavFile::Create(const char *file_path) {
  int sample_rate;
  uint32_t channels;

  auto wav_file_parser = createWavPcmFileParser(file_path);
  if (!wav_file_parser->open()) {
    printf("Open wav pcm file parser for %s failed.\n", file_path);
    return NULL;
  }

  if (wav_file_parser->getBitsPerSample() != 16) {
    fprintf(stderr, "Unsupported file %s.\n", file_path);
    return NULL;
  }

  sample_rate = wav_file_parser->getSampleRate();
  if (!check_sample_rate(sample_rate)) {
    fprintf(stderr, "Invalid sample rate: %u.\n", sample_rate);
    return NULL;
  }

  channels = wav_file_parser->getNumberOfChannels();
  if (channels > 4) {
    fprintf(stderr, "Invalid channels %u.\n", channels);
    return NULL;
  }

  return std::make_unique<AudioMixerSourceWavFile>(std::move(wav_file_parser));
}

AudioMixerSourceWavFile::AudioMixerSourceWavFile(
    std::unique_ptr<WavPcmFileParser> &&wav_file_parser)
    : id_(wav_file_mixer_source_id.fetch_add(1)), sample_rate_(), channels_(),
      wav_file_parser_(std::move(wav_file_parser)) {
  sample_rate_ = wav_file_parser_->getSampleRate();
  channels_ = wav_file_parser_->getNumberOfChannels();
}

AudioMixerSourceWavFile::~AudioMixerSourceWavFile() {}

webrtc::AudioMixer::Source::AudioFrameInfo
AudioMixerSourceWavFile::GetAudioFrameWithInfo(
    int sample_rate_hz, webrtc::AudioFrame *audio_frame) {
  int length;
  size_t samples_per_channel;

  reset_audio_frame(audio_frame);

  if (!wav_file_parser_->hasNext()) {
    return webrtc::AudioMixer::Source::AudioFrameInfo::kMuted;
  }

  samples_per_channel = sample_rate_ / 100;
  length = samples_per_channel * channels_ * sizeof(int16_t);

  audio_frame_.UpdateFrame(0, nullptr, samples_per_channel, sample_rate_,
                           webrtc::AudioFrame::SpeechType::kNormalSpeech,
                           webrtc::AudioFrame::VADActivity::kVadUnknown,
                           channels_);

  wav_file_parser_->getNext(
      reinterpret_cast<char *>(audio_frame_.mutable_data()), &length);

  if (sample_rate_hz != sample_rate_) {
    audio_frame->sample_rate_hz_ = sample_rate_hz;
    audio_frame->num_channels_ = wav_file_parser_->getNumberOfChannels();

    webrtc::voe::RemixAndResample(audio_frame_, &resampler_, audio_frame);
  } else {
    audio_frame->CopyFrom(audio_frame_);
  }

  return webrtc::AudioMixer::Source::AudioFrameInfo::kNormal;
}

// A way for a mixer implementation to distinguish participants.
int AudioMixerSourceWavFile::Ssrc() const { return id_; }

// A way for this source to say that GetAudioFrameWithInfo called
// with this sample rate or higher will not cause quality loss.
int AudioMixerSourceWavFile::PreferredSampleRate() const {
  return sample_rate_;
}

bool AudioMixerSourceWavFile::IsEof() const {
  return !wav_file_parser_->hasNext();
}
