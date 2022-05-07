#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
}

#include "media_demuxer.h"

class IVideoFrameProcessing {
public:
  virtual ~IVideoFrameProcessing();
  virtual void processVideoFrame(AVFrame *frame) = 0;
};

class VideoEncodedFrameDecoder: public IVideoPacketProcessing {
public:
  VideoEncodedFrameDecoder();
  virtual ~VideoEncodedFrameDecoder();

  int initialize(AVCodecParameters *codecpar);
  int close();
  int flush();
  int setVideoFrameProcessing(
      std::shared_ptr<IVideoFrameProcessing> processing);
  AVCodecContext* getVideoCodecContext();

public:
  void processVideoPacket(AVPacket *video_packet, AVStream *src_stream)
      override;

private:
  int decodeLocked(AVCodecContext *avCodecContext, AVFrame *avFrame,
      AVPacket *pkt);

private:
  std::mutex lock_;
  AVCodec *video_codec_;
  AVCodecContext *av_codec_context_;
  AVFrame *av_frame_;

  std::shared_ptr<IVideoFrameProcessing> frame_processing_;
};

