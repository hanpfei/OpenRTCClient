
#include "audio_manager.h"
#include "audio_record_jni.h"
#include "include/audio_device.h"
#include "base/checks.h"
#include "base/logging.h"

namespace awaudio {

template<class InputType>
class AudioRecorderTemplate : public AudioRecorderInterface {
public:
  AudioRecorderTemplate(AudioLayer audio_layer,
                        AudioManager *audio_manager)
      : audio_layer_(audio_layer),
        audio_manager_(audio_manager),
        input_(audio_manager_),
        initialized_(false) {
    RTC_LOG(LS_INFO) << __FUNCTION__;
    RTC_CHECK(audio_manager);
  }

  virtual ~AudioRecorderTemplate() { RTC_LOG(LS_INFO) << __FUNCTION__; }

  int32_t ActiveAudioLayer(AudioLayer& audioLayer) const override {
    RTC_LOG(LS_INFO) << __FUNCTION__;
    audioLayer = audio_layer_;
    return 0;
  }

  int32_t Init() override {
    RTC_LOG(LS_INFO) << __FUNCTION__;
//    RTC_DCHECK(thread_checker_.IsCurrent());
    RTC_DCHECK(!initialized_);
    if (input_.Init() != 0) {
      return -1;
    }
    initialized_ = true;
    return 0;
  }

  int32_t Terminate() override {
    RTC_LOG(LS_INFO) << __FUNCTION__;
//    RTC_DCHECK(thread_checker_.IsCurrent());
    int32_t err = input_.Terminate();
    initialized_ = false;
    RTC_DCHECK_EQ(err, 0);
    return err;
  }

  bool Initialized() const override {
    RTC_LOG(LS_INFO) << __FUNCTION__;
//    RTC_DCHECK(thread_checker_.IsCurrent());
    return initialized_;
  }

  int32_t RecordingIsAvailable(bool& available) override {
    RTC_LOG(LS_INFO) << __FUNCTION__;
    available = true;
    return 0;
  }

  int32_t InitRecording() override {
    RTC_LOG(LS_INFO) << __FUNCTION__;
    return input_.InitRecording();
  }

  bool RecordingIsInitialized() const override {
    RTC_LOG(LS_INFO) << __FUNCTION__;
    return input_.RecordingIsInitialized();
  }

  int32_t StartRecording() override {
    RTC_LOG(LS_INFO) << __FUNCTION__;
    if (!audio_manager_->IsCommunicationModeEnabled()) {
      RTC_LOG(LS_WARNING)
          << "The application should use MODE_IN_COMMUNICATION audio mode!";
    }
    return input_.StartRecording();
  }

  int32_t StopRecording() override {
    // Avoid using audio manger (JNI/Java cost) if recording was inactive.
    RTC_LOG(LS_INFO) << __FUNCTION__;
    if (!Recording())
      return 0;
    int32_t err = input_.StopRecording();
    return err;
  }

  bool Recording() const override { return input_.Recording(); }

  int32_t StereoRecordingIsAvailable(bool& available) override {
    RTC_LOG(LS_INFO) << __FUNCTION__;
    available = audio_manager_->IsStereoRecordSupported();
    return 0;
  }

  int32_t SetStereoRecording(bool enable) override {
    RTC_LOG(LS_INFO) << __FUNCTION__;
    bool available = audio_manager_->IsStereoRecordSupported();
    // Android does not support changes between mono and stero on the fly.
    // Instead, the native audio layer is configured via the audio manager
    // to either support mono or stereo. It is allowed to call this method
    // if that same state is not modified.
    return (enable == available) ? 0 : -1;
  }

  int32_t StereoRecording(bool& enabled) const override {
    RTC_LOG(LS_INFO) << __FUNCTION__;
    enabled = audio_manager_->IsStereoRecordSupported();
    return 0;
  }

  void SetDataCallback(AudioRecorderDataCallback *data_callback) {
    RTC_LOG(LS_INFO) << __FUNCTION__;
    input_.SetDataCallback(data_callback);
  }

