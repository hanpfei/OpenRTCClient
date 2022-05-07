#pragma once

#include "media_demuxer.h"

class IAudiorameProcessing {
public:
  virtual ~IAudiorameProcessing();
  virtual void processAudioFrame(AVFrame *frame) = 0;
};

class AudioEncodedFrameDecoder: public IAudioPacketProcessing {
public:
  AudioEncodedFrameDecoder();
  virtual ~AudioEncodedFrameDecoder();

  int initialize(AVCodecParameters *codecpar);
  int close();
  int flush();

  int setVideoFrameProcessing(std::shared_ptr<IAudiorameProcessing> processing);

public:
  void processAudioPacket(AVPacket *audio_packet, AVStream *src_stream)
      override;

private:
  int decode(AVCodecContext *avCodecContext, AVPacket *pkt, AVFrame *avFrame);

private:
  std::mutex lock_;
  const AVCodec *audio_codec_;
  AVCodecContext *audio_codec_context_;
  AVFrame *audio_frame_;

  std::shared_ptr<IAudiorameProcessing> frame_processing_;
};

