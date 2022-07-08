
#include "crash_handler.h"

#if defined(WEBRTC_WIN)
#include <codecvt>
#endif
#include <memory>
#include <mutex>
#if defined(WEBRTC_WIN)
#include <string>
#endif

#if defined(WEBRTC_LINUX) || defined(WEBRTC_ANDROID)
#include "client/linux/handler/exception_handler.h"
#elif defined(WEBRTC_MAC) || defined(WEBRTC_IOS)
#include "client/mac/handler/exception_handler.h"
#elif defined(WEBRTC_WIN)
#include "client/windows/handler/exception_handler.h"
#endif

namespace open_rtc {

static std::mutex sLock;
static FilterCallback sFilterCallback = nullptr;
static MinidumpCallback sMinidumpCallback = nullptr;
static std::unique_ptr<google_breakpad::ExceptionHandler> sExceptionHandler;

#if defined(WEBRTC_LINUX) || defined(WEBRTC_ANDROID)
static bool MinidumpCallbackLinux(
    const google_breakpad::MinidumpDescriptor& descriptor,
    void* context,
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

#if defined(WEBRTC_WIN)
static std::wstring stringToWstring(const std::string& str) {
  // setup converter
  typedef std::codecvt_utf8<wchar_t> convert_type;
  std::wstring_convert<convert_type, wchar_t> converter;

  // use converter (.to_bytes: wstr->str, .from_bytes: str->wstr)
  return converter.from_bytes(str);
}

static bool FilterCallbackWin(void* context,
                       EXCEPTION_POINTERS* exinfo,
                       MDRawAssertionInfo* assertion) {
  if (sFilterCallback) {
    return sFilterCallback(context);
  }
  return true;
}

static bool MinidumpCallbackWin(const wchar_t* dump_path,
                         const wchar_t* minidump_id,
                         void* context,
                         EXCEPTION_POINTERS* exinfo,
                         MDRawAssertionInfo* assertion,
                         bool succeeded) {
  if (sMinidumpCallback) {
    std::wstring dump_path_wstr(dump_path);
    std::string dump_path_str;
    dump_path_str.assign(dump_path_wstr.begin(), dump_path_wstr.end());

    std::wstring minidump_id_wstr(minidump_id);
    std::string minidump_id_str;
    minidump_id_str.assign(minidump_id_wstr.begin(), minidump_id_wstr.end());

    return sMinidumpCallback(dump_path_str.c_str(), minidump_id_str.c_str(),
                             context, succeeded);
  }
  return true;
}
#endif

void InstallCrashHandler(const std::string& dump_path,
                         FilterCallback filter,
                         MinidumpCallback callback,
                         void* callback_context) {
  std::lock_guard<std::mutex> lk(sLock);
  if (!sExceptionHandler) {
    sFilterCallback = filter;
    sMinidumpCallback = callback;
#if defined(WEBRTC_LINUX) || defined(WEBRTC_ANDROID)
    google_breakpad::MinidumpDescriptor descriptor(dump_path);
    sExceptionHandler = std::make_unique<google_breakpad::ExceptionHandler>(
        descriptor, filter, MinidumpCallbackLinux, callback_context, true, -1);
#elif defined(WEBRTC_MAC)
    sExceptionHandler = std::make_unique<google_breakpad::ExceptionHandler>(
        dump_path, filter, callback, callback_context, true, nullptr);
#elif defined(WEBRTC_WIN)
    std::wstring w_dump_path = stringToWstring(dump_path);

    sExceptionHandler = std::make_unique<google_breakpad::ExceptionHandler>(
        w_dump_path,
        FilterCallbackWin,    // FilterCallback filter,
        MinidumpCallbackWin,  // MinidumpCallback callback,
        callback_context,     // void* callback_context,
        google_breakpad::ExceptionHandler::HANDLER_ALL  // int handler_types,
    );
#endif
  }
}

void UnInstallCrashHandler() {
  std::lock_guard<std::mutex> lk(sLock);
  sExceptionHandler.reset();
  sFilterCallback = nullptr;
  sMinidumpCallback = nullptr;
}

}  // namespace sszrtc
