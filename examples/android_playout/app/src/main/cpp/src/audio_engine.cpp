
#include "audio_engine.h"

#include "android_debug.h"

namespace awaudio {

AudioEngine::AudioEngine() : audio_layer_(AudioLayer::kPlatformDefaultAudio) {
}

AudioEngine::~AudioEngine() {
  DeleteAudioPlayer();
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

bool AudioEngine::CreateAudioPlayer(int streamType) {
  if (!audio_manager_) {
    LOGW("Audio engine has not been initialized!");
    return false;
  }
  if (audio_player_) {
    LOGW("Audio player is existing!");
    return true;
  }
  auto audio_player = awaudio::AudioPlayerInterface::Create(audio_layer_,audio_manager_.get());
  if (!audio_player) {
    LOGW("Create audio player with layer %d failed !", static_cast<int>(audio_layer_));
    return false;
  }
  audio_player->SetSpeakerStreamType(streamType);
  if ((audio_player->Init() != 0) || (audio_player->InitPlayout() != 0)) {
    LOGW("Init audio player with layer %d failed !", static_cast<int>(audio_layer_));
    return false;
  }

  audio_player_ = std::move(audio_player);
  return true;
}

void AudioEngine::DeleteAudioPlayer() {
  if (audio_player_) {
    audio_player_->SetDataCallback(nullptr);
    audio_player_data_callback_.reset();
    if (audio_player_->Playing()) {
      audio_player_->StopPlayout();
    }
    if (audio_player_->Initialized()) {
      audio_player_->Terminate();
    }
    audio_player_.reset();
  }
}

void AudioEngine::SetAudioPlayerDataCallback(std::shared_ptr<awaudio::AudioPlayerDataCallback> callback) {
  audio_player_data_callback_ = callback;
  if (!audio_player_) {
    return;
  }
  if (!audio_player_->PlayoutIsInitialized()) {
    return;
  }
  audio_player_->SetDataCallback(audio_player_data_callback_.get());
}

bool AudioEngine::StartPlayout() {
  if (!audio_player_) {
    return false;
  }
  return audio_player_->StartPlayout() == 0;
}

bool AudioEngine::SetPlayoutVolume(int volume) {
  if (!audio_player_) {
    return false;
  }
  return audio_player_->SetSpeakerVolume(static_cast<uint32_t>(volume)) == 0;
}

}  // namespace awaudio