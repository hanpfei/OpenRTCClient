
#include <stdio.h>

#include "pulseaudio_player.h"
#include "wav_pcm_file_parser.h"

static std::unique_ptr<WavPcmFileParser>
createWavPcmFileParser(const char *filepath) {
  std::unique_ptr<WavPcmFileParser> parser(new WavPcmFileParser(filepath));
  return parser;
}

int main(int argc, char *argv[]) {
  uint8_t databuf[8192] = {0};
  int length = 8192;

  if (argc < 2) {
    printf("Usage: %s [wav_file_path]\n", argv[0]);
    return -1;
  }

  printf("File path: %s\n", argv[1]);
  auto wav_file_parser = createWavPcmFileParser(argv[1]);
  if (!wav_file_parser->open()) {
    printf("Usage: %s [wav_file_path]\n", argv[0]);
    return -1;
  }

  if (wav_file_parser->getBitsPerSample() != 16) {
    fprintf(stderr, "Unsupported file.\n");
    return -1;
  }

  auto player = PulseAudioPlayer::create(wav_file_parser->getNumberOfChannels(),
                                         wav_file_parser->getSampleRate());
  if (!player) {
    fprintf(stderr, "Create player failed.\n");
    return -1;
  }

  while (wav_file_parser->hasNext()) {
    length = 8192;
    wav_file_parser->getNext(reinterpret_cast<char *>(databuf), &length);
    if (length > 0) {
      player->writeData(databuf, length);
    }
  }

  return 0;
}
