#include "video_encoded_frame_decoder.h"

#include "log.h"

IVideoFrameProcessing::~IVideoFrameProcessing() = default;

VideoEncodedFrameDecoder::VideoEncodedFrameDecoder() :
    video_codec_(nullptr), av_codec_context_(nullptr), av_frame_(nullptr) {
}

VideoEncodedFrameDecoder::~VideoEncodedFrameDecoder() {
  close();
}

int VideoEncodedFrameDecoder::initialize(AVCodecParameters *codecpar) {
  // Find the video codec
  video_codec_ = avcodec_find_decoder(codecpar->codec_id);
  if (!video_codec_) {
    LOGI("Codec for %d not found", static_cast<int>(codecpar->codec_id));
    return -1;
  }

  av_codec_context_ = avcodec_alloc_context3(video_codec_);
  if (!av_codec_context_) {
    LOGI("Could not allocate avCodecContext\n");
    return -1;
  }

  /* For some codecs, such as msmpeg4 and mpeg4, width and height
   MUST be initialized there because this information is not
   available in the bitstream. */
  avcodec_parameters_to_context(av_codec_context_, codecpar);

  /* open it */
  if (avcodec_open2(av_codec_context_, video_codec_, nullptr) < 0) {
    LOGI("Could not open avCodec\n");
    return -1;
  }

  av_frame_ = av_frame_alloc();
  if (!av_frame_) {
    LOGI("Could not allocate video avFrame\n");
    return -1;
  }

  return 0;
}

int VideoEncodedFrameDecoder::close() {
  if (av_codec_context_) {
    avcodec_free_context(&av_codec_context_);
    av_codec_context_ = nullptr;
  }
  if (av_frame_) {
    av_frame_free(&av_frame_);
    av_frame_ = nullptr;
  }
  return 0;
}

int VideoEncodedFrameDecoder::flush() {
  std::lock_guard<std::mutex> _l(lock_);
  if (av_codec_context_) {
    avcodec_flush_buffers(av_codec_context_);
  }
  return 0;
}

int VideoEncodedFrameDecoder::setVideoFrameProcessing(
    std::shared_ptr<IVideoFrameProcessing> processing) {
  frame_processing_ = processing;
  return 0;
}

AVCodecContext* VideoEncodedFrameDecoder::getVideoCodecContext() {
  return av_codec_context_;
}

void VideoEncodedFrameDecoder::processVideoPacket(AVPacket *video_packet,
    AVStream *src_stream) {
  if (video_packet->size) {
    std::lock_guard<std::mutex> _l(lock_);
    if (decodeLocked(av_codec_context_, av_frame_, video_packet) == 0) {
      if (frame_processing_) {
        frame_processing_->processVideoFrame(av_frame_);
      }
    }
  }
}

// Decode video frame from packet.
int VideoEncodedFrameDecoder::decodeLocked(AVCodecContext *avCodecContext,
    AVFrame *avFrame, AVPacket *pkt) {
  // Send packet to codec
  int ret = avcodec_send_packet(av_codec_context_, pkt);
  if (ret < 0) {
    LOGW("Error sending a packet for decoding: %s\n", av_err2str(ret));
    return -1;
  }

  // Decode video frame and save it into avFrame
  ret = avcodec_receive_frame(av_codec_context_, avFrame);
  if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
    return -1;
  } else if (ret < 0) {
    LOGI("Error during decoding\n");
    return -1;
  }

  if (av_codec_context_->frame_number % 100 == 0) {
    LOGI("Saving avFrame %3d\n", av_codec_context_->frame_number);
  }

  return 0;
}

