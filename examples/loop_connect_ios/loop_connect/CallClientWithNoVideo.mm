
#import <Foundation/Foundation.h>

#import "CallClientWithNoVideo.h"

#include <future>
#include <memory>
#include <utility>

#import "sdk/objc/base/RTCVideoRenderer.h"
#import "sdk/objc/components/video_codec/RTCDefaultVideoDecoderFactory.h"
#import "sdk/objc/components/video_codec/RTCDefaultVideoEncoderFactory.h"
#import "sdk/objc/helpers/RTCCameraPreviewView.h"

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_event_log/rtc_event_log_factory.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "media/engine/webrtc_media_engine.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "sdk/objc/native/api/video_capturer.h"
#include "sdk/objc/native/api/video_decoder_factory.h"
#include "sdk/objc/native/api/video_encoder_factory.h"
#include "sdk/objc/native/api/video_renderer.h"

static const char kAudioLabel[] = "audio_label";
static const char kVideoLabel[] = "video_label";
static const char kStreamId[] = "stream_id";

namespace {

class CreateOfferObserver : public webrtc::CreateSessionDescriptionObserver {
 public:
  explicit CreateOfferObserver(const std::string &role);
  virtual ~CreateOfferObserver();

  void OnSuccess(webrtc::SessionDescriptionInterface* desc) override;
  void OnFailure(webrtc::RTCError error) override;
  
  std::future<std::string> GetSDP();

 private:
  const std::string role_;
  std::promise<std::string> sdp_promise_;
};

class SetRemoteSessionDescriptionObserver : public webrtc::SetRemoteDescriptionObserverInterface {
 public:
  explicit SetRemoteSessionDescriptionObserver(const std::string &role);
  virtual ~SetRemoteSessionDescriptionObserver();
  void OnSetRemoteDescriptionComplete(webrtc::RTCError error) override;

private:
  const std::string role_;
};

class SetLocalSessionDescriptionObserver : public webrtc::SetSessionDescriptionObserver {
 public:
  explicit SetLocalSessionDescriptionObserver(const std::string &role);
  virtual ~SetLocalSessionDescriptionObserver();
  void OnSuccess() override;
  void OnFailure(webrtc::RTCError error) override;

private:
  const std::string role_;
};

}  // namespace

CallClientWithNoVideo::CallClientWithNoVideo() : call_started_(false) {
  thread_checker_.Detach();
  CreatePeerConnectionFactory();
}

void CallClientWithNoVideo::CreatePeerConnectionFactory() {
  network_thread_ = rtc::Thread::CreateWithSocketServer();
  network_thread_->SetName("network_thread", nullptr);
  RTC_CHECK(network_thread_->Start()) << "Failed to start thread";

  worker_thread_ = rtc::Thread::Create();
  worker_thread_->SetName("worker_thread", nullptr);
  RTC_CHECK(worker_thread_->Start()) << "Failed to start thread";

  signaling_thread_ = rtc::Thread::Create();
  signaling_thread_->SetName("signaling_thread", nullptr);
  RTC_CHECK(signaling_thread_->Start()) << "Failed to start thread";

  webrtc::PeerConnectionFactoryDependencies dependencies;
    
  dependencies.network_thread = network_thread_.get();
  dependencies.worker_thread = worker_thread_.get();
  dependencies.signaling_thread = signaling_thread_.get();

  dependencies.task_queue_factory = webrtc::CreateDefaultTaskQueueFactory();

  cricket::MediaEngineDependencies media_deps;

  media_deps.task_queue_factory = dependencies.task_queue_factory.get();
  media_deps.audio_encoder_factory = webrtc::CreateBuiltinAudioEncoderFactory();
  media_deps.audio_decoder_factory = webrtc::CreateBuiltinAudioDecoderFactory();

  media_deps.video_encoder_factory = webrtc::ObjCToNativeVideoEncoderFactory(
      [[RTC_OBJC_TYPE(RTCDefaultVideoEncoderFactory) alloc] init]);
  media_deps.video_decoder_factory = webrtc::ObjCToNativeVideoDecoderFactory(
      [[RTC_OBJC_TYPE(RTCDefaultVideoDecoderFactory) alloc] init]);

  media_deps.audio_processing = webrtc::AudioProcessingBuilder().Create();

  dependencies.media_engine = cricket::CreateMediaEngine(std::move(media_deps));

  RTC_LOG(LS_INFO) << "Media engine created: " << dependencies.media_engine.get();
  dependencies.call_factory = webrtc::CreateCallFactory();
  dependencies.event_log_factory =
      std::make_unique<webrtc::RtcEventLogFactory>(dependencies.task_queue_factory.get());
  pcf_ = webrtc::CreateModularPeerConnectionFactory(std::move(dependencies));
  RTC_LOG(LS_INFO) << "PeerConnectionFactory created: " << pcf_;
}

