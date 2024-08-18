
#include "audio_engine.h"

#include "android_debug.h"

namespace awaudio {

AudioEngine::AudioEngine() : audio_layer_(AudioLayer::kPlatformDefaultAudio) {
}

AudioEngine::~AudioEngine() {
  DeleteAudioRecorder();
  if (audio_manager_) {
    audio_manager_->Close();
    audio_manager_.reset();
  }
}

bool AudioEngine::Initialize(AudioLayer audioLayer) {
  auto audio_manager = std::make_unique<awaudio::AudioManager>();
  audio_manager->SetActiveAudioLayer(audioLayer);
  if (!audio_manager->Init()) {
    LOGW("Create audio engine failed!");
    return false;
  }

  audio_manager_ = std::move(audio_manager);
  audio_layer_ = audioLayer;
  return true;
}

bool AudioEngine::CreateAudioRecorder() {
      if (!audio_manager_) {
        LOGW("Audio engine has not been initialized!");
        return false;
      }
      if (audio_recorder_) {
        LOGW("Audio recorder is existing!");
        return true;
      }
      auto audio_recorder = awaudio::AudioRecorderInterface::Create(audio_layer_, audio_manager_.get());
      if (!audio_recorder) {
        LOGW("Create audio recorder with layer %d failed !", static_cast<int>(audio_layer_));
        return false;
      }
      if ((audio_recorder->Init() != 0) || (audio_recorder->InitRecording() != 0)) {
        LOGW("Init audio recorder with layer %d failed !", static_cast<int>(audio_layer_));
        return false;
      }
      audio_recorder_ = std::move(audio_recorder);
      return true;
    }

    void AudioEngine::DeleteAudioRecorder() {
      if (audio_recorder_) {
        audio_recorder_->SetDataCallback(nullptr);
        audio_recorder_data_callback_.reset();
        if (audio_recorder_->Recording()) {
          audio_recorder_->StopRecording();
        }
        if (audio_recorder_->Initialized()) {
          audio_recorder_->Terminate();
        }
        audio_recorder_.reset();
      }
    }

    void AudioEngine::SetAudioRecorderDataCallback(std::shared_ptr<awaudio::AudioRecorderDataCallback> callback) {
      audio_recorder_data_callback_ = callback;
      if (!audio_recorder_) {
        return;
      }
      if (!audio_recorder_->RecordingIsInitialized()) {
        return;
      }
      audio_recorder_->SetDataCallback(audio_recorder_data_callback_.get());
    }

    bool AudioEngine::StartRecording() {
      if (!audio_recorder_) {
        return false;
      }
      return audio_recorder_->StartRecording() == 0;
    }

}  // namespace awaudio