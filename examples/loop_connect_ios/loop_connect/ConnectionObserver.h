
#pragma once

#include "api/peer_connection_interface.h"
#include "api/scoped_refptr.h"

class ConnectionObserver : public webrtc::PeerConnectionObserver {
 public:
  explicit ConnectionObserver(const std::string &tag);
  virtual ~ConnectionObserver();

  void SetConnection(rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection);
  rtc::scoped_refptr<webrtc::VideoTrackInterface> GetRemoteVideoTrack();
  
  void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) override;
  void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override;
  void OnRenegotiationNeeded() override;
  void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) override;
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override;
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;
  void OnAddTrack(
        rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
        const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>&
                  streams) override;
  
 private:
  const std::string tag_;
  std::vector<std::shared_ptr<webrtc::IceCandidateInterface>> candidates_;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  rtc::scoped_refptr<webrtc::VideoTrackInterface> remote_video_track_;
};
