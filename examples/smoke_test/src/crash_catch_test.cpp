
#include <codecvt>

#include "media_test_utils/test_base.h"
#if defined(WEBRTC_LINUX) || defined(WEBRTC_ANDROID)
#include "third_party/breakpad/breakpad/src/client/linux/handler/exception_handler.h"
#elif defined(WEBRTC_MAC)
#include "third_party/breakpad/breakpad/src/client/mac/handler/exception_handler.h"
#elif defined(WEBRTC_WIN)
#include "third_party/breakpad/breakpad/src/client/windows/handler/exception_handler.h"
#endif

#include "third_party/breakpad/breakpad/src/client/crash_handler.h"

#if defined(WEBRTC_LINUX) || defined(WEBRTC_ANDROID)
static bool MinidumpCallback(
    const google_breakpad::MinidumpDescriptor& descriptor,
    void* context,
    bool succeeded) {
  printf("Dump path: %s\n", descriptor.path());
  return succeeded;
}
#elif defined(WEBRTC_MAC)
bool MinidumpCallback(const char* dump_dir,
                      const char* minidump_id,
                      void* context,
                      bool succeeded) {
  printf("Dump path: %s, minidump_id %s\n", dump_dir, minidump_id);
  return succeeded;
}
#endif

bool minidumpCallbackGeneral(const char* dump_file_path,
                             void* context,
                            bool succeeded) {
  printf("Dump file path: %s\n", dump_file_path);
  return succeeded;
}

class CrashCatchTest : public testing::Test {
 public:
  void SetUp() override {}

  void TearDown() override {}
};

static void crashfunc() {
  volatile int* a = (int*)(NULL);
  *a = 1;
}

#if defined(WEBRTC_POSIX)
__attribute__((destructor)) static void UninitializeCrashHandler(void) {
  open_rtc::UnInstallCrashHandler();
}
#endif

TEST_F(CrashCatchTest, DISABLED_crash_catch_common) {
  open_rtc::UnInstallCrashHandler();
  printf("Build root %s\n", BUILD_ROOT);
  open_rtc::InstallCrashHandler();

  open_rtc::ConfigCrashDumpPath(BUILD_ROOT, minidumpCallbackGeneral, nullptr);
  crashfunc();
}

#if defined(WEBRTC_WIN)
std::wstring stringToWstring(const std::string& t_str) {
  // setup converter
  typedef std::codecvt_utf8<wchar_t> convert_type;
  std::wstring_convert<convert_type, wchar_t> converter;

  // use converter (.to_bytes: wstr->str, .from_bytes: str->wstr)
  return converter.from_bytes(t_str);
}
#endif

TEST_F(CrashCatchTest, DISABLED_crash_catch) {
#if defined(WEBRTC_LINUX) || defined(WEBRTC_ANDROID)
  google_breakpad::MinidumpDescriptor descriptor(BUILD_ROOT);
  google_breakpad::ExceptionHandler eh(descriptor, NULL, MinidumpCallback, NULL,
                                       true, -1);
#elif defined(WEBRTC_MAC)
  google_breakpad::ExceptionHandler eh(BUILD_ROOT, NULL, MinidumpCallback, NULL,
                                       true, nullptr);
#elif defined(WEBRTC_WIN)
  std::wstring dump_path = stringToWstring(".");

  auto handler_ptr = std::make_unique<google_breakpad::ExceptionHandler>(
      dump_path,
      nullptr,  // FilterCallback filter,
      nullptr,  // MinidumpCallback callback,
      nullptr,  // void* callback_context,
      google_breakpad::ExceptionHandler::HANDLER_ALL  // int handler_types,
  );
#endif
  crashfunc();
}
