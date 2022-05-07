#include "media_demuxer.h"

#include "log.h"

IVideoPacketProcessing::~IVideoPacketProcessing() = default;
IAudioPacketProcessing::~IAudioPacketProcessing() = default;

MediaDemuxer::MediaDemuxer() :
    format_context_(nullptr), video_index_(-1), video_stream_(nullptr), video_codecpar_(
        nullptr), audio_index_(-1), audio_stream_(nullptr), audio_codecpar_(
        nullptr) {
}

MediaDemuxer::~MediaDemuxer() {
  close();
}

int MediaDemuxer::open(const char *url) {
  if (!url || *url == 0) {
    LOGW("Invalid url.");
    return -1;
  }
  url_ = url;

  //Initialize format_context_
  int ret = avformat_open_input(&format_context_, url, nullptr, nullptr);
  if (ret < 0) {
    LOGI("avformat_open_input %s failed：%s", url, av_err2str(ret));
    return -1;
  }

  //video_index
  video_index_ = av_find_best_stream(format_context_, AVMEDIA_TYPE_VIDEO, -1,
      -1, nullptr, 0);
  if (video_index_ < 0) {
    LOGI("Find video stream failed: %s", av_err2str(video_index_));
    return -1;
  }

  //in_stream、in_codecpar
  video_stream_ = format_context_->streams[video_index_];
  video_codecpar_ = video_stream_->codecpar;
  if (video_codecpar_->codec_type != AVMEDIA_TYPE_VIDEO) {
    LOGI("The video codec type is invalid!");
    return -1;
  }

  /* Initialize audio */
  // Find stream index of audio stream
  audio_index_ = av_find_best_stream(format_context_, AVMEDIA_TYPE_AUDIO, -1,
      -1, nullptr, 0);
  if (audio_index_ < 0) {
    LOGW("Find audio stream failed: %s", av_err2str(audio_index_));
    return -1;
  }

  // Get audio stream and audio codec parameters
  audio_stream_ = format_context_->streams[audio_index_];
  audio_codecpar_ = audio_stream_->codecpar;
  if (audio_codecpar_->codec_type != AVMEDIA_TYPE_AUDIO) {
    LOGW("The audio codec type is invalid!");
    return -1;
  }

  // Initialize packet
  //initialize packet
  av_init_packet(&packet_);
  packet_.data = nullptr;
  packet_.size = 0;

  return 0;
}

int MediaDemuxer::demuxOneFrame() {
  std::lock_guard<std::mutex> _l(lock_);
  if (av_read_frame(format_context_, &packet_) == 0) {
    if (packet_.stream_index == video_index_) {
      if (video_packet_processing_) {
        video_packet_processing_->processVideoPacket(&packet_, video_stream_);
      }
    } else if (packet_.stream_index == audio_index_) {
      if (audio_packet_processing_) {
        audio_packet_processing_->processAudioPacket(&packet_, audio_stream_);
      }
    }
  } else {
    return -1;
  }

  return 0;
}

int MediaDemuxer::seek(float position) {
  std::lock_guard<std::mutex> _l(lock_);
  if (video_stream_) {
    auto target_video_frame = static_cast<int64_t>(video_stream_->duration
        * position);
    LOGW("Video duration %" PRId64 ", target video position %" PRId64 "",
        video_stream_->duration, target_video_frame);
    avformat_seek_file(format_context_, video_index_, 0, target_video_frame,
        target_video_frame, AVSEEK_FLAG_FRAME);
  }
  if (audio_stream_) {
    auto target_audio_frame = static_cast<int64_t>(audio_stream_->duration
        * position);
    LOGW("Audio duration %" PRId64 ", target audio duration %" PRId64 "",
        audio_stream_->duration, target_audio_frame);
    avformat_seek_file(format_context_, audio_index_, 0, target_audio_frame,
        target_audio_frame, AVSEEK_FLAG_FRAME);
  }

  return 0;
}

int MediaDemuxer::close() {
  if (format_context_) {
    avformat_close_input(&format_context_);
    format_context_ = nullptr;
  }
  return 0;
}

int MediaDemuxer::setVideoPacketProcessing(
    std::shared_ptr<IVideoPacketProcessing> processing) {
  video_packet_processing_ = processing;
  return 0;
}

AVCodecParameters* MediaDemuxer::getVideoCodecParameters() {
  return video_codecpar_;
}

int MediaDemuxer::setAudioPacketProcessing(
    std::shared_ptr<IAudioPacketProcessing> processing) {
  audio_packet_processing_ = processing;
  return 0;
}

AVCodecParameters* MediaDemuxer::getAudioCodecParameters() {
  return audio_codecpar_;
}
