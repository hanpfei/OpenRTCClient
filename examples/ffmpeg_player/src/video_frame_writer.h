#pragma once

#include <mutex>
#include <vector>

#include "video_encoded_frame_decoder.h"

class VideoFrameWriter: public IVideoFrameProcessing {
public:
  VideoFrameWriter();
  virtual ~VideoFrameWriter();

  int initialize(const char *dstPath);
  int close();
  AVFrame* popVideoFrame();
  int getVideoFrames();

public:
  void processVideoFrame(AVFrame *frame) override;

private:
  void yuv_save(AVFrame *avFrame);

private:
  std::string file_path_;
  FILE *out_file_ = nullptr;

  std::mutex lock_;
  std::vector<AVFrame*> video_frames_;
};

