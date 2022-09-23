
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

namespace {

class CrashHandler {
 public:
  CrashHandler();
  ~CrashHandler();
#if defined(WEBRTC_LINUX) || defined(WEBRTC_ANDROID)
  bool CrashFilterLinux();
  bool MinidumpCreatedLinux(const char* dump_file_path,
                            bool succeeded);
#elif defined(WEBRTC_WIN)
  bool CrashFilterWin(EXCEPTION_POINTERS* exinfo,
                      MDRawAssertionInfo* assertion);
  bool MinidumpCreatedWin(const char* dump_file_path,
                          EXCEPTION_POINTERS* exinfo,
                          MDRawAssertionInfo* assertion,
                          bool succeeded);
#elif defined(WEBRTC_MAC)
  bool CrashFilterMac();
  bool MinidumpCreatedMac(const char* dump_file_path,
                          bool succeeded);
#endif

  bool ConfigCrashDumpPath(const std::string& dump_path,
                           CrashDumpCallback callback,
                           void* callback_context);

 private:
  bool CrashFilter();
  bool MinidumpCreated(const char* dump_file_path, bool succeeded);

 private:
  std::unique_ptr<google_breakpad::ExceptionHandler> exception_handler_;

  std::mutex lock_;
  std::string crashdump_dir_path_;
  CrashDumpCallback crashdump_callback_;
  void* crashdump_callback_context_;
};

#if defined(WEBRTC_LINUX) || defined(WEBRTC_ANDROID)
static bool FilterCallbackLinux(void* context) {
  auto crash_handler = reinterpret_cast<CrashHandler*>(context);
  return crash_handler->CrashFilterLinux();
}

static bool MinidumpCallbackLinux(const google_breakpad::MinidumpDescriptor& descriptor,
                                  void* context, bool succeeded) {
  auto crash_handler = reinterpret_cast<CrashHandler*>(context);
  return crash_handler->MinidumpCreatedLinux(descriptor.path(), succeeded);
}

#elif defined(WEBRTC_WIN)
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
  auto crash_handler = reinterpret_cast<CrashHandler*>(context);
  return crash_handler->CrashFilterWin(exinfo, assertion);
}

static bool MinidumpCallbackWin(const wchar_t* dump_path,
                         const wchar_t* minidump_id,
                         void* context,
                         EXCEPTION_POINTERS* exinfo,
                         MDRawAssertionInfo* assertion,
                         bool succeeded) {
  auto crash_handler = reinterpret_cast<CrashHandler*>(context);

  std::wstring dump_path_wstr(dump_path);
  std::string dump_path_str;
  dump_path_str.assign(dump_path_wstr.begin(), dump_path_wstr.end());

  std::wstring minidump_id_wstr(minidump_id);
  std::string minidump_id_str;
  minidump_id_str.assign(minidump_id_wstr.begin(), minidump_id_wstr.end());

  std::string filepath = dump_path_str + "\\" + minidump_id_str + ".dmp";
  return crash_handler->MinidumpCreatedWin(filepath.c_str(), exinfo, assertion,
                                           succeeded);
}

#elif defined(WEBRTC_MAC)
static bool FilterCallbackMac(void* context) {
  auto crash_handler = reinterpret_cast<CrashHandler*>(context);
  return crash_handler->CrashFilterMac();
}

static bool MinidumpCallbackMac(const char* dump_dir,
                                const char* minidump_id,
                                void* context, bool succeeded) {
  auto crash_handler = reinterpret_cast<CrashHandler*>(context);

  std::string dump_path_str(dump_dir);
  std::string minidump_id_str(minidump_id);

  std::string filepath = dump_path_str + "/" + minidump_id_str + ".dmp";
  return crash_handler->MinidumpCreatedMac(filepath.c_str(), succeeded);
}
#endif

CrashHandler::CrashHandler() {
#if defined(WEBRTC_LINUX) || defined(WEBRTC_ANDROID)
    google_breakpad::MinidumpDescriptor descriptor("");
    exception_handler_ = std::make_unique<google_breakpad::ExceptionHandler>(
        descriptor, FilterCallbackLinux, MinidumpCallbackLinux, this, true, -1);
#elif defined(WEBRTC_WIN)
  exception_handler_ = std::make_unique<google_breakpad::ExceptionHandler>(
      stringToWstring(""),
      FilterCallbackWin,    // FilterCallback filter,
      MinidumpCallbackWin,  // MinidumpCallback callback,
      this,                 // void* callback_context,
      google_breakpad::ExceptionHandler::HANDLER_ALL  // int handler_types,
  );
#elif defined(WEBRTC_MAC)
  exception_handler_ = std::make_unique<google_breakpad::ExceptionHandler>(
      "", FilterCallbackMac, MinidumpCallbackMac, this, true, nullptr);
#endif
}

