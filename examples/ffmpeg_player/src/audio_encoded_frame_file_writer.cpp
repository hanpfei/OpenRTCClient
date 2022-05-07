#include "audio_encoded_frame_file_writer.h"

#include "log.h"

AudioEncodedFrameFileWriter::AudioEncodedFrameFileWriter() :
    format_context_(nullptr), out_format_(nullptr), out_stream_(nullptr) {
}

AudioEncodedFrameFileWriter::~AudioEncodedFrameFileWriter() {
  close();
}

int AudioEncodedFrameFileWriter::initialize(const char *dstPath,
    AVCodecParameters *codecpar) {
  if (!dstPath || *dstPath == 0) {
    LOGW("Invalid destination file path.");
    return -1;
  }
  file_path_ = dstPath;
  LOGI("Destination file path: %s", file_path_.c_str());

  //out_fmt_ctx
  format_context_ = avformat_alloc_context();
  out_format_ = av_guess_format(NULL, dstPath, NULL);
  format_context_->oformat = out_format_;
  if (!out_format_) {
    LOGW("Cloud not guess file format");
    return -1;
  }

  //out_stream
  out_stream_ = avformat_new_stream(format_context_, NULL);
  if (!out_stream_) {
    LOGW("Failed to create out stream");
    return -1;
  }

  //拷贝编解码器参数
  int ret = avcodec_parameters_copy(out_stream_->codecpar, codecpar);
  if (ret < 0) {
    LOGW("avcodec_parameters_copy：%s", av_err2str(ret));
    return -1;
  }
  out_stream_->codecpar->codec_tag = 0;

  // Create and initialize the AVIOContext of target file
  if ((ret = avio_open(&format_context_->pb, dstPath, AVIO_FLAG_WRITE)) < 0) {
    LOGW("avio_open %s：%s (%s)", dstPath, av_err2str(ret), strerror(errno));
    return -1;
  }

  // Write file header
  if ((ret = avformat_write_header(format_context_, nullptr)) < 0) {
    LOGW("avformat_write_header：%s", av_err2str(ret));
    return -1;
  }

  return 0;
}

int AudioEncodedFrameFileWriter::close() {
  if (format_context_) {
    // Write trailer
    av_write_trailer(format_context_);

    // Release resource
    if (format_context_->pb) {
      avio_close(format_context_->pb);
    }
    avformat_free_context(format_context_);
    format_context_ = nullptr;
  }
  return 0;
}

void AudioEncodedFrameFileWriter::processAudioPacket(AVPacket *audio_packet,
    AVStream *src_stream) {
  audio_packet->pts = av_rescale_q(audio_packet->pts, src_stream->time_base,
      out_stream_->time_base);
  audio_packet->dts = audio_packet->pts;
  audio_packet->duration = av_rescale_q(audio_packet->duration,
      src_stream->time_base, out_stream_->time_base);
  audio_packet->pos = -1;
  audio_packet->stream_index = 0;

  //写入
  av_interleaved_write_frame(format_context_, audio_packet);

  //释放packet
  av_packet_unref(audio_packet);
}
