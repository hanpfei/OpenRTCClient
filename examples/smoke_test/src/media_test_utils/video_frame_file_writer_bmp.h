
#pragma once

#include <future>
#include <mutex>

#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"

class VideoFrameFileWriterBmp
    : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
 public:
  VideoFrameFileWriterBmp();
  VideoFrameFileWriterBmp(const std::string& dirpath);

  virtual ~VideoFrameFileWriterBmp();

  void OnFrame(const webrtc::VideoFrame& video_frame) override;

 private:
  void SetSize(int width, int height);
  void WriteBMPHeader(FILE* file);

 private:
  const std::string dirpath_;
  int width_;
  int height_;
  std::unique_ptr<uint8_t[]> image_;
  int video_frame_index_;
};

class FakeVideoSink : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
 public:
  FakeVideoSink();
  explicit FakeVideoSink(int64_t time);
  FakeVideoSink(int64_t time, int32_t target_captured_frames);

  virtual ~FakeVideoSink();

  void EnableWriteFile(bool enable);

  void OnFrame(const webrtc::VideoFrame& video_frame) override;

  int32_t GetCapturedVideoFrames();
  std::future<int32_t> GetCaptureFramesResult();

 private:
  std::mutex lock_;
  bool write_file_;
  std::unique_ptr<VideoFrameFileWriterBmp> file_writer_;
  int64_t end_time_;
  int32_t captured_video_frames_;
  int32_t target_captured_frames_;
  std::promise<int32_t> capture_frames_;
  int64_t last_frame_incoming_time_;
};
