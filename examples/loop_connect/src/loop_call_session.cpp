
#include "loop_call_session.h"

#include <future>
#include <memory>
#include <utility>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_event_log/rtc_event_log_factory.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"

#include "capture_video_track_source.h"

static const char kAudioLabel[] = "audio_label";
static const char kVideoLabel[] = "video_label";
static const char kStreamId[] = "stream_id";

namespace {

class CreateOfferObserver : public webrtc::CreateSessionDescriptionObserver {
public:
  explicit CreateOfferObserver(const std::string &role);
  virtual ~CreateOfferObserver();

  void OnSuccess(webrtc::SessionDescriptionInterface *desc) override;
  void OnFailure(webrtc::RTCError error) override;

  std::future<std::string> GetSDP();

private:
  const std::string role_;
  std::promise<std::string> sdp_promise_;
};

class SetRemoteSessionDescriptionObserver
    : public webrtc::SetRemoteDescriptionObserverInterface {
public:
  explicit SetRemoteSessionDescriptionObserver(const std::string &role);
  virtual ~SetRemoteSessionDescriptionObserver();
  void OnSetRemoteDescriptionComplete(webrtc::RTCError error) override;

private:
  const std::string role_;
};

class SetLocalSessionDescriptionObserver
    : public webrtc::SetSessionDescriptionObserver {
public:
  explicit SetLocalSessionDescriptionObserver(const std::string &role);
  virtual ~SetLocalSessionDescriptionObserver();
  void OnSuccess() override;
  void OnFailure(webrtc::RTCError error) override;

private:
  const std::string role_;
};

} // namespace

LoopCallSesstion::LoopCallSesstion() : call_started_(false) {
  CreatePeerConnectionFactory();
}

LoopCallSesstion::~LoopCallSesstion() {
  send_observer_ = nullptr;
  recv_observer_ = nullptr;

  send_connection_ = nullptr;
  recv_connection_ = nullptr;

  local_audio_track_ = nullptr;
  audio_source_ = nullptr;

  video_source_ = nullptr;
  remote_sink_ = nullptr;

  pcf_ = nullptr;

  signaling_thread_ = nullptr;
  worker_thread_ = nullptr;
  network_thread_ = nullptr;
}

void LoopCallSesstion::CreatePeerConnectionFactory() {
  network_thread_ = rtc::Thread::CreateWithSocketServer();
  network_thread_->SetName("network_thread", nullptr);
  if (!network_thread_->Start()) {
    fprintf(stderr, "Failed to start network thread\n");
  }

  worker_thread_ = rtc::Thread::Create();
  worker_thread_->SetName("worker_thread", nullptr);
  if (!worker_thread_->Start()) {
    fprintf(stderr, "Failed to start worker thread\n");
  }

  signaling_thread_ = rtc::Thread::Create();
  signaling_thread_->SetName("signaling_thread", nullptr);
  if (!signaling_thread_->Start()) {
    fprintf(stderr, "Failed to start worker signaling thread\n");
  }

  pcf_ = webrtc::CreatePeerConnectionFactory(
      network_thread_.get() /* network_thread */,
      worker_thread_.get() /* worker_thread */,
      signaling_thread_.get() /* signaling_thread */, nullptr /* default_adm */,
      webrtc::CreateBuiltinAudioEncoderFactory(),
      webrtc::CreateBuiltinAudioDecoderFactory(),
      webrtc::CreateBuiltinVideoEncoderFactory(),
      webrtc::CreateBuiltinVideoDecoderFactory(), nullptr /* audio_mixer */,
      nullptr /* audio_processing */);

  fprintf(stdout, "PeerConnectionFactory created: %p\n", pcf_.get());
}

bool LoopCallSesstion::LoopCallStarted() {
  std::lock_guard<std::mutex> lk_(pc_mutex_);
  return call_started_;
}

