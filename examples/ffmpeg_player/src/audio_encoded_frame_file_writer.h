#pragma once

#include <string>

#include "media_demuxer.h"

class AudioEncodedFrameFileWriter: public IAudioPacketProcessing {
public:
  AudioEncodedFrameFileWriter();
  virtual ~AudioEncodedFrameFileWriter();

  int initialize(const char *dstPath, AVCodecParameters *codecpar);
  int close();

public:
  void processAudioPacket(AVPacket *audio_packet, AVStream *src_stream)
      override;

private:
  std::string file_path_;

  AVFormatContext *format_context_;
  AVOutputFormat *out_format_;
  AVStream *out_stream_;
};

