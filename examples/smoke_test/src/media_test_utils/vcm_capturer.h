/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_VCM_CAPTURER_H_
#define TEST_VCM_CAPTURER_H_

#include <memory>
#include <vector>

#include "api/scoped_refptr.h"
#include "modules/video_capture/video_capture.h"
#include "pc/video_track_source.h"
#include "test_video_capturer.h"

class VcmCapturer : public TestVideoCapturer,
                    public rtc::VideoSinkInterface<webrtc::VideoFrame> {
 public:
  static VcmCapturer* Create(size_t width,
                             size_t height,
                             size_t target_fps,
                             size_t capture_device_index);
  virtual ~VcmCapturer();

  void OnFrame(const webrtc::VideoFrame& frame) override;

 private:
  VcmCapturer();
  bool Init(size_t width,
            size_t height,
            size_t target_fps,
            size_t capture_device_index);
  void Destroy();

  rtc::scoped_refptr<webrtc::VideoCaptureModule> vcm_;
  webrtc::VideoCaptureCapability capability_;
};

class CapturerTrackSource : public webrtc::VideoTrackSource {
 public:
  static rtc::scoped_refptr<CapturerTrackSource> Create();

 protected:
  explicit CapturerTrackSource(std::unique_ptr<VcmCapturer> capturer);

 private:
  rtc::VideoSourceInterface<webrtc::VideoFrame>* source() override;
  std::unique_ptr<VcmCapturer> capturer_;
};

#endif  // TEST_VCM_CAPTURER_H_