void LoopCallSesstion::CreateConnections() {
  std::lock_guard<std::mutex> lk_(pc_mutex_);

  webrtc::PeerConnectionInterface::RTCConfiguration config;
  config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
  // DTLS SRTP has to be disabled for loopback to work.
  config.enable_dtls_srtp = false;

  send_observer_ = std::make_unique<ConnectionObserver>("SendConn");
  webrtc::PeerConnectionDependencies pc_dependencies(send_observer_.get());
  send_connection_ =
      pcf_->CreatePeerConnection(config, std::move(pc_dependencies));
  fprintf(stdout, "Send PeerConnection created: %p\n", send_connection_.get());

  recv_observer_ = std::make_unique<ConnectionObserver>("RecvConn");

  webrtc::PeerConnectionDependencies recv_pc_dependencies(recv_observer_.get());
  recv_connection_ =
      pcf_->CreatePeerConnection(config, std::move(recv_pc_dependencies));
  fprintf(stdout, "Receive PeerConnection created: %p\n",
          recv_connection_.get());

  send_connection_->AddTrack(local_audio_track_, {kStreamId});
  fprintf(stdout, "Local auido track set up: %p\n", local_audio_track_.get());

  if (video_source_) {
    rtc::scoped_refptr<webrtc::VideoTrackInterface> local_video_track =
        pcf_->CreateVideoTrack(kVideoLabel, video_source_);
    send_connection_->AddTransceiver(local_video_track);
    fprintf(stdout, "Local video sink set up: %p\n", local_video_track.get());
  }
}

void LoopCallSesstion::Connect() {
  std::lock_guard<std::mutex> lk_(pc_mutex_);

  std::string send_role("SendConn");
  rtc::scoped_refptr<CreateOfferObserver> observer(
      new rtc::RefCountedObject<CreateOfferObserver>(send_role));
  send_connection_->CreateOffer(
      observer, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());

  std::future<std::string> sdp_future = observer->GetSDP();
  std::string sdp = sdp_future.get();

  std::unique_ptr<webrtc::SessionDescriptionInterface> send_offer(
      webrtc::CreateSessionDescription(webrtc::SdpType::kOffer, sdp));
  send_connection_->SetLocalDescription(
      new rtc::RefCountedObject<SetLocalSessionDescriptionObserver>("SendConn"),
      send_offer.release());

  std::unique_ptr<webrtc::SessionDescriptionInterface> recv_send_offer(
      webrtc::CreateSessionDescription(webrtc::SdpType::kOffer, sdp));
  recv_connection_->SetRemoteDescription(
      std::move(recv_send_offer),
      new rtc::RefCountedObject<SetRemoteSessionDescriptionObserver>(
          "RecvConn"));

  rtc::scoped_refptr<CreateOfferObserver> recv_answer_observer(
      new rtc::RefCountedObject<CreateOfferObserver>("RecvConn"));
  recv_connection_->CreateAnswer(
      recv_answer_observer,
      webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());

  std::future<std::string> recv_sdp_answer_future =
      recv_answer_observer->GetSDP();
  std::string recv_sdp_answer = recv_sdp_answer_future.get();

  std::unique_ptr<webrtc::SessionDescriptionInterface> recv_answer(
      webrtc::CreateSessionDescription(webrtc::SdpType::kAnswer,
                                       recv_sdp_answer));
  send_connection_->SetRemoteDescription(
      std::move(recv_answer),
      new rtc::RefCountedObject<SetRemoteSessionDescriptionObserver>(
          "SendConn"));

  std::unique_ptr<webrtc::SessionDescriptionInterface> recv_recv_answer(
      webrtc::CreateSessionDescription(webrtc::SdpType::kAnswer,
                                       recv_sdp_answer));
  recv_connection_->SetLocalDescription(
      new rtc::RefCountedObject<SetLocalSessionDescriptionObserver>("RecvConn"),
      recv_recv_answer.release());

  send_observer_->SetConnection(recv_connection_);
  recv_observer_->SetConnection(send_connection_);
}

void LoopCallSesstion::StartLoopCall(size_t capture_device_index) {
  {
    std::lock_guard<std::mutex> lk_(pc_mutex_);
    if (call_started_) {
      fprintf(stderr, "Loop call already started.\n");
      return;
    }
    call_started_ = true;
  }

  cricket::AudioOptions audio_options;
  audio_source_ = pcf_->CreateAudioSource(audio_options);
  local_audio_track_ = pcf_->CreateAudioTrack(kAudioLabel, audio_source_);

  video_source_ = CaptureVideoTrackSource::Create(capture_device_index);

  CreateConnections();
  Connect();
}

void LoopCallSesstion::DestroyConnections() {
  std::lock_guard<std::mutex> lk_(pc_mutex_);
  for (const rtc::scoped_refptr<webrtc::RtpTransceiverInterface> &tranceiver :
       send_connection_->GetTransceivers()) {
    send_connection_->RemoveTrack(tranceiver->sender().get());
  }

  send_observer_->SetConnection(nullptr);
  recv_observer_->SetConnection(nullptr);

  send_connection_ = nullptr;
  recv_connection_ = nullptr;
  local_audio_track_ = nullptr;
  audio_source_ = nullptr;
  video_source_ = nullptr;
  remote_sink_ = nullptr;
}

