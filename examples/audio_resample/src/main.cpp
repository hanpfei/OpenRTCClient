
#include <stdio.h>
#include <stdlib.h>

#include "audio_resample_utils.h"
#include "audio_wav_file_writer.h"
#include "wav_pcm_file_parser.h"

static std::unique_ptr<WavPcmFileParser>
createWavPcmFileParser(const char *filepath) {
  std::unique_ptr<WavPcmFileParser> parser(new WavPcmFileParser(filepath));
  return parser;
}

static uint32_t supported_rates[] = {32000, 44100, 48000, 64000, 88200, 96000};

static bool check_sample_rate(uint32_t sample_rate) {
  for (size_t i = 0; i < sizeof(supported_rates) / sizeof(supported_rates[0]);
       ++i) {
    if (sample_rate == supported_rates[i]) {
      return true;
    }
  }
  return false;
}

int main(int argc, char *argv[]) {
  int length;

  char *in_file_path;
  char *out_file_path;
  uint32_t in_sample_rate;
  uint32_t out_sample_rate;
  uint32_t channels;

  int16_t in_data[7680] = {0};
  int16_t out_data[7680] = {0};

  int total_samples;

  if (argc < 4) {
    printf("Usage: %s [input_wav_file_path] [output_wav_file_path] "
           "[out_sample_rate]\n",
           argv[0]);
    return -1;
  }

  in_file_path = argv[1];
  out_file_path = argv[2];
  out_sample_rate = atoi(argv[3]);

  printf("In file path: %s, out file path %s, out sample rate %d\n",
         in_file_path, out_file_path, out_sample_rate);

  auto wav_file_parser = createWavPcmFileParser(argv[1]);
  if (!wav_file_parser->open()) {
	    printf("Usage: %s [input_wav_file_path] [output_wav_file_path] "
	           "[out_sample_rate]\n",
	           argv[0]);
    return -1;
  }

  if (wav_file_parser->getBitsPerSample() != 16) {
    fprintf(stderr, "Unsupported file.\n");
    return -1;
  }

  in_sample_rate = wav_file_parser->getSampleRate();
  if ((!check_sample_rate(in_sample_rate)) ||
      (!check_sample_rate(out_sample_rate))) {
    fprintf(stderr, "Invalid sample rate: in %u, out %u.\n", in_sample_rate,
            out_sample_rate);
    return -1;
  }

  channels = wav_file_parser->getNumberOfChannels();
  if (channels > 4) {
    fprintf(stderr, "Invalid channels %u.\n", channels);
    return -1;
  }

  auto writer = AudioWavFileWriter::CreateFileWrite(out_file_path, channels,
                                                    out_sample_rate);
  if (!writer) {
    fprintf(stderr, "Create wav file writer for %s failed.\n", out_file_path);
    return -1;
  }

  auto resampler = std::make_unique<webrtc::PushResampler<int16_t>>();

  AudioPcmDataInfo audioFrameInfo;

  while (wav_file_parser->hasNext()) {
    length = in_sample_rate / 100 * channels * sizeof(int16_t);
    wav_file_parser->getNext(reinterpret_cast<char *>(in_data), &length);
    if (length > 0) {
      total_samples = AudioResampleUtils::Resample(
          channels, in_sample_rate, channels, out_sample_rate, in_data,
          length / (channels * sizeof(int16_t)), resampler.get(), out_data);

      audioFrameInfo.samplesOut = total_samples;
      writer->writeWavAudioPcmData(out_data, audioFrameInfo);
    }
  }

  writer->postWriteWavFile();

  return 0;
}
