
#include "conn_utils.h"

#include "rtc_base/logging.h"
#include "rtc_base/ref_counted_object.h"

SetSessionDescriptionObserver* SetSessionDescriptionObserver::Create(
    const std::string& tag) {
  return new rtc::RefCountedObject<SetSessionDescriptionObserver>(tag);
}

SetSessionDescriptionObserver::SetSessionDescriptionObserver(
    const std::string& tag)
    : tag_(tag) {}

SetSessionDescriptionObserver::SetSessionDescriptionObserver() {}

void SetSessionDescriptionObserver::OnSuccess() {
  RTC_LOG(LS_INFO) << "OnSuccess " << tag_;
}

void SetSessionDescriptionObserver::OnFailure(webrtc::RTCError error) {
  RTC_LOG(LS_INFO) << "OnFailure " << tag_ << " error type, " << ToString(error.type())
      << ", error message " << error.message();
}

void SetSessionDescriptionObserver::SetDescription(
    std::unique_ptr<webrtc::SessionDescriptionInterface>&& desc) {
  desc_ = std::move(desc);
}
