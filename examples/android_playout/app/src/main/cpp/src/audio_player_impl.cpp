
#include "audio_manager.h"
#include "audio_track_jni.h"
#include "base/checks.h"
#include "base/logging.h"
#include "include/audio_device.h"

namespace awaudio {

template <class OutputType>
class AudioPlayerTemplate : public AudioPlayerInterface {
public:
  AudioPlayerTemplate(AudioLayer audio_layer,
                      AudioManager* audio_manager)
      : audio_layer_(audio_layer),
        audio_manager_(audio_manager),
        output_(audio_manager_),
        initialized_(false) {
    RTC_LOG(LS_INFO) << __FUNCTION__;
    RTC_CHECK(audio_manager);
  }

  virtual ~AudioPlayerTemplate() { RTC_LOG(LS_INFO) << __FUNCTION__; }

  int32_t ActiveAudioLayer(AudioLayer& audioLayer) const override {
    RTC_LOG(LS_INFO) << __FUNCTION__;
    audioLayer = audio_layer_;
    return 0;
  }

  int32_t Init() override {
    RTC_LOG(LS_INFO) << __FUNCTION__;
//    RTC_DCHECK(thread_checker_.IsCurrent());
    RTC_DCHECK(!initialized_);

    if (output_.Init() != 0) {
      return -1;
    }
    initialized_ = true;
    return 0;
  }

  int32_t Terminate() override {
    RTC_LOG(LS_INFO) << __FUNCTION__;
//    RTC_DCHECK(thread_checker_.IsCurrent());
    int32_t err = output_.Terminate();
    initialized_ = false;
    RTC_DCHECK_EQ(err, 0);
    return err;
  }

  bool Initialized() const override {
    RTC_LOG(LS_INFO) << __FUNCTION__;
//    RTC_DCHECK(thread_checker_.IsCurrent());
    return initialized_;
  }

  int32_t PlayoutIsAvailable(bool& available) override {
    RTC_LOG(LS_INFO) << __FUNCTION__;
    available = true;
    return 0;
  }

  int32_t InitPlayout() override {
    RTC_LOG(LS_INFO) << __FUNCTION__;
    return output_.InitPlayout();
  }

  bool PlayoutIsInitialized() const override {
    RTC_LOG(LS_INFO) << __FUNCTION__;
    return output_.PlayoutIsInitialized();
  }

  int32_t StartPlayout() override {
    RTC_LOG(LS_INFO) << __FUNCTION__;
    if (!audio_manager_->IsCommunicationModeEnabled()) {
      RTC_LOG(LS_WARNING)
          << "The application should use MODE_IN_COMMUNICATION audio mode!";
    }
    return output_.StartPlayout();
  }

  int32_t StopPlayout() override {
    // Avoid using audio manger (JNI/Java cost) if playout was inactive.
    if (!Playing())
      return 0;
    RTC_LOG(LS_INFO) << __FUNCTION__;
    int32_t err = output_.StopPlayout();
    return err;
  }

  bool Playing() const override {
    RTC_LOG(LS_INFO) << __FUNCTION__;
    return output_.Playing();
  }

  int SetSpeakerStreamType(int streamType) override {
    return output_.SetSpeakerStreamType(streamType);
  }

  int32_t SpeakerVolumeIsAvailable(bool& available) override {
    RTC_LOG(LS_INFO) << __FUNCTION__;
    return output_.SpeakerVolumeIsAvailable(available);
  }

  int32_t SetSpeakerVolume(uint32_t volume) override {
    RTC_LOG(LS_INFO) << __FUNCTION__;
    return output_.SetSpeakerVolume(volume);
  }

  int32_t SpeakerVolume(uint32_t& volume) const override {
    RTC_LOG(LS_INFO) << __FUNCTION__;
    return output_.SpeakerVolume(volume);
  }

  int32_t MaxSpeakerVolume(uint32_t& maxVolume) const override {
    RTC_LOG(LS_INFO) << __FUNCTION__;
    return output_.MaxSpeakerVolume(maxVolume);
  }

  int32_t MinSpeakerVolume(uint32_t& minVolume) const override {
    RTC_LOG(LS_INFO) << __FUNCTION__;
    return output_.MinSpeakerVolume(minVolume);
  }

  // Returns true if the audio manager has been configured to support stereo
  // and false otherwised. Default is mono.
  int32_t StereoPlayoutIsAvailable(bool& available) override {
    RTC_LOG(LS_INFO) << __FUNCTION__;
    available = audio_manager_->IsStereoPlayoutSupported();
    return 0;
  }

  int32_t SetStereoPlayout(bool enable) override {
    RTC_LOG(LS_INFO) << __FUNCTION__;
    bool available = audio_manager_->IsStereoPlayoutSupported();
    // Android does not support changes between mono and stero on the fly.
    // Instead, the native audio layer is configured via the audio manager
    // to either support mono or stereo. It is allowed to call this method
    // if that same state is not modified.
    return (enable == available) ? 0 : -1;
  }

  int32_t StereoPlayout(bool& enabled) const override {
    enabled = audio_manager_->IsStereoPlayoutSupported();
    return 0;
  }

  int32_t PlayoutDelay(uint16_t& delay_ms) const override {
    // Best guess we can do is to use half of the estimated total delay.
    delay_ms = audio_manager_->GetDelayEstimateInMilliseconds() / 2;
    RTC_DCHECK_GT(delay_ms, 0);
    return 0;
  }

  void SetDataCallback(AudioPlayerDataCallback *data_callback) override {
    RTC_LOG(LS_INFO) << __FUNCTION__;
    output_.SetDataCallback(data_callback);
  }

private:
  // Local copy of the audio layer set during construction of the
  // AudioPlayerTemplate instance. Read only value.
  const AudioLayer audio_layer_;

  // Non-owning raw pointer to AudioManager instance given to use at
  // construction. The real object is owned by engine and the
  // life time is the same as that of the engine, hence there
  // is no risk of reading a NULL pointer at any time in this class.
  AudioManager* const audio_manager_;

  OutputType output_;

  bool initialized_;
};

std::unique_ptr<AudioPlayerInterface> AudioPlayerInterface::Create(AudioLayer audio_layer,
                                                                   AudioManager *audio_manager) {
  std::unique_ptr<AudioPlayerInterface> audio_player;
  if (audio_layer == kAndroidJavaAudio) {
    audio_player = std::make_unique<AudioPlayerTemplate<AudioTrackJni>>(audio_layer, audio_manager);
  }
  return audio_player;
}

}  // namespace awaudio