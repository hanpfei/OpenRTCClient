
#import "ConnectionObserver.h"

ConnectionObserver::ConnectionObserver(const std::string &tag) : tag_(tag) {}

ConnectionObserver::~ConnectionObserver() = default;

void ConnectionObserver::OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) {
  RTC_LOG(LS_INFO) << "OnSignalingChange: " << new_state;
}

void ConnectionObserver::OnDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
  RTC_LOG(LS_INFO) << "OnDataChannel";
}

void ConnectionObserver::OnRenegotiationNeeded() {
  RTC_LOG(LS_INFO) << "OnRenegotiationNeeded";
}

void ConnectionObserver::OnIceConnectionChange(
    webrtc::PeerConnectionInterface::IceConnectionState new_state) {
  RTC_LOG(LS_INFO) << "OnIceConnectionChange: " << new_state;
}

void ConnectionObserver::OnIceGatheringChange(
    webrtc::PeerConnectionInterface::IceGatheringState new_state) {
  RTC_LOG(LS_INFO) << "OnIceGatheringChange: " << new_state;
}

void ConnectionObserver::OnIceCandidate(const webrtc::IceCandidateInterface* candidate) {
  std::string out_str;
  candidate->ToString(&out_str);
  RTC_LOG(LS_INFO) << "OnIceCandidate: sdp_mid " << candidate->sdp_mid()
      << ", sdp_mline_index " << candidate->sdp_mline_index()
      << ", candidate " << candidate->candidate().ToString();
  if (peer_connection_) {
    peer_connection_->AddIceCandidate(candidate);
  } else {
    std::shared_ptr<webrtc::IceCandidateInterface> ice_candidate = webrtc::CreateIceCandidate(candidate->sdp_mid(),
        candidate->sdp_mline_index(),
        candidate->candidate());
    candidates_.emplace_back(ice_candidate);
  }
}

void ConnectionObserver::OnAddTrack(
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
    const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>
        &streams) {
  auto *track = receiver->track().get();
  if (track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind) {
    remote_video_track_ = static_cast<webrtc::VideoTrackInterface *>(track);
  }
  printf("[%s]: OnAddTrack %p %s\n", tag_.c_str(), track,
         track->kind().c_str());
}

void ConnectionObserver::SetConnection(rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection) {
  peer_connection_ = peer_connection;
  
  if (peer_connection_) {
    for (std::shared_ptr<webrtc::IceCandidateInterface> ice_candidate : candidates_) {
      peer_connection_->AddIceCandidate(ice_candidate.get());
    }
  }
}

rtc::scoped_refptr<webrtc::VideoTrackInterface>
ConnectionObserver::GetRemoteVideoTrack() {
  return remote_video_track_;
}
