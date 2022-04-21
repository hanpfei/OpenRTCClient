
#include "capture_video_track_source.h"

#include "api/video/i420_buffer.h"
#include "modules/video_capture/video_capture_factory.h"
#include "rtc_base/ref_counted_object.h"

namespace {
bool FindBestMatchCapability(
    size_t width, size_t height, size_t target_fps,
    webrtc::VideoCaptureCapability &capability,
    std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> &device_info,
    char unique_name[256]) {
  webrtc::VideoCaptureCapability requested_capability;
  requested_capability.width = static_cast<int32_t>(width);
  requested_capability.height = static_cast<int32_t>(height);
  requested_capability.maxFPS = static_cast<int32_t>(target_fps);
  requested_capability.videoType = webrtc::VideoType::kI420;

  webrtc::VideoCaptureCapability resulting_capability;
  if (device_info->GetBestMatchedCapability(unique_name, requested_capability,
                                            resulting_capability) == 0) {
    capability = resulting_capability;
    return true;
  }

  bool found = false;
  int32_t number = device_info->NumberOfCapabilities(unique_name);
  for (int32_t i = 0; i < number; ++i) {
    device_info->GetCapability(unique_name, i, resulting_capability);
    if (resulting_capability.width == static_cast<int32_t>(width) &&
        resulting_capability.height == static_cast<int32_t>(height) &&
        resulting_capability.maxFPS >= static_cast<int32_t>(target_fps)) {
      capability = resulting_capability;
      found = true;
      break;
    }
  }
  return found;
}
} // namespace

CaptureVideoTrackSource::~CaptureVideoTrackSource() {
  if (vcm_ && vcm_->CaptureStarted()) {
    vcm_->StopCapture();
  }
}

void CaptureVideoTrackSource::SetState(SourceState new_state) {
  if (state_ != new_state) {
    state_ = new_state;
  }
}

void CaptureVideoTrackSource::UpdateVideoAdapter() {
  video_adapter_.OnSinkWants(broadcaster_.wants());
}

void CaptureVideoTrackSource::AddOrUpdateSink(
    rtc::VideoSinkInterface<webrtc::VideoFrame> *sink,
    const rtc::VideoSinkWants &wants) {
  broadcaster_.AddOrUpdateSink(sink, wants);
  UpdateVideoAdapter();
}

void CaptureVideoTrackSource::RemoveSink(
    rtc::VideoSinkInterface<webrtc::VideoFrame> *sink) {
  broadcaster_.RemoveSink(sink);
  UpdateVideoAdapter();
}

void CaptureVideoTrackSource::OnFrame(const webrtc::VideoFrame &frame) {
  printf("CaptureVideoTrackSource::OnFrame()\n");
  int cropped_width = 0;
  int cropped_height = 0;
  int out_width = 0;
  int out_height = 0;

  if (!video_adapter_.AdaptFrameResolution(
          frame.width(), frame.height(), frame.timestamp_us() * 1000,
          &cropped_width, &cropped_height, &out_width, &out_height)) {
    // Drop frame in order to respect frame rate constraint.
    return;
  }

  if (out_height != frame.height() || out_width != frame.width()) {
    // Video adapter has requested a down-scale. Allocate a new buffer and
    // return scaled version.
    // For simplicity, only scale here without cropping.
    rtc::scoped_refptr<webrtc::I420Buffer> scaled_buffer =
        webrtc::I420Buffer::Create(out_width, out_height);
    scaled_buffer->ScaleFrom(*frame.video_frame_buffer()->ToI420());
    webrtc::VideoFrame::Builder new_frame_builder =
        webrtc::VideoFrame::Builder()
            .set_video_frame_buffer(scaled_buffer)
            .set_rotation(webrtc::kVideoRotation_0)
            .set_timestamp_us(frame.timestamp_us())
            .set_id(frame.id());
    if (frame.has_update_rect()) {
      webrtc::VideoFrame::UpdateRect new_rect =
          frame.update_rect().ScaleWithFrame(frame.width(), frame.height(), 0,
                                             0, frame.width(), frame.height(),
                                             out_width, out_height);
      new_frame_builder.set_update_rect(new_rect);
    }
    broadcaster_.OnFrame(new_frame_builder.build());

  } else {
    // No adaptations needed, just return the frame as is.
    broadcaster_.OnFrame(frame);
  }
}

CaptureVideoTrackSource::CaptureVideoTrackSource()
    : state_(kLive), remote_(false) {}

bool CaptureVideoTrackSource::Init(size_t capture_device_index, size_t width,
                                   size_t height, size_t target_fps) {
  std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(
      webrtc::VideoCaptureFactory::CreateDeviceInfo());
  if (!info) {
    fprintf(stderr, "Cannot create video capture device info\n");
    return false;
  }
  uint32_t num_devices = info->NumberOfDevices();
  if (capture_device_index >= num_devices) {
    fprintf(stderr, "Invalid capture device index %zu\n", capture_device_index);
    return false;
  }

  char device_name[256] = {0};
  char unique_name[256] = {0};
  if (info->GetDeviceName(static_cast<uint32_t>(capture_device_index),
                          device_name, sizeof(device_name), unique_name,
                          sizeof(unique_name)) != 0) {
    return false;
  }
  fprintf(stdout, "Device name %s, unique name %s.\n", device_name,
          unique_name);

  webrtc::VideoCaptureCapability capability;
  if (!FindBestMatchCapability(width, height, target_fps, capability, info,
                               unique_name)) {
    fprintf(stderr, "Cannot find proper capability.\n");
    return false;
  }

  rtc::scoped_refptr<webrtc::VideoCaptureModule> vcm =
      webrtc::VideoCaptureFactory::Create(unique_name);
  if (!vcm) {
    return false;
  }
  vcm->RegisterCaptureDataCallback(this);

  if (vcm->StartCapture(capability_) != 0) {
    return false;
  }

  capability_ = capability;
  vcm_ = vcm;
  fprintf(stdout, "Create video capture successfully.\n");

  vcm_->CaptureStarted();
  return true;
}

rtc::scoped_refptr<CaptureVideoTrackSource>
CaptureVideoTrackSource::Create(size_t capture_device_index, size_t width,
                                size_t height, size_t fps) {
  rtc::scoped_refptr<CaptureVideoTrackSource> capture_video_source(
      new rtc::RefCountedObject<CaptureVideoTrackSource>());
  if (!capture_video_source->Init(capture_device_index, width, height, fps)) {
    return nullptr;
  }
  return capture_video_source;
}

bool CaptureVideoTrackSource::GetCaptureDeviceList(
    std::vector<std::string> &devices) {
  std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(
      webrtc::VideoCaptureFactory::CreateDeviceInfo());
  if (!info) {
    fprintf(stderr, "Cannot create video capture device info\n");
    return false;
  }
  uint32_t num_devices = info->NumberOfDevices();
  if (num_devices == 0) {
    fprintf(stderr, "No capture device found\n");
    return false;
  }

  for (uint32_t i = 0; i < num_devices; ++i) {
    char device_name[256] = {0};
    char unique_name[256] = {0};
    if (info->GetDeviceName(static_cast<uint32_t>(i), device_name,
                            sizeof(device_name), unique_name,
                            sizeof(unique_name)) == 0) {
      devices.emplace_back(std::string(device_name));
    }
  }

  return !devices.empty();
}
