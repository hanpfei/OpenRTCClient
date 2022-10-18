
#include <chrono>
#include <memory>
#include <thread>

#include "api/audio/audio_mixer.h"
#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/audio_options.h"
#include "api/create_peerconnection_factory.h"
#include "api/peer_connection_interface.h"
#include "api/rtp_sender_interface.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "media_test_utils/audio_utils.h"
#include "media_test_utils/conn_utils.h"
#include "media_test_utils/test_base.h"
#include "media_test_utils/vcm_capturer.h"
#include "media_test_utils/video_frame_file_writer_bmp.h"
#include "modules/video_capture/video_capture.h"
#include "modules/video_capture/video_capture_factory.h"
#include "pc/video_track_source.h"
#include "system_wrappers/include/field_trial.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#if defined(WEBRTC_WIN)
#include "rtc_base/win32_socket_init.h"
#endif

#if WEBRTC_ENABLE_PROTOBUF
#include "modules/audio_coding/audio_network_adaptor/config.pb.h"
#endif

static const char kAudioLabel[] = "audio_label";
static const char kVideoLabel[] = "video_label";
static const char kStreamId[] = "stream_id";

class ConnectionObserver : public webrtc::PeerConnectionObserver {
 public:
  ConnectionObserver(const std::string& tag) : tag_(tag) {}
  virtual ~ConnectionObserver() = default;

  void SetPeerConnection(
      rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection) {
    peer_connection_ = peer_connection;
    RTC_LOG(LS_INFO) << "[" << tag_ << "]: " << "Set peer connection %p and add ice candidate "
        << peer_connection_.get();
    while (!pending_ice_candidates_.empty()) {
      auto ice_candidate = *pending_ice_candidates_.begin();
      pending_ice_candidates_.erase(pending_ice_candidates_.begin());
      if (!peer_connection_->AddIceCandidate(ice_candidate.get())) {
        RTC_LOG(LS_WARNING) << "[" << tag_ << "]: "
            << "Failed to apply the received candidate when set "
            << "peerconnection";
      }
    }
  }

