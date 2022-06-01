
#include <chrono>
#include <cstring>
#include <mutex>
#include <thread>

#include "media_test_utils/test_base.h"
#include "modules/video_capture/video_capture.h"
#include "modules/video_capture/video_capture_factory.h"
#include "pc/video_track_source.h"
#include "media_test_utils/video_frame_file_writer_bmp.h"
#include "rtc_base/logging.h"

class WebrtcCameraTest : public testing::Test {
 public:
  void SetUp() override {}

  void TearDown() override {}
};

TEST_F(WebrtcCameraTest, device_info) {
  std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> device_info(
      webrtc::VideoCaptureFactory::CreateDeviceInfo());
  ASSERT_TRUE(device_info);


  int num_devices = device_info->NumberOfDevices();
  ASSERT_GT(num_devices, 0);

  webrtc::VideoCaptureCapability capability;
  for (int i = 0; i < num_devices; ++i) {
    char device_name[256] = {0};
    char unique_name[256] = {0};
    if (device_info->GetDeviceName(static_cast<uint32_t>(i), device_name,
                                   sizeof(device_name), unique_name,
                                   sizeof(unique_name)) != 0) {
      break;
    }
    RTC_LOG(LS_INFO) << "device name: " << device_name << ", " << "unique name: " << unique_name;

    int32_t capability_number = device_info->NumberOfCapabilities(unique_name);
    if (capability_number <= 0) {
      RTC_LOG(LS_WARNING) << "Camera device has no capabilities.";
    }
    for (int i = 0; i < capability_number; ++i) {
      device_info->GetCapability(unique_name, i, capability);
      RTC_LOG(LS_INFO) << "Video Capture Capability: width: "
          << capability.width << ", height: " << capability.height
          << ", maxFPS: " << capability.maxFPS
          << ", videoType: " << static_cast<int>(capability.videoType);
    }
  }
}

TEST_F(WebrtcCameraTest, positive) {
  std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> device_info(
      webrtc::VideoCaptureFactory::CreateDeviceInfo());

  int num_devices = device_info->NumberOfDevices();
  ASSERT_GT(num_devices, 0);

  rtc::scoped_refptr<webrtc::VideoCaptureModule> vcm;
  char device_name[256] = {0};
  char unique_name[256] = {0};
  for (int i = 0; i < num_devices; ++i) {
    memset(unique_name, 0, sizeof(unique_name));
    memset(device_name, 0, sizeof(device_name));
    if (device_info->GetDeviceName(static_cast<uint32_t>(i), device_name,
                                   sizeof(device_name), unique_name,
                                   sizeof(unique_name)) != 0) {
      break;
    }

    vcm = webrtc::VideoCaptureFactory::Create(unique_name);
    if (vcm) {
      break;
    }
  }

  ASSERT_TRUE(vcm);

  std::unique_ptr<FakeVideoSink> video_sink = std::make_unique<FakeVideoSink>();
  //  video_sink->EnableWriteFile(true);
  vcm->RegisterCaptureDataCallback(video_sink.get());

  size_t width = 640;
  size_t height = 480;
  size_t target_fps = 30;

  webrtc::VideoCaptureCapability capability;
  device_info->GetCapability(vcm->CurrentDeviceName(), 0, capability);

  capability.width = static_cast<int32_t>(width);
  capability.height = static_cast<int32_t>(height);
  capability.maxFPS = static_cast<int32_t>(target_fps);
  capability.videoType = webrtc::VideoType::kI420;

  webrtc::VideoCaptureCapability best_matched_capability;
  device_info->GetBestMatchedCapability(unique_name, capability,
                                        best_matched_capability);

  best_matched_capability.videoType = webrtc::VideoType::kI420;
  RTC_LOG(LS_INFO) << "Start Capture with Best Video Capture Capability: width: "
            << capability.width << ", height: " << capability.height
            << ", maxFPS: " << capability.maxFPS
            << ", videoType: " << static_cast<int>(capability.videoType);

  EXPECT_EQ(vcm->StartCapture(best_matched_capability), 0);
  EXPECT_TRUE(vcm->CaptureStarted());

  int32_t wait_time = 0;
  while (wait_time < 5750 && video_sink->GetCapturedVideoFrames() < 15) {
    std::this_thread::sleep_for(std::chrono::milliseconds(179));
    wait_time += 179;
  }

  EXPECT_EQ(vcm->StopCapture(), 0);
  vcm->DeRegisterCaptureDataCallback();

  RTC_LOG(LS_INFO) << "Captured Video Frames: " << video_sink->GetCapturedVideoFrames();
  EXPECT_GT(video_sink->GetCapturedVideoFrames(), 0);
}
