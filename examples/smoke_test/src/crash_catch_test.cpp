
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

bool minidumpCallback(const char* dump_dir,
                      const char* minidump_id,
                      void* context,
                      bool succeeded) {
  printf("Dump path: %s, minidump_id %s\n", dump_dir, minidump_id);
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

TEST_F(CrashCatchTest, DISABLED_crash_catch_common) {
  open_rtc::InstallCrashHandler(BUILD_ROOT, nullptr, minidumpCallback, nullptr);
  crashfunc();
}

TEST_F(CrashCatchTest, DISABLED_crash_catch) {
#if defined(WEBRTC_LINUX) || defined(WEBRTC_ANDROID)
  google_breakpad::MinidumpDescriptor descriptor(BUILD_ROOT);
  google_breakpad::ExceptionHandler eh(descriptor, NULL, MinidumpCallback, NULL,
                                       true, -1);
#elif defined(WEBRTC_MAC)
  printf("Build root %s\n", BUILD_ROOT);
  google_breakpad::ExceptionHandler eh(BUILD_ROOT, NULL, MinidumpCallback, NULL,
                                       true, nullptr);
#endif
  crashfunc();
}
