
#include "pa_recorder.h"

#include <errno.h>
#include <math.h>
#include <pulse/error.h>
#include <stdio.h>

PulseAudioRecorder::PulseAudioRecorder(int16_t channels, int32_t sampleRateHz)
    : pa_simp(NULL), numberOfChannels(channels), sampleRateHz(sampleRateHz) {}

PulseAudioRecorder::~PulseAudioRecorder() {
  if (pa_simp) {
    pa_simple_drain(pa_simp, NULL);
    pa_simple_free(pa_simp);
    pa_simp = NULL;
  }
}

bool PulseAudioRecorder::createPaSimple() {
  pa_simple *simple;
  pa_sample_spec ss = {.format = PA_SAMPLE_S16LE,
                       .rate = (uint32_t)sampleRateHz,
                       .channels = (uint8_t)numberOfChannels};

  if (!(simple = pa_simple_new(NULL, "Record", PA_STREAM_RECORD, NULL,
                               "recorder", &ss, NULL, NULL, &errno))) {
    fprintf(stderr, "pa_simple_new() failed: %s\n", pa_strerror(errno));
    return false;
  }
  pa_simp = simple;
  return true;
}

int PulseAudioRecorder::readData(void *data, size_t bytes) {
  if (!pa_simp) {
    fprintf(stderr, "writeData() failed: invalid state\n");
    return -1;
  }

  if (pa_simple_read(pa_simp, data, bytes, NULL) < 0) {
    fprintf(stderr, "pa_simple_write() failed: %s\n", pa_strerror(errno));
    return -1;
  }

  return 0;
}

std::unique_ptr<PulseAudioRecorder>
PulseAudioRecorder::create(int16_t channels, int32_t sampleRateHz) {
  auto player = std::make_unique<PulseAudioRecorder>(channels, sampleRateHz);
  if (!player->createPaSimple()) {
    return NULL;
  }
  return player;
}