void LoopCallSesstion::StopLoopCall() {
  DestroyConnections();

  std::lock_guard<std::mutex> lk_(pc_mutex_);
  if (!call_started_) {
    fprintf(stderr, "Loop call has not been started.\n");
    return;
  }
  call_started_ = false;
}

rtc::scoped_refptr<webrtc::VideoTrackInterface>
LoopCallSesstion::GetRemoteVideoTrack() {
  if (recv_observer_) {
    return recv_observer_->GetRemoteVideoTrack();
  }
  return nullptr;
}

ConnectionObserver::ConnectionObserver(const std::string &tag) : tag_(tag) {}

ConnectionObserver::~ConnectionObserver() = default;

void ConnectionObserver::OnSignalingChange(
    webrtc::PeerConnectionInterface::SignalingState new_state) {
  fprintf(stdout, "OnSignalingChange: %d\n", new_state);
}

void ConnectionObserver::OnDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
  fprintf(stdout, "OnDataChannel: %p\n", data_channel.get());
}

void ConnectionObserver::OnRenegotiationNeeded() {
  fprintf(stdout, "OnRenegotiationNeeded\n");
}

void ConnectionObserver::OnIceConnectionChange(
    webrtc::PeerConnectionInterface::IceConnectionState new_state) {
  fprintf(stdout, "OnIceConnectionChange: %d\n", new_state);
}

void ConnectionObserver::OnIceGatheringChange(
    webrtc::PeerConnectionInterface::IceGatheringState new_state) {
  fprintf(stdout, "OnIceGatheringChange: %d\n", new_state);
}

void ConnectionObserver::OnIceCandidate(
    const webrtc::IceCandidateInterface *candidate) {
  std::string out_str;
  candidate->ToString(&out_str);
  fprintf(stdout,
          "OnIceCandidate: sdp_mid %s, sdp_mline_index %d, candidate %s\n",
          candidate->sdp_mid().c_str(), candidate->sdp_mline_index(),
          candidate->candidate().ToString().c_str());
  if (peer_connection_) {
    peer_connection_->AddIceCandidate(candidate);
  } else {
    std::shared_ptr<webrtc::IceCandidateInterface> ice_candidate =
        webrtc::CreateIceCandidate(candidate->sdp_mid(),
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

void ConnectionObserver::SetConnection(
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection) {
  peer_connection_ = peer_connection;

  if (peer_connection_) {
    for (std::shared_ptr<webrtc::IceCandidateInterface> ice_candidate :
         candidates_) {
      peer_connection_->AddIceCandidate(ice_candidate.get());
    }
  }
}

rtc::scoped_refptr<webrtc::VideoTrackInterface>
ConnectionObserver::GetRemoteVideoTrack() {
  return remote_video_track_;
}

CreateOfferObserver::CreateOfferObserver(const std::string &role)
    : role_(role) {}

CreateOfferObserver::~CreateOfferObserver() = default;

void CreateOfferObserver::OnSuccess(webrtc::SessionDescriptionInterface *desc) {
  std::string sdp;
  desc->ToString(&sdp);

  sdp_promise_.set_value(sdp);
}

void CreateOfferObserver::OnFailure(webrtc::RTCError error) {
  fprintf(stderr, "%s: Failed to create offer for %s\n", role_.c_str(),
          error.message());
  sdp_promise_.set_value("");
}

std::future<std::string> CreateOfferObserver::GetSDP() {
  return sdp_promise_.get_future();
}

SetRemoteSessionDescriptionObserver::SetRemoteSessionDescriptionObserver(
    const std::string &role)
    : role_(role) {}

SetRemoteSessionDescriptionObserver::~SetRemoteSessionDescriptionObserver() =
    default;

void SetRemoteSessionDescriptionObserver::OnSetRemoteDescriptionComplete(
    webrtc::RTCError error) {
  fprintf(stderr, "%s: Set remote description failed for %s\n", role_.c_str(),
          error.message());
}

SetLocalSessionDescriptionObserver::SetLocalSessionDescriptionObserver(
    const std::string &role)
    : role_(role) {}

SetLocalSessionDescriptionObserver::~SetLocalSessionDescriptionObserver() =
    default;

void SetLocalSessionDescriptionObserver::OnSuccess() {
  fprintf(stdout, "%s: Set local description success!\n", role_.c_str());
}

void SetLocalSessionDescriptionObserver::OnFailure(webrtc::RTCError error) {
  fprintf(stderr, "%s: Set local description failure for %s\n", role_.c_str(),
          error.message());
}
