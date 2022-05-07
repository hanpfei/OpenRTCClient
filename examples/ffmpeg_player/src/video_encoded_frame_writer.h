#pragma once

#include <string>

#include "media_demuxer.h"

class VideoEncodedFrameWriter: public IVideoPacketProcessing {
public:
  VideoEncodedFrameWriter();
  virtual ~VideoEncodedFrameWriter();

  int initialize(const char *dstPath, AVCodecParameters *codecpar);
  int close();

public:
  void processVideoPacket(AVPacket *video_packet, AVStream *src_stream)
      override;

private:
  std::string file_path_;

  AVFormatContext *format_context_;
  AVOutputFormat *out_format_;
  AVStream *out_stream_;
};