bool CallClientWithNoVideo::LoopCallStarted() {
  webrtc::MutexLock lock(&pc_mutex_);
  return call_started_;
}

void CallClientWithNoVideo::CreateConnections() {
  webrtc::MutexLock lock(&pc_mutex_);
  
  webrtc::PeerConnectionInterface::RTCConfiguration config;
  config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
  // DTLS SRTP has to be disabled for loopback to work.
  config.enable_dtls_srtp = false;

  send_observer_ = std::make_unique<ConnectionObserver>("SendConn");

  webrtc::PeerConnectionDependencies pc_dependencies(send_observer_.get());
  send_connection_ = pcf_->CreatePeerConnection(config, std::move(pc_dependencies));
  RTC_LOG(LS_INFO) << "Send PeerConnection created: " << send_connection_;

  recv_observer_ = std::make_unique<ConnectionObserver>("RecvConn");

  webrtc::PeerConnectionDependencies recv_pc_dependencies(recv_observer_.get());
  recv_connection_ = pcf_->CreatePeerConnection(config, std::move(recv_pc_dependencies));
  RTC_LOG(LS_INFO) << "Receive PeerConnection created: " << recv_connection_;
  
  send_connection_->AddTrack(local_audio_track_, {kStreamId});
  RTC_LOG(LS_INFO) << "Local auido track set up: " << local_audio_track_;
}

