#include "video_encoded_frame_writer.h"

#include "log.h"

VideoEncodedFrameWriter::VideoEncodedFrameWriter() :
    format_context_(nullptr), out_format_(nullptr), out_stream_(nullptr) {
}

VideoEncodedFrameWriter::~VideoEncodedFrameWriter() {
  close();
}

int VideoEncodedFrameWriter::initialize(const char *dstPath,
    AVCodecParameters *codecpar) {
  if (!dstPath || *dstPath == 0) {
    LOGW("Invalid destination file path.");
    return -1;
  }
  file_path_ = dstPath;
  LOGI("Destination file path: %s", file_path_.c_str());

  //out_fmt_ctx
  format_context_ = avformat_alloc_context();
  out_format_ = av_guess_format(NULL, file_path_.c_str(), NULL);
  if (!out_format_) {
    LOGW("Could not guess file format");
    return -1;
  }

  format_context_->oformat = out_format_;

  //Create out_stream
  out_stream_ = avformat_new_stream(format_context_, NULL);
  if (!out_stream_) {
    LOGW("Failed to create out stream");
    return -1;
  }

  //拷贝编解码器参数
  int ret = avcodec_parameters_copy(out_stream_->codecpar, codecpar);
  if (ret < 0) {
    LOGI("avcodec_parameters_copy：%s", av_err2str(ret));
    return -1;
  }
  out_stream_->codecpar->codec_tag = 0;

  //创建并初始化目标文件的AVIOContext
  if ((ret = avio_open(&format_context_->pb, file_path_.c_str(),
      AVIO_FLAG_WRITE)) < 0) {
    LOGI("avio_open：%s", av_err2str(ret));
    return -1;
  }

  //写文件头
  if ((ret = avformat_write_header(format_context_, nullptr)) < 0) {
    LOGI("avformat_write_header：%s", av_err2str(ret));
    return -1;
  }

  return 0;
}

void VideoEncodedFrameWriter::processVideoPacket(AVPacket *video_packet,
    AVStream *src_stream) {
  //输入流和输出流的时间基可能不同，因此要根据时间基的不同对时间戳pts进行转换
  video_packet->pts = av_rescale_q(video_packet->pts, src_stream->time_base,
      out_stream_->time_base);
  video_packet->dts = video_packet->pts;
  //根据时间基转换duration
  video_packet->duration = av_rescale_q(video_packet->duration,
      src_stream->time_base, out_stream_->time_base);
  video_packet->pos = -1;
  video_packet->stream_index = 0;

  //写入
  av_interleaved_write_frame(format_context_, video_packet);

  //释放packet
  av_packet_unref(video_packet);
}

int VideoEncodedFrameWriter::close() {
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