 public:
  // inherit from webrtc::PeerConnectionObserver
  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) override {}
  void OnAddTrack(
      rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
      const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>&
          streams) override {
    auto* track = receiver->track().get();
    if (track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind) {
      auto* video_track = static_cast<webrtc::VideoTrackInterface*>(track);
      auto video_sink = std::make_shared<FakeVideoSink>();
      video_sink->EnableWriteFile(true);
      video_track->AddOrUpdateSink(video_sink.get(), rtc::VideoSinkWants());
      video_sinks_.emplace_back(video_sink);
    }
    RTC_LOG(LS_WARNING) << "[" << tag_ << "]: " << "OnAddTrack " << track->kind();
  }
  void OnRemoveTrack(
      rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) override {
    RTC_LOG(LS_WARNING) << "[" << tag_ << "]: " << "OnRemoveTrack";
  }
  void OnDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override {}
  void OnRenegotiationNeeded() override {}
  void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) override {}
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override {}

  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override {
    std::string sdpMid = candidate->sdp_mid();
    int sdpMLineIndex = candidate->sdp_mline_index();
    std::string sdp;
    if (!candidate->ToString(&sdp)) {
      RTC_LOG(LS_WARNING) << "[" << tag_ << "]: " << "Failed to serialize candidate";
    }

    webrtc::SdpParseError error;
    std::shared_ptr<webrtc::IceCandidateInterface> tcandidate(
        webrtc::CreateIceCandidate(sdpMid, sdpMLineIndex, sdp, &error));
    if (!tcandidate.get()) {
      RTC_LOG(LS_WARNING) << "[" << tag_ << "]: " << "Can't parse received candidate message. "
          << "SdpParseError was: " << error.description;
      return;
    }
    if (!peer_connection_) {
      RTC_LOG(LS_WARNING) << "[" << tag_ << "]: " << "No valid peer connection";
      pending_ice_candidates_.emplace_back(tcandidate);
      return;
    }
    if (!peer_connection_->AddIceCandidate(tcandidate.get())) {
      RTC_LOG(LS_WARNING) << "[" << tag_ << "]: " << "Failed to apply the received candidate";
    }
  }

  void OnIceConnectionReceivingChange(bool receiving) override {}

  // Triggered when media is received on a new stream from remote peer.
  void OnAddStream(
      rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override {
    RTC_LOG(LS_WARNING) << "[" << tag_ << "]: " << "OnAddStream";
  }

  // Triggered when a remote peer closes a stream.
  void OnRemoveStream(
      rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override {
    RTC_LOG(LS_WARNING) << "[" << tag_ << "]: " << "OnRemoveStream";
  }

 private:
  std::string tag_;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  std::vector<std::shared_ptr<FakeVideoSink>> video_sinks_;
  std::vector<std::shared_ptr<webrtc::IceCandidateInterface>>
      pending_ice_candidates_;
};

class WebrtcPeerConnectionTest : public testing::Test {
 public:
  void SetUp() override {}

  void TearDown() override {}

  rtc::scoped_refptr<webrtc::PeerConnectionInterface> CreatePeerConnection(
      bool dtls,
      rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
          peer_connection_factory,
      std::shared_ptr<webrtc::PeerConnectionObserver> observer) {
    webrtc::PeerConnectionInterface::RTCConfiguration config;
    config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
    config.enable_dtls_srtp = dtls;
    webrtc::PeerConnectionInterface::IceServer server;
    server.uri = "stun:stun.l.google.com:19302";
    config.servers.push_back(server);
    config.set_video_codec_type(static_cast<int>(webrtc::kVideoCodecH264));

    auto peer_connection = peer_connection_factory->CreatePeerConnection(
        config, nullptr, nullptr, observer.get());
    return peer_connection;
  }
};

class AudioDeviceModuleGuard {
public:
  AudioDeviceModuleGuard(
      rtc::scoped_refptr<webrtc::AudioDeviceModule> adm,
      std::shared_ptr<rtc::Thread> thread) :
      adm_(adm), thread_(thread) {

  }
  ~AudioDeviceModuleGuard() {
    thread_->Invoke<int32_t>(RTC_FROM_HERE, [&]() {
      adm_ = nullptr;
      return 0;
    });
  }

  rtc::scoped_refptr<webrtc::AudioDeviceModule> adm() {
    return adm_;
  }
private:
  rtc::scoped_refptr<webrtc::AudioDeviceModule> adm_;
  std::shared_ptr<rtc::Thread> thread_;
};

static std::shared_ptr<AudioDeviceModuleGuard> CreateAudioDeviceModuleGuard(
    std::shared_ptr<rtc::Thread> thread,
    webrtc::TaskQueueFactory *task_queue_facotry) {
  std::shared_ptr<AudioDeviceModuleGuard> admGuard;
  thread->Invoke<int32_t>(RTC_FROM_HERE,
      [&]() {
        auto adm = webrtc::AudioDeviceModule::Create(
            webrtc::AudioDeviceModule::kPlatformDefaultAudio,
            task_queue_facotry);
        admGuard = std::make_shared<AudioDeviceModuleGuard>(adm, thread);
        return 0;
      });
  return admGuard;
}

static rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> CreatePeerConnectionFactory(
    std::shared_ptr<rtc::Thread> thread_holder,
    std::shared_ptr<rtc::Thread> thread,
    rtc::scoped_refptr<webrtc::AudioDeviceModule> adm) {
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory;
  thread_holder->Invoke<int32_t>(RTC_FROM_HERE,
      [&]() {
        peer_connection_factory = webrtc::CreatePeerConnectionFactory(
            nullptr /* network_thread */, thread.get() /* worker_thread */,
            nullptr /* signaling_thread */, adm /* default_adm */,
            webrtc::CreateBuiltinAudioEncoderFactory(),
            webrtc::CreateBuiltinAudioDecoderFactory(),
            webrtc::CreateBuiltinVideoEncoderFactory(),
            webrtc::CreateBuiltinVideoDecoderFactory(),
            nullptr /* audio_mixer */, nullptr /* audio_processing */);
        return 0;
      });
  return peer_connection_factory;
}

TEST_F(WebrtcPeerConnectionTest, peerconnection_connect) {
  // rtc::LogMessage::SetLogToStderr(false);

#if defined(WEBRTC_WIN)
  rtc::WinsockInitializer winsock_init;
#endif

  std::shared_ptr<rtc::Thread> thread_holder = rtc::Thread::Create();
  thread_holder->Start();

  auto task_queue_facotry = webrtc::CreateDefaultTaskQueueFactory();

  std::shared_ptr<rtc::Thread> thread = rtc::Thread::Create();
  thread->Start();

  auto admGuard = CreateAudioDeviceModuleGuard(thread,
      task_queue_facotry.get());

  /*-------- Create peer connection factory -----------*/
  auto peer_connection_factory = CreatePeerConnectionFactory(thread_holder,
      thread, admGuard->adm());
  ASSERT_TRUE(peer_connection_factory);

  /*-------- Create peer connection -----------*/
  auto connection_observer =
      std::make_shared<ConnectionObserver>("SendConnection");

  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_send;
  thread_holder->Invoke<int32_t>(RTC_FROM_HERE, [&]() {
    peer_connection_send = CreatePeerConnection(
        /*dtls*/ true, peer_connection_factory, connection_observer);
    return 0;
  });
  ASSERT_TRUE(peer_connection_send);

  /*-------- To create receive peer connection -----------*/
  auto conn_observer_recv =
      std::make_shared<ConnectionObserver>("RecvConnection");

  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_recv;
  thread_holder->Invoke<int32_t>(RTC_FROM_HERE, [&]() {
    peer_connection_recv = CreatePeerConnection(
        /*dtls*/ true, peer_connection_factory, conn_observer_recv);
    return 0;
  });
  ASSERT_TRUE(peer_connection_recv);

  /*-------- Create and add media tracks for send connection -----------*/
  rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track;
  rtc::scoped_refptr<webrtc::RtpSenderInterface> audio_sender;
  rtc::scoped_refptr<CapturerTrackSource> video_device;
  rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track;
  rtc::scoped_refptr<webrtc::RtpSenderInterface> video_sender;
  thread_holder->Invoke<int32_t>(RTC_FROM_HERE, [&]() {
    audio_track = peer_connection_factory->CreateAudioTrack(
        kAudioLabel,
        peer_connection_factory->CreateAudioSource(cricket::AudioOptions()));

    auto result_or_error =
        peer_connection_send->AddTrack(audio_track, {kStreamId});
    if (!result_or_error.ok()) {
      RTC_LOG(LS_WARNING) << "Failed to add audio track to PeerConnection: "
                  << result_or_error.error().message();
      return -1;
    }
    audio_sender = result_or_error.value();

    video_device = CapturerTrackSource::Create();
    if (video_device) {
      video_track =
          peer_connection_factory->CreateVideoTrack(kVideoLabel, video_device);

      result_or_error =
          peer_connection_send->AddTrack(video_track, {kStreamId});
      if (!result_or_error.ok()) {
        RTC_LOG(LS_WARNING) << "Failed to add video track to PeerConnection: "
            << result_or_error.error().message();
      }
      video_sender = result_or_error.value();
    } else {
      RTC_LOG(LS_WARNING) << "OpenVideoCaptureDevice failed";
    }
    return 0;
  });
  ASSERT_TRUE(audio_track);
  ASSERT_TRUE(video_device);
  ASSERT_TRUE(video_track);

  /*-------- Create offer for send connection -----------*/
  rtc::Event create_offer_event;
  rtc::scoped_refptr<
      CreateSessionDescriptionObserver<webrtc::PeerConnectionInterface>>
      create_session_desc_observer_sender(
          new rtc::RefCountedObject<CreateSessionDescriptionObserver<
              webrtc::PeerConnectionInterface>>(
              create_offer_event, peer_connection_send,
              SetSessionDescriptionObserver::Create(
                  "SendConnection:SetLocalDescription")));

  thread_holder->Invoke<int32_t>(RTC_FROM_HERE, [&]() {
    peer_connection_send->CreateOffer(
        create_session_desc_observer_sender.get(),
        webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
    return 0;
  });

  create_offer_event.Wait(10000);

  /*-------- Set remote description for receive connection --------*/
  webrtc::SdpParseError error;

  webrtc::SdpType sender_sdp_type =
      create_session_desc_observer_sender->GetSdpType();
  std::string sender_sdp = create_session_desc_observer_sender->GetSdpString();
  std::unique_ptr<webrtc::SessionDescriptionInterface> sender_session_desc =
      webrtc::CreateSessionDescription(sender_sdp_type, sender_sdp, &error);
  ASSERT_TRUE(sender_session_desc);

  EXPECT_EQ(sender_sdp_type, webrtc::SdpType::kOffer);
  RTC_LOG(LS_INFO) << "Sender SDP type " << create_session_desc_observer_sender->GetSdpTypeStr()
        << " and set remote description for receive connection";
  //  LOGI("Sender SDP type %s, content %s",
  //       create_session_desc_observer_sender->GetSdpTypeStr().c_str(),
  //       sender_sdp.c_str());

  peer_connection_recv->SetRemoteDescription(
      SetSessionDescriptionObserver::Create(
          "RecvConnection:SetRemoteDescription"),
      sender_session_desc.release());

  /*-------- Create answer for receive connection --------*/
  rtc::Event create_answer_event;
  rtc::scoped_refptr<
      CreateSessionDescriptionObserver<webrtc::PeerConnectionInterface>>
      session_desc_observer_recv(
          new rtc::RefCountedObject<CreateSessionDescriptionObserver<
              webrtc::PeerConnectionInterface>>(
              create_answer_event, peer_connection_recv,
              SetSessionDescriptionObserver::Create(
                  "RecvConnection:SetLocalDescription")));
  peer_connection_recv->CreateAnswer(
      session_desc_observer_recv.get(),
      webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
  create_answer_event.Wait(10000);

  /*-------- Set remote description for send connection -----------*/
  webrtc::SdpType recv_sdp_type = session_desc_observer_recv->GetSdpType();
  std::string recv_sdp = session_desc_observer_recv->GetSdpString();
  std::unique_ptr<webrtc::SessionDescriptionInterface> recv_session_desc =
      webrtc::CreateSessionDescription(recv_sdp_type, recv_sdp, &error);

  EXPECT_EQ(recv_sdp_type, webrtc::SdpType::kAnswer);
  RTC_LOG(LS_INFO) << "Receiver SDP type " << session_desc_observer_recv->GetSdpTypeStr()
      << " and set remote description for receive connection";
  //  LOGI("Receiver SDP type %s, content %s",
  //      session_desc_observer_recv->GetSdpTypeStr().c_str(),
  //      recv_sdp.c_str());

  if (!recv_session_desc) {
    RTC_LOG(LS_INFO) << "Can't parse received session description message. "
        << "SdpParseError was: %s" << error.description;
  }
  ASSERT_TRUE(recv_session_desc);

  peer_connection_send->SetRemoteDescription(
      SetSessionDescriptionObserver::Create(
          "SendConnection:SetRemoteDescription"),
      recv_session_desc.release());

  conn_observer_recv->SetPeerConnection(peer_connection_send);
  connection_observer->SetPeerConnection(peer_connection_recv);

  std::this_thread::sleep_for(std::chrono::milliseconds(10079));

  thread_holder->Invoke<int32_t>(RTC_FROM_HERE, [&]() {
    peer_connection_send->RemoveTrack(audio_sender);
    peer_connection_send->RemoveTrack(video_sender);
    video_track = nullptr;
    video_device = nullptr;
    audio_track = nullptr;
    peer_connection_factory = nullptr;
    return 0;
  });

  conn_observer_recv->SetPeerConnection(nullptr);
  connection_observer->SetPeerConnection(nullptr);
  peer_connection_recv = nullptr;
  peer_connection_send = nullptr;

  RTC_LOG(LS_INFO) << "End";
}

TEST_F(WebrtcPeerConnectionTest, peerconnection_connect_video_only) {
  // rtc::LogMessage::SetLogToStderr(false);

#if defined(WEBRTC_WIN)
  rtc::WinsockInitializer winsock_init;
#endif

  std::shared_ptr<rtc::Thread> thread_holder = rtc::Thread::Create();
  thread_holder->Start();

  auto task_queue_facotry = webrtc::CreateDefaultTaskQueueFactory();

  std::shared_ptr<rtc::Thread> thread = rtc::Thread::Create();
  thread->Start();

  auto admGuard = CreateAudioDeviceModuleGuard(thread,
      task_queue_facotry.get());

  /*-------- Create peer connection factory -----------*/
  auto peer_connection_factory = CreatePeerConnectionFactory(thread_holder,
      thread, admGuard->adm());
  ASSERT_TRUE(peer_connection_factory);

  /*-------- Create peer connection -----------*/
  auto connection_observer =
      std::make_shared<ConnectionObserver>("SendConnection");

  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_send;
  thread_holder->Invoke<int32_t>(RTC_FROM_HERE, [&]() {
    peer_connection_send = CreatePeerConnection(
        /*dtls*/ true, peer_connection_factory, connection_observer);
    return 0;
  });
  ASSERT_TRUE(peer_connection_send);

  /*-------- To create receive peer connection -----------*/
  auto conn_observer_recv =
      std::make_shared<ConnectionObserver>("RecvConnection");

  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_recv;
  thread_holder->Invoke<int32_t>(RTC_FROM_HERE, [&]() {
    peer_connection_recv = CreatePeerConnection(
        /*dtls*/ true, peer_connection_factory, conn_observer_recv);
    return 0;
  });
  ASSERT_TRUE(peer_connection_recv);

  /*-------- Create and add media tracks for send connection -----------*/
  rtc::scoped_refptr<CapturerTrackSource> video_device;
  rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track;
  rtc::scoped_refptr<webrtc::RtpSenderInterface> video_sender;
  thread_holder->Invoke<int32_t>(RTC_FROM_HERE, [&]() {
    video_device = CapturerTrackSource::Create();
    if (video_device) {
      video_track =
          peer_connection_factory->CreateVideoTrack(kVideoLabel, video_device);

      auto result_or_error =
          peer_connection_send->AddTrack(video_track, {kStreamId});
      if (!result_or_error.ok()) {
        RTC_LOG(LS_WARNING) << "Failed to add video track to PeerConnection: "
            << result_or_error.error().message();
      }
      video_sender = result_or_error.value();
    } else {
      RTC_LOG(LS_WARNING) << "OpenVideoCaptureDevice failed";
    }
    return 0;
  });
  ASSERT_TRUE(video_device);
  ASSERT_TRUE(video_track);

  /*-------- Create offer for send connection -----------*/
  rtc::Event create_offer_event;
  rtc::scoped_refptr<
      CreateSessionDescriptionObserver<webrtc::PeerConnectionInterface>>
      create_session_desc_observer_sender(
          new rtc::RefCountedObject<CreateSessionDescriptionObserver<
              webrtc::PeerConnectionInterface>>(
              create_offer_event, peer_connection_send,
              SetSessionDescriptionObserver::Create(
                  "SendConnection:SetLocalDescription")));

  thread_holder->Invoke<int32_t>(RTC_FROM_HERE, [&]() {
    peer_connection_send->CreateOffer(
        create_session_desc_observer_sender.get(),
        webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
    return 0;
  });

  create_offer_event.Wait(10000);

  /*-------- Set remote description for receive connection --------*/
  webrtc::SdpParseError error;

  webrtc::SdpType sender_sdp_type =
      create_session_desc_observer_sender->GetSdpType();
  std::string sender_sdp = create_session_desc_observer_sender->GetSdpString();
  std::unique_ptr<webrtc::SessionDescriptionInterface> sender_session_desc =
      webrtc::CreateSessionDescription(sender_sdp_type, sender_sdp, &error);
  ASSERT_TRUE(sender_session_desc);

  EXPECT_EQ(sender_sdp_type, webrtc::SdpType::kOffer);
  RTC_LOG(LS_INFO) << "Sender SDP type " << create_session_desc_observer_sender->GetSdpTypeStr()
        << " and set remote description for receive connection";
  //  LOGI("Sender SDP type %s, content %s",
  //       create_session_desc_observer_sender->GetSdpTypeStr().c_str(),
  //       sender_sdp.c_str());

  peer_connection_recv->SetRemoteDescription(
      SetSessionDescriptionObserver::Create(
          "RecvConnection:SetRemoteDescription"),
      sender_session_desc.release());

  /*-------- Create answer for receive connection --------*/
  rtc::Event create_answer_event;
  rtc::scoped_refptr<
      CreateSessionDescriptionObserver<webrtc::PeerConnectionInterface>>
      session_desc_observer_recv(
          new rtc::RefCountedObject<CreateSessionDescriptionObserver<
              webrtc::PeerConnectionInterface>>(
              create_answer_event, peer_connection_recv,
              SetSessionDescriptionObserver::Create(
                  "RecvConnection:SetLocalDescription")));
  peer_connection_recv->CreateAnswer(
      session_desc_observer_recv.get(),
      webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
  create_answer_event.Wait(10000);

  /*-------- Set remote description for send connection -----------*/
  webrtc::SdpType recv_sdp_type = session_desc_observer_recv->GetSdpType();
  std::string recv_sdp = session_desc_observer_recv->GetSdpString();
  std::unique_ptr<webrtc::SessionDescriptionInterface> recv_session_desc =
      webrtc::CreateSessionDescription(recv_sdp_type, recv_sdp, &error);

  EXPECT_EQ(recv_sdp_type, webrtc::SdpType::kAnswer);
  RTC_LOG(LS_INFO) << "Receiver SDP type " << session_desc_observer_recv->GetSdpTypeStr()
      << " and set remote description for receive connection";
  //  LOGI("Receiver SDP type %s, content %s",
  //      session_desc_observer_recv->GetSdpTypeStr().c_str(),
  //      recv_sdp.c_str());

  if (!recv_session_desc) {
    RTC_LOG(LS_INFO) << "Can't parse received session description message. "
        << "SdpParseError was: %s" << error.description;
  }
  ASSERT_TRUE(recv_session_desc);

  peer_connection_send->SetRemoteDescription(
      SetSessionDescriptionObserver::Create(
          "SendConnection:SetRemoteDescription"),
      recv_session_desc.release());

  conn_observer_recv->SetPeerConnection(peer_connection_send);
  connection_observer->SetPeerConnection(peer_connection_recv);

  std::this_thread::sleep_for(std::chrono::milliseconds(10079));

  thread_holder->Invoke<int32_t>(RTC_FROM_HERE, [&]() {
    peer_connection_send->RemoveTrack(video_sender);
    video_track = nullptr;
    video_device = nullptr;
    peer_connection_factory = nullptr;
    return 0;
  });

  conn_observer_recv->SetPeerConnection(nullptr);
  connection_observer->SetPeerConnection(nullptr);
  peer_connection_recv = nullptr;
  peer_connection_send = nullptr;

  RTC_LOG(LS_INFO) << "End";
}

class RecoverFieldTrial {
public:
  RecoverFieldTrial() :
      prev_field_trial_(webrtc::field_trial::GetFieldTrialString()) {}
  ~RecoverFieldTrial() {
    webrtc::field_trial::InitFieldTrialsFromString(prev_field_trial_);
  }

private:
  const char* prev_field_trial_;
};

TEST_F(WebrtcPeerConnectionTest, peerconnection_connect_enable_nack) {
  // rtc::LogMessage::SetLogToStderr(false);

  RecoverFieldTrial recover_field_trial;
  webrtc::field_trial::InitFieldTrialsFromString("WebRTC-Audio-NACK/Enabled/");

#if defined(WEBRTC_WIN)
  rtc::WinsockInitializer winsock_init;
#endif

  std::shared_ptr<rtc::Thread> thread_holder = rtc::Thread::Create();
  thread_holder->Start();

  auto task_queue_facotry = webrtc::CreateDefaultTaskQueueFactory();
  std::shared_ptr<rtc::Thread> thread = rtc::Thread::Create();
  thread->Start();

  auto admGuard = CreateAudioDeviceModuleGuard(thread,
      task_queue_facotry.get());

  /*-------- Create peer connection factory -----------*/
  auto peer_connection_factory = CreatePeerConnectionFactory(thread_holder,
      thread, admGuard->adm());
  ASSERT_TRUE(peer_connection_factory);

  /*-------- Create peer connection -----------*/
  auto connection_observer =
      std::make_shared<ConnectionObserver>("SendConnection");

  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_send;
  thread_holder->Invoke<int32_t>(RTC_FROM_HERE, [&]() {
    peer_connection_send = CreatePeerConnection(
        /*dtls*/ true, peer_connection_factory, connection_observer);
    return 0;
  });
  ASSERT_TRUE(peer_connection_send);

  /*-------- To create receive peer connection -----------*/
  auto conn_observer_recv =
      std::make_shared<ConnectionObserver>("RecvConnection");

  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_recv;
  thread_holder->Invoke<int32_t>(RTC_FROM_HERE, [&]() {
    peer_connection_recv = CreatePeerConnection(
        /*dtls*/ true, peer_connection_factory, conn_observer_recv);
    return 0;
  });
  ASSERT_TRUE(peer_connection_recv);

  /*-------- Create and add media tracks for send connection -----------*/
  rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track;
  rtc::scoped_refptr<webrtc::RtpSenderInterface> audio_sender;
  rtc::scoped_refptr<CapturerTrackSource> video_device;
  rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track;
  rtc::scoped_refptr<webrtc::RtpSenderInterface> video_sender;
  thread_holder->Invoke<int32_t>(RTC_FROM_HERE, [&]() {
    audio_track = peer_connection_factory->CreateAudioTrack(
        kAudioLabel,
        peer_connection_factory->CreateAudioSource(cricket::AudioOptions()));

    auto result_or_error =
        peer_connection_send->AddTrack(audio_track, {kStreamId});
    if (!result_or_error.ok()) {
      RTC_LOG(LS_WARNING) << "Failed to add audio track to PeerConnection: "
                  << result_or_error.error().message();
      return -1;
    }
    audio_sender = result_or_error.value();

    video_device = CapturerTrackSource::Create();
    if (video_device) {
      video_track =
          peer_connection_factory->CreateVideoTrack(kVideoLabel, video_device);

      result_or_error =
          peer_connection_send->AddTrack(video_track, {kStreamId});
      if (!result_or_error.ok()) {
        RTC_LOG(LS_WARNING) << "Failed to add video track to PeerConnection: "
            << result_or_error.error().message();
      }
      video_sender = result_or_error.value();
    } else {
      RTC_LOG(LS_WARNING) << "OpenVideoCaptureDevice failed";
    }
    return 0;
  });
  ASSERT_TRUE(audio_track);
  ASSERT_TRUE(video_device);
  ASSERT_TRUE(video_track);

  /*-------- Create offer for send connection -----------*/
  rtc::Event create_offer_event;
  rtc::scoped_refptr<
      CreateSessionDescriptionObserver<webrtc::PeerConnectionInterface>>
      create_session_desc_observer_sender(
          new rtc::RefCountedObject<CreateSessionDescriptionObserver<
              webrtc::PeerConnectionInterface>>(
              create_offer_event, peer_connection_send,
              SetSessionDescriptionObserver::Create(
                  "SendConnection:SetLocalDescription")));

  thread_holder->Invoke<int32_t>(RTC_FROM_HERE, [&]() {
    peer_connection_send->CreateOffer(
        create_session_desc_observer_sender.get(),
        webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
    return 0;
  });

  create_offer_event.Wait(10000);

  /*-------- Set remote description for receive connection --------*/
  webrtc::SdpParseError error;

  webrtc::SdpType sender_sdp_type =
      create_session_desc_observer_sender->GetSdpType();
  std::string sender_sdp = create_session_desc_observer_sender->GetSdpString();
  std::unique_ptr<webrtc::SessionDescriptionInterface> sender_session_desc =
      webrtc::CreateSessionDescription(sender_sdp_type, sender_sdp, &error);
  ASSERT_TRUE(sender_session_desc);

  EXPECT_EQ(sender_sdp_type, webrtc::SdpType::kOffer);
  RTC_LOG(LS_INFO) << "Sender SDP type " << create_session_desc_observer_sender->GetSdpTypeStr()
        << " and set remote description for receive connection";
  //  LOGI("Sender SDP type %s, content %s",
  //       create_session_desc_observer_sender->GetSdpTypeStr().c_str(),
  //       sender_sdp.c_str());

  peer_connection_recv->SetRemoteDescription(
      SetSessionDescriptionObserver::Create(
          "RecvConnection:SetRemoteDescription"),
      sender_session_desc.release());

  /*-------- Create answer for receive connection --------*/
  rtc::Event create_answer_event;
  rtc::scoped_refptr<
      CreateSessionDescriptionObserver<webrtc::PeerConnectionInterface>>
      session_desc_observer_recv(
          new rtc::RefCountedObject<CreateSessionDescriptionObserver<
              webrtc::PeerConnectionInterface>>(
              create_answer_event, peer_connection_recv,
              SetSessionDescriptionObserver::Create(
                  "RecvConnection:SetLocalDescription")));
  peer_connection_recv->CreateAnswer(
      session_desc_observer_recv.get(),
      webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
  create_answer_event.Wait(10000);

  /*-------- Set remote description for send connection -----------*/
  webrtc::SdpType recv_sdp_type = session_desc_observer_recv->GetSdpType();
  std::string recv_sdp = session_desc_observer_recv->GetSdpString();
  std::unique_ptr<webrtc::SessionDescriptionInterface> recv_session_desc =
      webrtc::CreateSessionDescription(recv_sdp_type, recv_sdp, &error);

  EXPECT_EQ(recv_sdp_type, webrtc::SdpType::kAnswer);
  RTC_LOG(LS_INFO) << "Receiver SDP type " << session_desc_observer_recv->GetSdpTypeStr()
      << " and set remote description for receive connection";
  //  LOGI("Receiver SDP type %s, content %s",
  //      session_desc_observer_recv->GetSdpTypeStr().c_str(),
  //      recv_sdp.c_str());

  if (!recv_session_desc) {
    RTC_LOG(LS_INFO) << "Can't parse received session description message. "
        << "SdpParseError was: %s" << error.description;
  }
  ASSERT_TRUE(recv_session_desc);

  peer_connection_send->SetRemoteDescription(
      SetSessionDescriptionObserver::Create(
          "SendConnection:SetRemoteDescription"),
      recv_session_desc.release());

  conn_observer_recv->SetPeerConnection(peer_connection_send);
  connection_observer->SetPeerConnection(peer_connection_recv);

  std::this_thread::sleep_for(std::chrono::milliseconds(10079));

  thread_holder->Invoke<int32_t>(RTC_FROM_HERE, [&]() {
    peer_connection_send->RemoveTrack(audio_sender);
    peer_connection_send->RemoveTrack(video_sender);
    video_track = nullptr;
    video_device = nullptr;
    audio_track = nullptr;
    peer_connection_factory = nullptr;
    return 0;
  });

  conn_observer_recv->SetPeerConnection(nullptr);
  connection_observer->SetPeerConnection(nullptr);
  peer_connection_recv = nullptr;
  peer_connection_send = nullptr;

  RTC_LOG(LS_INFO) << "End";
}

TEST_F(WebrtcPeerConnectionTest, connection_audio_echo) {
  // rtc::LogMessage::SetLogToStderr(false);

#if defined(WEBRTC_WIN)
  rtc::WinsockInitializer winsock_init;
#endif

  std::shared_ptr<rtc::Thread> thread_holder = rtc::Thread::Create();
  thread_holder->Start();

  auto task_queue_facotry = webrtc::CreateDefaultTaskQueueFactory();

  std::shared_ptr<rtc::Thread> thread = rtc::Thread::Create();
  thread->Start();

  auto admGuard = CreateAudioDeviceModuleGuard(thread,
      task_queue_facotry.get());

  /*-------- Create peer connection factory -----------*/
  auto peer_connection_factory = CreatePeerConnectionFactory(thread_holder,
      thread, admGuard->adm());
  ASSERT_TRUE(peer_connection_factory);

  /*-------- Create peer connection -----------*/
  auto connection_observer =
      std::make_shared<ConnectionObserver>("SendConnection");

  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_send;
  thread_holder->Invoke<int32_t>(RTC_FROM_HERE, [&]() {
    peer_connection_send = CreatePeerConnection(
        /*dtls*/ true, peer_connection_factory, connection_observer);
    return 0;
  });
  ASSERT_TRUE(peer_connection_send);

  /*-------- To create receive peer connection -----------*/
  auto conn_observer_recv =
      std::make_shared<ConnectionObserver>("RecvConnection");

  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_recv;
  thread_holder->Invoke<int32_t>(RTC_FROM_HERE, [&]() {
    peer_connection_recv = CreatePeerConnection(
        /*dtls*/ true, peer_connection_factory, conn_observer_recv);
    return 0;
  });
  ASSERT_TRUE(peer_connection_recv);

  /*-------- Create and add media tracks for send connection -----------*/
  rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track;
  rtc::scoped_refptr<webrtc::RtpSenderInterface> audio_sender;
  thread_holder->Invoke<int32_t>(RTC_FROM_HERE, [&]() {
    audio_track = peer_connection_factory->CreateAudioTrack(
        kAudioLabel,
        peer_connection_factory->CreateAudioSource(cricket::AudioOptions()));

    auto result_or_error =
        peer_connection_send->AddTrack(audio_track, {kStreamId});
    if (!result_or_error.ok()) {
      RTC_LOG(LS_WARNING) << "Failed to add audio track to PeerConnection: "
                  << result_or_error.error().message();
      return -1;
    }
    audio_sender = result_or_error.value();

    return 0;
  });
  ASSERT_TRUE(audio_track);

  /*-------- Create offer for send connection -----------*/
  rtc::Event create_offer_event;
  rtc::scoped_refptr<
      CreateSessionDescriptionObserver<webrtc::PeerConnectionInterface>>
      create_session_desc_observer_sender(
          new rtc::RefCountedObject<CreateSessionDescriptionObserver<
              webrtc::PeerConnectionInterface>>(
              create_offer_event, peer_connection_send,
              SetSessionDescriptionObserver::Create(
                  "SendConnection:SetLocalDescription")));

  thread_holder->Invoke<int32_t>(RTC_FROM_HERE, [&]() {
    peer_connection_send->CreateOffer(
        create_session_desc_observer_sender.get(),
        webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
    return 0;
  });

  create_offer_event.Wait(10000);

  /*-------- Set remote description for receive connection --------*/
  webrtc::SdpParseError error;

  webrtc::SdpType sender_sdp_type =
      create_session_desc_observer_sender->GetSdpType();
  std::string sender_sdp = create_session_desc_observer_sender->GetSdpString();
  std::unique_ptr<webrtc::SessionDescriptionInterface> sender_session_desc =
      webrtc::CreateSessionDescription(sender_sdp_type, sender_sdp, &error);
  ASSERT_TRUE(sender_session_desc);

  EXPECT_EQ(sender_sdp_type, webrtc::SdpType::kOffer);
  RTC_LOG(LS_INFO) << "Sender SDP type " << create_session_desc_observer_sender->GetSdpTypeStr()
        << " and set remote description for receive connection";
  //  LOGI("Sender SDP type %s, content %s",
  //       create_session_desc_observer_sender->GetSdpTypeStr().c_str(),
  //       sender_sdp.c_str());

  peer_connection_recv->SetRemoteDescription(
      SetSessionDescriptionObserver::Create(
          "RecvConnection:SetRemoteDescription"),
      sender_session_desc.release());

  /*-------- Create answer for receive connection --------*/
  rtc::Event create_answer_event;
  rtc::scoped_refptr<
      CreateSessionDescriptionObserver<webrtc::PeerConnectionInterface>>
      session_desc_observer_recv(
          new rtc::RefCountedObject<CreateSessionDescriptionObserver<
              webrtc::PeerConnectionInterface>>(
              create_answer_event, peer_connection_recv,
              SetSessionDescriptionObserver::Create(
                  "RecvConnection:SetLocalDescription")));
  peer_connection_recv->CreateAnswer(
      session_desc_observer_recv.get(),
      webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
  create_answer_event.Wait(10000);

  /*-------- Set remote description for send connection -----------*/
  webrtc::SdpType recv_sdp_type = session_desc_observer_recv->GetSdpType();
  std::string recv_sdp = session_desc_observer_recv->GetSdpString();
  std::unique_ptr<webrtc::SessionDescriptionInterface> recv_session_desc =
      webrtc::CreateSessionDescription(recv_sdp_type, recv_sdp, &error);

  EXPECT_EQ(recv_sdp_type, webrtc::SdpType::kAnswer);
  RTC_LOG(LS_INFO) << "Receiver SDP type " << session_desc_observer_recv->GetSdpTypeStr()
      << " and set remote description for receive connection";
  //  LOGI("Receiver SDP type %s, content %s",
  //      session_desc_observer_recv->GetSdpTypeStr().c_str(),
  //      recv_sdp.c_str());

  if (!recv_session_desc) {
    RTC_LOG(LS_INFO) << "Can't parse received session description message. "
        << "SdpParseError was: %s" << error.description;
  }
  ASSERT_TRUE(recv_session_desc);

  peer_connection_send->SetRemoteDescription(
      SetSessionDescriptionObserver::Create(
          "SendConnection:SetRemoteDescription"),
      recv_session_desc.release());

  conn_observer_recv->SetPeerConnection(peer_connection_send);
  connection_observer->SetPeerConnection(peer_connection_recv);

  std::this_thread::sleep_for(std::chrono::milliseconds(10079));

  thread_holder->Invoke<int32_t>(RTC_FROM_HERE, [&]() {
    peer_connection_send->RemoveTrack(audio_sender);
    audio_track = nullptr;
    peer_connection_factory = nullptr;
    return 0;
  });

  conn_observer_recv->SetPeerConnection(nullptr);
  connection_observer->SetPeerConnection(nullptr);
  peer_connection_recv = nullptr;
  peer_connection_send = nullptr;

  RTC_LOG(LS_INFO) << "End";
}

TEST_F(WebrtcPeerConnectionTest, audio_network_adaptive_config) {
  std::list<ANAConfigItem> configlist = {
      {ANAConfigType::kFecController, 3214, 0.23f},
      {ANAConfigType::kFrameLengthController, 7219, 0.37f},
      {ANAConfigType::kBitrateController, 0, 0.37f},
      {ANAConfigType::kChannelController, 0, 0.0f}
  };

  std::string config_string;
  BuildANAConfigString(configlist, &config_string);

  webrtc::audio_network_adaptor::config::ControllerManager controller_manager_config;
  ASSERT_TRUE(controller_manager_config.ParseFromString(config_string));
  for (int i = 0; i < controller_manager_config.controllers_size(); ++i) {
    auto& controller_config = controller_manager_config.controllers(i);
    switch (controller_config.controller_case()) {
      case webrtc::audio_network_adaptor::config::Controller::kFecController:
        RTC_LOG(LS_INFO) << "Controller::kFecController!!!";
        break;
      case webrtc::audio_network_adaptor::config::Controller::kFecControllerRplrBased:
        // FecControllerRplrBased has been removed and can't be used anymore.
        RTC_LOG(LS_INFO) << "Controller::kFecControllerRplrBased!!!";
        continue;
      case webrtc::audio_network_adaptor::config::Controller::kFrameLengthController:
        RTC_LOG(LS_INFO) << "Controller::kFrameLengthController!!!";
        break;
      case webrtc::audio_network_adaptor::config::Controller::kChannelController:
        RTC_LOG(LS_INFO) << "Controller::kChannelController!!!";
        break;
      case webrtc::audio_network_adaptor::config::Controller::kDtxController:
        RTC_LOG(LS_INFO) << "Controller::kDtxController!!!";
        break;
      case webrtc::audio_network_adaptor::config::Controller::kBitrateController:
        RTC_LOG(LS_INFO) << "Controller::kBitrateController!!!";
        break;
      case webrtc::audio_network_adaptor::config::Controller::kFrameLengthControllerV2:
        RTC_LOG(LS_INFO) << "Controller::kFrameLengthControllerV2!!!";
        break;
      default:
        break;
    }
    if (controller_config.has_scoring_point()) {
      auto& scoring_point = controller_config.scoring_point();
      RTC_LOG(LS_INFO) << "has_uplink_bandwidth_bps "
          << scoring_point.has_uplink_bandwidth_bps()
          << ", has_uplink_packet_loss_fraction "
          << scoring_point.has_uplink_packet_loss_fraction()
          << ", uplink_bandwidth_bps "
          << scoring_point.uplink_bandwidth_bps()
          << ", uplink_packet_loss_fraction "
          << scoring_point.uplink_packet_loss_fraction();
    }
  }
}

TEST_F(WebrtcPeerConnectionTest, connection_audio_enable_ana) {
  // rtc::LogMessage::SetLogToStderr(false);

#if defined(WEBRTC_WIN)
  rtc::WinsockInitializer winsock_init;
#endif

  std::shared_ptr<rtc::Thread> thread_holder = rtc::Thread::Create();
  thread_holder->Start();

  auto task_queue_facotry = webrtc::CreateDefaultTaskQueueFactory();

  std::shared_ptr<rtc::Thread> thread = rtc::Thread::Create();
  thread->Start();

  auto admGuard = CreateAudioDeviceModuleGuard(thread,
      task_queue_facotry.get());

  /*-------- Create peer connection factory -----------*/
  auto peer_connection_factory = CreatePeerConnectionFactory(thread_holder,
      thread, admGuard->adm());
  ASSERT_TRUE(peer_connection_factory);

  /*-------- Create peer connection -----------*/
  auto connection_observer =
      std::make_shared<ConnectionObserver>("SendConnection");

  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_send;
  thread_holder->Invoke<int32_t>(RTC_FROM_HERE, [&]() {
    peer_connection_send = CreatePeerConnection(
        /*dtls*/ true, peer_connection_factory, connection_observer);
    return 0;
  });
  ASSERT_TRUE(peer_connection_send);

  /*-------- To create receive peer connection -----------*/
  auto conn_observer_recv =
      std::make_shared<ConnectionObserver>("RecvConnection");

  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_recv;
  thread_holder->Invoke<int32_t>(RTC_FROM_HERE, [&]() {
    peer_connection_recv = CreatePeerConnection(
        /*dtls*/ true, peer_connection_factory, conn_observer_recv);
    return 0;
  });
  ASSERT_TRUE(peer_connection_recv);

  /*-------- Create and add media tracks for send connection -----------*/
  rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track;
  rtc::scoped_refptr<webrtc::RtpSenderInterface> audio_sender;
  thread_holder->Invoke<int32_t>(RTC_FROM_HERE, [&]() {
    cricket::AudioOptions audio_options;
#if WEBRTC_ENABLE_PROTOBUF
    audio_options.audio_network_adaptor = true;
    std::list<ANAConfigItem> configlist = {
          {ANAConfigType::kFecController, 3214, 0.23f},
//          {ANAConfigType::kFrameLengthController, 7219, 0.37f},
//          {ANAConfigType::kBitrateController, 0, 0.37f},
//          {ANAConfigType::kChannelController, 0, 0.0f}
      };

    std::string config_string;
    BuildANAConfigString(configlist, &config_string);
    audio_options.audio_network_adaptor_config = config_string;
#endif
    audio_track = peer_connection_factory->CreateAudioTrack(
        kAudioLabel,
        peer_connection_factory->CreateAudioSource(audio_options));

    auto result_or_error =
        peer_connection_send->AddTrack(audio_track, {kStreamId});
    if (!result_or_error.ok()) {
      RTC_LOG(LS_WARNING) << "Failed to add audio track to PeerConnection: "
                  << result_or_error.error().message();
      return -1;
    }
    audio_sender = result_or_error.value();

    return 0;
  });
  ASSERT_TRUE(audio_track);

  /*-------- Create offer for send connection -----------*/
  rtc::Event create_offer_event;
  rtc::scoped_refptr<
      CreateSessionDescriptionObserver<webrtc::PeerConnectionInterface>>
      create_session_desc_observer_sender(
          new rtc::RefCountedObject<CreateSessionDescriptionObserver<
              webrtc::PeerConnectionInterface>>(
              create_offer_event, peer_connection_send,
              SetSessionDescriptionObserver::Create(
                  "SendConnection:SetLocalDescription")));

  thread_holder->Invoke<int32_t>(RTC_FROM_HERE, [&]() {
    peer_connection_send->CreateOffer(
        create_session_desc_observer_sender.get(),
        webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
    return 0;
  });

  create_offer_event.Wait(10000);

  /*-------- Set remote description for receive connection --------*/
  webrtc::SdpParseError error;

  webrtc::SdpType sender_sdp_type =
      create_session_desc_observer_sender->GetSdpType();
  std::string sender_sdp = create_session_desc_observer_sender->GetSdpString();
  std::unique_ptr<webrtc::SessionDescriptionInterface> sender_session_desc =
      webrtc::CreateSessionDescription(sender_sdp_type, sender_sdp, &error);
  ASSERT_TRUE(sender_session_desc);

  EXPECT_EQ(sender_sdp_type, webrtc::SdpType::kOffer);
  RTC_LOG(LS_INFO) << "Sender SDP type " << create_session_desc_observer_sender->GetSdpTypeStr()
        << " and set remote description for receive connection";
  //  LOGI("Sender SDP type %s, content %s",
  //       create_session_desc_observer_sender->GetSdpTypeStr().c_str(),
  //       sender_sdp.c_str());

  peer_connection_recv->SetRemoteDescription(
      SetSessionDescriptionObserver::Create(
          "RecvConnection:SetRemoteDescription"),
      sender_session_desc.release());

  /*-------- Create answer for receive connection --------*/
  rtc::Event create_answer_event;
  rtc::scoped_refptr<
      CreateSessionDescriptionObserver<webrtc::PeerConnectionInterface>>
      session_desc_observer_recv(
          new rtc::RefCountedObject<CreateSessionDescriptionObserver<
              webrtc::PeerConnectionInterface>>(
              create_answer_event, peer_connection_recv,
              SetSessionDescriptionObserver::Create(
                  "RecvConnection:SetLocalDescription")));
  peer_connection_recv->CreateAnswer(
      session_desc_observer_recv.get(),
      webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
  create_answer_event.Wait(10000);

  /*-------- Set remote description for send connection -----------*/
  webrtc::SdpType recv_sdp_type = session_desc_observer_recv->GetSdpType();
  std::string recv_sdp = session_desc_observer_recv->GetSdpString();
  std::unique_ptr<webrtc::SessionDescriptionInterface> recv_session_desc =
      webrtc::CreateSessionDescription(recv_sdp_type, recv_sdp, &error);

  EXPECT_EQ(recv_sdp_type, webrtc::SdpType::kAnswer);
  RTC_LOG(LS_INFO) << "Receiver SDP type " << session_desc_observer_recv->GetSdpTypeStr()
      << " and set remote description for receive connection";
  //  LOGI("Receiver SDP type %s, content %s",
  //      session_desc_observer_recv->GetSdpTypeStr().c_str(),
  //      recv_sdp.c_str());

  if (!recv_session_desc) {
    RTC_LOG(LS_INFO) << "Can't parse received session description message. "
        << "SdpParseError was: %s" << error.description;
  }
  ASSERT_TRUE(recv_session_desc);

  peer_connection_send->SetRemoteDescription(
      SetSessionDescriptionObserver::Create(
          "SendConnection:SetRemoteDescription"),
      recv_session_desc.release());

  conn_observer_recv->SetPeerConnection(peer_connection_send);
  connection_observer->SetPeerConnection(peer_connection_recv);

  std::this_thread::sleep_for(std::chrono::milliseconds(10079));

  thread_holder->Invoke<int32_t>(RTC_FROM_HERE, [&]() {
    peer_connection_send->RemoveTrack(audio_sender);
    audio_track = nullptr;
    peer_connection_factory = nullptr;
    return 0;
  });

  conn_observer_recv->SetPeerConnection(nullptr);
  connection_observer->SetPeerConnection(nullptr);
  peer_connection_recv = nullptr;
  peer_connection_send = nullptr;

  RTC_LOG(LS_INFO) << "End";
}
