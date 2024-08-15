
#include "pulseaudio_player.h"

#include <errno.h>
#include <math.h>
#include <pulse/error.h>
#include <stdio.h>

PulseAudioPlayer::PulseAudioPlayer(int16_t channels, int32_t sampleRateHz)
    : pa_simp(NULL), numberOfChannels(channels), sampleRateHz(sampleRateHz) {}

PulseAudioPlayer::~PulseAudioPlayer() {
  if (pa_simp) {
    pa_simple_drain(pa_simp, NULL);
    pa_simple_free(pa_simp);
    pa_simp = NULL;
  }
}

bool PulseAudioPlayer::createPaSimple() {
  pa_simple *simple;
  pa_sample_spec ss = {.format = PA_SAMPLE_S16LE,
                       .rate = (uint32_t)sampleRateHz,
                       .channels = (uint8_t)numberOfChannels};

  if (!(simple = pa_simple_new(NULL, NULL, PA_STREAM_PLAYBACK, NULL, "playback",
                               &ss, NULL, NULL, &errno))) {
    fprintf(stderr, "pa_simple_new() failed: %s\n", pa_strerror(errno));
    return false;
  }
  pa_simp = simple;
  return true;
}

int PulseAudioPlayer::writeData(const void *data, size_t bytes) {
  if (!pa_simp) {
    fprintf(stderr, "writeData() failed: invalid state\n");
    return -1;
  }
  if (pa_simple_write(pa_simp, data, bytes, NULL) < 0) {
    fprintf(stderr, "pa_simple_write() failed: %s\n", pa_strerror(errno));
    return -1;
  }

  return 0;
}

std::unique_ptr<PulseAudioPlayer>
PulseAudioPlayer::create(int16_t channels, int32_t sampleRateHz) {
  auto player = std::make_unique<PulseAudioPlayer>(channels, sampleRateHz);
  if (!player->createPaSimple()) {
    return NULL;
  }
  return player;
}
