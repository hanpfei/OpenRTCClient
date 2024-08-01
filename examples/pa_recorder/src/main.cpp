
#include <signal.h>
#include <stdio.h>

#include "pa_recorder.h"
#include "audio_wav_file_writer.h"

static bool stopped = false;

void sig_handler(int sig) {
    printf("Caught signal %d\n", sig);

    stopped = true;
}

int main(int argc, char *argv[]) {
  uint8_t databuf[8192] = {0};

  size_t nChannels = 2;
  uint32_t sampleRateHz = 48000;

  if (argc < 2) {
    printf("Usage: %s [wav_file_path]\n", argv[0]);
    return -1;
  }

  printf("File path: %s\n", argv[1]);
  auto wav_file_writer = AudioWavFileWriter::CreateFileWrite(argv[1], nChannels, sampleRateHz);

  auto recorder = PulseAudioRecorder::create(nChannels, sampleRateHz);
  if (!recorder) {
    fprintf(stderr, "Create player failed.\n");
    return -1;
  }

  signal(SIGINT, sig_handler);

  AudioPcmDataInfo audioFrameInfo;
  audioFrameInfo.samplesOut = sizeof(databuf) / sizeof(int16_t);
  while (!stopped) {
    recorder->readData(databuf, sizeof(databuf));
    wav_file_writer->writeWavAudioPcmData(databuf, audioFrameInfo);
  }

  wav_file_writer->postWriteWavFile();

  return 0;
}
