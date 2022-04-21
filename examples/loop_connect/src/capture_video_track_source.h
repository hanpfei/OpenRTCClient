#pragma once

#include "api/media_stream_interface.h"
#include "api/notifier.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "media/base/video_adapter.h"
#include "media/base/video_broadcaster.h"
#include "modules/video_capture/video_capture.h"

class CaptureVideoTrackSource
    : public rtc::VideoSinkInterface<webrtc::VideoFrame>,
      public webrtc::Notifier<webrtc::VideoTrackSourceInterface> {
public:
  static rtc::scoped_refptr<CaptureVideoTrackSource>
  Create(size_t capture_device_index, size_t width = 640, size_t height = 480,
         size_t fps = 30);

  static bool GetCaptureDeviceList(std::vector<std::string> &devices);

  virtual ~CaptureVideoTrackSource();

  // inherit from webrtc::VideoTrackSourceInterface
public:
  void SetState(SourceState new_state);

  SourceState state() const override { return state_; }
  bool remote() const override { return remote_; }

  bool is_screencast() const override { return false; }
  absl::optional<bool> needs_denoising() const override {
    return absl::nullopt;
  }

  bool GetStats(Stats *stats) override { return false; }

  bool SupportsEncodedOutput() const override { return false; }
  void GenerateKeyFrame() override {}

  void AddEncodedSink(
      rtc::VideoSinkInterface<webrtc::RecordableEncodedFrame> *sink) override {}
  void RemoveEncodedSink(
      rtc::VideoSinkInterface<webrtc::RecordableEncodedFrame> *sink) override {}

  void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame> *sink,
                       const rtc::VideoSinkWants &wants) override;
  void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame> *sink) override;

  // inherit from rtc::VideoSinkInterface<webrtc::VideoFrame>
public:
  void OnFrame(const webrtc::VideoFrame &frame) override;

protected:
  CaptureVideoTrackSource();

private:
  bool Init(size_t capture_device_index, size_t width, size_t height,
            size_t target_fps);
  void Destroy();

private:
  rtc::VideoSinkWants GetSinkWants();
  void UpdateVideoAdapter();

protected:
private:
  rtc::scoped_refptr<webrtc::VideoCaptureModule> vcm_;
  webrtc::VideoCaptureCapability capability_;

  rtc::VideoBroadcaster broadcaster_;
  cricket::VideoAdapter video_adapter_;

  SourceState state_;
  const bool remote_;
};
