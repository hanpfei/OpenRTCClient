
#pragma once

#include <mutex>

#include "api/peer_connection_interface.h"
#include "api/scoped_refptr.h"

class ConnectionObserver : public webrtc::PeerConnectionObserver {
public:
  explicit ConnectionObserver(const std::string &tag);
  virtual ~ConnectionObserver();

  void SetConnection(
      rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection);
  rtc::scoped_refptr<webrtc::VideoTrackInterface> GetRemoteVideoTrack();

  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) override;
  void OnDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override;
  void OnRenegotiationNeeded() override;
  void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) override;
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override;
  void OnIceCandidate(const webrtc::IceCandidateInterface *candidate) override;
  void
  OnAddTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
             const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>
                 &streams) override;

private:
  const std::string tag_;
  std::vector<std::shared_ptr<webrtc::IceCandidateInterface>> candidates_;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  rtc::scoped_refptr<webrtc::VideoTrackInterface> remote_video_track_;
};

class LoopCallSesstion {
public:
  LoopCallSesstion();
  virtual ~LoopCallSesstion();

  bool LoopCallStarted();
  void StartLoopCall(size_t capture_device_index);
  void StopLoopCall();
  rtc::scoped_refptr<webrtc::VideoTrackInterface> GetRemoteVideoTrack();

private:
  void CreatePeerConnectionFactory();

  void CreateConnections();
  void Connect();

  void DestroyConnections();

  bool call_started_;

  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> pcf_;
  std::unique_ptr<rtc::Thread> network_thread_;
  std::unique_ptr<rtc::Thread> worker_thread_;
  std::unique_ptr<rtc::Thread> signaling_thread_;

  std::unique_ptr<rtc::VideoSinkInterface<webrtc::VideoFrame>> remote_sink_;
  rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> video_source_;

  rtc::scoped_refptr<webrtc::AudioSourceInterface> audio_source_;
  rtc::scoped_refptr<webrtc::AudioTrackInterface> local_audio_track_;

  std::unique_ptr<ConnectionObserver> send_observer_;
  std::unique_ptr<ConnectionObserver> recv_observer_;

  std::mutex pc_mutex_;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> send_connection_;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> recv_connection_;
};