void CallClientWithNoVideo::Connect() {
  webrtc::MutexLock lock(&pc_mutex_);
  
  std::string send_role("SendConn");
  rtc::scoped_refptr<CreateOfferObserver> observer(new rtc::RefCountedObject<CreateOfferObserver>(send_role));
  send_connection_->CreateOffer(observer,
                                webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
  
  std::future<std::string> sdp_future = observer->GetSDP();
  std::string sdp = sdp_future.get();
  
  std::unique_ptr<webrtc::SessionDescriptionInterface> send_offer(
      webrtc::CreateSessionDescription(webrtc::SdpType::kOffer, sdp));
  send_connection_->SetLocalDescription(new rtc::RefCountedObject<SetLocalSessionDescriptionObserver>("SendConn"),
                                        send_offer.release());

  std::unique_ptr<webrtc::SessionDescriptionInterface> recv_send_offer(
      webrtc::CreateSessionDescription(webrtc::SdpType::kOffer, sdp));
  recv_connection_->SetRemoteDescription(std::move(recv_send_offer),
                            new rtc::RefCountedObject<SetRemoteSessionDescriptionObserver>("RecvConn"));

  rtc::scoped_refptr<CreateOfferObserver> recv_answer_observer(new rtc::RefCountedObject<CreateOfferObserver>("RecvConn"));
  recv_connection_->CreateAnswer(recv_answer_observer,
                                 webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
  
  std::future<std::string> recv_sdp_answer_future = recv_answer_observer->GetSDP();
  std::string recv_sdp_answer = recv_sdp_answer_future.get();
  
  std::unique_ptr<webrtc::SessionDescriptionInterface> recv_answer(
      webrtc::CreateSessionDescription(webrtc::SdpType::kAnswer, recv_sdp_answer));
  send_connection_->SetRemoteDescription(std::move(recv_answer),
                                         new rtc::RefCountedObject<SetRemoteSessionDescriptionObserver>("SendConn"));

  std::unique_ptr<webrtc::SessionDescriptionInterface> recv_recv_answer(
      webrtc::CreateSessionDescription(webrtc::SdpType::kAnswer, recv_sdp_answer));
  recv_connection_->SetLocalDescription(new rtc::RefCountedObject<SetLocalSessionDescriptionObserver>("RecvConn"),
                                        recv_recv_answer.release());
  
  send_observer_->SetConnection(recv_connection_);
  recv_observer_->SetConnection(send_connection_);
}

void CallClientWithNoVideo::StartLoopCall() {
  {
    webrtc::MutexLock lock(&pc_mutex_);
    if (call_started_) {
      RTC_LOG(LS_WARNING) << "Call already started.";
      return;
    }
    call_started_ = true;
  }

  cricket::AudioOptions audio_options;
  audio_source_ = pcf_->CreateAudioSource(audio_options);
  
  local_audio_track_ = pcf_->CreateAudioTrack(kAudioLabel, audio_source_);

  CreateConnections();
  Connect();
}

void CallClientWithNoVideo::DestroyConnections() {
  webrtc::MutexLock lock(&pc_mutex_);
  for (const rtc::scoped_refptr<webrtc::RtpTransceiverInterface>& tranceiver :
       send_connection_->GetTransceivers()) {
    send_connection_->RemoveTrack(tranceiver->sender().get());
  }

  send_observer_->SetConnection(nullptr);
  recv_observer_->SetConnection(nullptr);

  send_connection_ = nullptr;
  recv_connection_ = nullptr;
  local_audio_track_ = nullptr;
  audio_source_ = nullptr;
}

void CallClientWithNoVideo::StopLoopCall() {
  DestroyConnections();

  webrtc::MutexLock lock(&pc_mutex_);
  if (!call_started_) {
    RTC_LOG(LS_WARNING) << "Call already started.";
    return;
  }
  call_started_ = false;
}

CreateOfferObserver::CreateOfferObserver(const std::string &role)
    : role_(role) {}

CreateOfferObserver::~CreateOfferObserver() = default;

void CreateOfferObserver::OnSuccess(webrtc::SessionDescriptionInterface* desc) {
  std::string sdp;
  desc->ToString(&sdp);

  sdp_promise_.set_value(sdp);
}

void CreateOfferObserver::OnFailure(webrtc::RTCError error) {
  RTC_LOG(LS_INFO) << role_ << ": " << "Failed to create offer: " << error.message();
  sdp_promise_.set_value("");
}

std::future<std::string> CreateOfferObserver::GetSDP() {
  return sdp_promise_.get_future();
}

SetRemoteSessionDescriptionObserver::SetRemoteSessionDescriptionObserver(const std::string &role) : role_(role) {}

SetRemoteSessionDescriptionObserver::~SetRemoteSessionDescriptionObserver() = default;

void SetRemoteSessionDescriptionObserver::OnSetRemoteDescriptionComplete(webrtc::RTCError error) {
  RTC_LOG(LS_INFO) << role_ << ": " << "Set remote description: " << error.message();
}

SetLocalSessionDescriptionObserver::SetLocalSessionDescriptionObserver(const std::string &role) : role_(role) {}

SetLocalSessionDescriptionObserver::~SetLocalSessionDescriptionObserver() = default;

void SetLocalSessionDescriptionObserver::OnSuccess() {
  RTC_LOG(LS_INFO) << role_ << ": " << "Set local description success!";
}

void SetLocalSessionDescriptionObserver::OnFailure(webrtc::RTCError error) {
  RTC_LOG(LS_INFO) << role_ << ": " << "Set local description failure: " << error.message();
}