CrashHandler::~CrashHandler() {}

bool CrashHandler::CrashFilter() {
  std::string dir_path;
  {
    std::lock_guard<std::mutex> lk(lock_);
    dir_path = crashdump_dir_path_;
  }
  if (dir_path.empty()) {
    return false;
  }
  return true;
}

bool CrashHandler::MinidumpCreated(const char* dump_file_path,
                                   bool succeeded) {
  CrashDumpCallback callback = nullptr;
  void* context = nullptr;
  {
    std::lock_guard<std::mutex> lk(lock_);
    callback = crashdump_callback_;
    context = crashdump_callback_context_;
  }

  bool result = false;
  if (callback) {
    result = callback(dump_file_path, context, succeeded);
  }
#if defined(WEBRTC_ANDROID)
   return false;
#else
  return result;
#endif
}

#if defined(WEBRTC_LINUX) || defined(WEBRTC_ANDROID)
bool CrashHandler::CrashFilterLinux() {
  return CrashFilter();
}

bool CrashHandler::MinidumpCreatedLinux(const char* dump_file_path,
                                        bool succeeded){
  return MinidumpCreated(dump_file_path, succeeded);
}

#elif defined(WEBRTC_WIN)
bool CrashHandler::CrashFilterWin(EXCEPTION_POINTERS* exinfo,
                                  MDRawAssertionInfo* assertion) {
  return CrashFilter();
}

bool CrashHandler::MinidumpCreatedWin(const char* dump_file_path,
                                      EXCEPTION_POINTERS* exinfo,
                                      MDRawAssertionInfo* assertion,
                                      bool succeeded){
  return MinidumpCreated(dump_file_path, succeeded);
}

#elif defined(WEBRTC_MAC)
bool CrashHandler::CrashFilterMac() {
  return CrashFilter();
}

bool CrashHandler::MinidumpCreatedMac(const char* dump_file_path,
                                      bool succeeded){
  return MinidumpCreated(dump_file_path, succeeded);
}
#endif

bool CrashHandler::ConfigCrashDumpPath(const std::string& dump_path,
                                       CrashDumpCallback callback,
                                       void* callback_context) {
  std::lock_guard<std::mutex> lk(lock_);
  crashdump_dir_path_ = dump_path;
  crashdump_callback_ = callback;
  crashdump_callback_context_ = callback_context;

#if defined(WEBRTC_LINUX) || defined(WEBRTC_ANDROID)
  google_breakpad::MinidumpDescriptor descriptor(dump_path);
  exception_handler_->set_minidump_descriptor(descriptor);
#elif defined(WEBRTC_WIN)
  auto dump_path_str = stringToWstring(dump_path);
  exception_handler_->set_dump_path(dump_path_str);
#elif defined(WEBRTC_MAC)
  exception_handler_->set_dump_path(dump_path);
#endif
  return true;
}

}  // namespace

static std::mutex sLock;
static int32_t ref_count_ = 0;
static std::unique_ptr<CrashHandler> sCrashHandler;

static volatile bool sLoaded = false;
class LoadWatcher {
  public:
  LoadWatcher() {
    sLoaded = true;
  }
  ~LoadWatcher() {
    sLoaded = false;
  }
};

static LoadWatcher sLoadWatcher;

static void DoInstallCrashHandlerLocked() {
  if (!sCrashHandler) {
    sCrashHandler = std::make_unique<CrashHandler>();
  }
}

void InstallCrashHandler() {
  if (!sLoaded) {
    return;
  }
  std::lock_guard<std::mutex> lk(sLock);
  if (!sCrashHandler && ref_count_ == 0) {
    DoInstallCrashHandlerLocked();
  }
  ++ref_count_;
}

bool ConfigCrashDumpPath(const char* dump_path,
                         CrashDumpCallback callback,
                         void* callback_context) {
  if (!sLoaded) {
    return false;
  }
  std::lock_guard<std::mutex> lk(sLock);
  if (!sCrashHandler) {
    DoInstallCrashHandlerLocked();
  }
  if (sCrashHandler) {
    return sCrashHandler->ConfigCrashDumpPath(dump_path, callback,
                                              callback_context);
  }
  return false;
}

void UnInstallCrashHandler() {
  if (!sLoaded) {
    return;
  }
  std::lock_guard<std::mutex> lk(sLock);
  if (ref_count_ == 1) {
    sCrashHandler.reset();
  }
  if (ref_count_ > 0) {
    --ref_count_;
  }
}

}  // namespace sszrtc