  // Returns true if the device both supports built in AEC and the device
  // is not blacklisted.
  // Currently, if OpenSL ES is used in both directions, this method will still
  // report the correct value and it has the correct effect. As an example:
  // a device supports built in AEC and this method returns true. Libjingle
  // will then disable the WebRTC based AEC and that will work for all devices
  // (mainly Nexus) even when OpenSL ES is used for input since our current
  // implementation will enable built-in AEC by default also for OpenSL ES.
  // The only "bad" thing that happens today is that when Libjingle calls
  // OpenSLESRecorder::EnableBuiltInAEC() it will not have any real effect and
  // a "Not Implemented" log will be filed. This non-perfect state will remain
  // until I have added full support for audio effects based on OpenSL ES APIs.
  bool BuiltInAECIsAvailable() const override {
    RTC_LOG(LS_INFO) << __FUNCTION__;
    return audio_manager_->IsAcousticEchoCancelerSupported();
  }

  // TODO(henrika): add implementation for OpenSL ES based audio as well.
  int32_t EnableBuiltInAEC(bool enable) override {
    RTC_LOG(LS_INFO) << __FUNCTION__ << "(" << enable << ")";
    RTC_CHECK(BuiltInAECIsAvailable()) << "HW AEC is not available";
    return input_.EnableBuiltInAEC(enable);
  }

  // Returns true if the device both supports built in AGC and the device
  // is not blacklisted.
  // TODO(henrika): add implementation for OpenSL ES based audio as well.
  // In addition, see comments for BuiltInAECIsAvailable().
  bool BuiltInAGCIsAvailable() const override {
    RTC_LOG(LS_INFO) << __FUNCTION__;
    return audio_manager_->IsAutomaticGainControlSupported();
  }

  // TODO(henrika): add implementation for OpenSL ES based audio as well.
  int32_t EnableBuiltInAGC(bool enable) override {
    RTC_LOG(LS_INFO) << __FUNCTION__ << "(" << enable << ")";
    RTC_CHECK(BuiltInAGCIsAvailable()) << "HW AGC is not available";
    return input_.EnableBuiltInAGC(enable);
  }

  // Returns true if the device both supports built in NS and the device
  // is not blacklisted.
  // TODO(henrika): add implementation for OpenSL ES based audio as well.
  // In addition, see comments for BuiltInAECIsAvailable().
  bool BuiltInNSIsAvailable() const override {
    RTC_LOG(LS_INFO) << __FUNCTION__;
    return audio_manager_->IsNoiseSuppressorSupported();
  }

  // TODO(henrika): add implementation for OpenSL ES based audio as well.
  int32_t EnableBuiltInNS(bool enable) override {
    RTC_LOG(LS_INFO) << __FUNCTION__ << "(" << enable << ")";
    RTC_CHECK(BuiltInNSIsAvailable()) << "HW NS is not available";
    return input_.EnableBuiltInNS(enable);
  }

private:
  // Local copy of the audio layer set during construction of the
  // AudioPlayerTemplate instance. Read only value.
  const AudioLayer audio_layer_;

  // Non-owning raw pointer to AudioManager instance given to use at
  // construction. The real object is owned by engine and the
  // life time is the same as that of the engine, hence there
  // is no risk of reading a NULL pointer at any time in this class.
  AudioManager *const audio_manager_;

  InputType input_;

  bool initialized_;
};

std::unique_ptr<AudioRecorderInterface> AudioRecorderInterface::Create(AudioLayer audio_layer,
                                                                       AudioManager *audio_manager) {
  std::unique_ptr<AudioRecorderInterface> audio_recorder;
  if (audio_layer == kAndroidJavaAudio) {
    audio_recorder = std::make_unique<AudioRecorderTemplate<AudioRecordJni>>(audio_layer, audio_manager);
  }
  return audio_recorder;
}

}  // namespace awaudio