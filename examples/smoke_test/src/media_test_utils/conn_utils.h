
#pragma once

#include <string>

#include "api/jsep.h"
#include "api/scoped_refptr.h"
#include "rtc_base/event.h"

class SetSessionDescriptionObserver
    : public webrtc::SetSessionDescriptionObserver {
 public:
  static SetSessionDescriptionObserver* Create(const std::string& tag);
  SetSessionDescriptionObserver(const std::string& tag);
  SetSessionDescriptionObserver();
  void OnSuccess() override;
  void OnFailure(webrtc::RTCError error) override;

  void SetDescription(
      std::unique_ptr<webrtc::SessionDescriptionInterface>&& desc);

 private:
  std::string tag_;
  std::unique_ptr<webrtc::SessionDescriptionInterface> desc_;
};

template <typename ConnectionType>
class CreateSessionDescriptionObserver
    : public webrtc::CreateSessionDescriptionObserver {
 public:
  CreateSessionDescriptionObserver(
      rtc::Event& event,
      rtc::scoped_refptr<ConnectionType> peer_connection,
      rtc::scoped_refptr<SetSessionDescriptionObserver>
          set_session_desc_observer)
      : event_(event),
        peer_connection_(peer_connection),
        set_session_desc_observer_(set_session_desc_observer),
        sdp_type_(webrtc::SdpType::kOffer) {}

  virtual ~CreateSessionDescriptionObserver() = default;

  void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
    sdp_type_ = desc->GetType();
    sdp_type_str_ = desc->type();
    desc->ToString(&sdp_);

    peer_connection_->SetLocalDescription(set_session_desc_observer_.get(),
                                          desc);

    event_.Set();
  }

  // The OnFailure callback takes an RTCError, which consists of an
  // error code and a string.
  // RTCError is non-copyable, so it must be passed using std::move.
  // Earlier versions of the API used a string argument. This version
  // is removed; its functionality was the same as passing
  // error.message.
  void OnFailure(webrtc::RTCError error) override { event_.Set(); }

  webrtc::SdpType GetSdpType() { return sdp_type_; }

  std::string GetSdpTypeStr() { return sdp_type_str_; }

  std::string GetSdpString() { return sdp_; }

 private:
  rtc::Event& event_;
  rtc::scoped_refptr<ConnectionType> peer_connection_;
  rtc::scoped_refptr<SetSessionDescriptionObserver> set_session_desc_observer_;
  webrtc::SdpType sdp_type_;
  std::string sdp_type_str_;
  std::string sdp_;
};
