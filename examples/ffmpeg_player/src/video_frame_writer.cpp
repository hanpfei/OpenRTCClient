#include "video_frame_writer.h"

#include "log.h"

VideoFrameWriter::VideoFrameWriter() :
    out_file_(nullptr) {
}

VideoFrameWriter::~VideoFrameWriter() {
  close();
}

int VideoFrameWriter::initialize(const char *dstPath) {
  if (dstPath && *dstPath != 0) {
    auto *out_file = fopen(dstPath, "wbe");
    if (!out_file) {
      LOGI("Could not open out file: %s", strerror(errno));
      return -1;
    }
    out_file_ = out_file;
    file_path_ = dstPath;
  }

  return 0;
}

int VideoFrameWriter::close() {
  if (out_file_) {
    fclose(out_file_);
    out_file_ = nullptr;
  }
  std::lock_guard<std::mutex> _l(lock_);
  while (!video_frames_.empty()) {
    AVFrame *frame = video_frames_.front();
    video_frames_.erase(video_frames_.begin());
    av_frame_unref(frame);
    av_frame_free(&frame);
  }
  return 0;
}

void VideoFrameWriter::processVideoFrame(AVFrame *frame) {
  if (out_file_) {
    yuv_save(frame);
  }
  AVFrame *copied_frame = av_frame_clone(frame);
  std::lock_guard<std::mutex> _l(lock_);
  video_frames_.push_back(copied_frame);
}

AVFrame* VideoFrameWriter::popVideoFrame() {
  AVFrame *frame = nullptr;
  std::lock_guard<std::mutex> _l(lock_);
  if (!video_frames_.empty()) {
    frame = video_frames_.front();
    video_frames_.erase(video_frames_.begin());
  }
  return frame;
}

int VideoFrameWriter::getVideoFrames() {
  std::lock_guard<std::mutex> _l(lock_);
  return static_cast<int>(video_frames_.size());
}

//将avFrame保存为yuv文件
void VideoFrameWriter::yuv_save(AVFrame *avFrame) {
  int width = avFrame->width;
  int height = avFrame->height;
  for (int i = 0; i < height; i++) {
    fwrite(avFrame->data[0] + i * avFrame->linesize[0], 1, width, out_file_);
  }
  for (int j = 0; j < height / 2; j++) {
    fwrite(avFrame->data[1] + j * avFrame->linesize[1], 1, width / 2,
        out_file_);
  }
  for (int k = 0; k < height / 2; k++) {
    fwrite(avFrame->data[2] + k * avFrame->linesize[2], 1, width / 2,
        out_file_);
  }
}
