
#pragma once

#include <memory>
#include <string>
#include <vector>

#import "sdk/objc/base/RTCMacros.h"
#import "sdk/objc/base/RTCVideoRenderer.h"
#import "sdk/objc/components/capturer/RTCCameraVideoCapturer.h"

#include "api/peer_connection_interface.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "rtc_base/synchronization/mutex.h"

#import "ConnectionObserver.h"

class CallClient {
public:
  CallClient();
  
  bool LoopCallStarted();
  void StartLoopCall(RTC_OBJC_TYPE(RTCVideoCapturer) * capturer,
                     id<RTC_OBJC_TYPE(RTCVideoRenderer)> remote_renderer);
  void StopLoopCall();
  
private:
  void CreatePeerConnectionFactory();

  void CreateConnections();
  void Connect();

  void DestroyConnections();

  webrtc::SequenceChecker thread_checker_;

  bool call_started_ RTC_GUARDED_BY(thread_checker_);

  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> pcf_ RTC_GUARDED_BY(thread_checker_);
  std::unique_ptr<rtc::Thread> network_thread_ RTC_GUARDED_BY(thread_checker_);
  std::unique_ptr<rtc::Thread> worker_thread_ RTC_GUARDED_BY(thread_checker_);
  std::unique_ptr<rtc::Thread> signaling_thread_ RTC_GUARDED_BY(thread_checker_);
  
  std::unique_ptr<rtc::VideoSinkInterface<webrtc::VideoFrame>> remote_sink_
      RTC_GUARDED_BY(thread_checker_);
  RTC_OBJC_TYPE(RTCCameraVideoCapturer) * capturer_;
  rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> video_source_
      RTC_GUARDED_BY(thread_checker_);
  
  rtc::scoped_refptr<webrtc::AudioSourceInterface> audio_source_;
  rtc::scoped_refptr<webrtc::AudioTrackInterface> local_audio_track_;
  
  std::unique_ptr<ConnectionObserver> send_observer_;
  std::unique_ptr<ConnectionObserver> recv_observer_;
  
  webrtc::Mutex pc_mutex_;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> send_connection_ RTC_GUARDED_BY(pc_mutex_);
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> recv_connection_ RTC_GUARDED_BY(pc_mutex_);
};
