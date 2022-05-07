#include "audio_encoded_frame_decoder.h"

#include "log.h"

IAudiorameProcessing::~IAudiorameProcessing() = default;

AudioEncodedFrameDecoder::AudioEncodedFrameDecoder() :
    audio_codec_(nullptr), audio_codec_context_(nullptr), audio_frame_(nullptr) {
}

AudioEncodedFrameDecoder::~AudioEncodedFrameDecoder() {
  close();
}

int AudioEncodedFrameDecoder::initialize(AVCodecParameters *codecpar) {
  audio_codec_ = avcodec_find_decoder(codecpar->codec_id);
  if (!audio_codec_) {
    LOGW("Codec not found: %d", static_cast<int>(codecpar->codec_id));
    return -1;
  }

  audio_codec_context_ = avcodec_alloc_context3(audio_codec_);
  if (!audio_codec_context_) {
    LOGW("Could not allocate avCodecContext");
    return -1;
  }

  avcodec_parameters_to_context(audio_codec_context_, codecpar);

  if (avcodec_open2(audio_codec_context_, audio_codec_, nullptr) < 0) {
    LOGW("Could not open avCodec");
    return -1;
  }

  return 0;
}

int AudioEncodedFrameDecoder::close() {
  if (audio_codec_context_) {
    avcodec_free_context(&audio_codec_context_);
    audio_codec_context_ = nullptr;
  }
  if (audio_frame_) {
    av_frame_free(&audio_frame_);
    audio_frame_ = nullptr;
  }
  return 0;
}

int AudioEncodedFrameDecoder::flush() {
  std::lock_guard<std::mutex> _l(lock_);
  if (audio_codec_context_) {
    avcodec_flush_buffers(audio_codec_context_);
  }
  return 0;
}

int AudioEncodedFrameDecoder::setVideoFrameProcessing(
    std::shared_ptr<IAudiorameProcessing> processing) {
  frame_processing_ = processing;
  return 0;
}

void AudioEncodedFrameDecoder::processAudioPacket(AVPacket *audio_packet,
    AVStream *src_stream) {
  std::lock_guard<std::mutex> _l(lock_);
  // If avFrame has not been allocated, allocate it.
  if (!audio_frame_) {
    if (!(audio_frame_ = av_frame_alloc())) {
      LOGW("Could not allocate video avFrame");
      return;
    }
  }
  audio_packet->pos = -1;
  audio_packet->stream_index = 0;

  if (audio_packet->size) {
    if (decode(audio_codec_context_, audio_packet, audio_frame_) == 0) {
      if (frame_processing_) {
        frame_processing_->processAudioFrame(audio_frame_);
      }
    }
  }
  //释放packet
  av_packet_unref(audio_packet);
}

int AudioEncodedFrameDecoder::decode(AVCodecContext *avCodecContext,
    AVPacket *pkt, AVFrame *avFrame) {
  int ret;

  /* send the packet with the compressed data to the decoder */
  ret = avcodec_send_packet(avCodecContext, pkt);
  if (ret < 0) {
    LOGW("Error sending a packet for decoding: %s", av_err2str(ret));
    return -1;
  }

  // Decode audio frame and save it into avFrame
  /* read all the output frames (in general there may be any number of them */
  ret = avcodec_receive_frame(avCodecContext, avFrame);
  if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
    return -1;
  } else if (ret < 0) {
    LOGW("Error during decoding");
    return -1;
  }
  return 0;
}
