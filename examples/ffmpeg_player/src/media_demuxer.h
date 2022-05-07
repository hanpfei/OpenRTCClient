#pragma once

#include <memory>
#include <mutex>
#include <string>

extern "C" {
#include "libavformat/avformat.h"
}

class IVideoPacketProcessing {
public:
  virtual ~IVideoPacketProcessing();
  virtual void processVideoPacket(AVPacket *video_packet,
      AVStream *src_stream) = 0;
};

class IAudioPacketProcessing {
public:
  virtual ~IAudioPacketProcessing();
  virtual void processAudioPacket(AVPacket *audio_packet,
      AVStream *src_stream) = 0;
};

class MediaDemuxer {
public:
  MediaDemuxer();
  virtual ~MediaDemuxer();

  int open(const char *url);
  int demuxOneFrame();
  int close();
  int seek(float position);

  int setVideoPacketProcessing(std::shared_ptr<IVideoPacketProcessing>);
  AVCodecParameters* getVideoCodecParameters();

  int setAudioPacketProcessing(std::shared_ptr<IAudioPacketProcessing>);
  AVCodecParameters* getAudioCodecParameters();

private:
  AVFormatContext *format_context_;
  std::string url_;

  std::mutex lock_;
  int video_index_;
  AVStream *video_stream_;
  AVCodecParameters *video_codecpar_;

  std::shared_ptr<IVideoPacketProcessing> video_packet_processing_;

  int audio_index_;
  AVStream *audio_stream_;
  AVCodecParameters *audio_codecpar_;

  std::shared_ptr<IAudioPacketProcessing> audio_packet_processing_;

  AVPacket packet_;
};

