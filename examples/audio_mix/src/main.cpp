
#include <stdio.h>
#include <stdlib.h>

#include "api/audio/audio_mixer.h"
#include "modules/audio_mixer/audio_mixer_impl.h"

#include "audio_mixer_source_wav_file.h"
#include "audio_resample_utils.h"
#include "pulseaudio_player.h"
#include "wav_pcm_file_parser.h"

static bool Eof(const std::vector<std::shared_ptr<AudioMixerSourceWavFile>>
                    &mixer_sources) {
  for (auto source = mixer_sources.begin(); source != mixer_sources.end();
       ++source) {
    if (!(*source)->IsEof()) {
      return false;
    }
  }
  return true;
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    printf("Usage: %s file1 file2 . . . \n", argv[0]);
    return -1;
  }

  size_t target_channels = 2;

  rtc::scoped_refptr<webrtc::AudioMixer> audio_mixer =
      webrtc::AudioMixerImpl::Create();

  std::vector<std::shared_ptr<AudioMixerSourceWavFile>> mixer_sources;

  printf("In files: \n");
  for (int i = 1; i < argc; ++i) {
    printf("    %s\n", argv[i]);
    auto source = AudioMixerSourceWavFile::Create(argv[i]);
    if (!source) {
      printf("Invalid WAV audio file format %s\n", argv[i]);
      continue;
    }
    audio_mixer->AddSource(source.get());
    mixer_sources.emplace_back(std::move(source));
  }

  webrtc::AudioFrame audio_frame_for_mixing;
  audio_mixer->Mix(target_channels, &audio_frame_for_mixing);

  auto player = PulseAudioPlayer::create(
      target_channels, audio_frame_for_mixing.sample_rate_hz());
  if (!player) {
    fprintf(stderr, "Create player failed.\n");
    return -1;
  }
  fprintf(stderr, "Output sample rate %d.\n", audio_frame_for_mixing.sample_rate_hz());

  player->writeData(audio_frame_for_mixing.data(),
                    audio_frame_for_mixing.samples_per_channel() *
                        audio_frame_for_mixing.num_channels() *
                        sizeof(int16_t));

  while (!Eof(mixer_sources)) {
    audio_mixer->Mix(target_channels, &audio_frame_for_mixing);
    player->writeData(audio_frame_for_mixing.data(),
                      audio_frame_for_mixing.samples_per_channel() *
                          audio_frame_for_mixing.num_channels() *
                          sizeof(int16_t));
  }

  return 0;
}
