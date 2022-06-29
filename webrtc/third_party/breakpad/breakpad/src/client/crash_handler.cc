
#include "crash_handler.h"

#include <memory>
#include <mutex>

#if defined(WEBRTC_LINUX) || defined(WEBRTC_ANDROID)
#include "client/linux/handler/exception_handler.h"
#elif defined(WEBRTC_MAC) || defined(WEBRTC_IOS)
#include "client/mac/handler/exception_handler.h"
#elif defined(WEBRTC_WIN)
#include "client/windows/handler/exception_handler.h"
#endif

namespace open_rtc {

static std::mutex sLock;
MinidumpCallback sMinidumpCallback = nullptr;
static std::unique_ptr<google_breakpad::ExceptionHandler> sExceptionHandler;

#if defined(WEBRTC_LINUX) || defined(WEBRTC_ANDROID)
static bool minidumpCallback(
    const google_breakpad::MinidumpDescriptor &descriptor, void *context,
    bool succeeded) {
  if (sMinidumpCallback) {
    std::string path(descriptor.path());
    auto start_pos = path.rfind('/') + 1;
    auto end_pos = path.rfind('.');
    auto minidump_id = path.substr(start_pos, end_pos - start_pos);
    return sMinidumpCallback(descriptor.directory().c_str(),
        minidump_id.c_str(), context, succeeded);
  }
  return succeeded;
}
#endif

void InstallCrashHandler(const std::string& dump_path,
                         FilterCallback filter,
                         MinidumpCallback callback,
                         void* callback_context) {
  std::lock_guard<std::mutex> lk(sLock);
  if (!sExceptionHandler) {
#if defined(WEBRTC_LINUX) || defined(WEBRTC_ANDROID)
    sMinidumpCallback = callback;
    google_breakpad::MinidumpDescriptor descriptor(dump_path);
    sExceptionHandler = std::make_unique<google_breakpad::ExceptionHandler>(
        descriptor, filter, minidumpCallback, callback_context, true, -1);

#elif defined(WEBRTC_MAC)
    sExceptionHandler = std::make_unique<google_breakpad::ExceptionHandler>(
        dump_path, filter, callback, callback_context, true, nullptr);
#endif
  }
}

void UnInstallCrashHandler() {
  std::lock_guard<std::mutex> lk(sLock);
  sExceptionHandler.reset();
}

}  // namespace sszrtc
