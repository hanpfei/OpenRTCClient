#pragma once

#include <memory>

#include "audio_manager.h"

namespace awaudio {

class AudioEngine {
public:
  AudioEngine();
  ~AudioEngine();

  bool Initialize(AudioLayer audioLayer);

  bool CreateAudioPlayer(int streamType);
  void DeleteAudioPlayer();

  void SetAudioPlayerDataCallback(std::shared_ptr<awaudio::AudioPlayerDataCallback> callback);

  bool StartPlayout();

  bool SetPlayoutVolume(int volume);

private:
  AudioLayer audio_layer_;
  std::unique_ptr<awaudio::AudioManager> audio_manager_;

  std::shared_ptr<awaudio::AudioPlayerDataCallback> audio_player_data_callback_;
  std::unique_ptr<awaudio::AudioPlayerInterface> audio_player_;

  std::shared_ptr<awaudio::AudioRecorderDataCallback> audio_recorder_data_callback_;
  std::unique_ptr<awaudio::AudioRecorderInterface> audio_recorder_;
};

}  // namespace awaudio